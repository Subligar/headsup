#include "rml_pure.h"

#include "argb.h"
#include "utf16.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace headsup
{
    namespace
    {
        using Rml::Input::KeyIdentifier;

        constexpr int kWheelDelta = 120; // WHEEL_DELTA: one notch of the wheel

        // Windows' virtual keys (winuser.h) by number, for the ones outside the runs of letters, figures, numpad
        // figures and function keys.
        struct KeyName
        {
            uint32_t virtualKey;
            KeyIdentifier key;
        };
        constexpr KeyName kKeyNames[] = {
            {0x08, KeyIdentifier::KI_BACK}, {0x09, KeyIdentifier::KI_TAB}, {0x0C, KeyIdentifier::KI_CLEAR},
            {0x0D, KeyIdentifier::KI_RETURN}, {0x10, KeyIdentifier::KI_LSHIFT}, {0x11, KeyIdentifier::KI_LCONTROL},
            {0x12, KeyIdentifier::KI_LMENU}, {0x13, KeyIdentifier::KI_PAUSE}, {0x14, KeyIdentifier::KI_CAPITAL},
            {0x15, KeyIdentifier::KI_KANA}, {0x17, KeyIdentifier::KI_JUNJA}, {0x18, KeyIdentifier::KI_FINAL},
            {0x19, KeyIdentifier::KI_KANJI}, {0x1B, KeyIdentifier::KI_ESCAPE}, {0x1C, KeyIdentifier::KI_CONVERT},
            {0x1D, KeyIdentifier::KI_NONCONVERT}, {0x1E, KeyIdentifier::KI_ACCEPT},
            {0x1F, KeyIdentifier::KI_MODECHANGE}, {0x20, KeyIdentifier::KI_SPACE}, {0x21, KeyIdentifier::KI_PRIOR},
            {0x22, KeyIdentifier::KI_NEXT}, {0x23, KeyIdentifier::KI_END}, {0x24, KeyIdentifier::KI_HOME},
            {0x25, KeyIdentifier::KI_LEFT}, {0x26, KeyIdentifier::KI_UP}, {0x27, KeyIdentifier::KI_RIGHT},
            {0x28, KeyIdentifier::KI_DOWN}, {0x29, KeyIdentifier::KI_SELECT}, {0x2A, KeyIdentifier::KI_PRINT},
            {0x2B, KeyIdentifier::KI_EXECUTE}, {0x2C, KeyIdentifier::KI_SNAPSHOT}, {0x2D, KeyIdentifier::KI_INSERT},
            {0x2E, KeyIdentifier::KI_DELETE}, {0x2F, KeyIdentifier::KI_HELP}, {0x5B, KeyIdentifier::KI_LWIN},
            {0x5C, KeyIdentifier::KI_RWIN}, {0x5D, KeyIdentifier::KI_APPS}, {0x5F, KeyIdentifier::KI_SLEEP},
            {0x6A, KeyIdentifier::KI_MULTIPLY}, {0x6B, KeyIdentifier::KI_ADD}, {0x6C, KeyIdentifier::KI_SEPARATOR},
            {0x6D, KeyIdentifier::KI_SUBTRACT}, {0x6E, KeyIdentifier::KI_DECIMAL}, {0x6F, KeyIdentifier::KI_DIVIDE},
            {0x90, KeyIdentifier::KI_NUMLOCK}, {0x91, KeyIdentifier::KI_SCROLL},
            {0x92, KeyIdentifier::KI_OEM_NEC_EQUAL}, {0xA0, KeyIdentifier::KI_LSHIFT},
            {0xA1, KeyIdentifier::KI_RSHIFT}, {0xA2, KeyIdentifier::KI_LCONTROL}, {0xA3, KeyIdentifier::KI_RCONTROL},
            {0xA4, KeyIdentifier::KI_LMENU}, {0xA5, KeyIdentifier::KI_RMENU}, {0xBA, KeyIdentifier::KI_OEM_1},
            {0xBB, KeyIdentifier::KI_OEM_PLUS}, {0xBC, KeyIdentifier::KI_OEM_COMMA},
            {0xBD, KeyIdentifier::KI_OEM_MINUS}, {0xBE, KeyIdentifier::KI_OEM_PERIOD}, {0xBF, KeyIdentifier::KI_OEM_2},
            {0xC0, KeyIdentifier::KI_OEM_3}, {0xDB, KeyIdentifier::KI_OEM_4}, {0xDC, KeyIdentifier::KI_OEM_5},
            {0xDD, KeyIdentifier::KI_OEM_6}, {0xDE, KeyIdentifier::KI_OEM_7}, {0xDF, KeyIdentifier::KI_OEM_8},
            {0xE2, KeyIdentifier::KI_OEM_102}};

        struct KeyRun
        {
            uint32_t first, last; // virtual keys
            KeyIdentifier firstKey;
        };
        constexpr KeyRun kKeyRuns[] = {
            {'0', '9', KeyIdentifier::KI_0},
            {'A', 'Z', KeyIdentifier::KI_A},
            {0x60, 0x69, KeyIdentifier::KI_NUMPAD0},
            {0x70, 0x87, KeyIdentifier::KI_F1},
        };
    }

    ScreenVertex PlacedVertex(float x, float y, uint8_t r, uint8_t g, uint8_t b, uint8_t a, float u, float v,
        float translateX, float translateY, float shareU, float shareV)
    {
        return ScreenVertex{x + translateX - kPixelCenter, y + translateY - kPixelCenter, 0.0f, 1.0f, Argb(a, r, g, b),
            u * shareU, v * shareV};
    }

    uint32_t LayerTint(float opacity)
    {
        constexpr float kMax = 255.0f;
        const auto level     = static_cast<uint32_t>(std::lround(std::clamp(opacity, 0.0f, 1.0f) * kMax));
        return Argb(static_cast<uint8_t>(level), static_cast<uint8_t>(level), static_cast<uint8_t>(level), static_cast<uint8_t>(level));
    }

    size_t DrawnDepth(const std::vector<bool>& made, size_t depth)
    {
        while (depth > 0 && !made[depth - 1])
            --depth;
        return depth;
    }

    bool ReleaseIdleLayers(int& idleRenders, bool pushed)
    {
        idleRenders = pushed ? 0 : idleRenders + 1;
        return idleRenders == kIdleRendersBeforeRelease;
    }

    std::array<ScreenVertex, 6> LayerQuad(int width, int height, float shareU, float shareV, uint32_t tint)
    {
        const auto corner = [&](float x, float y) {
            ScreenVertex v = PlacedVertex(x, y, 0, 0, 0, 0, x == 0.0f ? 0.0f : 1.0f, y == 0.0f ? 0.0f : 1.0f, 0.0f, 0.0f, shareU, shareV);
            v.color        = tint;
            return v;
        };
        const float w = static_cast<float>(width), h = static_cast<float>(height);
        const ScreenVertex topLeft = corner(0.0f, 0.0f), topRight = corner(w, 0.0f), bottomLeft = corner(0.0f, h),
                           bottomRight = corner(w, h);
        return {topLeft, topRight, bottomRight, topLeft, bottomRight, bottomLeft};
    }

    void Transformed(const float* m, float& x, float& y)
    {
        constexpr int kColumn = 4; // floats in a column
        const float outX = m[0] * x + m[kColumn] * y + m[3 * kColumn];
        const float outY = m[1] * x + m[kColumn + 1] * y + m[3 * kColumn + 1];
        const float outW = m[3] * x + m[kColumn + 3] * y + m[3 * kColumn + 3];
        const float over = outW != 0.0f ? 1.0f / outW : 1.0f;
        x = outX * over;
        y = outY * over;
    }

    void ArgbFromRgba(const uint8_t* rgba, size_t pixels, uint32_t* argb)
    {
        for (size_t i = 0; i < pixels; ++i, rgba += 4)
            argb[i] = Argb(rgba[3], rgba[0], rgba[1], rgba[2]);
    }

    std::optional<PixelRect> ClampedRect(int left, int top, int right, int bottom, int width, int height)
    {
        const int x0 = std::max(left, 0), y0 = std::max(top, 0);
        const int x1 = std::min(right, width), y1 = std::min(bottom, height);
        if (x1 <= x0 || y1 <= y0) return std::nullopt;
        return PixelRect{x0, y0, x1 - x0, y1 - y0};
    }

    PixelEdges MaskEdges(const ScreenVertex* vertices, size_t count, float translateX, float translateY,
        const float* transform)
    {
        if (count == 0) return {};
        float left = std::numeric_limits<float>::max(), top = left, right = std::numeric_limits<float>::lowest(), bottom = right;
        for (size_t i = 0; i < count; ++i)
        {
            float x = vertices[i].x + kPixelCenter + translateX, y = vertices[i].y + kPixelCenter + translateY;
            if (transform != nullptr) Transformed(transform, x, y);
            left   = std::min(left, x);
            top    = std::min(top, y);
            right  = std::max(right, x);
            bottom = std::max(bottom, y);
        }
        return PixelEdges{static_cast<int>(std::lround(left)), static_cast<int>(std::lround(top)),
            static_cast<int>(std::lround(right)), static_cast<int>(std::lround(bottom))};
    }

    PixelEdges Shared(const PixelEdges& a, const PixelEdges& b)
    {
        return PixelEdges{std::max(a.left, b.left), std::max(a.top, b.top), std::min(a.right, b.right),
            std::min(a.bottom, b.bottom)};
    }

    void Moved(ScreenVertex& v, float translateX, float translateY, const float* transform)
    {
        v.x += translateX;
        v.y += translateY;
        if (transform == nullptr) return;
        v.x += kPixelCenter;
        v.y += kPixelCenter;
        Transformed(transform, v.x, v.y);
        v.x -= kPixelCenter;
        v.y -= kPixelCenter;
    }

    namespace
    {
        constexpr size_t kBoxCorners = 4;

        // Which side of the line from a to b p is on: above 0 to its left, as an outline goes round.
        float Side(const ScreenVertex& a, const ScreenVertex& b, const ScreenVertex& p)
        {
            return (b.x - a.x) * (p.y - a.y) - (b.y - a.y) * (p.x - a.x);
        }

        uint32_t MixedColor(uint32_t from, uint32_t to, float t)
        {
            uint32_t color = 0;
            for (int shift = 0; shift < 32; shift += 8)
            {
                const float a = static_cast<float>((from >> shift) & 0xFF), b = static_cast<float>((to >> shift) & 0xFF);
                color |= static_cast<uint32_t>(std::lround(a + (b - a) * t)) << shift;
            }
            return color;
        }

        ScreenVertex Between(const ScreenVertex& p, const ScreenVertex& q, float t)
        {
            const auto mix = [t](float a, float b) { return a + (b - a) * t; };
            return ScreenVertex{mix(p.x, q.x), mix(p.y, q.y), mix(p.z, q.z), mix(p.rhw, q.rhw), MixedColor(p.color, q.color, t),
                mix(p.u, q.u), mix(p.v, q.v)};
        }

        bool Inside(const std::vector<ScreenVertex>& shape, const ScreenVertex& p)
        {
            for (size_t i = 0; i < shape.size(); ++i)
                if (Side(shape[i], shape[(i + 1) % shape.size()], p) < 0.0f) return false;
            return true;
        }

        // A convex polygon cut to the outline, one of its edges at a time (Sutherland and Hodgman's).
        std::vector<ScreenVertex> Cut(std::vector<ScreenVertex> polygon, const std::vector<ScreenVertex>& shape)
        {
            std::vector<ScreenVertex> kept;
            for (size_t e = 0; e < shape.size() && !polygon.empty(); ++e)
            {
                const ScreenVertex &a = shape[e], &b = shape[(e + 1) % shape.size()];
                kept.clear();
                for (size_t i = 0; i < polygon.size(); ++i)
                {
                    const ScreenVertex &p = polygon[i], &q = polygon[(i + 1) % polygon.size()];
                    const float sp = Side(a, b, p), sq = Side(a, b, q);
                    if (sp >= 0.0f) kept.push_back(p);
                    if ((sp >= 0.0f) != (sq >= 0.0f)) kept.push_back(Between(p, q, sp / (sp - sq)));
                }
                polygon.swap(kept);
            }
            return polygon;
        }
    }

    std::vector<ScreenVertex> MaskShape(const ScreenVertex* vertices, size_t count, float translateX, float translateY,
        const float* transform)
    {
        if (count <= kBoxCorners) return {};
        std::vector<ScreenVertex> points(vertices, vertices + count);
        for (ScreenVertex& v : points)
            Moved(v, translateX, translateY, transform);
        std::ranges::sort(points, [](const ScreenVertex& a, const ScreenVertex& b) { return a.x != b.x ? a.x < b.x : a.y < b.y; });
        // Andrew's monotone chain: the lower outline left to right, then the upper back, each turning left only.
        std::vector<ScreenVertex> hull;
        for (int pass = 0; pass < 2; ++pass)
        {
            const size_t start = hull.size();
            for (const ScreenVertex& p : points)
            {
                while (hull.size() >= start + 2 && Side(hull[hull.size() - 2], hull.back(), p) <= 0.0f)
                    hull.pop_back();
                hull.push_back(p);
            }
            hull.pop_back(); // the next pass starts from it
            std::ranges::reverse(points);
        }
        if (hull.size() <= kBoxCorners) hull.clear();
        return hull;
    }

    std::vector<ScreenVertex> SharedShape(const std::vector<ScreenVertex>& a, const std::vector<ScreenVertex>& b)
    {
        return Cut(a, b);
    }

    void ClipToShape(std::vector<ScreenVertex>& triangles, const std::vector<ScreenVertex>& shape)
    {
        std::vector<ScreenVertex> out;
        out.reserve(triangles.size());
        for (size_t i = 0; i + 2 < triangles.size(); i += 3)
        {
            const ScreenVertex* t = &triangles[i];
            if (Inside(shape, t[0]) && Inside(shape, t[1]) && Inside(shape, t[2]))
            {
                out.insert(out.end(), t, t + 3);
                continue;
            }
            const std::vector<ScreenVertex> piece = Cut({t[0], t[1], t[2]}, shape);
            for (size_t k = 1; k + 1 < piece.size(); ++k)
                out.insert(out.end(), {piece[0], piece[k], piece[k + 1]});
        }
        triangles.swap(out);
    }

    KeyIdentifier KeyOf(uint32_t virtualKey)
    {
        for (const KeyRun& run : kKeyRuns)
            if (virtualKey >= run.first && virtualKey <= run.last)
                return static_cast<KeyIdentifier>(static_cast<int>(run.firstKey) + static_cast<int>(virtualKey - run.first));
        for (const KeyName& name : kKeyNames)
            if (name.virtualKey == virtualKey) return name.key;
        return KeyIdentifier::KI_UNKNOWN;
    }

    int ModifiersOf(bool ctrl, bool shift, bool alt, bool capsLock, bool numLock)
    {
        return (ctrl ? Rml::Input::KM_CTRL : 0) | (shift ? Rml::Input::KM_SHIFT : 0) | (alt ? Rml::Input::KM_ALT : 0) |
               (capsLock ? Rml::Input::KM_CAPSLOCK : 0) | (numLock ? Rml::Input::KM_NUMLOCK : 0);
    }

    float WheelNotches(int delta) { return -static_cast<float>(delta) / static_cast<float>(kWheelDelta); }

    size_t ColumnSlotAt(std::span<const float> middles, float y)
    {
        return static_cast<size_t>(std::ranges::upper_bound(middles, y) - middles.begin());
    }

    size_t ByteUnder(std::string_view text, float x, FunctionRef<float(std::string_view)> widthOf)
    {
        size_t nearest = 0;
        float off      = std::abs(x);
        for (size_t at = 1; at <= text.size(); ++at)
        {
            if (at < text.size() && ContinuesCharacter(text[at])) continue;
            if (const float here = std::abs(x - widthOf(text.substr(0, at))); here < off)
            {
                nearest = at;
                off     = here;
            }
        }
        return nearest;
    }
}
