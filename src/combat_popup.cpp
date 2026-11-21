#include "combat_popup.h"

#include "argb.h"
#include "auto_check.h"
#include "bar_content.h"
#include "bar_widgets.h"
#include "status_icons.h"
#include "bar_statuses.h"
#include "markup.h"
#include "plate_image.h"
#include "screen_vertex.h"

#include <algorithm>
#include <charconv>
#include <cmath>
#include <cstring>
#include <utility>

namespace headsup
{
    void PopupTies::Ties::Put(uintptr_t key, const PopupEvent& event)
    {
        const auto known = std::find(keys.begin(), keys.end(), key);
        const size_t at  = known != keys.end() ? static_cast<size_t>(known - keys.begin())
                                               : std::exchange(next, (next + 1) % kKeptTies);
        keys[at]   = key;
        events[at] = event;
    }

    void PopupTies::Ties::Forget(uintptr_t key) { std::replace(keys.begin(), keys.end(), key, uintptr_t{0}); }

    std::optional<PopupEvent> PopupTies::Ties::Of(uintptr_t key) const
    {
        const auto known = std::find(keys.begin(), keys.end(), key);
        if (key == 0 || known == keys.end()) return std::nullopt;
        return events[static_cast<size_t>(known - keys.begin())];
    }

    namespace
    {
        // By the server's msg.lua ids of the messages whose value is the status put on the target: gains, receives and
        // is, from a spell, an ability, a weapon skill, an item or an additional effect.
        constexpr uint32_t kStatusMessages[] = {127, 160, 164, 166, 186, 230, 236, 237, 242, 243, 266, 267, 268, 271, 277, 278,
            319, 320, 374, 375};
    }

    bool IsStatusMessage(uint32_t message) { return std::ranges::find(kStatusMessages, message) != std::end(kStatusMessages); }

    namespace
    {
        // By the server's msg_basic.h and msg.lua ids of the messages whose value is damage the actor dealt: a result's
        // own, and an additional effect's (a skillchain's too, SkillchainDamage). Spikes and counters are the target's
        // damage to the actor, so they are left out.
        constexpr uint32_t kDamageMessages[] = {1, 2, 67, 77, 110, 157, 185, 187, 197, 227, 252, 264, 274, 281, 317, 352,
            353, 379, 522, 576, 577, 747};
        constexpr uint32_t kAddedDamageMessages[] = {161, 163, 229};
    }

    namespace
    {
        struct Skillchain
        {
            std::string_view name;
            Element element; // its color's: one of the two or four it is made of
        };
        // By the server's ActionProcSkillChain, from 1.
        constexpr Skillchain kSkillchains[] = {{"Light", Element::Light}, {"Darkness", Element::Dark},
            {"Gravitation", Element::Earth}, {"Fragmentation", Element::Lightning}, {"Distortion", Element::Ice},
            {"Fusion", Element::Fire}, {"Compression", Element::Dark}, {"Liquefaction", Element::Fire},
            {"Induration", Element::Ice}, {"Reverberation", Element::Water}, {"Transfixion", Element::Light},
            {"Scission", Element::Earth}, {"Detonation", Element::Wind}, {"Impaction", Element::Lightning},
            {"Radiance", Element::Light}, {"Umbra", Element::Dark}};
        constexpr uint32_t kSkillchainCount = std::size(kSkillchains);
        // The server sends 287 + n for a skillchain's damage and 384 + n for what it heals. The client's own table
        // also pops Radiance and Umbra as these.
        constexpr uint32_t kSkillchainDamage = 287, kSkillchainHealing = 384;
        constexpr uint32_t kRadianceDamage = 767, kUmbraDamage = 768, kRadianceHealing = 769, kUmbraHealing = 770;
        constexpr uint32_t kRadiance = 15, kUmbra = 16;
        // The game's kinds of popup, from its decision.
        constexpr uint8_t kDamage = 1, kRecovery = 2, kMiss = 3;

        std::optional<Skillchain> SkillchainOf(uint32_t message)
        {
            for (const uint32_t first : {kSkillchainDamage, kSkillchainHealing})
                if (message > first && message <= first + kSkillchainCount) return kSkillchains[message - first - 1];
            if (message == kRadianceDamage || message == kRadianceHealing) return kSkillchains[kRadiance - 1];
            if (message == kUmbraDamage || message == kUmbraHealing) return kSkillchains[kUmbra - 1];
            return std::nullopt;
        }

