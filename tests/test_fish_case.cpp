#include "bar_theme.h"
#include "fish_case.h"
#include "rml_session.h"
#include "settings.h"
#include "test.h"

#include <RmlUi/Core.h>

#include <algorithm>
#include <cmath>
#include <tuple>
#include <vector>

using namespace headsup;

namespace
{
    constexpr uint16_t kRustyBucket = 90, kGiantCatfish = 4469, kCrayfish = 4472, kRedTerrapin = 4402, kRyuguTitan = 4305;
    constexpr float kFull = 10.6f; // the Case's own run, as it was designed

    bool Near(float a, float b, float within = 0.001f) { return std::abs(a - b) < within; }
}

TEST(the_strip_stops_on_the_catch_and_holds_only_the_pool)
{
    const std::vector<PoolEntry> pool{{kRustyBucket, 90}, {kMoatCarp, 115}, {kCrayfish, 115}};
    for (uint32_t seed = 1; seed <= 50; ++seed)
    {
        const CaseRun run = SpinCase(kGiantCatfish, pool, seed);
        CHECK_EQ(run.tiles.size(), size_t{52});
        CHECK_EQ(run.tiles[46], kGiantCatfish);
        for (size_t i = 0; i < run.tiles.size(); ++i)
            CHECK(i == 46 || run.tiles[i] == kRustyBucket || run.tiles[i] == kMoatCarp || run.tiles[i] == kCrayfish);
        CHECK(std::abs(run.stop - 46.0f) <= 0.38f);
    }
    CHECK(SpinCase(kGiantCatfish, pool, 7).tiles == SpinCase(kGiantCatfish, pool, 7).tiles);
    CHECK(SpinCase(kGiantCatfish, pool, 7).tiles != SpinCase(kGiantCatfish, pool, 8).tiles);
}

TEST(likelier_catches_fill_more_tiles)
{
    const std::vector<PoolEntry> pool{{kMoatCarp, 3}, {kCrayfish, 1}};
    int carp = 0, free = 0;
    for (uint32_t seed = 1; seed <= 200; ++seed)
    {
        const CaseRun run = SpinCase(kRustyBucket, pool, seed);
        for (size_t i = 0; i < run.tiles.size(); ++i)
            if (i != static_cast<size_t>(kCaseWinner))
            {
                ++free;
                carp += run.tiles[i] == kMoatCarp;
            }
    }
    const float share = static_cast<float>(carp) / static_cast<float>(free);
    CHECK(share > 0.70f && share < 0.80f);
}

TEST(with_no_pool_every_tile_is_the_catch)
{
    const CaseRun run = SpinCase(kGiantCatfish, {}, 5);
    CHECK(std::ranges::all_of(run.tiles, [](uint16_t tile) { return tile == kGiantCatfish; }));
}

TEST(the_case_runs_its_whole_timeline_in_the_time_given)
{
    const CaseRun run = SpinCase(kMoatCarp, {{kMoatCarp, 1}}, 3);
    CHECK(Near(CaseAt(run, 0.25f, kFull).veil, 0.5f)); // halfway up a 0.5 s smooth step
    const CaseView mid = CaseAt(run, 3.5f, kFull);    // half the spin: 1 - 0.5^3 of the way
    CHECK(Near(mid.along, 3.0f + (run.stop - 3.0f) * 0.875f));
    CHECK(mid.strip == 1.0f && mid.reveal == 0.0f && mid.settled == 0.0f);
    CHECK(Near(CaseAt(run, 6.5f, kFull).along, run.stop));
    const CaseView shown = CaseAt(run, 8.5f, kFull);
    CHECK(shown.strip == 0.0f && shown.reveal == 1.0f && shown.named == 1.0f && Near(shown.iconScale, 1.0f));
    CHECK(!CaseAt(run, 10.59f, kFull).over && CaseAt(run, 10.6f, kFull).over && CaseAt(run, 0.0f, kFull).veil == 0.0f);
    // Half as long: everything happens twice as fast.
    CHECK(Near(CaseAt(run, 1.75f, kFull / 2.0f).along, mid.along));
    CHECK(CaseAt(run, 5.3f, kFull / 2.0f).over);
}

