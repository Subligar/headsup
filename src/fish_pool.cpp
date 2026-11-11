#include "fish_pool.h"

#include <algorithm>
#include <cmath>
#include <span>

namespace headsup
{
    namespace
    {
        struct FishAreaRow
        {
            uint16_t zone;
            uint8_t area, type, height, radius;
            float x, y, z;
            uint16_t group, firstPoint, points;
        };
        struct AreaPoint
        {
            float x, z;
        };
        struct GroupFishRow
        {
            uint16_t group, item;
        };
        struct BaitRow
        {
            uint16_t bait;
            bool lure, shellfish;
            uint16_t firstAffinity, affinities;
        };
        struct AffinityRow
        {
            uint16_t item;
            uint8_t power;
        };
        struct RodRow
        {
            uint16_t rod;
            uint8_t size;
            bool legendary;
        };
#include "generated/fishing.inc"

        enum AreaType : uint8_t { kWholeZone, kCylinder, kPolygon };
        constexpr size_t kMinPolygonPoints = 3;
        // isInsidePoly's ray runs to this x and to z 0, not straight along x: the server's own quirk, kept.
        constexpr AreaPoint kRayEnd{10000.0f, 0.0f};
        // Phoenix never loads a fish's hour, moon and month patterns, so each takes pattern 0's modifiers:
        // floor(25 * (1.25 * 3 + 0.75 * 2 + 0.75) / 3).
        constexpr int kBaseHookChance = 50;
        constexpr int kBaitBonus[] = {0, 35, 65, 80}; // by the bait's power over the fish
        constexpr int kLureBonus[] = {0, 30, 60, 75};
        constexpr double kUnderSkillShare = 0.25, kOverSkillShare = 0.15;
        constexpr int kOverSkillSlack = 10;
        constexpr int kSmallerFishPenalty = 3, kLargerFishPenalty = 5;
        constexpr int kShellfishBonus = 50;
        constexpr int kFullRarity = 1000;
        constexpr int kMinHookChance = 20, kMaxHookChance = 120;
        constexpr int kSkillReach = 100; // a fish further above your skill than this never bites
        constexpr int kItemChance = 100;
        constexpr uint16_t kOnlyWeight = 1;

        template <typename Row, typename Key, typename Projection>
        std::span<const Row> RowsOf(std::span<const Row> rows, Key key, Projection by)
        {
            const auto found = std::ranges::equal_range(rows, key, {}, by);
            return {found.begin(), found.end()};
        }

        std::span<const FishAreaRow> AreasOf(uint16_t zone) { return RowsOf<FishAreaRow>(kFishAreas, zone, &FishAreaRow::zone); }

        bool WithinHeight(const FishAreaRow& area, float y)
        {
            const int half = area.height / 2;
            return y >= area.y - half && y <= area.y + half;
        }

        bool OnSegment(AreaPoint p, AreaPoint q, AreaPoint r)
        {
            return q.x <= std::max(p.x, r.x) && q.x >= std::min(p.x, r.x) && q.z <= std::max(p.z, r.z) && q.z >= std::min(p.z, r.z);
        }

        int Orientation(AreaPoint p, AreaPoint q, AreaPoint r)
        {
            // The server rounds the first difference only.
            const float value = std::round(q.z - p.z) * (r.x - q.x) - (q.x - p.x) * (r.z - q.z);
            return value == 0 ? 0 : value > 0 ? 1 : 2;
        }

        bool Intersect(AreaPoint p1, AreaPoint q1, AreaPoint p2, AreaPoint q2)
        {
            const int o1 = Orientation(p1, q1, p2), o2 = Orientation(p1, q1, q2), o3 = Orientation(p2, q2, p1),
                      o4 = Orientation(p2, q2, q1);
            return (o1 != o2 && o3 != o4) || (o1 == 0 && OnSegment(p1, p2, q1)) || (o2 == 0 && OnSegment(p1, q2, q1)) ||
                   (o3 == 0 && OnSegment(p2, p1, q2)) || (o4 == 0 && OnSegment(p2, q1, q2));
        }

        bool InsidePolygon(const FishAreaRow& area, AreaPoint p)
        {
            const std::span<const AreaPoint> points(kAreaPoints + area.firstPoint, area.points);
            if (points.size() < kMinPolygonPoints) return false;
            int crossings = 0;
            for (size_t i = 0; i < points.size(); ++i)
            {
                const AreaPoint a = points[i], b = points[(i + 1) % points.size()];
                if (!Intersect(a, b, p, kRayEnd)) continue;
                if (Orientation(a, p, b) == 0) return OnSegment(a, p, b);
                ++crossings;
            }
            return crossings % 2 == 1;
        }