        bool SkillchainDamage(uint32_t message)
        {
            return (message > kSkillchainDamage && message <= kSkillchainDamage + kSkillchainCount) ||
                   message == kRadianceDamage || message == kUmbraDamage;
        }

        // By the server's msg_basic.h ids; the first that fits wins. 264, damage to an area spell's next target, is the
        // spell's when the popup before it, from the same actor, was a spell's.
        std::optional<PopupType> TypeOf(const PopupEvent& e, bool byYou, bool onYou, bool afterSpell)
        {
            if (IsStatusMessage(e.message)) return e.kind == kDamage ? PopupType::Debuff : PopupType::Buff;
            if (SkillchainOf(e.message)) return PopupType::Skillchain;
            switch (e.message)
            {
            case 252:
            case 379:
            case 747: return PopupType::Burst;
            case 67:
            case 353: return PopupType::Critical;
            case 2: return PopupType::Spell;
            case 264:
                if (afterSpell) return PopupType::Spell;
                break;
            default: break;
            }
            switch (e.kind)
            {
            case kRecovery: return PopupType::Recovery;
            case kMiss: return PopupType::Miss;
            case kDamage:
                if (byYou) return PopupType::Dealt;
                if (onYou) return PopupType::Taken;
                return std::nullopt;
            default: return std::nullopt;
            }
        }
    }

    namespace
    {
        // Plain damage between others goes where yours dealt goes on a mob and where yours taken goes on a player or pet;
        // a popup tied to no message goes where yours dealt goes.
        PopupSide SideOf(const std::optional<PopupEvent>& e, const PopupSides& sides)
        {
            const PopupType kind = !e        ? PopupType::Dealt
                                   : e->type ? *e->type
                                   : e->target < kFirstPlayerIndex ? PopupType::Dealt
                                                                   : PopupType::Taken;
            return sides[static_cast<size_t>(kind)];
        }
    }

    uint8_t StatusPopupKind(uint32_t message, uint32_t value)
    {
        const auto status = static_cast<uint16_t>(value);
        if (!IsStatusMessage(message) || status != value || *StatusName(status) == '\0') return 0;
        return IsDebuff(status) ? kDamage : kRecovery;
    }

    namespace
    {
        // The server's packing: fields of bits from byte kActionBits / 8, each from its lowest bit.
        constexpr uint32_t kActionBits = 40;
        constexpr uint32_t kActorBits = 32, kTargetsBits = 6, kResultsSumBits = 4, kCommandBits = 4, kArgumentBits = 32,
                           kInfoBits = 32, kTargetIdBits = 32, kResultsBits = 4;
        // A result: miss, kind, animation, info, distortion and knockback, then value, message and modifier; an
        // additional effect's 10 bits, value and message, and spikes' 34 bits, after a bit each that says they are there.
        constexpr uint32_t kResultLeadBits = 3 + 2 + 12 + 5 + 2 + 3, kValueBits = 17, kMessageBits = 10, kModifierBits = 31,
                           kAddedLeadBits = 6 + 4, kSpikesBits = 6 + 4 + 14 + 10;

        class BitReader
        {
        public:
            BitReader(const uint8_t* data, uint32_t size) : m_Data(data), m_Bits(size * 8) {}
            // Nothing once past the end.
            std::optional<uint32_t> Read(uint32_t bits)
            {
                if (m_Bit + bits > m_Bits) return std::nullopt;
                uint32_t value = 0;
                for (uint32_t i = 0; i < bits; ++i, ++m_Bit)
                    value |= static_cast<uint32_t>(m_Data[m_Bit / 8] >> m_Bit % 8 & 1) << i;
                return value;
            }
            void Skip(uint32_t bits) { m_Bit += bits; }

        private:
            const uint8_t* m_Data;
            uint32_t m_Bits, m_Bit = kActionBits;
        };
    }