TEST(a_click_sounds_as_each_tile_passes_the_line)
{
    const CaseRun run = SpinCase(kMoatCarp, {{kMoatCarp, 1}}, 3);
    const std::vector<float> clicks = CaseClicks(run, kFull);
    CHECK_EQ(clicks.size(), size_t{43}); // edges 3.5 to 45.5
    CHECK(clicks.front() > 0.5f && clicks.back() <= 6.5f);
    for (size_t i = 2; i < clicks.size(); ++i) // the strip slows: each gap is longer
        CHECK(clicks[i] - clicks[i - 1] > clicks[i - 1] - clicks[i - 2]);
    CHECK(Near(CaseClicks(run, kFull / 2.0f)[10], clicks[10] / 2.0f));
    CHECK(Near(CaseAt(run, clicks[10], kFull).along, 13.5f, 0.01f)); // the eleventh edge is tile 13.5
}

TEST(a_tile_takes_its_color_from_the_skill_its_catch_asks)
{
    CHECK_EQ(CaseColor(kMoatCarp), 0xB0C3D9u);     // skill 11
    CHECK_EQ(CaseColor(kGiantCatfish), 0x4B69FFu); // 31
    CHECK_EQ(CaseColor(kRedTerrapin), 0x8847FFu);  // 53
    CHECK_EQ(CaseColor(kRyuguTitan), 0xE4AE39u);   // legendary
    CHECK_EQ(CaseColor(1), CaseColor(kMoatCarp));  // nothing fishing hands out
}

TEST(the_reveal_names_the_skill_and_no_grade)
{
    CHECK_EQ(CaseSkillLine(kMoatCarp), "Fishing skill 11");
    CHECK_EQ(CaseSkillLine(kRustyBucket), "Fishing skill 1");
    CHECK_EQ(CaseSkillLine(1), "");
}

TEST(the_test_button_spins_to_a_moat_carp_without_bait)
{
    const std::vector<PoolEntry> pool{{kRustyBucket, 1}, {kCrayfish, 1}};
    CHECK_EQ(TestCatch(Angler{100, 0, 0, 0, 17391, 0, 20}, pool, 1), kMoatCarp);
    CHECK_EQ(TestCatch(Angler{100, 0, 0, 0, 17391, 17396, 20}, {}, 1), kMoatCarp);
    bool bucket = false, crayfish = false;
    for (uint32_t seed = 1; seed <= 40; ++seed)
    {
        const uint16_t caught = TestCatch(Angler{100, 0, 0, 0, 17391, 17396, 20}, pool, seed);
        CHECK(caught == kRustyBucket || caught == kCrayfish);
        bucket |= caught == kRustyBucket;
        crayfish |= caught == kCrayfish;
    }
    CHECK(bucket && crayfish);
}

TEST(the_case_starts_in_the_middle_and_stays_on_the_screen)
{
    CHECK(CasePlace(kFishBarUnplaced, kFishBarUnplaced, 100, 2560, 1440) == (CaseSpot{1280, 720, 2.0f}));
    CHECK(CasePlace(300, 200, 50, 1920, 1080) == (CaseSpot{300, 200, 0.75f}));
    CHECK(CasePlace(5000, -40, 200, 1920, 1080) == (CaseSpot{1920, 0, 3.0f})); // a spot from a larger screen
}

namespace
{
    // A 1280 by 720 screen with the Case in its middle at full size: screen pixels are the layout's own.
    constexpr CaseSpot kLayoutSpot{640, 360, 1};
    constexpr CaseWords kWords{5, 4, 6};
    constexpr uint32_t kGold = 0xF2C94C, kTint = 0x0E0924, kBandInk = 0x080A14;

    std::vector<CaseQuad> Frame(float seconds, bool background = true, CaseWords words = kWords, CaseSpot spot = kLayoutSpot,
        float width = 1280, float height = 720, float length = kFull)
    {
        return CaseFrame(SpinCase(kMoatCarp, {{kMoatCarp, 1}, {kRustyBucket, 1}}, 3), seconds, length, spot, width, height, background, words);
    }
    std::vector<CaseQuad> Of(const std::vector<CaseQuad>& quads, CasePicture picture)
    {
        std::vector<CaseQuad> found;
        for (const CaseQuad& q : quads)
            if (q.picture == picture) found.push_back(q);
        return found;
    }
    uint32_t Rgb(const CaseQuad& q) { return q.argb & 0xFFFFFF; }
    int Alpha(const CaseQuad& q) { return static_cast<int>(q.argb >> 24); }
    float CenterX(const CaseQuad& q) { return q.x + q.width / 2.0f; }
    float CenterY(const CaseQuad& q) { return q.y + q.height / 2.0f; }
    bool Box(const CaseQuad& q, float x, float y, float w, float h)
    {
        return Near(q.x, x, 0.01f) && Near(q.y, y, 0.01f) && Near(q.width, w, 0.01f) && Near(q.height, h, 0.01f);
    }
}