        bool InsideCylinder(const FishAreaRow& area, AreaPoint p)
        {
            const float dx = std::abs(p.x - area.x), dz = std::abs(p.z - area.z), r = area.radius;
            if (dx > r || dz > r) return false;
            return dx + dz <= r || dx * dx + dz * dz <= r * r;
        }

        template <typename Row>
        const Row* ById(std::span<const Row> rows, uint16_t id, uint16_t Row::*key)
        {
            const auto found = RowsOf<Row>(rows, id, key);
            return found.empty() ? nullptr : &found.front();
        }

        int PowerOver(const BaitRow& bait, uint16_t item)
        {
            const std::span<const AffinityRow> all(kAffinities + bait.firstAffinity, bait.affinities);
            const AffinityRow* row = ById<AffinityRow>(all, item, &AffinityRow::item);
            return row == nullptr ? 0 : row->power;
        }

        // fishingutils.cpp's CalculateHookChance.
        int HookChance(const FishRecord& fish, const BaitRow& bait, int power, const RodRow* rod, int skill)
        {
            int chance = kBaseHookChance + (bait.lure ? kLureBonus : kBaitBonus)[power];
            if (fish.skill > skill) chance -= std::min(static_cast<int>(std::floor((fish.skill - skill) * kUnderSkillShare)), chance);
            if (skill - kOverSkillSlack > fish.skill)
                chance -= std::min(static_cast<int>(std::floor((skill - kOverSkillSlack - fish.skill) * kOverSkillShare)), chance);
            if (rod != nullptr && !rod->legendary)
            {
                if (fish.size < rod->size) chance -= std::min(kSmallerFishPenalty, chance);
                else if (rod->size < fish.size) chance -= std::min(kLargerFishPenalty, chance);
            }
            if (bait.shellfish && fish.shellfish) chance += kShellfishBonus;
            if (fish.rarity < kFullRarity)
                chance = static_cast<int>(std::floor(static_cast<float>(chance) * (static_cast<float>(fish.rarity) / kFullRarity)));
            return std::clamp(chance, kMinHookChance, kMaxHookChance);
        }

        std::span<const GroupFishRow> GroupOf(const FishAreaRow& area)
        {
            return RowsOf<GroupFishRow>(kGroupFish, area.group, &GroupFishRow::group);
        }

        void Add(std::vector<PoolEntry>& pool, uint16_t item, int weight)
        {
            if (std::ranges::find(pool, item, &PoolEntry::item) == pool.end())
                pool.push_back(PoolEntry{item, static_cast<uint16_t>(weight)});
        }
    }

    const FishRecord* FishById(uint16_t item) { return ById<FishRecord>(kFish, item, &FishRecord::item); }

    std::optional<uint8_t> FishingArea(uint16_t zone, float x, float y, float z)
    {
        const AreaPoint p{x, z};
        for (const FishAreaRow& area : AreasOf(zone))
            if (area.type == kWholeZone || (area.type == kCylinder && WithinHeight(area, y) && InsideCylinder(area, p)) ||
                (area.type == kPolygon && WithinHeight(area, y) && InsidePolygon(area, p)))
                return area.area;
        return std::nullopt;
    }

    std::vector<PoolEntry> CatchPool(const Angler& angler, FunctionRef<bool(uint16_t)> hasKeyItem)
    {
        std::vector<PoolEntry> pool;
        const std::span<const FishAreaRow> areas = AreasOf(angler.zone);
        const std::optional<uint8_t> here = FishingArea(angler.zone, angler.x, angler.y, angler.z);
        const BaitRow* bait = ById<BaitRow>(kBaits, angler.bait, &BaitRow::bait);
        if (!here || bait == nullptr)
        {
            for (const FishAreaRow& area : areas)
                for (const GroupFishRow& row : GroupOf(area))
                    if (!FishById(row.item)->quest) Add(pool, row.item, kOnlyWeight);
        }
        else
        {
            const RodRow* rod = ById<RodRow>(kRods, angler.rod, &RodRow::rod);
            for (const GroupFishRow& row : GroupOf(*std::ranges::find(areas, *here, &FishAreaRow::area)))
            {
                const FishRecord& fish = *FishById(row.item);
                const bool keyed = fish.keyItem == 0 || hasKeyItem(fish.keyItem);
                if (fish.isItem)
                {
                    const int weight = kItemChance * fish.rarity / kFullRarity;
                    if ((fish.questOnly || keyed) && !fish.quest && weight > 0) Add(pool, row.item, weight);
                    continue;
                }
                const int power = PowerOver(*bait, row.item);
                const bool reach = angler.skill >= fish.skill || fish.skill - angler.skill <= kSkillReach;
                if (power > 0 && reach && keyed) Add(pool, row.item, HookChance(fish, *bait, power, rod, angler.skill));
            }
        }
        std::ranges::sort(pool, {}, &PoolEntry::item);
        return pool;
    }
}