    std::optional<SentAction> ParseSentAction(const uint8_t* data, uint32_t size)
    {
        BitReader bits(data, size);
        SentAction sent;
        const std::optional<uint32_t> actor = bits.Read(kActorBits), targets = bits.Read(kTargetsBits);
        if (!actor || !targets) return std::nullopt;
        sent.actor = *actor;
        bits.Skip(kResultsSumBits + kCommandBits + kArgumentBits + kInfoBits);
        const bool lone = *targets == 1;
        for (uint32_t t = 0; t < *targets; ++t)
        {
            bits.Skip(kTargetIdBits);
            const std::optional<uint32_t> results = bits.Read(kResultsBits);
            if (!results) return std::nullopt;
            for (uint32_t r = 0; r < *results; ++r)
            {
                bits.Skip(kResultLeadBits);
                const std::optional<uint32_t> value = bits.Read(kValueBits), message = bits.Read(kMessageBits);
                bits.Skip(kModifierBits);
                const std::optional<uint32_t> added = bits.Read(1);
                std::optional<uint32_t> addedValue = 0, addedMessage = 0;
                if (added && *added)
                {
                    bits.Skip(kAddedLeadBits);
                    addedValue   = bits.Read(kValueBits);
                    addedMessage = bits.Read(kMessageBits);
                }
                const std::optional<uint32_t> spikes = bits.Read(1);
                if (spikes && *spikes) bits.Skip(kSpikesBits);
                if (!value || !message || !addedValue || !addedMessage || !spikes) return std::nullopt;
                if (std::ranges::find(kDamageMessages, *message) != std::end(kDamageMessages)) sent.damage += *value;
                if (std::ranges::find(kAddedDamageMessages, *addedMessage) != std::end(kAddedDamageMessages) ||
                    SkillchainDamage(*addedMessage))
                    sent.damage += *addedValue;
                const bool kept = lone ? r < kMostLoneResults : r == 0;
                if (kept && sent.records < kMostSentRecords) sent.messages[sent.records++] = static_cast<uint16_t>(*message);
            }
        }
        return sent;
    }

    void SentActions::Put(const SentAction& action)
    {
        if (m_Count < kKept) m_Kept[m_Count++] = action;
    }

    std::optional<SentAction> SentActions::Take(uint32_t actor, uint8_t records)
    {
        for (size_t i = 0; i < m_Count; ++i)
        {
            if (m_Kept[i].actor != actor || m_Kept[i].records != records) continue;
            const SentAction taken = m_Kept[i];
            std::copy(m_Kept.begin() + static_cast<std::ptrdiff_t>(i) + 1, m_Kept.begin() + static_cast<std::ptrdiff_t>(m_Count),
                m_Kept.begin() + static_cast<std::ptrdiff_t>(i));
            --m_Count;
            return taken;
        }
        return std::nullopt;
    }

    void EarlyPopups::Parsed(uint16_t* mark, const uint32_t* message, uint32_t sent)
    {
        const bool blanked = m_Restore && *message == 0 && IsStatusMessage(sent);
        *mark              = blanked ? static_cast<uint16_t>(kRestored | sent) : 0;
    }

    bool EarlyPopups::Begin(uint16_t* mark, uint32_t* message)
    {
        if (!m_On) return false;
        const uint16_t was = *mark;
        *mark              = 0;
        m_Mark             = mark;
        Restore(was, message);
        return true;
    }

    void EarlyPopups::End()
    {
        m_Mark = nullptr;
        Unrestore();
    }

    void EarlyPopups::Show(uint16_t* mark, uint32_t* message)
    {
        m_Skip = mark != nullptr && *mark == kEarlyPopped;
        if (m_Skip) *mark = 0;
        else if (mark != nullptr) Restore(*mark, message);
    }

    void EarlyPopups::Shown()
    {
        m_Skip = false;
        Unrestore();
    }

    void EarlyPopups::Restore(uint16_t mark, uint32_t* message)
    {
        const uint32_t sent = mark & kRestoredMessage;
        if ((mark & ~kRestoredMessage) != kRestored || !IsStatusMessage(sent) || *message != 0) return;
        *message  = sent;
        m_Message = message;
    }

    void EarlyPopups::Unrestore()
    {
        if (m_Message != nullptr) *m_Message = 0;
        m_Message = nullptr;
    }

    void PopupTies::SetParty(std::span<const uint32_t> ids)
    {
        m_Party.fill(0);
        std::copy_n(ids.begin(), std::min(ids.size(), m_Party.size()), m_Party.begin());
    }