TEST(the_spinning_strip_is_a_band_tiles_a_title_and_a_gold_line)
{
    const std::vector<CaseQuad> quads = Frame(3.5f);
    CHECK(Box(Of(quads, CasePicture::Band).at(0), 120, 274, 1040, 132));
    const CaseQuad title = Of(quads, CasePicture::Title).at(0);
    CHECK(Near(title.width, 150) && Near(title.height, 30) && Near(CenterX(title), 640) && Near(CenterY(title), 246));
    const CaseQuad line = Of(quads, CasePicture::Solid).back();
    CHECK(Box(line, 639, 276, 2, 128) && Rgb(line) == kGold);
    const float along = CaseAt(SpinCase(kMoatCarp, {{kMoatCarp, 1}, {kRustyBucket, 1}}, 3), 3.5f, kFull).along;
    const std::vector<CaseQuad> tiles = Of(quads, CasePicture::Tile);
    const auto nearest = std::ranges::min_element(tiles, {}, [](const CaseQuad& q) { return std::abs(CenterX(q) - 640.0f); });
    CHECK(Near(CenterX(*nearest), 640.0f + (std::round(along) - along) * 136.0f, 0.01f));
    CHECK(Near(nearest->width, 132) && Near(nearest->height, 104));
    CHECK(Of(quads, CasePicture::Name).empty() && Of(quads, CasePicture::Skill).empty() && Of(quads, CasePicture::ItemCrisp).empty());
}

TEST(tiles_fade_out_toward_the_strips_ends)
{
    bool faded = false;
    for (const CaseQuad& tile : Of(Frame(3.5f), CasePicture::Tile))
    {
        const float off = std::abs(CenterX(tile) - 640.0f);
        CHECK(off < 520.0f);
        CHECK(std::abs(Alpha(tile) - 255.0f * std::clamp((520.0f - off) / 170.0f, 0.0f, 1.0f)) <= 1.0f);
        faded |= Alpha(tile) < 255;
    }
    CHECK(faded);
}

TEST(once_it_stops_the_catch_is_lit_and_the_rest_dimmed)
{
    const std::vector<CaseQuad> quads = Frame(7.0f);
    int edges = 0, dims = 0;
    for (const CaseQuad& q : Of(quads, CasePicture::Solid))
    {
        if (Rgb(q) == CaseColor(kMoatCarp) && (Near(q.width, 3) || Near(q.height, 3))) ++edges;
        if (Rgb(q) == kBandInk && Near(q.width, 132) && Near(q.height, 104)) ++dims;
    }
    CHECK_EQ(edges, 4);
    CHECK_EQ(dims, static_cast<int>(Of(quads, CasePicture::Tile).size()) - 1);
}

TEST(the_reveal_is_the_catch_large_with_its_name_and_skill)
{
    const std::vector<CaseQuad> quads = Frame(8.5f);
    CHECK(Of(quads, CasePicture::Band).empty() && Of(quads, CasePicture::Tile).empty());
    const CaseQuad icon = Of(quads, CasePicture::ItemCrisp).at(0);
    CHECK(icon.item == kMoatCarp && Box(icon, 528, 288, 224, 224));
    const CaseQuad name = Of(quads, CasePicture::Name).at(0);
    CHECK(Near(name.width, 200) && Near(name.height, 50) && Near(CenterX(name), 640) && Near(CenterY(name), 162));
    const auto underline = std::ranges::find_if(quads, [](const CaseQuad& q) { return q.picture == CasePicture::Solid && Near(q.height, 3); });
    CHECK(underline != quads.end() && Near(underline->width, 248) && Near(CenterY(*underline), 193.5f) && Rgb(*underline) == CaseColor(kMoatCarp));
    const CaseQuad skill = Of(quads, CasePicture::Skill).at(0);
    CHECK(Near(skill.width, 132) && Near(skill.height, 22) && Near(CenterX(skill), 640) && Near(CenterY(skill), 217));
}

TEST(only_the_glows_add_their_light)
{
    for (float t = 0.0f; t < kFull; t += 0.1f)
        for (const CaseQuad& q : Frame(t)) CHECK(q.add == (q.picture == CasePicture::Glow));
}

