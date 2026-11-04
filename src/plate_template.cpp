#include "plate_template.h"

#include "utf16.h"

#include <algorithm>
#include <iterator>
#include <utility>

namespace headsup
{
    namespace
    {
        constexpr uint8_t kMobKinds    = KindBit(PlateKind::Mobs);
        constexpr uint8_t kPlayerKinds = KindBit(PlateKind::Players) | KindBit(PlateKind::Self);
        constexpr uint8_t kNpcKinds    = KindBit(PlateKind::Npcs);
        constexpr uint8_t kPetKinds    = KindBit(PlateKind::Pets);
        constexpr uint8_t kAnyKinds    = kMobKinds | kPlayerKinds | kNpcKinds | kPetKinds;

        constexpr bool kText = true;
        // The order is the chips' order on each page that offers the tag, so it need not follow the enum.
        const TagEntry kTags[] = {
            {Tag::Name, "$name", kAnyKinds, kText},
            {Tag::Gm, "$gm", kPlayerKinds, false, Icon::Gm, &PlayerStatus::gm},
            {Tag::Mentor, "$mentor", kPlayerKinds, false, Icon::Mentor, &PlayerStatus::mentor},
            {Tag::New, "$new", kPlayerKinds, false, Icon::NewAdventurer, &PlayerStatus::newAdventurer},
            {Tag::Sync, "$sync", kPlayerKinds, false, Icon::LevelSync, &PlayerStatus::levelSync},
            {Tag::Away, "$away", kPlayerKinds, false, Icon::Away, &PlayerStatus::away},
            {Tag::Linkshell, "$linkshell", kPlayerKinds, false, Icon::Linkshell, &PlayerStatus::linkshell},
            {Tag::Bazaar, "$bazaar", kPlayerKinds, false, Icon::Bazaar, &PlayerStatus::bazaar},
            {Tag::Seeking, "$seeking", kPlayerKinds, false, Icon::Invite, &PlayerStatus::seekingParty},
            {Tag::Job, "$job", kPlayerKinds, kText},
            {Tag::Level, "$level", kMobKinds | kPlayerKinds, kText},
            {Tag::JobLevel, "$joblevel", kPlayerKinds, kText},
            {Tag::SubLevel, "$sublevel", kPlayerKinds, kText},
            // The fishing bar's line (FishBarTags), on no nameplate's page.
            {Tag::Fish, "$fish", 0},
            {Tag::FishSize, "$size", 0},
            {Tag::Feeling, "$feeling", 0},
            {Tag::LineSnap, "$linesnap", 0},
            {Tag::RodBreak, "$rodbreak", 0},
            {Tag::Title, "$title", kPlayerKinds, kText},
            {Tag::Con, "$con", kMobKinds, kText},
            {Tag::Id, "$id", kMobKinds, kText},
            {Tag::ServerId, "$serverid", kMobKinds, kText},
            {Tag::Ph, "$ph", kMobKinds, kText},
            {Tag::Detect, "$detect", kMobKinds},
            {Tag::Weak, "$weak", kMobKinds, false, {}, nullptr, BadgeGroup::Weak},
            {Tag::Resist, "$resist", kMobKinds, false, {}, nullptr, BadgeGroup::Resist},
            {Tag::Status, "$status", kMobKinds, false, {}, nullptr, BadgeGroup::Status},
            {Tag::Immune, "$immune", kMobKinds, false, {}, nullptr, BadgeGroup::Immune},
            {Tag::Owner, "$owner", kPetKinds, kText},
            {Tag::Break, "$break", 0}, // Enter in a line's box makes it, and no page offers it as a chip
        };
        static_assert(std::size(kTags) == kTagCount);

        constexpr char kNameOpen = '{', kNameClose = '}'; // around a tag's name, where a letter follows it

        bool IsLower(char c) { return c >= 'a' && c <= 'z'; }

        // The tag whose name past its mark is the letters, if any.
        const TagEntry* FindTag(std::string_view letters)
        {
            for (const TagEntry& entry : kTags)
                if (letters == entry.name + 1) return &entry;
            return nullptr;
        }
    }

    const TagEntry& EntryOf(Tag tag) { return *std::ranges::find(kTags, tag, &TagEntry::tag); }

    const char* TagName(Tag tag) { return EntryOf(tag).name; }

    std::optional<Icon> TagIcon(Tag tag) { return EntryOf(tag).icon; }

    namespace
    {
        // A piece of a line's text: a slot, or text with what it shows (a doubled mark is one) beside what was typed.
        struct Token
        {
            TagSlot slot;
            std::string typed, shown;
        };