    uint8_t PopupTies::Decide(const uint32_t* actor, const uint32_t* target, uint32_t message, uint32_t value, uint8_t answer,
        uint8_t& kind) const
    {
        if (answer == 0 || kind != 0 || !m_Statuses) return answer;
        // Each an index then a server id.
        const auto ours = [&](const uint32_t* who) {
            return who[0] == m_You || (m_Pet != 0 && who[0] == m_Pet) || std::ranges::find(m_Party, who[1]) != m_Party.end();
        };
        const uint8_t status = StatusPopupKind(message, value);
        if (status == 0 || !(ours(actor) || ours(target))) return answer;
        kind = status;
        return 0;
    }

    void PopupTies::Decided(PopupEvent event)
    {
        const auto yours      = [&](uint32_t index) { return index == m_You || (m_Pet != 0 && index == m_Pet); };
        const bool afterSpell = m_Last && m_Last->actor == event.actor &&
                                (m_Last->type == PopupType::Spell || m_Last->type == PopupType::Burst);
        event.type = TypeOf(event, yours(event.actor), yours(event.target), afterSpell);
        m_Pending = m_Last = event;
    }

    // A task or particle the game makes again at an address it freed is a new one.
    void PopupTies::Started(uintptr_t task)
    {
        if (!m_Pending)
        {
            m_Tasks.Forget(task);
            return;
        }
        PopupEvent event = *std::exchange(m_Pending, std::nullopt);
        std::array<bool, kKeptTies + 1> held{}; // the kept ties hold at most kKeptTies lines
        for (size_t i = 0; i < kKeptTies; ++i)
        {
            const PopupEvent& e = m_Tasks.events[i];
            if (m_Spread && m_Tasks.keys[i] != 0 && e.target == event.target && m_Now - e.started < kPopupSpreadSeconds &&
                SideOf(e, m_Sides) == SideOf(event, m_Sides))
                held[e.line] = true;
        }
        event.started = m_Now;
        event.line    = static_cast<uint8_t>(std::find(held.begin(), held.end(), false) - held.begin());
        m_Tasks.Put(task, event);
    }

    void PopupTies::Built(uintptr_t particle, uintptr_t task)
    {
        if (const std::optional<PopupEvent> event = m_Tasks.Of(task)) m_Particles.Put(particle, *event);
        else m_Particles.Forget(particle);
    }

    PopupEvent SampleEvent(const PopupSample& sample, uint32_t you, uint32_t other)
    {
        return PopupEvent{sample.message, sample.yours ? you : other, you, sample.kind};
    }

    namespace
    {
        // By the server's msg_basic.h ids, each with the client's kind for it (its byte table at 0x35CAE0).
        constexpr PopupSample kTestSamples[] = {{1, 128, kDamage, true}, {1, 64, kDamage, false}, {67, 210, kDamage, true},
            {2, 356, kDamage, true}, {252, 512, kDamage, true}, {kSkillchainDamage + 6, 412, kDamage, true},
            {7, 34, kRecovery, false}, {25, 20, kRecovery, false}, {15, 0, kMiss, false}, {32, 0, kMiss, false},
            {230, 40, kRecovery, true}, {236, 4, kDamage, false}}; // Protect, and Paralysis
    }

    std::optional<PopupSample> PopupTestRun::Next(double now)
    {
        if (m_Next >= std::size(kTestSamples) || now < m_Start + static_cast<double>(m_Next) * kTestGap) return std::nullopt;
        return kTestSamples[m_Next++];
    }

    void CombatPopupList::NewFrame()
    {
        m_Popups.clear();
        m_Open = false;
    }

    void CombatPopupList::Begin(const char* text, uintptr_t particle, std::optional<PopupEvent> event)
    {
        m_Particle = particle;
        m_Event    = event;
        // The game's text has no terminator when it fills its 16 bytes: nothing past them is read.
        const size_t length = text != nullptr ? strnlen(text, kMaxCombatPopupText) : 0;
        if (length > 0) std::memcpy(m_Text, text, length);
        m_Text[length] = '\0';
        m_Open         = true;
        m_Added        = false;
    }