TEST(the_tint_covers_the_screen_and_goes_with_the_background)
{
    const std::vector<CaseQuad> quads = Frame(0.25f);
    CHECK(quads.at(0).picture == CasePicture::Solid && Box(quads[0], 0, 0, 1280, 720) && Rgb(quads[0]) == kTint);
    CHECK(std::abs(Alpha(quads[0]) - 255.0f * 0.62f * 0.5f) <= 1.0f);
    CHECK(quads.at(1).picture == CasePicture::Vignette && Box(quads[1], 0, 0, 1280, 720));
    const std::vector<CaseQuad> bare = Frame(0.25f, false);
    CHECK(Of(bare, CasePicture::Vignette).empty() && !Of(bare, CasePicture::Band).empty());
    CHECK(std::ranges::none_of(bare, [](const CaseQuad& q) { return Box(q, 0, 0, 1280, 720); }));
}

TEST(the_case_follows_its_spot_and_scale)
{
    const std::vector<CaseQuad> quads = Frame(3.5f, true, kWords, CaseSpot{1000, 500, 2}, 2560, 1440);
    CHECK(Near(CenterX(Of(quads, CasePicture::Solid).back()), 1000));
    const CaseQuad band = Of(quads, CasePicture::Band).at(0);
    CHECK(Near(band.width, 2080) && Near(CenterX(band), 1000) && Near(CenterY(band), 460));
    CHECK(Box(quads.at(0), 0, 0, 2560, 1440));
}

TEST(a_catch_with_no_skill_line_shows_only_its_name)
{
    const std::vector<CaseQuad> quads = Frame(8.5f, true, CaseWords{5, 4, 0});
    CHECK(!Of(quads, CasePicture::Name).empty() && Of(quads, CasePicture::Skill).empty());
}

TEST(nothing_is_drawn_before_it_starts_or_after_it_ends)
{
    CHECK(Frame(0.0f).empty() && Frame(kFull).empty());
    const std::vector<CaseQuad> half = Frame(1.75f, true, kWords, kLayoutSpot, 1280, 720, kFull / 2.0f), full = Frame(3.5f);
    CHECK_EQ(half.size(), full.size());
    for (size_t i = 0; i < std::min(half.size(), full.size()); ++i)
        CHECK(half[i].picture == full[i].picture && Box(half[i], full[i].x, full[i].y, full[i].width, full[i].height) && half[i].argb == full[i].argb);
}

TEST(the_move_boxes_stand_where_the_case_draws)
{
    for (const auto& [spot, width, height] : {std::tuple{kLayoutSpot, 1280.0f, 720.0f}, std::tuple{CaseSpot{1000, 500, 2}, 2560.0f, 1440.0f}})
    {
        const CaseBoxSet boxes = CaseBoxesAt(spot);
        const auto same = [](const CaseBox& b, const CaseQuad& q) { return Box(q, b.x, b.y, b.width, b.height); };
        const std::vector<CaseQuad> spinning = Frame(3.5f, true, kWords, spot, width, height), shown = Frame(8.5f, true, kWords, spot, width, height);
        CHECK(same(boxes.band, Of(spinning, CasePicture::Band).at(0)));
        CHECK(same(boxes.line, Of(spinning, CasePicture::Solid).back()));
        CHECK(same(boxes.icon, Of(shown, CasePicture::ItemCrisp).at(0)));
        const CaseQuad name = Of(shown, CasePicture::Name).at(0);
        CHECK(Near(boxes.name.height, name.height, 0.01f) && Near(boxes.name.y + boxes.name.height / 2.0f, CenterY(name), 0.01f));
    }
}

namespace
{
    Rml::Element* Group(test::RmlSession& rml)
    {
        Rml::ElementDocument* document = rml.context->GetDocument(kCaseBoxesDocument);
        return document == nullptr ? nullptr : document->GetElementById(kCaseGroupId);
    }
    Rml::Vector2f Screen(test::RmlSession& rml)
    {
        const Rml::Vector2i size = rml.context->GetDimensions();
        return {static_cast<float>(size.x), static_cast<float>(size.y)};
    }
    CaseBox Shown(Rml::Element* box)
    {
        const Rml::Vector2f at = box->GetAbsoluteOffset(Rml::BoxArea::Border), size = box->GetBox().GetSize(Rml::BoxArea::Border);
        return CaseBox{at.x, at.y, size.x, size.y};
    }
    bool Within(const CaseBox& a, const CaseBox& b)
    {
        return std::abs(a.x - b.x) <= 1 && std::abs(a.y - b.y) <= 1 && std::abs(a.width - b.width) <= 1 && std::abs(a.height - b.height) <= 1;
    }
}