        std::vector<Token> Tokens(std::string_view text)
        {
            std::vector<Token> tokens;
            const auto literal = [&](std::string_view typed, std::string_view shown) {
                if (tokens.empty() || !tokens.back().slot.empty()) tokens.push_back(Token{});
                tokens.back().typed += typed;
                tokens.back().shown += shown;
            };
            for (size_t i = 0; i < text.size();)
            {
                if (text[i] != kTagMark)
                {
                    literal(text.substr(i, 1), text.substr(i, 1));
                    ++i;
                    continue;
                }
                if (i + 1 < text.size() && text[i + 1] == kTagMark)
                {
                    literal(text.substr(i, 2), text.substr(i, 1));
                    i += 2;
                    continue;
                }
                // The name straight after the mark, or in braces after it, where a bar sets more names beside it.
                const bool braced = i + 1 < text.size() && text[i + 1] == kNameOpen;
                size_t end        = i + (braced ? 2 : 1);
                TagSlot slot;
                bool known = true;
                for (bool more = true; more;)
                {
                    const size_t name = end;
                    while (end < text.size() && IsLower(text[end])) ++end;
                    const TagEntry* entry = FindTag(text.substr(name, end - name));
                    known                 = known && entry != nullptr;
                    if (entry != nullptr) slot.push_back(entry->tag);
                    more = braced && end < text.size() && text[end] == kSlotBar;
                    if (more) ++end;
                }
                const bool tags   = known && (!braced || (end < text.size() && text[end] == kNameClose));
                const size_t past = tags && braced ? end + 1 : end;
                if (tags)
                    tokens.push_back(Token{std::move(slot), {}, {}});
                else
                    literal(text.substr(i, past - i), text.substr(i, past - i));
                i = past;
            }
            return tokens;
        }

        bool OddMarksAtEnd(std::string_view text)
        {
            size_t marks = 0;
            while (marks < text.size() && text[text.size() - 1 - marks] == kTagMark) ++marks;
            return marks % 2 != 0;
        }

        // The stretch and the byte in it that a caret stands for in these runs.
        RunCaret Placed(const TemplateRuns& runs, RunCaret caret)
        {
            const int last = static_cast<int>(runs.texts.size()) - 1;
            if (caret.run < 0 || caret.run > last) return RunCaret{last, runs.texts.back().size()};
            return RunCaret{caret.run, std::min(caret.at, runs.texts[static_cast<size_t>(caret.run)].size())};
        }
    }

    std::vector<Atom> ParseTemplate(std::string_view text)
    {
        std::vector<Atom> atoms;
        for (Token& token : Tokens(text))
            atoms.push_back(Atom{std::move(token.slot), std::move(token.shown)});
        return atoms;
    }

    TemplateRuns RunsOf(std::string_view text)
    {
        TemplateRuns runs{{std::string()}, {}};
        for (Token& token : Tokens(text))
        {
            if (token.slot.empty())
            {
                runs.texts.back() = std::move(token.typed);
                continue;
            }
            runs.slots.push_back(std::move(token.slot));
            runs.texts.emplace_back();
        }
        return runs;
    }

    std::string JoinRuns(const TemplateRuns& runs)
    {
        std::string text;
        for (size_t i = 0; i < runs.texts.size(); ++i)
        {
            text += runs.texts[i];
            if (i >= runs.slots.size()) break;
            if (OddMarksAtEnd(runs.texts[i])) text += kTagMark;
            const TagSlot& slot     = runs.slots[i];
            const std::string& next = runs.texts[i + 1];
            // A small letter straight after the name would be read as more of it.
            const bool braced = slot.size() > 1 || (!next.empty() && IsLower(next.front()));
            text += kTagMark;
            if (braced) text += kNameOpen;
            for (size_t tag = 0; tag < slot.size(); ++tag)
            {
                if (tag > 0) text += kSlotBar;
                text += TagName(slot[tag]) + 1;
            }
            if (braced) text += kNameClose;
        }
        return text;
    }

    std::vector<Tag> TagsFor(PlateKind kind)
    {
        std::vector<Tag> tags;
        for (const TagEntry& entry : kTags)
            if (entry.kinds & KindBit(kind)) tags.push_back(entry.tag);
        return tags;
    }

    bool PutTag(std::string& text, RunCaret& caret, const TagSlot& slot)
    {
        TemplateRuns runs  = RunsOf(text);
        const RunCaret at  = Placed(runs, caret);
        const auto run     = static_cast<size_t>(at.run);
        const std::string after = runs.texts[run].substr(at.at);
        runs.texts[run].resize(at.at);
        runs.texts.insert(runs.texts.begin() + at.run + 1, after);
        runs.slots.insert(runs.slots.begin() + at.run, slot);
        std::string put = JoinRuns(runs);
        if (put.size() > kMaxTemplate) return false;
        text  = std::move(put);
        caret = RunCaret{at.run + 1, 0};
        return true;
    }