    void CombatPopupList::Add(const void* vertices, size_t count, size_t stride)
    {
        if (!m_Open || vertices == nullptr || count == 0 || stride < sizeof(ScreenVertex)) return;
        if (!std::exchange(m_Added, true))
        {
            m_Popups.emplace_back();
            std::memcpy(m_Popups.back().text, m_Text, sizeof(m_Text));
            m_Popups.back().particle = m_Particle;
            m_Popups.back().event    = m_Event;
        }
        CombatPopup& popup = m_Popups.back();
        const auto* bytes  = static_cast<const uint8_t*>(vertices);
        for (size_t i = 0; i < count; ++i)
        {
            ScreenVertex vertex{};
            std::memcpy(&vertex, bytes + i * stride, sizeof(vertex));
            popup.box.Add(vertex.x, vertex.y);
            if (i == 0)
            {
                popup.depth = vertex.z;
                popup.color = vertex.color;
            }
        }
    }
}

namespace headsup
{
    namespace
    {
        // By the server's msg_basic.h ids of the messages the game shows as a miss.
        std::string_view MissWord(uint32_t message)
        {
            switch (message)
            {
            case 32:
            case 282: return "Evade";
            case 70: return "Parry";
            case 30: return "Anticipate";
            case 14:
            case 31:
            case 535: return "Shadow";
            case 85:
            case 284:
            case 655: return "Resist";
            case 75:
            case 156:
            case 189:
            case 283:
            case 323:
            case 355:
            case 408:
            case 422:
            case 423:
            case 425: return "No Effect";
            case 114:
            case 137:
            case 153:
            case 244: return "Fail";
            default: return kMissWord;
            }
        }

        constexpr std::string_view kDrainWord = "Drain ", kMpLabel = " MP", kTpLabel = " TP";
        struct AmountLabel
        {
            std::string_view before, after;
        };
        // By the server's msg_basic.h ids of amounts of MP and TP, and of HP, MP and TP drained.
        AmountLabel LabelOf(uint32_t message)
        {
            switch (message)
            {
            case 25:
            case 152:
            case 224:
            case 276:
            case 451:
            case 588: return {{}, kMpLabel};
            case 452: return {{}, kTpLabel};
            case 132:
            case 161:
            case 187:
            case 227: return {kDrainWord, {}};
            case 162:
            case 225:
            case 228: return {kDrainWord, kMpLabel};
            case 165:
            case 226:
            case 454: return {kDrainWord, kTpLabel};
            default: return {};
            }
        }
    }

    PopupSwitches PopupSwitchesOf(const Settings& settings) { return PopupSwitches{settings.combatPopupWords, settings.combatPopupLabels,
            settings.combatPopupCustomColors, settings.combatPopupSkillchainNames}; }

    std::string CombatPopupWords(std::string_view text, std::optional<PopupEvent> event, const PopupSwitches& on)
    {
        if (text == std::string_view(&kMissGlyph, 1)) return std::string(event && on.words ? MissWord(event->message) : kMissWord);
        if (const std::optional<Skillchain> skillchain = event && on.skillchainNames ? SkillchainOf(event->message) : std::nullopt)
            return std::string(skillchain->name) + " " + std::string(text);
        if (event && IsStatusMessage(event->message))
        {
            uint32_t status = 0;
            const auto [end, failed] = std::from_chars(text.data(), text.data() + text.size(), status);
            const char* name         = failed == std::errc{} && end == text.data() + text.size() && status <= UINT16_MAX
                                           ? StatusName(static_cast<uint16_t>(status))
                                           : "";
            if (*name != '\0') return name;
        }
        const AmountLabel label = event && on.labels ? LabelOf(event->message) : AmountLabel{};
        return std::string(label.before) + std::string(text) + std::string(label.after);
    }

    namespace
    {
        bool Twins(const CombatPopup& earlier, const CombatPopup& later)
        {
            return std::string_view(earlier.text) == later.text &&
                   std::hypot(later.box.CenterX() - earlier.box.CenterX(), later.box.CenterY() - earlier.box.CenterY()) <=
                       later.box.Height() / 2.0f;
        }
    }

    namespace
    {
        uint32_t PickedColor(const CombatPopup& p, const std::array<ColorPick, kPopupTypeCount>& colors)
        {
            const uint32_t game = ShownColor(p.color, kCombatPopupModulate);
            if (!p.event || !p.event->type) return game;
            const ColorPick& pick = colors[static_cast<size_t>(*p.event->type)];
            if (pick.source == ColorSource::Custom) return ToArgb(pick.custom);
            const std::optional<Skillchain> skillchain = SkillchainOf(p.event->message);
            return pick.source == ColorSource::Element && skillchain ? ElementColor(skillchain->element) : game;
        }
    }

