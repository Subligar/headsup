#pragma once

#include "nameplate.h"
#include "screen_box.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace headsup
{
    // The game keeps a combat popup's text in 16 bytes of its particle, with no room for a terminator.
    constexpr size_t kMaxCombatPopupText = 16;

    // What a popup is: the message the game started it for (msg_basic's ids), and the entity indexes of who did it and
    // to whom.
    struct PopupEvent
    {
        uint32_t message = 0, actor = 0, target = 0;
        uint8_t kind = 0; // the game's: 1 damage, 2 recovery, 3 miss
        std::optional<PopupType> type = std::nullopt; // PopupTies works it out; none for plain damage between others
        // When the game started it, and how many lines under the game's place it is drawn, so popups started together
        // on one target each read.
        double started = 0.0;
        uint8_t line   = 0;
        bool operator==(const PopupEvent&) const = default;
    };

    // How long a popup keeps its line: the game's lifetime of one, 22 to 23 frames at about 30 fps, and a little more.
    constexpr double kPopupSpreadSeconds = 0.8;

    // Ties each popup particle to its message, from the game's three calls as it starts one: its decision to pop, the
    // effect's task it starts, and the particles it builds from that task. It keeps the last kKeptTies of each, in
    // fixed arrays, as the calls come on the game's stack.
    class PopupTies
    {
    public:
        static constexpr size_t kKeptTies = 64;
        // Whose popups are yours, by entity index: yours and your pet's (0 for none).
        void SetYou(uint32_t you, uint32_t pet)
        {
            m_You = you;
            m_Pet = pet;
        }
        // At Present: the time the popups the game starts next count as started at.
        void SetNow(double now) { m_Now = now; }
        // At Present: whether popups started together spread over lines (Show Right Away), or all keep line 0.
        void SetSpread(bool spread) { m_Spread = spread; }
        // At Present: the column each kind of popup goes in, PopupSidesOf's; popups in one column of a target spread apart.
        void SetSides(const PopupSides& sides) { m_Sides = sides; }
        // At Present: the server ids of you, your party and your alliance, and whether statuses pop (Status Popups is on
        // and the game's Damage Display shows popups).
        void SetParty(std::span<const uint32_t> ids);
        void SetStatuses(bool pop) { m_Statuses = pop; }
        // HeadsUp's answer to the game's popup decision for message with value, actor and target each an index then an
        // id, after the game answered answer with kind: the game's, or 0 ("pop") with a status's kind where the game
        // would pop nothing (kind 0) and you, your pet or your party did it or took it.
        uint8_t Decide(const uint32_t* actor, const uint32_t* target, uint32_t message, uint32_t value, uint8_t answer,
            uint8_t& kind) const;
        // The game decided to pop one for event: the next start is its, with the event's type, and the first line no
        // popup started on its target and side in the last kPopupSpreadSeconds holds.
        void Decided(PopupEvent event);
        void Started(uintptr_t task);
        void Built(uintptr_t particle, uintptr_t task);
        std::optional<PopupEvent> Of(uintptr_t particle) const { return m_Particles.Of(particle); }

    private:
        struct Ties
        {
            std::array<uintptr_t, kKeptTies> keys{};
            std::array<PopupEvent, kKeptTies> events{};
            size_t next = 0; // the oldest, written over next
            void Put(uintptr_t key, const PopupEvent& event);
            void Forget(uintptr_t key);
            std::optional<PopupEvent> Of(uintptr_t key) const;
        };
        std::optional<PopupEvent> m_Pending, m_Last;
        Ties m_Tasks, m_Particles;
        uint32_t m_You = 0, m_Pet = 0;
        double m_Now  = 0.0;
        bool m_Spread = true;
        PopupSides m_Sides{};
        std::array<uint32_t, kPartyMembers> m_Party{};
        bool m_Statuses = false;
    };

    // The game's popup decision's answer that means "do not": anything but 0.
    constexpr uint8_t kPopupNo = 1;

    // An action packet (0x028) as the server sent it, before any addon changed it: its actor's id, and the message of
    // each target record the game builds from it, one for each result of a lone target (at most kMostLoneResults),
    // else one for each target, its first result's.
    constexpr uint16_t kActionPacket = 0x028;
    constexpr size_t kMostSentRecords = 15, kMostLoneResults = 8;
    struct SentAction
    {
        uint32_t actor  = 0;
        uint8_t records = 0;
        std::array<uint16_t, kMostSentRecords> messages{};
    };
    std::optional<SentAction> ParseSentAction(const uint8_t* data, uint32_t size);

    // The frame's sent actions, each kept until the game's parse of it takes it, in fixed room: Ashita hands HeadsUp
    // every packet of a chunk before the game parses the first.
    class SentActions
    {
    public:
        static constexpr size_t kKept = 32;
        // At Present: one the game never parsed is no one's.
        void Clear() { m_Count = 0; }
        void Put(const SentAction& action);
        // The oldest kept one from actor with that many records, taken; none.
        std::optional<SentAction> Take(uint32_t actor, uint8_t records);

    private:
        std::array<SentAction, kKept> m_Kept{};
        size_t m_Count = 0;
    };

    // Popups started as soon as the game has parsed their action, not when its animation reaches the hit. HeadsUp calls
    // the game's own start for each target record's message then, and the game's later call for that record skips its
    // popup and prints only the chat line. A record's mark is two bytes of it the game never uses: kEarlyPopped once
    // its popup started early, so a record parsed before HeadsUp loaded pops as it always did.
    // An addon that prints its own chat line (SimpleLog) blanks the message of the packet the game parses, and the
    // game's start leaves a record with message 0 before its decision. For a status message HeadsUp would pop, the
    // mark keeps the message the server sent (kRestored and its id), and the record holds it again only while the
    // start runs, with the chat line's decision saying no, so the line stays the addon's.
    class EarlyPopups
    {
    public:
        static constexpr uint16_t kEarlyPopped = 0x5A59, kRestored = 0xA400, kRestoredMessage = 0x03FF;
        // At Present: whether popups start early (Show Right Away), and whether blanked statuses come back (Status
        // Popups is on and statuses pop).
        void SetOn(bool on) { m_On = on; }
        void SetRestore(bool restore) { m_Restore = restore; }
        // Once the game has parsed a record: its mark, kRestored with sent where the record's message is blanked and
        // sent, the message the server sent, is a status message; else cleared.
        void Parsed(uint16_t* mark, const uint32_t* message, uint32_t sent);
        // Around HeadsUp's early call for one record, whose mark is cleared; false, and nothing to call, while off.
        bool Begin(uint16_t* mark, uint32_t* message);
        void End();
        // The game started a popup's effect: inside an early call, its record is marked.
        void Started()
        {
            if (m_Mark != nullptr) *m_Mark = kEarlyPopped;
        }
        // Around the game's own call for a record (its mark and message, or none): a popup it started early is
        // skipped, once.
        void Show(uint16_t* mark, uint32_t* message);
        void Shown();
        // What HeadsUp answers the game's decision for the popup or for the chat line, or none to ask the game.
        std::optional<uint8_t> Answer(bool popup) const
        {
            if (popup ? m_Skip : m_Mark != nullptr || m_Message != nullptr) return kPopupNo;
            return std::nullopt;
        }

    private:
        // The record holds its mark's message until Unrestore.
        void Restore(uint16_t mark, uint32_t* message);
        void Unrestore();
        uint16_t* m_Mark    = nullptr;
        uint32_t* m_Message = nullptr; // a record holding a message it is to lose again
        bool m_Skip = false, m_On = true, m_Restore = false;
    };

    // One of the Test button's samples: a message (msg_basic's ids) with its value and the game's kind for it, done by
    // you, or by another to you.
    struct PopupSample
    {
        uint32_t message = 0, value = 0;
        uint8_t kind = 0;
        bool yours = false;
    };
    // What the game would decide on for sample over you; other is any other entity, or you with none.
    PopupEvent SampleEvent(const PopupSample& sample, uint32_t you, uint32_t other);

    // The Test button's run: a sample of each type, kTestGap seconds apart from its start.
    class PopupTestRun
    {
    public:
        static constexpr double kTestGap = 0.35;
        void Start(double now)
        {
            m_Start = now;
            m_Next  = 0;
        }
        // The next sample due by now, each once; none once the run is over.
        std::optional<PopupSample> Next(double now);

    private:
        double m_Start = 0.0;
        size_t m_Next  = SIZE_MAX;
    };

    // One call of the game's popup draw: its text, and what the quads it sent drew. The box holds all its quads, in
    // the scene image's pixels; the depth and color are its last quad's (the color its first vertex's).
    struct CombatPopup
    {
        char text[kMaxCombatPopupText + 1] = {};
        uint32_t color = 0;
        ScreenBox box;
        float depth = 0.0f;
        uintptr_t particle = 0;
        std::optional<PopupEvent> event;
    };

    // The frame's popups, grouped by the call that drew them, never by counting quads.
    class CombatPopupList
    {
    public:
        void NewFrame();
        // The game's draw of one popup starts: its text, the particle it is drawn from and what that is for, if known.
        void Begin(const char* text, uintptr_t particle, std::optional<PopupEvent> event);
        bool Open() const { return m_Open; }
        // A quad the open draw sent, as ScreenVertex vertices stride bytes apart; nothing outside a draw.
        void Add(const void* vertices, size_t count, size_t stride);
        void End() { m_Open = false; }
        // A draw that sent no quad drew nothing and is not here.
        const std::vector<CombatPopup>& Popups() const { return m_Popups; }

    private:
        std::vector<CombatPopup> m_Popups;
        char m_Text[kMaxCombatPopupText + 1] = {};
        uintptr_t m_Particle = 0;
        std::optional<PopupEvent> m_Event;
        bool m_Open = false, m_Added = false;
    };

    // The kind of popup HeadsUp asks the game for a message that puts a status (value) on its target: 1, damage's, for a
    // debuff, 2, recovery's, for a buff; 0 for any other message, or a status with no name.
    uint8_t StatusPopupKind(uint32_t message, uint32_t value);
    bool IsStatusMessage(uint32_t message);

    // The game draws its Miss picture as this one glyph of its damage figures; HeadsUp writes the word.
    constexpr char kMissGlyph       = ':';
    constexpr const char* kMissWord = "Miss";
    // The popup's draw treats 808080 as no tint, as the game's names do, so it shows a vertex color twice as bright.
    constexpr uint32_t kCombatPopupModulate = 2;
    // The black edge around a popup's letters, per pixel of the height they are drawn at.
    constexpr float kCombatPopupEdgePerHeight = 0.1f;

    // Which of HeadsUp's own touches popups get, by the card's switches; each one off gives what the game shows.
    struct PopupSwitches
    {
        bool words = true;
        bool labels = true;
        bool colors = true;
        bool skillchainNames = true;
        bool placement = true;
    };
    PopupSwitches PopupSwitchesOf(const Settings& settings);
    // The column each kind of popup goes in: as the settings pick, or with Custom Placement off all in one, as all go
    // where the game puts them.
    PopupSides PopupSidesOf(const Settings& settings);

    // A number as the game wrote it, an amount of MP or TP and a drain labeled ("20 MP", "Drain 55"); for the Miss
    // glyph, what its message says happened (Evade, Parry, Shadow, Resist, No Effect, Fail and the like), or Miss.
    std::string CombatPopupWords(std::string_view text, std::optional<PopupEvent> event, const PopupSwitches& on = {});

    // A popup as HeadsUp draws it, in the scene image's pixels: its words in an opaque color at an alpha out of
    // kOpaqueAlpha, the middle of their capitals on center, across the share of their width left of center, in a line
    // lineHeight tall, at the game's depth.
    struct ShownCombatPopup
    {
        std::string words;
        uint32_t color = 0, alpha = 0;
        ScreenPoint center;
        float lineHeight = 0.0f, depth = 0.0f, across = 0.5f;
    };

    // A target's body on screen, in the scene image's pixels: the rows of the bottom of the game's name over its head
    // and of its feet where the camera gives them.
    struct PopupBody
    {
        float nameBottom = 0.0f;
        std::optional<float> feet;
    };
    // A body beside which a popup goes is taken to reach this share of its height to either side of its middle.
    constexpr float kPopupBodyReach = 0.5f;
    // Where the frame's popups go: the column of each kind, how far up its target's body from the feet (0) to the top
    // of its head (1) they start, a nudge in the scene image's pixels, and each target's body by entity index,
    // where known.
    struct PopupPlacing
    {
        PopupSides sides{};
        float height = 0.0f;
        ScreenPoint nudge;
        std::function<std::optional<PopupBody>(uint32_t target)> bodyOf;
    };
    // Two popups of a frame with the same text whose centers are within half a box height are one, the later, which the
    // game draws on top. A line is as tall as its box at 100 percent: the box already shrinks with distance. A popup
    // with a type takes its pick's color, by PopupType, at the game's alpha. It goes in its kind's column, at the start
    // height: Left or Right its words end or start past its target's body, in the Middle they are centered on the
    // game's place. With the body's height not known it keeps the game's height, one line past the game's place Left
    // or Right. Its line moves it that many lines down, and the nudge moves it on.
    std::vector<ShownCombatPopup> ShownCombatPopups(const std::vector<CombatPopup>& popups, int sizePercent,
        const std::array<ColorPick, kPopupTypeCount>& colors, const PopupPlacing& placing, const PopupSwitches& on = {});

    // A popup's text as drawn into its texture: the image's size and halo in its own pixels, and its capitals' top and
    // bottom in rows under the halo, drawn raster pixels tall.
    struct CombatTextSize
    {
        float width = 0.0f, height = 0.0f, halo = 0.0f, capitalTop = 0.0f, capitalBottom = 0.0f;
        int raster = 0;
    };
    // Its quad in the scene image, its corner on whole pixels: across (a share of its width) left of center, the middle
    // of its capitals on center down, its line lineHeight tall. pixelShape is the image's pixels per screen pixel across
    // over those down (PlaceSquarePixels' toX / toY), so the game's copy to the screen gives the letters their shape back.
    SquareQuad PlaceCombatPopupText(ScreenPoint center, float lineHeight, const CombatTextSize& text, float pixelShape,
        float across);
    // How many pixels the edge reaches around letters drawn raster pixels tall.
    int CombatPopupEdge(int raster);

    // One line each for the draw dump: what the game's call drew, and what HeadsUp drew.
    std::string DescribeCombatPopup(const CombatPopup& popup);
    std::string DescribeCombatPopup(const ShownCombatPopup& popup);
}
