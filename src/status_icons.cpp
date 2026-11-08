#include "status_icons.h"

#include <ft2build.h>
#include FT_GZIP_H
#include FT_SYSTEM_H

#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <iterator>
#include <vector>

namespace headsup
{
    namespace
    {
        struct PackedIcon
        {
            uint16_t id;
            uint8_t side; // pixels; the icon is square
            uint32_t at;  // where its B, G, R, A bytes start in its pack, unpacked
        };
        struct StatusPack
        {
            const unsigned char* packed;
            size_t packedSize, size;
            const PackedIcon* icons;
            size_t count;
        };
#include "generated/status_packs.inc"

        // XIUI 1.8.4's debuffs (libs/bufftable.lua, the 1s of statusEffects), plus KO, Ninjutsu Ele Debuff, Elemental
        // Resistance Down and Hysteria. XIUI counts a status missing from its table as a debuff; here it is a buff, as
        // most of those Phoenix has are (the Prowesses, the Trust auras).
        constexpr uint16_t kDebuffs[] = {
            0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 28, 29, 30, 31,
            128, 129, 130, 131, 132, 133, 134, 135, 136, 137, 138, 139, 140, 141, 142, 143, 144, 145, 146, 147,
            148, 149, 155, 156, 168, 171, 172, 173, 174, 175, 186, 189, 192, 193, 194, 223, 259, 260, 261, 262,
            263, 264, 291, 298, 299, 309, 391, 392, 393, 394, 395, 396, 397, 398, 399, 400, 404, 448, 449, 450,
            451, 452, 536, 557, 558, 559, 560, 561, 562, 563, 564, 565, 566, 567, 572, 576, 597, 630, 631, 793,
            798, 802, 804, 805};

        // What FreeType's inflate allocates with.
        FT_MemoryRec_ g_Heap{nullptr, [](FT_Memory, long size) { return std::malloc(static_cast<size_t>(size)); },
            [](FT_Memory, void* block) { std::free(block); },
            [](FT_Memory, long, long size, void* block) { return std::realloc(block, static_cast<size_t>(size)); }};

        // A pack's pixels, unpacked once; empty when they would not unpack.
        const std::vector<unsigned char>& Unpacked(int pack)
        {
            static std::vector<unsigned char> unpacked[kStatusPackCount];
            std::vector<unsigned char>& pixels = unpacked[pack];
            const StatusPack& from             = kStatusPacks[pack];
            if (!pixels.empty()) return pixels;
            pixels.resize(from.size);
            FT_ULong size = pixels.size();
            if (FT_Gzip_Uncompress(&g_Heap, pixels.data(), &size, from.packed, from.packedSize) != 0 || size != from.size)
                pixels.clear();
            return pixels;
        }
    }

    std::string StatusIconsName(int pack)
    {
        return pack >= 0 && pack < kStatusPackCount ? std::string(kStatusPackNames[pack]) : kVanillaStatusName;
    }

    int StatusIconsNamed(std::string_view name, int fallback)
    {
        if (name == kVanillaStatusName) return kVanillaStatusIcons;
        const auto* pack = std::ranges::find(kStatusPackNames, name);
        return pack != std::end(kStatusPackNames) ? static_cast<int>(pack - std::begin(kStatusPackNames)) : fallback;
    }

    bool IsDebuff(uint16_t id) { return std::binary_search(std::begin(kDebuffs), std::end(kDebuffs), id); }

    std::optional<Image> PackIcon(int pack, uint16_t id)
    {
        if (pack < 0 || pack >= kStatusPackCount) return std::nullopt;
        const StatusPack& from  = kStatusPacks[pack];
        const PackedIcon* end   = from.icons + from.count;
        const PackedIcon* icon  = std::lower_bound(from.icons, end, id, [](const PackedIcon& i, uint16_t want) { return i.id < want; });
        if (icon == end || icon->id != id) return std::nullopt;
        const std::vector<unsigned char>& pixels = Unpacked(pack);
        if (pixels.empty()) return std::nullopt;
        Image image{icon->side, icon->side, std::vector<uint32_t>(static_cast<size_t>(icon->side) * icon->side)};
        // The bytes are B, G, R, A: an ARGB word's bytes in memory.
        std::memcpy(image.argb.data(), pixels.data() + icon->at, image.argb.size() * sizeof(uint32_t));
        return image;
    }
}
