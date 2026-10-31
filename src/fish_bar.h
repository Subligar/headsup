#pragma once

#include "bar_theme.h"
#include "settings.h"

#include <RmlUi/Core.h>

#include <cstdint>
#include <optional>

namespace headsup
{
    // A hooked fish as the game keeps it while you fight it (LocateFish): its stamina now and when full.
    struct FishStamina
    {
        int32_t now = 0, full = 0;
    };
    // The share of its stamina the fish has left, 0 to 1; nothing for a fish of no stamina.
    std::optional<float> FishShare(const FishStamina& fish);

    constexpr int kFishBarHeight         = 18; // pixels, its frame counted
    constexpr float kFishBarStartDown    = 0.7f; // of the screen's height, until it is dragged somewhere
    constexpr const char* kFishBarDocument = "fishing";
    constexpr const char* kFishBarId     = "fishbar";
    constexpr const char* kFishFillId    = "fishfill";

    struct FishPlace
    {
        float x = 0.0f, y = 0.0f;
    };
    // Its top left: kFishBarUnplaced in the middle low on the screen, else where it was dragged, kept on the screen.
    FishPlace FishBarPlace(int x, int y, int width, float screenWidth, float screenHeight);

    // HeadsUp's own fishing bar, which the game draws in its party window: a document of the overlay's that drags to
    // its place.
    class FishBar : private Rml::EventListener
    {
    public:
        ~FishBar() override { Close(); }
        // Each frame: shows the fish's share left, or the bar full while preview (the settings window is open) to be
        // placed; hides it with neither. True when it was dragged somewhere new, which settings then hold.
        bool Update(Rml::Context* context, const BarPalette& palette, Settings& settings, std::optional<float> share,
            bool preview);
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
