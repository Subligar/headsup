#include "fish_case.h"
#include "settings.h"
#include "test.h"

#include <algorithm>
#include <cmath>
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
