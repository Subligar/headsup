#pragma once

#include "bar_theme.h"
#include "fish_pool.h"
#include "settings.h"

#include <RmlUi/Core.h>

#include <optional>

#include <cstdint>
#include <string>
#include <vector>

namespace headsup
{
    // The longest the game's catch animation was seen to take, from the catch message until you can cast again.
    constexpr float kCatchAnimationSeconds = 9.0f;
    constexpr float kCaseBuffer            = 0.5f; // so the Case never ends before you can cast
    constexpr float kCaseSeconds           = kCatchAnimationSeconds + kCaseBuffer;

    constexpr int kCaseTiles = 52, kCaseWinner = 46;
    // stop: where the line ends, on the winner's tile; a whole number is a tile's middle.
    struct CaseRun
    {
        std::vector<uint16_t> tiles;
        float stop = 0.0f;
    };
    // Tiles picked from the pool by weight, the catch on kCaseWinner. An empty pool: every tile the catch.
    CaseRun SpinCase(uint16_t caught, const std::vector<PoolEntry>& pool, uint32_t seed);

    // What the Case shows `seconds` in, of a run `length` long. Every share is 0 to 1; lift is in layout pixels and
    // along in tiles.
    struct CaseView
    {
        float veil = 0, strip = 0, lift = 0, along = 0, settled = 0, reveal = 0, iconScale = 0, named = 0;
        bool over = true;
    };
    CaseView CaseAt(const CaseRun& run, float seconds, float length);
    // When each tile's edge crosses the line, a click each.
    std::vector<float> CaseClicks(const CaseRun& run, float length);
    // A tile's color by the skill its catch asks, 0xRRGGBB.
    uint32_t CaseColor(uint16_t item);
    // "Fishing skill 11"; empty for what no fishing hands out.
    std::string CaseSkillLine(uint16_t item);
    // What the Test button spins to: a pick from the pool by weight, or a Moat Carp with no bait or no pool.
    uint16_t TestCatch(const Angler& angler, const std::vector<PoolEntry>& pool, uint32_t seed);

    // The Case's middle on the screen and the pixels of one layout pixel: the layout is 1280 by 720, scaled to the
    // screen's height, times the scale in percent. Placed, across and down are ten-thousandths of the screen; unplaced, it
    // is centered across and a third down. It stays on the screen.
    struct CaseSpot
    {
        float x = 0, y = 0, unit = 1;
        bool operator==(const CaseSpot&) const = default;
    };
    CaseSpot CasePlace(int across, int down, int scalePercent, float screenWidth, float screenHeight);

    enum class CasePicture : uint8_t { Solid, Vignette, Glow, Tile, Rise, Band, Title, Name, Skill, Item, ItemCrisp };
    constexpr int kCaseFixedPictures = static_cast<int>(CasePicture::Title); // those painted once, by CaseImage
    // One upright rectangle of a frame, in screen pixels from its top left, tinted by argb. add: its light is added to
    // what is under it. item: whose icon, for Item (smoothed) and ItemCrisp (its pixels kept square).
    struct CaseQuad
    {
        CasePicture picture;
        bool add;
        float x, y, width, height;
        uint32_t argb;
        uint16_t item = 0;
    };
    // Each text picture's width over its height; 0 for none.
    struct CaseWords
    {
        float title = 0, name = 0, skill = 0;
    };
    // What a Case draws `seconds` in, back to front. background off: no tint and no vignette. Nothing before it
    // starts or once it is over.
    std::vector<CaseQuad> CaseFrame(const CaseRun& run, float seconds, float length, const CaseSpot& spot, float screenWidth,
        float screenHeight, bool background, const CaseWords& words);
    // The heights the three texts show at, in layout pixels.
    constexpr float kCaseTitleHeight = 30.0f, kCaseNameHeight = 50.0f, kCaseSkillHeight = 22.0f;

    // Where Move's boxes go, in screen pixels: the band, the line, the reveal's icon and its name's line.
    struct CaseBox
    {
        float x = 0, y = 0, width = 0, height = 0;
        bool operator==(const CaseBox&) const = default;
    };
    struct CaseBoxSet
    {
        CaseBox band, line, icon, name;
    };
    CaseBoxSet CaseBoxesAt(const CaseSpot& spot);

    constexpr const char* kCaseBoxesDocument = "case";
    constexpr const char* kCaseGroupId       = "casegroup"; // the dragged piece, holding the four boxes

    // Move's boxes: where the Case's strip, line and reveal go, dragged as one piece.
    class CaseBoxes : private Rml::EventListener
    {
    public:
        ~CaseBoxes() override { Close(); }
        // Each frame: shows them while moving, else hides. True when they were dragged somewhere new, which the
        // settings then hold.
        bool Update(Rml::Context* context, const BarPalette& palette, Settings& settings, bool moving);
        // Before the context goes.
        void Close();

    private:
        void ProcessEvent(Rml::Event& event) override;

        Rml::ElementDocument* m_Document = nullptr;
        Rml::Element* m_Group            = nullptr;
        BarPalette m_Palette{};
        std::optional<CaseSpot> m_Shown;   // where the boxes stand now
        Rml::Vector2f m_From{}, m_By{};     // a drag's first pointer and how far it has come
        bool m_Dragging = false, m_Dropped = false;
    };
}