TEST(the_boxes_show_only_while_moving)
{
    test::RmlSession rml;
    Settings settings;
    CaseBoxes boxes;
    const BarPalette& palette = PaletteOf(BarTheme::PhoenixGold);
    boxes.Update(rml.context, palette, settings, false);
    CHECK(rml.context->GetDocument(kCaseBoxesDocument) == nullptr);
    boxes.Update(rml.context, palette, settings, true);
    rml.context->Update();
    CHECK(Group(rml) != nullptr && Group(rml)->IsVisible());
    const Rml::Vector2f screen = Screen(rml);
    const CaseBoxSet want = CaseBoxesAt(CasePlace(kFishBarUnplaced, kFishBarUnplaced, settings.fishCaseScale, screen.x, screen.y));
    const CaseBox expected[] = {want.band, want.line, want.icon, want.name};
    CHECK_EQ(Group(rml)->GetNumChildren(), 4);
    for (int i = 0; i < 4; ++i) CHECK(Within(Shown(Group(rml)->GetChild(i)), expected[i]));
    boxes.Update(rml.context, palette, settings, false);
    rml.context->Update();
    CHECK(!Group(rml)->IsVisible());
    CHECK_EQ(rml.Said(), "");
    boxes.Close();
}

TEST(the_boxes_drag_the_case_to_a_place_of_its_own)
{
    test::RmlSession rml;
    Settings settings;
    CaseBoxes boxes;
    const BarPalette& palette = PaletteOf(BarTheme::PhoenixGold);
    CHECK(!boxes.Update(rml.context, palette, settings, true));
    rml.context->Update();
    const Rml::Vector2f screen = Screen(rml);
    const CaseBox band = Shown(Group(rml)->GetChild(0));
    const auto drag = [&](int dx, int dy) {
        const int x = static_cast<int>(band.x + band.width / 2.0f), y = static_cast<int>(band.y + band.height / 2.0f);
        rml.context->ProcessMouseMove(x, y, 0);
        rml.context->ProcessMouseButtonDown(0, 0);
        for (int step = 1; step <= 4; ++step)
        {
            rml.context->ProcessMouseMove(x + dx * step / 4, y + dy * step / 4, 0);
            rml.context->Update();
            CHECK(!boxes.Update(rml.context, palette, settings, true)); // written once, when it is let go
        }
        rml.context->ProcessMouseButtonUp(0, 0);
    };
    drag(200, -100);
    CHECK(boxes.Update(rml.context, palette, settings, true));
    CHECK_EQ(settings.fishCaseX, static_cast<int>(screen.x / 2.0f) + 200);
    CHECK_EQ(settings.fishCaseY, static_cast<int>(screen.y / 2.0f) - 100);
    CHECK(!boxes.Update(rml.context, palette, settings, true));
    settings.fishCaseX = settings.fishCaseY = kFishBarUnplaced;
    boxes.Update(rml.context, palette, settings, true);
    rml.context->Update();
    drag(static_cast<int>(screen.x) * 2, 0);
    boxes.Update(rml.context, palette, settings, true);
    CHECK_EQ(settings.fishCaseX, static_cast<int>(screen.x));
    CHECK_EQ(rml.Said(), "");
    boxes.Close();
}

TEST(the_boxes_follow_the_scale)
{
    test::RmlSession rml;
    Settings settings;
    CaseBoxes boxes;
    const BarPalette& palette = PaletteOf(BarTheme::PhoenixGold);
    boxes.Update(rml.context, palette, settings, true);
    rml.context->Update();
    const float before = Shown(Group(rml)->GetChild(0)).width;
    settings.fishCaseScale = 200;
    boxes.Update(rml.context, palette, settings, true);
    rml.context->Update();
    CHECK(std::abs(Shown(Group(rml)->GetChild(0)).width - 2.0f * before) <= 1.0f);
    CHECK_EQ(rml.Said(), "");
    boxes.Close();
}

TEST(hidden_boxes_take_no_click)
{
    test::RmlSession rml;
    Settings settings;
    CaseBoxes boxes;
    const BarPalette& palette = PaletteOf(BarTheme::PhoenixGold);
    boxes.Update(rml.context, palette, settings, true);
    rml.context->Update();
    const CaseBox band = Shown(Group(rml)->GetChild(0));
    boxes.Update(rml.context, palette, settings, false);
    rml.context->Update();
    rml.context->ProcessMouseMove(static_cast<int>(band.x + band.width / 2.0f), static_cast<int>(band.y + band.height / 2.0f), 0);
    rml.context->Update();
    const Rml::Element* hover = rml.context->GetHoverElement();
    CHECK(hover == nullptr || hover->GetOwnerDocument() != rml.context->GetDocument(kCaseBoxesDocument));
    CHECK_EQ(rml.Said(), "");
    boxes.Close();
}
