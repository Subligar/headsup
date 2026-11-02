#include "bar_look.h"
#include "fish_bar.h"
#include "plate_template.h"
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

TEST(a_hook_and_then_a_feeling_say_what_is_on_the_line)
{
    // Phoenix's FISHMESSAGEOFFSET_ numbers past the zone's first fishing message (fishingutils.h).
    FishCatch fish;
    ReadFishMessage(fish, 0x08);
    CHECK(fish.size == FishSize::Small && !fish.feeling);
    ReadFishMessage(fish, 0x2A);
    CHECK(fish.size == FishSize::Small && fish.feeling == FishFeeling::Bad);
    // The next bite starts again.
    ReadFishMessage(fish, 0x32);
    CHECK(fish.size == FishSize::Large && !fish.feeling);
    ReadFishMessage(fish, 0x2B);
    CHECK(fish.feeling == FishFeeling::Terrible);
    const std::pair<uint16_t, FishSize> sizes[] = {{0x08, FishSize::Small}, {0x32, FishSize::Large},
        {0x33, FishSize::Item}, {0x34, FishSize::Monster}};
    for (const auto& [message, size] : sizes)
    {
        ReadFishMessage(fish, message);
        CHECK(fish.size == size);
    }
    const std::pair<uint16_t, FishFeeling> feelings[] = {{0x29, FishFeeling::Good}, {0x2A, FishFeeling::Bad},
        {0x2B, FishFeeling::Terrible}, {0x2C, FishFeeling::LowSkill}, {0x2D, FishFeeling::VeryLowSkill},
        {0x2E, FishFeeling::ExtremelyLowSkill}, {0x36, FishFeeling::Epic},
        {0x35, FishFeeling::Good}}; // the keen angler's sense comes only in place of a good feeling, and names the fish
    for (const auto& [message, feeling] : feelings)
    {
        ReadFishMessage(fish, message);
        CHECK(fish.feeling == feeling);
    }
    // The catch's own messages and the rest leave it as it is.
    const FishCatch before = fish;
    for (const uint16_t message : {0x00, 0x04, 0x06, 0x07, 0x27, 0x28, 0x40, 0x3FF})
        ReadFishMessage(fish, message);
    CHECK(fish == before);
}

TEST(the_fish_line_shows_what_is_known_of_the_catch)
{
    CHECK_EQ(FishLine("${size|fish} $feeling", FishCatch{}), "Fish ");
    CHECK_EQ(FishLine("${size|fish} $feeling", FishCatch{FishSize::Small, FishFeeling::Good}), "Small Fish Good Feeling");
    CHECK_EQ(FishLine("$size", FishCatch{FishSize::Large, std::nullopt}), "Large Fish");
    CHECK_EQ(FishLine("$size", FishCatch{FishSize::Item, std::nullopt}), "Item");
    CHECK_EQ(FishLine("$size", FishCatch{FishSize::Monster, std::nullopt}), "Monster");
    CHECK_EQ(FishLine("$feeling", FishCatch{FishSize::Small, FishFeeling::ExtremelyLowSkill}), "Extremely Low Skill");
    // A bad feeling is the only warning of a line about to snap, a terrible one of a rod about to break.
    const FishCatch bad{FishSize::Large, FishFeeling::Bad}, terrible{FishSize::Large, FishFeeling::Terrible};
    CHECK_EQ(FishLine("$linesnap$rodbreak", bad), "Line Break");
    CHECK_EQ(FishLine("$linesnap$rodbreak", terrible), "Rod Break");
    CHECK_EQ(FishLine("$linesnap$rodbreak", FishCatch{FishSize::Large, FishFeeling::Good}), "");
    CHECK_EQ(FishLine("$fish!", bad), "Fish!");
    // A nameplate's tags show nothing here.
    CHECK_EQ(FishLine("$name $fish", bad), " Fish");
    CHECK(FishBarTags() == (std::vector<Tag>{Tag::Fish, Tag::FishSize, Tag::Feeling, Tag::LineSnap, Tag::RodBreak}));
}