    namespace
    {
        // The share of a popup's words left of its place, by side.
        constexpr float kStartsAt = 0.0f, kCentered = 0.5f, kEndsAt = 1.0f;

        // Where a popup the game put at center goes, its line counted.
        ShownCombatPopup Place(ShownCombatPopup shown, uint8_t line, std::optional<PopupBody> body, PopupSide side,
            const PopupPlacing& placing)
        {
            const float lines = static_cast<float>(line) * shown.lineHeight;
            const float clear =
                (body && body->feet ? kPopupBodyReach * (*body->feet - body->nameBottom) : shown.lineHeight) + placing.spread;
            if (side != PopupSide::Middle)
            {
                shown.center.x += side == PopupSide::Right ? clear : -clear;
                shown.across = side == PopupSide::Right ? kStartsAt : kEndsAt;
            }
            if (body && body->feet) shown.center.y = *body->feet + placing.height * (body->nameBottom - *body->feet);
            shown.center.y += lines + placing.nudge;
            return shown;
        }
    }

    std::vector<ShownCombatPopup> ShownCombatPopups(const std::vector<CombatPopup>& popups, int sizePercent,
        const std::array<ColorPick, kPopupTypeCount>& colors, const PopupPlacing& placing, const PopupSwitches& on,
        int fadePercent)
    {
        std::vector<ShownCombatPopup> shown;
        for (size_t i = 0; i < popups.size(); ++i)
        {
            const CombatPopup& p = popups[i];
            if (std::any_of(popups.begin() + static_cast<std::ptrdiff_t>(i) + 1, popups.end(),
                    [&](const CombatPopup& later) { return Twins(p, later); }))
                continue;
            const uint32_t game = std::min(Channel(p.color, kAlphaShift) * kCombatPopupModulate, kOpaqueAlpha);
            const float faded   = std::pow(static_cast<float>(game) / kOpaqueAlpha, static_cast<float>(fadePercent) / kPercent);
            const auto alpha    = static_cast<uint32_t>(std::lround(faded * kOpaqueAlpha));
            const std::optional<PopupBody> body = p.event && placing.bodyOf ? placing.bodyOf(p.event->target) : std::nullopt;
            const uint32_t color = on.colors ? PickedColor(p, colors) : ShownColor(p.color, kCombatPopupModulate);
            shown.push_back(Place(ShownCombatPopup{CombatPopupWords(p.text, p.event, on), color, alpha,
                                      ScreenPoint{p.box.CenterX(), p.box.CenterY()},
                                      p.box.Height() * static_cast<float>(sizePercent) / kPercent, p.depth, kCentered},
                p.event ? p.event->line : 0, body, SideOf(p.event, placing.sides), placing));
        }
        return shown;
    }

    SquareQuad PlaceCombatPopupText(ScreenPoint center, float lineHeight, const CombatTextSize& text, float pixelShape,
        float across)
    {
        const float fit = lineHeight / static_cast<float>(text.raster), width = text.width * fit * pixelShape;
        const float capitals = text.halo + (text.capitalTop + text.capitalBottom) / 2.0f;
        return SquareQuad{std::round(center.x - width * across), std::round(center.y - capitals * fit), width, text.height * fit};
    }

    int CombatPopupEdge(int raster, int thicknessPercent)
    {
        return WholePixels(static_cast<float>(raster) * kCombatPopupEdgePerHeight * static_cast<float>(thicknessPercent) / kPercent);
    }

    std::string DescribeCombatPopup(const CombatPopup& popup)
    {
        const ScreenBox& b = popup.box;
        return std::string(popup.text) + " | " + Printed("%08X", popup.color) + " | " + Decimal(b.minX, 2) + "," +
               Decimal(b.minY, 2) + " to " + Decimal(b.maxX, 2) + "," + Decimal(b.maxY, 2) + " | " + Decimal(popup.depth, 5) +
               " | message " + (popup.event ? std::to_string(popup.event->message) : "-");
    }

    std::string DescribeCombatPopup(const ShownCombatPopup& popup)
    {
        return popup.words + " | " + Printed("%08X", popup.color) + " alpha " + std::to_string(popup.alpha) + " | " +
               Decimal(popup.center.x, 2) + "," + Decimal(popup.center.y, 2) + " line " + Decimal(popup.lineHeight, 2) + " | " +
               Decimal(popup.depth, 5);
    }
}
