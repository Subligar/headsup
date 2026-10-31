#pragma once

#include "bar_theme.h"
#include "native_locate.h"
#include "screen_box.h"
#include "settings.h"

#include <RmlUi/Core.h>

#include <cstdint>
#include <optional>

namespace headsup
{
    // A hooked fish as the game keeps it while you fight it (LocateFish): its stamina when full and now, and its bar
    // as the game draws it, which eases down to now after each hit.
    struct FishStamina
    {
        int32_t full = 0, now = 0, drawn = 0;
    };
    // The share of its bar the game draws, 0 to 1; nothing for a fish of no stamina.
    std::optional<float> FishShare(const FishStamina& fish);
    // Your status while a fish is on the line, in today's fishing and in the old: the record stays a few seconds past it.
    constexpr uint32_t kHookedStatus = 57, kOldHookedStatus = 38;
    bool FishHooked(uint32_t status);

    // The game draws its own fish bar inside the party window's draw, at the top middle of the screen, far above the
    // window itself: a draw of the window's that lies wholly above the window's rectangle is the bar's.
    // The box of count pretransformed vertices, stride bytes apart, each starting with its x and y.
    ScreenBox VerticesBox(const void* vertices, uint32_t count, uint32_t stride);
    bool AboveWindow(const ScreenBox& box, const WindowRect& window);

    constexpr int kFishBarHeight         = 18; // pixels, its frame counted
    constexpr float kFishBarStartDown    = 0.7f; // of the screen's height, until it is dragged somewhere
    constexpr const char* kFishBarDocument = "fishing";
    constexpr const char* kFishBarId     = "fishbar";
    constexpr const char* kFishFillId    = "fishfill";
    constexpr const char* kFishNameId    = "fishname";
    constexpr const char* kFishName      = "Fish"; // over its top left, as the game's bar has it
    constexpr int kFishNamePixels = 15, kFishNameHeight = 18;

    struct FishPlace
    {
        float x = 0.0f, y = 0.0f;
    };
    // Its top left: kFishBarUnplaced in the middle low on the screen, else where it was dragged, kept on the screen
    // with its name over it.
    FishPlace FishBarPlace(int x, int y, int width, float screenWidth, float screenHeight);

    // HeadsUp's own fishing bar, which the game draws in its party window: a document of the overlay's that drags to
    // its place.
    class FishBar : private Rml::EventListener
    {
    public:
        ~FishBar() override { Close(); }
        // Each frame: shows the fish's share left while a fish is hooked, else hides. True when it was dragged somewhere
        // new, which settings then hold.
        bool Update(Rml::Context* context, const BarPalette& palette, Settings& settings, std::optional<float> share);
        // Before the context goes.
        void Close();

    private:
        void ProcessEvent(Rml::Event& event) override;

        Rml::ElementDocument* m_Document = nullptr;
        Rml::Element* m_Bar  = nullptr;
        Rml::Element* m_Fill = nullptr;
        BarPalette m_Palette{};
        Rml::Vector2f m_Grab{};  // the pointer from the bar's corner as the drag began
        std::optional<FishPlace> m_Dragged; // where the drag left it, not yet in the settings
        bool m_Dragging = false;
    };
}
