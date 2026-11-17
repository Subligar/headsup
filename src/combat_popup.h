#pragma once

#include "nameplate.h"
#include "screen_box.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
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
        bool operator==(const PopupEvent&) const = default;
    };

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
        // The game decided to pop one for event: the next start is its, with the event's type.
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
    };
    PopupSwitches PopupSwitchesOf(const Settings& settings);

    // A number as the game wrote it, an amount of MP or TP and a drain labeled ("20 MP", "Drain 55"); for the Miss
    // glyph, what its message says happened (Evade, Parry, Shadow, Resist, No Effect, Fail and the like), or Miss.
    std::string CombatPopupWords(std::string_view text, std::optional<PopupEvent> event, const PopupSwitches& on = {});

    // A popup as HeadsUp draws it, in the scene image's pixels: its words in an opaque color at an alpha out of
    // kOpaqueAlpha, the middle of their capitals on center, in a line lineHeight tall, at the game's depth.
    struct ShownCombatPopup
    {
        std::string words;
        uint32_t color = 0, alpha = 0;
        ScreenPoint center;
        float lineHeight = 0.0f, depth = 0.0f;
    };
    // Two popups of a frame with the same text whose centers are within half a box height are one, the later, which the
    // game draws on top. A line is as tall as its box at 100 percent: the box already shrinks with distance. A popup
    // with a type takes its pick's color, by PopupType, at the game's alpha.
    std::vector<ShownCombatPopup> ShownCombatPopups(const std::vector<CombatPopup>& popups, int sizePercent,
        const std::array<ColorPick, kPopupTypeCount>& colors, const PopupSwitches& on = {});

    // A popup's text as drawn into its texture: the image's size and halo in its own pixels, and its capitals' top and
    // bottom in rows under the halo, drawn raster pixels tall.
    struct CombatTextSize
    {
        float width = 0.0f, height = 0.0f, halo = 0.0f, capitalTop = 0.0f, capitalBottom = 0.0f;
        int raster = 0;
    };
    // Its quad in the scene image, its corner on whole pixels: centered across on center, the middle of its capitals on
    // center down, its line lineHeight tall. pixelShape is the image's pixels per screen pixel across over those down
    // (PlaceSquarePixels' toX / toY), so the game's copy to the screen gives the letters their shape back.
    SquareQuad PlaceCombatPopupText(ScreenPoint center, float lineHeight, const CombatTextSize& text, float pixelShape);
    // How many pixels the edge reaches around letters drawn raster pixels tall.
    int CombatPopupEdge(int raster);

    // One line each for the draw dump: what the game's call drew, and what HeadsUp drew.
    std::string DescribeCombatPopup(const CombatPopup& popup);
    std::string DescribeCombatPopup(const ShownCombatPopup& popup);
}