    bool JoinSlot(std::string& text, int slot, const TagSlot& more)
    {
        TemplateRuns runs = RunsOf(text);
        if (slot < 0 || slot >= static_cast<int>(runs.slots.size())) return false;
        TagSlot& into    = runs.slots[static_cast<size_t>(slot)];
        const auto breaks = [](const TagSlot& tags) { return std::ranges::find(tags, Tag::Break) != tags.end(); };
        if (breaks(into) || breaks(more)) return false; // a break is a place of its own
        const size_t had = into.size();
        for (const Tag tag : more)
            if (std::ranges::find(into, tag) == into.end()) into.push_back(tag);
        std::string joined = JoinRuns(runs);
        if (into.size() == had || joined.size() > kMaxTemplate) return false;
        text = std::move(joined);
        return true;
    }

    namespace
    {
        bool HasSlot(const TemplateRuns& runs, int slot) { return slot >= 0 && slot < static_cast<int>(runs.slots.size()); }

        // Takes the slot out, joins the stretches either side of it, and gives where they met.
        RunCaret Erase(TemplateRuns& runs, int slot)
        {
            const auto at = static_cast<size_t>(slot);
            const RunCaret met{slot, runs.texts[at].size()};
            runs.texts[at] += runs.texts[at + 1];
            runs.texts.erase(runs.texts.begin() + slot + 1);
            runs.slots.erase(runs.slots.begin() + slot);
            return met;
        }
    }

    std::string WithoutTag(std::string_view text, int slot, int tag, RunCaret& caret)
    {
        TemplateRuns runs = RunsOf(text);
        if (!HasSlot(runs, slot)) return std::string(text);
        TagSlot& tags = runs.slots[static_cast<size_t>(slot)];
        const int at  = tag == kLastTag ? static_cast<int>(tags.size()) - 1 : tag;
        if (at < 0 || at >= static_cast<int>(tags.size())) return std::string(text);
        if (tags.size() > 1)
            tags.erase(tags.begin() + at);
        else
            caret = Erase(runs, slot);
        return JoinRuns(runs);
    }

    bool MoveTag(std::string& text, int slot, int from, int to)
    {
        TemplateRuns runs = RunsOf(text);
        if (!HasSlot(runs, slot)) return false;
        TagSlot& tags   = runs.slots[static_cast<size_t>(slot)];
        const int count = static_cast<int>(tags.size());
        if (from == to || from < 0 || from >= count || to < 0 || to >= count) return false;
        const Tag moved = tags[static_cast<size_t>(from)];
        tags.erase(tags.begin() + from);
        tags.insert(tags.begin() + to, moved);
        text = JoinRuns(runs);
        return true;
    }

    std::string WithRun(std::string_view text, int run, std::string_view typed)
    {
        TemplateRuns runs = RunsOf(text);
        if (run < 0 || run >= static_cast<int>(runs.texts.size())) return std::string(text);
        std::string& stretch = runs.texts[static_cast<size_t>(run)];
        stretch              = typed;
        std::string joined   = JoinRuns(runs);
        // Too long, the stretch typed loses what is over, a whole character at a time.
        while (joined.size() > kMaxTemplate && !stretch.empty())
        {
            size_t end = stretch.size() - std::min(stretch.size(), joined.size() - kMaxTemplate);
            while (end > 0 && ContinuesCharacter(stretch[end])) --end;
            stretch.resize(end);
            joined = JoinRuns(runs);
        }
        return joined;
    }

    std::string UnusedTagsNote(const KindSettings& kind, std::initializer_list<Tag> tags, std::string_view sentence,
        std::span<const LayoutOption> names, std::string_view orWord)
    {
        constexpr std::string_view kComma = ", ";
        std::string named;
        size_t left = tags.size();
        for (const Tag tag : tags)
        {
            if (UsesTag(kind, tag)) return {};
            if (!named.empty()) named += left > 1 ? std::string(kComma) : " " + std::string(orWord) + " ";
            named += Said(names[static_cast<size_t>(tag)].name);
            --left;
        }
        return named.empty() ? named : Filled(sentence, kTagsMark, named);
    }

    bool UsesTag(const KindSettings& kind, Tag tag)
    {
        for (const LineDesign& line : kind.lines)
            for (const Atom& atom : ParseTemplate(line.text))
                if (std::ranges::find(atom.slot, tag) != atom.slot.end()) return true;
        return false;
    }
}
