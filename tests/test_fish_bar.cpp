#include "bar_look.h"
#include "fish_bar.h"
#include "rml_session.h"
#include "test.h"

#include <RmlUi/Core.h>

#include <cmath>

using namespace headsup;

TEST(a_hooked_fish_shows_the_bar_as_the_game_draws_it_against_its_full_stamina)
{
    // Read from the game during a fight (2026-10-31): full 2277, a hit took it to 1445 and the drawn bar eased down.
    CHECK(FishShare(FishStamina{2277, 1445, 1710}) == std::optional<float>{1710.0f / 2277.0f});
    CHECK(FishShare(FishStamina{2277, 2277, 2277}) == std::optional<float>{1.0f});
    // Landed: what is left of the record after the catch draws nothing left.
    CHECK(FishShare(FishStamina{2277, 0, 0}) == std::optional<float>{0.0f});
    CHECK(FishShare(FishStamina{2277, 2500, 2400}) == std::optional<float>{1.0f});
    CHECK(!FishShare(FishStamina{0, 100, 100}).has_value());
}

TEST(only_a_fish_on_the_line_counts_as_hooked)
{
    CHECK(FishHooked(57) && FishHooked(38));
    for (const uint32_t status : {0u, 56u, 58u, 62u, 43u, 50u})
        CHECK(!FishHooked(status));
}

TEST(the_fishing_bar_starts_centered_low_and_stays_on_the_screen)
{
    const FishPlace start = FishBarPlace(kFishBarUnplaced, kFishBarUnplaced, 300, 1920.0f, 1080.0f);
    CHECK_EQ(start.x, (1920.0f - 300.0f) / 2.0f);
    CHECK(start.y > 1080.0f / 2.0f && start.y + kFishBarHeight < 1080.0f);
    // Dragged to the top, its name still fits over it.
    CHECK_EQ(FishBarPlace(100, 0, 300, 1920.0f, 1080.0f).y, static_cast<float>(kFishNameHeight));
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

TEST(the_fishing_bar_shows_only_while_a_fish_is_on_the_line)
{
    for (int t = 0; t < kFixedThemeCount; ++t)
    {
        test::RmlSession rml;
        Settings settings;
        FishBar bar;
        const BarPalette& palette = PaletteOf(static_cast<BarTheme>(t));
        bar.Update(rml.context, palette, settings, std::nullopt);
        rml.context->Update();
        CHECK(rml.context->GetDocument(kFishBarDocument) == nullptr);
        bar.Update(rml.context, palette, settings, 0.25f);
        rml.context->Update();
        rml.context->Render();
        CHECK(Bar(rml) != nullptr && Bar(rml)->IsVisible());
        const float inside = Fill(rml)->GetParentNode()->GetBox().GetSize(Rml::BoxArea::Content).x;
        CHECK(std::abs(Fill(rml)->GetBox().GetSize(Rml::BoxArea::Border).x - inside / 4.0f) < 1.0f);
        CHECK_EQ(Bar(rml)->GetBox().GetSize(Rml::BoxArea::Border).x, static_cast<float>(settings.fishBarWidth));
        // Its name sits over its top left, clear of the bar.
        Rml::Element* name = rml.context->GetDocument(kFishBarDocument)->GetElementById(kFishNameId);
        const Rml::Vector2f corner = Bar(rml)->GetAbsoluteOffset(Rml::BoxArea::Border);
        const Rml::Vector2f at  = name->GetAbsoluteOffset(Rml::BoxArea::Border);
        CHECK(name->GetInnerRML() == kFishName && name->IsVisible());
        CHECK(std::abs(at.x - corner.x) < 4.0f);
        CHECK(at.y + name->GetBox().GetSize(Rml::BoxArea::Border).y <= corner.y + 1.0f && at.y >= 0.0f);
        bar.Update(rml.context, palette, settings, std::nullopt);
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
    CHECK(!bar.Update(rml.context, palette, settings, 1.0f));
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
    CHECK(bar.Update(rml.context, palette, settings, 1.0f));
    CHECK_EQ(settings.fishBarX, static_cast<int>(at.x) + 100);
    CHECK_EQ(settings.fishBarY, static_cast<int>(at.y) - 40);
    rml.context->Update();
    CHECK_EQ(Bar(rml)->GetAbsoluteOffset(Rml::BoxArea::Border).x, static_cast<float>(settings.fishBarX));
    CHECK(!bar.Update(rml.context, palette, settings, 1.0f));
    bar.Close();
}

TEST(the_games_fish_bar_is_the_party_windows_draws_above_the_party_window)
{
    // From a draw dump during a fight (2026-10-31, menu 1920x1080): the bar's fill and a letter of its "Fish", and a
    // draw of the party window in its own rectangle at the bottom right.
    const WindowRect party{1792, 1030, 1906, 1066};
    const auto box = [](float x0, float y0, float x1, float y1) {
        const float corners[] = {x0, y0, 0.0f, 1.0f, x1, y0, 0.0f, 1.0f, x0, y1, 0.0f, 1.0f, x1, y1, 0.0f, 1.0f};
        return VerticesBox(corners, 4, 4 * sizeof(float));
    };
    CHECK(AboveWindow(box(905.5f, 223.5f, 987.4f, 229.5f), party));
    CHECK(AboveWindow(box(901.5f, 215.5f, 910.5f, 226.5f), party));
    CHECK(!AboveWindow(box(1800.0f, 1035.0f, 1880.0f, 1050.0f), party));
    // Its empty rows run on below it, past the screen's bottom: still the window's.
    CHECK(!AboveWindow(box(1794.5f, 1076.5f, 1900.5f, 1078.5f), party));
    // One reaching into the window is the window's own.
    CHECK(!AboveWindow(box(1700.0f, 1020.0f, 1795.0f, 1045.0f), party));
    CHECK(!AboveWindow(ScreenBox{}, party));
}
