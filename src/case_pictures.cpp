#include "case_pictures.h"

#include <algorithm>
#include <array>
#include <cmath>

namespace headsup
{
    namespace
    {
        constexpr uint32_t kWhiteRgb      = 0xFFFFFF;
        constexpr float kFullByte      = 255.0f;
        constexpr uint32_t kAlphaBits = 24;
        constexpr float kFontShare     = 0.8f; // Georgia's letters' size over the height of the line GDI draws them in

        uint32_t Pixel(uint32_t rgb, float alpha)
        {
            return static_cast<uint32_t>(std::lround(std::clamp(alpha, 0.0f, 1.0f) * kFullByte)) << kAlphaBits | rgb;
        }

        template <typename Paint>
        Image Painted(int width, int height, Paint paint)
        {
            Image image;
            image.width  = width;
            image.height = height;
            image.argb.resize(static_cast<size_t>(width) * static_cast<size_t>(height));
            for (int y = 0; y < height; ++y)
                for (int x = 0; x < width; ++x) image.argb[static_cast<size_t>(y * width + x)] = paint(x, y);
            return image;
        }

        // A glow's light by its distance from the middle, 1 at its edge.
        float GlowAt(float distance)
        {
            constexpr std::array<std::array<float, 2>, 4> kStops{{{0.0f, 1.0f}, {0.18f, 0.75f}, {0.45f, 0.22f}, {1.0f, 0.0f}}};
            if (distance >= 1.0f) return 0.0f;
            for (size_t i = 1; i < kStops.size(); ++i)
                if (distance <= kStops[i][0])
                {
                    const float u = (distance - kStops[i - 1][0]) / (kStops[i][0] - kStops[i - 1][0]);
                    return kStops[i - 1][1] + (kStops[i][1] - kStops[i - 1][1]) * u;
                }
            return 0.0f;
        }

        uint32_t Mixed(uint32_t from, uint32_t to, float share)
        {
            uint32_t rgb = 0;
            for (const uint32_t shift : {16u, 8u, 0u})
            {
                const float a = static_cast<float>(from >> shift & 0xFF), b = static_cast<float>(to >> shift & 0xFF);
                rgb |= static_cast<uint32_t>(std::lround(a + (b - a) * share)) << shift;
            }
            return rgb;
        }
    }

    Image CaseImage(CasePicture picture)
    {
        switch (picture)
        {
        case CasePicture::Solid: return Painted(1, 1, [](int, int) { return Pixel(kWhiteRgb, 1.0f); });
        case CasePicture::Vignette:
        {
            // A 16:9 frame at a fifth of 1280 by 720: clear within 0.3 of its height of the middle, dark at 0.75 of its width.
            constexpr int kWidth = 256, kHeight = 144;
            constexpr float kClear = 0.3f * kHeight, kDark = 0.75f * kWidth;
            return Painted(kWidth, kHeight, [](int x, int y) {
                const float d = std::hypot(static_cast<float>(x) + 0.5f - kWidth / 2.0f, static_cast<float>(y) + 0.5f - kHeight / 2.0f);
                return Pixel(kWhiteRgb, (d - kClear) / (kDark - kClear));
            });
        }
        case CasePicture::Glow:
        {
            constexpr int kSide   = 128;
            constexpr float kHalf = kSide / 2.0f;
            // The outermost pixels are clear, so a stretched glow never shows its square's edge.
            return Painted(kSide, kSide, [](int x, int y) {
                const float d = std::hypot(static_cast<float>(x) + 0.5f - kHalf, static_cast<float>(y) + 0.5f - kHalf) / (kHalf - 1.0f);
                return Pixel(kWhiteRgb, GlowAt(d));
            });
        }
        case CasePicture::Tile:
        {
            constexpr int kWidth = 8, kHeight = 64;
            constexpr uint32_t kTop = 0xE4E7EC, kFoot = 0x8E949D;
            return Painted(kWidth, kHeight, [](int, int y) { return Pixel(Mixed(kTop, kFoot, static_cast<float>(y) / (kHeight - 1)), 1.0f); });
        }
        case CasePicture::Rise:
        {
            constexpr int kWidth = 4, kHeight = 64;
            return Painted(kWidth, kHeight, [](int, int y) { return Pixel(kWhiteRgb, static_cast<float>(y) / (kHeight - 1)); });
        }
        case CasePicture::Band:
        {
            // Its fade is the strip's: 170 of the 1040 pixels it spans, at each end.
            constexpr int kWidth = 256, kHeight = 4;
            constexpr float kFade = kWidth * 170.0f / 1040.0f;
            return Painted(kWidth, kHeight, [](int x, int) { return Pixel(kWhiteRgb, static_cast<float>(std::min(x, kWidth - 1 - x)) / kFade); });
        }
        default: return {};
        }
    }

    Image CaseTextImage(const Coverage& text, int drawn)
    {
        int last = -1;
        for (int y = 0; y < text.height; ++y)
            for (int x = text.width - 1; x > last; --x)
                if (text.alpha[static_cast<size_t>(y * text.width + x)] != 0) last = x;
        if (last < 0) return {};
        const int left = std::max(0, drawn - kCaseTextMargin), top = left;
        const int right = std::min(text.width, last + 1 + kCaseTextMargin), bottom = std::min(text.height, text.height - drawn + kCaseTextMargin);
        return Painted(right - left, bottom - top, [&](int x, int y) {
            return static_cast<uint32_t>(text.alpha[static_cast<size_t>((y + top) * text.width + x + left)]) << kAlphaBits | kWhiteRgb;
        });
    }

    int CaseFontPixels(float shown) { return std::max(1, static_cast<int>(std::lround(shown * kFontShare))); }
}
