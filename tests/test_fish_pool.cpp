#include "fish_pool.h"
#include "test.h"

#include <algorithm>
#include <optional>
#include <vector>

using namespace headsup;

namespace
{
    // Every value below was worked by hand from Phoenix's SQL and fishingutils.cpp's CalculateHookChance.
    constexpr uint16_t kWestRonfaure = 100, kBibikiBay = 4, kCarpentersLanding = 2, kPhanauetChannel = 1, kEasternAltepa = 114;
    constexpr uint16_t kLittleWorm = 17396, kRobberRig = 17002, kWillowRod = 17391, kLufaiseFly = 17005;
    constexpr uint16_t kPortWindurst = 240, kKazham = 250;
    constexpr uint16_t kTricorn = 4319, kSandfish = 4291, kRippedCap = 591;
    constexpr uint16_t kTricornKey = 1976;

    bool AnyKey(uint16_t) { return true; }
    bool NoKey(uint16_t) { return false; }

    std::optional<uint16_t> WeightOf(const std::vector<PoolEntry>& pool, uint16_t item)
    {
        const auto at = std::ranges::find(pool, item, &PoolEntry::item);
        return at == pool.end() ? std::nullopt : std::optional<uint16_t>(at->weight);
    }
}

TEST(a_spots_pool_is_its_fish_by_hook_chance_and_its_items_by_rarity)
{
    // Moat Carp 50 + 65 = 115. Giant Catfish 115 - floor(11 * 0.25) - 5 (a small rod) = 108, at rarity 900: 97.
    // Crayfish 115. Red Terrapin takes no Little Worm. Items: floor(100 * rarity / 1000).
    const std::vector<PoolEntry> pool = CatchPool(Angler{kWestRonfaure, 0, 0, 0, kWillowRod, kLittleWorm, 20}, AnyKey);
    CHECK(pool == (std::vector<PoolEntry>{{90, 90}, {688, 50}, {4401, 115}, {4469, 97}, {4472, 115}, {14117, 50}}));
}

TEST(skill_far_above_a_fish_lowers_its_chance)
{
    // 115 - floor((100 - 10 - 11) * 0.15) = 104
    CHECK(WeightOf(CatchPool(Angler{kWestRonfaure, 0, 0, 0, kWillowRod, kLittleWorm, 100}, AnyKey), 4401) == 104);
}

TEST(a_fish_over_a_hundred_levels_above_you_is_left_out)
{
    // Tricorn asks skill 128.
    CHECK(!WeightOf(CatchPool(Angler{kPhanauetChannel, 0, 0, 0, kWillowRod, kLufaiseFly, 0}, AnyKey), kTricorn));
    CHECK(WeightOf(CatchPool(Angler{kPhanauetChannel, 0, 0, 0, kWillowRod, kLufaiseFly, 28}, AnyKey), kTricorn));
}

TEST(a_fish_behind_a_key_item_needs_it)
{
    const Angler angler{kPhanauetChannel, 0, 0, 0, kWillowRod, kLufaiseFly, 128};
    CHECK(!WeightOf(CatchPool(angler, NoKey), kTricorn));
    CHECK(WeightOf(CatchPool(angler, [](uint16_t key) { return key == kTricornKey; }), kTricorn));
}

TEST(quest_items_stay_out_of_the_pool)
{
    // Port Windurst's and Kazham's whole-zone pools list the Ripped Cap, a quest's catch.
    for (const uint16_t zone : {kPortWindurst, kKazham})
    {
        CHECK(!WeightOf(CatchPool(Angler{zone, 0, 0, 0, kWillowRod, kLittleWorm, 50}, AnyKey), kRippedCap));
        CHECK(!WeightOf(CatchPool(Angler{zone, 0, 0, 0, kWillowRod, 0, 50}, AnyKey), kRippedCap));
        CHECK(!CatchPool(Angler{zone, 0, 0, 0, kWillowRod, 0, 50}, AnyKey).empty());
    }
}

TEST(a_lure_and_a_shellfish_bait_change_the_chance)
{
    // A lure at power 1 gives 30, not 35: Moat Carp 50 + 30 - floor(11 * 0.25) = 78. Sandfish is shellfish and the
    // Robber Rig pulls them: 50 + 30 - floor(50 * 0.25) + 50 = 118.
    const std::vector<PoolEntry> pool = CatchPool(Angler{kEasternAltepa, 0, 0, 0, kWillowRod, kRobberRig, 0}, AnyKey);
    CHECK(WeightOf(pool, 4401) == 78);
    CHECK(WeightOf(pool, kSandfish) == 118);
}

TEST(the_area_you_stand_in_picks_the_pool)
{
    // Bibiki Bay's area 1 is a polygon, x -450 to -260, z -980 to -860, 20 high around y 0.
    CHECK(FishingArea(kBibikiBay, -350, 0, -900) == std::optional<uint8_t>{1});
    CHECK(FishingArea(kBibikiBay, -350, 11, -900) != std::optional<uint8_t>{1});
    // Area 4 is a cylinder of radius 150 around (-110, -640), checked before the whole-zone area 6.
    CHECK(FishingArea(kBibikiBay, -110, 0, -500) == std::optional<uint8_t>{4});
    CHECK(FishingArea(kBibikiBay, -10, 0, -540) == std::optional<uint8_t>{4}); // 141 yalms out on the diagonal
    CHECK(FishingArea(kBibikiBay, -110, 0, -480) == std::optional<uint8_t>{6});
    // Carpenters' Landing has no whole-zone area.
    CHECK(!FishingArea(kCarpentersLanding, 9000, 0, 9000).has_value());
    CHECK(FishingArea(kWestRonfaure, 9000, 50, 9000) == std::optional<uint8_t>{1});
}

TEST(with_no_bait_or_outside_every_area_the_strip_gets_the_zones_catches)
{
    CHECK(CatchPool(Angler{kWestRonfaure, 0, 0, 0, kWillowRod, 0, 20}, AnyKey) ==
          (std::vector<PoolEntry>{{90, 1}, {688, 1}, {4401, 1}, {4402, 1}, {4469, 1}, {4472, 1}, {14117, 1}}));
    CHECK(!CatchPool(Angler{kCarpentersLanding, 9000, 0, 9000, kWillowRod, kLittleWorm, 20}, AnyKey).empty());
    CHECK(CatchPool(Angler{0, 0, 0, 0, 0, 0, 0}, AnyKey).empty());
}