TEST(the_bar_takes_the_color_of_the_catch_its_rod_and_line_first)
{
    Settings s;
    CHECK(!FishFill(s, FishCatch{}));
    CHECK(FishFill(s, FishCatch{FishSize::Small, FishFeeling::Good}) == ToArgb(s.fishSmallColor));
    CHECK(FishFill(s, FishCatch{FishSize::Large, std::nullopt}) == ToArgb(s.fishLargeColor));
    CHECK(FishFill(s, FishCatch{FishSize::Item, std::nullopt}) == ToArgb(s.fishItemColor));
    CHECK(FishFill(s, FishCatch{FishSize::Monster, FishFeeling::Good}) == ToArgb(s.fishMonsterColor));
    CHECK(FishFill(s, FishCatch{FishSize::Monster, FishFeeling::Bad}) == ToArgb(s.fishLineColor));
    CHECK(FishFill(s, FishCatch{FishSize::Small, FishFeeling::Terrible}) == ToArgb(s.fishRodColor));
    // Line snaps orange, rod breaks red and monsters purple, by default.
    CHECK(Channel(ToArgb(s.fishLineColor), kRedShift) > Channel(ToArgb(s.fishLineColor), kGreenShift) &&
          Channel(ToArgb(s.fishLineColor), kGreenShift) > Channel(ToArgb(s.fishLineColor), kBlueShift));
    CHECK(Channel(ToArgb(s.fishRodColor), kRedShift) > 2 * Channel(ToArgb(s.fishRodColor), kGreenShift));
    CHECK(Channel(ToArgb(s.fishMonsterColor), kBlueShift) > Channel(ToArgb(s.fishMonsterColor), kGreenShift) &&
          Channel(ToArgb(s.fishMonsterColor), kRedShift) > Channel(ToArgb(s.fishMonsterColor), kGreenShift));
    s.fishColorByCatch = false;
    CHECK(!FishFill(s, FishCatch{FishSize::Small, FishFeeling::Terrible}));
}

TEST(the_fishing_bar_starts_centered_low_and_stays_on_the_screen)
{
    constexpr int kHeight = 18, kName = 18;
    const FishPlace start = FishBarPlace(kFishBarUnplaced, kFishBarUnplaced, 300, kHeight, kName, 1920.0f, 1080.0f);
    CHECK_EQ(start.x, (1920.0f - 300.0f) / 2.0f);
    CHECK(start.y > 1080.0f / 2.0f && start.y + kHeight < 1080.0f);
    // Dragged to the top, its line still fits over it.
    CHECK_EQ(FishBarPlace(100, 0, 300, kHeight, kName, 1920.0f, 1080.0f).y, static_cast<float>(kName));
    CHECK_EQ(FishBarPlace(100, 0, 300, kHeight, 30, 1920.0f, 1080.0f).y, 30.0f);
    const FishPlace placed = FishBarPlace(100, 200, 300, kHeight, kName, 1920.0f, 1080.0f);
    CHECK_EQ(placed.x, 100.0f);
    CHECK_EQ(placed.y, 200.0f);
    // A place kept from a larger screen comes back onto this one, a taller bar higher.
    const FishPlace off = FishBarPlace(2500, 1400, 300, kHeight, kName, 1920.0f, 1080.0f);
    CHECK_EQ(off.x, 1920.0f - 300.0f);
    CHECK_EQ(off.y, 1080.0f - kHeight);
    CHECK_EQ(FishBarPlace(2500, 1400, 300, 40, kName, 1920.0f, 1080.0f).y, 1080.0f - 40.0f);
    // 15 px text, the game's own size, on an 18 px line.
    CHECK_EQ(FishNameHeight(kDefaultFishTextSize), 18);
}

