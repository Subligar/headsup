#include "item_picture.h"

#include <cstdlib>

namespace headsup
{
    std::optional<Image> ResourcePicture(const uint8_t* bmp, size_t size)
    {
        constexpr size_t kFileHead   = 14;
        constexpr size_t kInfoHead   = 40;
        constexpr int kLargest       = 256;
        constexpr uint32_t kIndexed  = 8;
        constexpr uint32_t kTrueColor = 32; // B, G, R, then the game's alpha of 0 or 0x80, left out: XIUI keys black too
        constexpr uint32_t kPalette  = 256;
        constexpr size_t kEntryBytes = 4;
        constexpr uint32_t kOpaque   = 0xFF000000u;
        const auto read = [&](size_t at, int bytes) {
            uint32_t value = 0;
            for (int i = bytes - 1; i >= 0; --i)
                value = value << 8 | bmp[at + static_cast<size_t>(i)];
            return value;
        };
        if (bmp == nullptr) return std::nullopt;
        if (size >= kFileHead && bmp[0] == 'B' && bmp[1] == 'M')
        {
            bmp += kFileHead;
            size -= kFileHead;
        }
        if (size < kInfoHead) return std::nullopt;
        const uint32_t head = read(0, 4);
        const auto width = static_cast<int32_t>(read(4, 4)), height = static_cast<int32_t>(read(8, 4));
        const uint32_t bits = read(14, 2), compression = read(16, 4), used = read(32, 4);
        const int rows = std::abs(height);
        if (head < kInfoHead || width <= 0 || width > kLargest || rows == 0 || rows > kLargest ||
            (bits != kIndexed && bits != kTrueColor) || compression != 0 || used > kPalette)
            return std::nullopt;
        const bool indexed   = bits == kIndexed;
        const size_t entries = !indexed ? 0 : used == 0 ? kPalette : used;
        const size_t pixels  = head + entries * kEntryBytes;
        const size_t stride  = indexed ? (static_cast<size_t>(width) + 3) & ~size_t{3} : static_cast<size_t>(width) * kEntryBytes;
        if (size < pixels + stride * static_cast<size_t>(rows)) return std::nullopt;
        Image image{width, rows, std::vector<uint32_t>(static_cast<size_t>(width) * static_cast<size_t>(rows), 0)};
        for (int y = 0; y < rows; ++y)
        {
            // Rows bottom up unless the height is negative.
            const size_t row = height > 0 ? static_cast<size_t>(rows - 1 - y) : static_cast<size_t>(y);
            for (int x = 0; x < width; ++x)
            {
                const size_t at = pixels + row * stride + static_cast<size_t>(x) * (indexed ? 1 : kEntryBytes);
                if (indexed && bmp[at] >= entries) continue;
                // A pixel's or palette entry's bytes are blue, green, red: read low byte first, that is 0xRRGGBB.
                const uint32_t rgb = read(indexed ? head + bmp[at] * kEntryBytes : at, 3);
                if (rgb != 0) image.argb[static_cast<size_t>(y) * static_cast<size_t>(width) + static_cast<size_t>(x)] = kOpaque | rgb;
            }
        }
        return image;
    }
}
