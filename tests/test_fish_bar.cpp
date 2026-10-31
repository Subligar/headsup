#include "bar_look.h"
#include "fish_bar.h"
#include "rml_session.h"
#include "test.h"

#include <RmlUi/Core.h>

#include <cmath>

using namespace headsup;

TEST(a_hooked_fish_shows_the_share_of_its_stamina_left)
{
    CHECK(FishShare(FishStamina{3000, 4000}) == std::optional<float>{0.75f});
    CHECK(FishShare(FishStamina{4000, 4000}) == std::optional<float>{1.0f});
    // The fight can raise it past full on a miss and take it under nothing on a hit.
    CHECK(FishShare(FishStamina{4200, 4000}) == std::optional<float>{1.0f});
    CHECK(FishShare(FishStamina{-20, 4000}) == std::optional<float>{0.0f});
    CHECK(!FishShare(FishStamina{100, 0}).has_value());
}

TEST(the_fishing_bar_starts_centered_low_and_stays_on_the_screen)
{
    const FishPlace start = FishBarPlace(kFishBarUnplaced, kFishBarUnplaced, 300, 1920.0f, 1080.0f);
    CHECK_EQ(start.x, (1920.0f - 300.0f) / 2.0f);
    CHECK(start.y > 1080.0f / 2.0f && start.y + kFishBarHeight < 1080.0f);
    const FishPlace placed = FishBarPlace(100, 200, 300, 1920.0f, 1080.0f);
    CHECK_EQ(placed.x, 100.0f);
    CHECK_EQ(placed.y, 200.0f);
    // A place kept from a larger screen comes back onto this one.
    const FishPlace off = FishBarPlace(2500, 1400, 300, 1920.0f, 1080.0f);
    CHECK_EQ(off.x, 1920.0f - 300.0f);
    CHECK_EQ(off.y, 1080.0f - kFishBarHeight);
}

namespace
{
    Rml::Element* Fill(test::RmlSession& rml) { return rml.context->GetDocument(kFishBarDocument)->GetElementById(kFishFillId); }
    Rml::Element* Bar(test::RmlSession& rml) { return rml.context->GetDocument(kFishBarDocument)->GetElementById(kFishBarId); }
}

TEST(the_fishing_bar_shows_while_a_fish_is_on_the_line_or_the_settings_window_is_open)
{
    for (int t = 0; t < kFixedThemeCount; ++t)
    {
        test::RmlSession rml;
        Settings settings;
        FishBar bar;
        const BarPalette& palette = PaletteOf(static_cast<BarTheme>(t));
        bar.Update(rml.context, palette, settings, std::nullopt, false);
        rml.context->Update();
        CHECK(rml.context->GetDocument(kFishBarDocument) == nullptr);
        bar.Update(rml.context, palette, settings, 0.25f, false);
        rml.context->Update();
        rml.context->Render();
        CHECK(Bar(rml) != nullptr && Bar(rml)->IsVisible());
        const float inside = Fill(rml)->GetParentNode()->GetBox().GetSize(Rml::BoxArea::Content).x;
        CHECK(std::abs(Fill(rml)->GetBox().GetSize(Rml::BoxArea::Border).x - inside / 4.0f) < 1.0f);
        CHECK_EQ(Bar(rml)->GetBox().GetSize(Rml::BoxArea::Border).x, static_cast<float>(settings.fishBarWidth));
        // With the window open and no fish, it shows full, to be placed.
        bar.Update(rml.context, palette, settings, std::nullopt, true);
        rml.context->Update();
        CHECK(std::abs(Fill(rml)->GetBox().GetSize(Rml::BoxArea::Border).x - inside) < 1.0f);
        bar.Update(rml.context, palette, settings, std::nullopt, false);
        rml.context->Update();
        CHECK(!Bar(rml)->IsVisible());
        CHECK_EQ(rml.Said(), "");
        bar.Close();
    }
}

TEST(the_fishing_bar_is_dragged_to_a_place_of_its_own)
{
    test::RmlSession rml;
    Settings settings;
    FishBar bar;
    const BarPalette& palette = PaletteOf(BarTheme::PhoenixGold);
    CHECK(!bar.Update(rml.context, palette, settings, 1.0f, false));
    rml.context->Update();
    const Rml::Vector2f at = Bar(rml)->GetAbsoluteOffset(Rml::BoxArea::Border);
    const int x = static_cast<int>(at.x) + 20, y = static_cast<int>(at.y) + kFishBarHeight / 2;
    rml.context->ProcessMouseMove(x, y, 0);
    rml.context->ProcessMouseButtonDown(0, 0);
    for (int step = 1; step <= 4; ++step)
    {
        rml.context->ProcessMouseMove(x + 25 * step, y - 10 * step, 0);
        rml.context->Update();
    }
    rml.context->ProcessMouseButtonUp(0, 0);
    CHECK(bar.Update(rml.context, palette, settings, 1.0f, false));
    CHECK_EQ(settings.fishBarX, static_cast<int>(at.x) + 100);
    CHECK_EQ(settings.fishBarY, static_cast<int>(at.y) - 40);
    rml.context->Update();
    CHECK_EQ(Bar(rml)->GetAbsoluteOffset(Rml::BoxArea::Border).x, static_cast<float>(settings.fishBarX));
    CHECK(!bar.Update(rml.context, palette, settings, 1.0f, false));
    bar.Close();
}