namespace
{
    Rml::Element* Fill(test::RmlSession& rml) { return rml.context->GetDocument(kFishBarDocument)->GetElementById(kFishFillId); }
    Rml::Element* Bar(test::RmlSession& rml) { return rml.context->GetDocument(kFishBarDocument)->GetElementById(kFishBarId); }
    Rml::Element* Name(test::RmlSession& rml) { return rml.context->GetDocument(kFishBarDocument)->GetElementById(kFishNameId); }
    FishBarView Showing(float share) { return FishBarView{share, "Small Fish", std::nullopt, kDetailFont}; }
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
        bar.Update(rml.context, palette, settings, Showing(0.25f));
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
        CHECK(name->GetInnerRML() == "Small Fish" && name->IsVisible());
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
    CHECK(!bar.Update(rml.context, palette, settings, Showing(1.0f)));
    rml.context->Update();
    const Rml::Vector2f at = Bar(rml)->GetAbsoluteOffset(Rml::BoxArea::Border);
    const int x = static_cast<int>(at.x) + 20, y = static_cast<int>(at.y) + settings.fishBarHeight / 2;
    rml.context->ProcessMouseMove(x, y, 0);
    rml.context->ProcessMouseButtonDown(0, 0);
    for (int step = 1; step <= 4; ++step)
    {
        rml.context->ProcessMouseMove(x + 25 * step, y - 10 * step, 0);
        rml.context->Update();
    }
    rml.context->ProcessMouseButtonUp(0, 0);
    CHECK(bar.Update(rml.context, palette, settings, Showing(1.0f)));
    CHECK_EQ(settings.fishBarX, static_cast<int>(at.x) + 100);
    CHECK_EQ(settings.fishBarY, static_cast<int>(at.y) - 40);
    rml.context->Update();
    CHECK_EQ(Bar(rml)->GetAbsoluteOffset(Rml::BoxArea::Border).x, static_cast<float>(settings.fishBarX));
    CHECK(!bar.Update(rml.context, palette, settings, Showing(1.0f)));
    // Not draggable, the same drag leaves it where it is.
    settings.fishBarDraggable = false;
    bar.Update(rml.context, palette, settings, Showing(1.0f));
    rml.context->Update();
    const Rml::Vector2f kept = Bar(rml)->GetAbsoluteOffset(Rml::BoxArea::Border);
    rml.context->ProcessMouseMove(static_cast<int>(kept.x) + 20, static_cast<int>(kept.y) + settings.fishBarHeight / 2, 0);
    rml.context->ProcessMouseButtonDown(0, 0);
    for (int step = 1; step <= 4; ++step)
    {
        rml.context->ProcessMouseMove(static_cast<int>(kept.x) + 20 + 25 * step, static_cast<int>(kept.y), 0);
        rml.context->Update();
    }
    rml.context->ProcessMouseButtonUp(0, 0);
    CHECK(!bar.Update(rml.context, palette, settings, Showing(1.0f)));
    CHECK_EQ(settings.fishBarX, static_cast<int>(at.x) + 100);
    bar.Close();
}

TEST(the_fishing_bar_takes_its_height_text_line_and_the_catchs_color)
{
    test::RmlSession rml;
    Settings settings;
    settings.fishBarHeight   = 30;
    settings.fishBarTextSize = 22;
    FishBar bar;
    const BarPalette& palette = PaletteOf(BarTheme::PhoenixGold);
    bar.Update(rml.context, palette, settings, FishBarView{0.5f, "Large Fish <Bad>", 0xFFFF8C1Au, kDetailFont});
    rml.context->Update();
    CHECK_EQ(Bar(rml)->GetBox().GetSize(Rml::BoxArea::Border).y, 30.0f);
    CHECK_EQ(Name(rml)->GetInnerRML(), "Large Fish &lt;Bad&gt;");
    CHECK_EQ(Name(rml)->GetComputedValues().font_size(), 22.0f);
    CHECK(Fill(rml)->GetProperty(Rml::PropertyId::Decorator)->ToString().find("#ff8c1a") != std::string::npos);
    // Its line changes with the catch, and its fill back to the theme's.
    bar.Update(rml.context, palette, settings, FishBarView{0.5f, "Item", std::nullopt, kDetailFont});
    rml.context->Update();
    CHECK_EQ(Name(rml)->GetInnerRML(), "Item");
    CHECK(Fill(rml)->GetProperty(Rml::PropertyId::Decorator)->ToString().find("#ff8c1a") == std::string::npos);
    CHECK_EQ(rml.Said(), "");
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
