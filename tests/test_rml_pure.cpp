#include "rml_pure.h"
#include "test.h"

#include <algorithm>
#include <cmath>
#include <utility>
#include <vector>

using namespace headsup;
using Rml::Input::KeyIdentifier;

TEST(the_byte_under_a_point_of_a_text_is_the_nearest_boundary_between_its_characters)
{
    // Every character 7 px wide: boundaries at 0, 7, 14, ...
    const auto wide = [](std::string_view text) {
        return 7.0f * static_cast<float>(std::ranges::count_if(text, [](char c) { return (c & 0xC0) != 0x80; }));
    };
    CHECK_EQ(ByteUnder("abcd", -3.0f, wide), size_t{0});
    CHECK_EQ(ByteUnder("abcd", 3.0f, wide), size_t{0});
    CHECK_EQ(ByteUnder("abcd", 10.0f, wide), size_t{1});
    CHECK_EQ(ByteUnder("abcd", 11.0f, wide), size_t{2});
    CHECK_EQ(ByteUnder("abcd", 1000.0f, wide), size_t{4});
    CHECK_EQ(ByteUnder("", 5.0f, wide), size_t{0});
    // A letter of two bytes is never split.
    const std::string accented = "a\xC3\xA9";
    CHECK_EQ(ByteUnder(accented, 10.0f, wide), size_t{1});
    CHECK_EQ(ByteUnder(accented, 12.0f, wide), size_t{3});
}

TEST(a_drop_in_a_column_of_buttons_goes_above_the_first_whose_middle_is_below_it)
{
    // Buttons 20 px tall with 4 px gaps: middles at 10, 34 and 58.
    const std::vector<float> middles{10.0f, 34.0f, 58.0f};
    CHECK_EQ(ColumnSlotAt(middles, 2.0f), size_t{0});   // the first's top half
    CHECK_EQ(ColumnSlotAt(middles, 15.0f), size_t{1});  // its bottom half: below it
    CHECK_EQ(ColumnSlotAt(middles, 22.0f), size_t{1});  // the gap after it
    CHECK_EQ(ColumnSlotAt(middles, 30.0f), size_t{1});  // the second's top half
    CHECK_EQ(ColumnSlotAt(middles, 40.0f), size_t{2});
    CHECK_EQ(ColumnSlotAt(middles, 65.0f), size_t{3});  // the last's bottom half: the end
    CHECK_EQ(ColumnSlotAt(middles, -5.0f), size_t{0});  // above them all, as on the tray's name
    CHECK_EQ(ColumnSlotAt({}, 5.0f), size_t{0});
}

TEST(a_vertex_is_moved_to_the_pixel_center_with_its_texture_share)
{
    // A 2x4 image padded into an 4x16 texture takes half its width and a quarter of its height.
    const ScreenVertex v = PlacedVertex(10.0f, 20.0f, 255, 128, 0, 255, 1.0f, 1.0f, 5.0f, 5.0f, 0.5f, 0.25f);
    CHECK_EQ(v.x, 14.5f);
    CHECK_EQ(v.y, 24.5f);
    CHECK_EQ(v.z, 0.0f);
    CHECK_EQ(v.rhw, 1.0f);
    CHECK_EQ(v.color, 0xFFFF8000u);
    CHECK_EQ(v.u, 0.5f);
    CHECK_EQ(v.v, 0.25f);
}

TEST(a_layer_is_laid_over_the_target_at_its_opacity_in_premultiplied_color)
{
    // Every channel of a premultiplied picture scales with the opacity, its alpha too.
    CHECK_EQ(LayerTint(1.0f), 0xFFFFFFFFu);
    CHECK_EQ(LayerTint(0.5f), 0x80808080u);
    CHECK_EQ(LayerTint(0.0f), 0u);
    CHECK_EQ(LayerTint(2.0f), 0xFFFFFFFFu);
    // Two triangles over the whole target, their texture coordinates the target's share of the padded layer, each
    // pixel's center on its texel's: a vertex at a pixel's x reads the layer at that x, never mirrored.
    constexpr float kWidth = 2560.0f, kHeight = 1440.0f, kShareU = 0.625f, kShareV = 0.703125f;
    const auto quad = LayerQuad(2560, 1440, kShareU, kShareV, 0x80808080u);
    float area = 0.0f;
    for (size_t t = 0; t < quad.size(); t += 3)
    {
        const ScreenVertex &a = quad[t], &b = quad[t + 1], &c = quad[t + 2];
        area += std::abs((b.x - a.x) * (c.y - a.y) - (c.x - a.x) * (b.y - a.y)) / 2.0f;
    }
    CHECK_EQ(area, kWidth * kHeight);
    for (const float x : {-kPixelCenter, kWidth - kPixelCenter}) // both halves, not one twice: all four corners
        for (const float y : {-kPixelCenter, kHeight - kPixelCenter})
            CHECK(std::ranges::any_of(quad, [&](const ScreenVertex& v) { return v.x == x && v.y == y; }));
    for (const ScreenVertex& v : quad)
    {
        CHECK(v.x == -kPixelCenter || v.x == kWidth - kPixelCenter);
        CHECK(v.y == -kPixelCenter || v.y == kHeight - kPixelCenter);
        CHECK_EQ(v.u, (v.x + kPixelCenter) / kWidth * kShareU);
        CHECK_EQ(v.v, (v.y + kPixelCenter) / kHeight * kShareV);
        CHECK_EQ(v.color, 0x80808080u);
    }
}

TEST(drawing_goes_into_the_highest_layer_direct3d_made)
{
    // A refused layer's drawing goes on into the one under it, down to the game's own target at 0.
    const std::vector<bool> stack = {true, false, true, false};
    CHECK_EQ(DrawnDepth(stack, 4), size_t{3});
    CHECK_EQ(DrawnDepth(stack, 3), size_t{3});
    CHECK_EQ(DrawnDepth(stack, 2), size_t{1});
    CHECK_EQ(DrawnDepth(stack, 0), size_t{0});
    const std::vector<bool> refused = {false, false};
    CHECK_EQ(DrawnDepth(refused, 2), size_t{0});
}

TEST(layers_are_let_go_once_none_was_drawn_into_for_a_while)
{
    int idle = 0;
    for (int render = 1; render < kIdleRendersBeforeRelease; ++render)
        CHECK(!ReleaseIdleLayers(idle, false));
    CHECK(ReleaseIdleLayers(idle, false));
    CHECK(!ReleaseIdleLayers(idle, false)); // once, not every render after
    CHECK(!ReleaseIdleLayers(idle, true));
    for (int render = 1; render < kIdleRendersBeforeRelease; ++render)
        CHECK(!ReleaseIdleLayers(idle, false)); // a layer drawn into starts the count again
    CHECK(ReleaseIdleLayers(idle, false));
}

TEST(rgba_bytes_become_argb_words)
{
    const uint8_t rgba[8] = {0x11, 0x22, 0x33, 0x44, 0xFF, 0x00, 0x80, 0x80};
    uint32_t argb[2]      = {};
    ArgbFromRgba(rgba, 2, argb);
    CHECK_EQ(argb[0], 0x44112233u);
    CHECK_EQ(argb[1], 0x80FF0080u);
}

TEST(premultiplying_scales_the_channels_by_the_alpha)
{
    // Half alpha: 255 to 128, 128 to 64 (64.25 rounded), 0 stays.
    CHECK_EQ(Premultiplied(0x80FF8000u), 0x80804000u);
    CHECK_EQ(Premultiplied(0xFF123456u), 0xFF123456u);
    CHECK_EQ(Premultiplied(0x00FFFFFFu), 0x00000000u);
}

TEST(a_scissor_rectangle_is_kept_inside_the_target)
{
    CHECK((ClampedRect(-10, -10, 100, 50, 80, 40) == PixelRect{0, 0, 80, 40}));
    CHECK((ClampedRect(10, 10, 20, 20, 80, 40) == PixelRect{10, 10, 10, 10}));
    CHECK(!ClampedRect(100, 0, 120, 10, 80, 40));
    CHECK(!ClampedRect(10, 10, 10, 20, 80, 40));
}

TEST(virtual_keys_get_rmlui_names)
{
    CHECK_EQ(KeyOf('A'), KeyIdentifier::KI_A);
    CHECK_EQ(KeyOf('Z'), KeyIdentifier::KI_Z);
    CHECK_EQ(KeyOf('7'), KeyIdentifier::KI_7);
    CHECK_EQ(KeyOf(0x0D), KeyIdentifier::KI_RETURN);
    CHECK_EQ(KeyOf(0x74), KeyIdentifier::KI_F5);
    CHECK_EQ(KeyOf(0x87), KeyIdentifier::KI_F24);
    CHECK_EQ(KeyOf(0x25), KeyIdentifier::KI_LEFT);
    CHECK_EQ(KeyOf(0x63), KeyIdentifier::KI_NUMPAD3);
    CHECK_EQ(KeyOf(0xBA), KeyIdentifier::KI_OEM_1);
    CHECK_EQ(KeyOf(0x10), KeyIdentifier::KI_LSHIFT);
    CHECK_EQ(KeyOf(0xA1), KeyIdentifier::KI_RSHIFT);
    CHECK_EQ(KeyOf(0x2E), KeyIdentifier::KI_DELETE);
    CHECK_EQ(KeyOf(0x07), KeyIdentifier::KI_UNKNOWN);
    CHECK_EQ(KeyOf(0x100), KeyIdentifier::KI_UNKNOWN);
}

TEST(modifiers_are_rmluis_bits)
{
    CHECK_EQ(ModifiersOf(true, false, true, false, false), Rml::Input::KM_CTRL | Rml::Input::KM_ALT);
    CHECK_EQ(ModifiersOf(false, true, false, true, true),
        Rml::Input::KM_SHIFT | Rml::Input::KM_CAPSLOCK | Rml::Input::KM_NUMLOCK);
    CHECK_EQ(ModifiersOf(false, false, false, false, false), 0);
}

TEST(wheel_deltas_become_notches_toward_the_user)
{
    CHECK_EQ(WheelNotches(120), -1.0f);
    CHECK_EQ(WheelNotches(-240), 2.0f);
    CHECK_EQ(WheelNotches(60), -0.5f);
}

TEST(a_transform_moves_a_point_and_divides_by_its_depth)
{
    // Column by column: twice as wide, three times as tall, moved 10 right and 20 down.
    const float scaleAndMove[16] = {2, 0, 0, 0, 0, 3, 0, 0, 0, 0, 1, 0, 10, 20, 0, 1};
    float x = 5.0f, y = 7.0f;
    Transformed(scaleAndMove, x, y);
    CHECK_EQ(x, 20.0f);
    CHECK_EQ(y, 41.0f);
    // A quarter turn: (1, 0) goes to (0, 1).
    const float turn[16] = {0, 1, 0, 0, -1, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1};
    x = 1.0f, y = 0.0f;
    Transformed(turn, x, y);
    CHECK_EQ(x, 0.0f);
    CHECK_EQ(y, 1.0f);
    // A perspective that doubles the depth at x = 4 halves the point.
    const float deep[16] = {1, 0, 0, 0.25f, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1};
    x = 4.0f, y = 8.0f;
    Transformed(deep, x, y);
    CHECK_EQ(x, 2.0f);
    CHECK_EQ(y, 4.0f);
}

namespace
{
    // The corners of a box as the renderer keeps them, at the pixel centers Direct3D samples.
    std::vector<ScreenVertex> Corners(float left, float top, float right, float bottom)
    {
        std::vector<ScreenVertex> corners;
        for (const float x : {left, right})
            for (const float y : {top, bottom})
                corners.push_back(ScreenVertex{x - kPixelCenter, y - kPixelCenter, 0.0f, 1.0f, 0, 0.0f, 0.0f});
        return corners;
    }
}

TEST(a_clip_masks_box_goes_where_its_translation_and_transform_put_it)
{
    const std::vector<ScreenVertex> box = Corners(100.0f, 50.0f, 300.0f, 150.0f);
    CHECK((MaskEdges(box.data(), box.size(), 10.0f, 20.0f, nullptr) == PixelEdges{110, 70, 310, 170}));
    // A list sliding in from 6 pixels up clips 6 pixels up.
    const float slide[16] = {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, -6, 0, 1};
    CHECK((MaskEdges(box.data(), box.size(), 10.0f, 20.0f, slide) == PixelEdges{110, 64, 310, 164}));
    // Half the size about the origin, and an edge between two pixels goes to the nearer.
    const float half[16] = {0.5f, 0, 0, 0, 0, 0.5f, 0, 0, 0, 0, 1, 0, 0, 0.4f, 0, 1};
    CHECK((MaskEdges(box.data(), box.size(), 10.0f, 20.0f, half) == PixelEdges{55, 35, 155, 85}));
    // Twice the size: the corners are doubled, not the pixel centers the vertices are kept at.
    const float twice[16] = {2, 0, 0, 0, 0, 2, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1};
    CHECK((MaskEdges(box.data(), box.size(), 10.0f, 20.0f, twice) == PixelEdges{220, 140, 620, 340}));
    // A quarter turn: the box around the turned corners, x from -170 to -70 and y from 110 to 310.
    const float turn[16] = {0, 1, 0, 0, -1, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1};
    CHECK((MaskEdges(box.data(), box.size(), 10.0f, 20.0f, turn) == PixelEdges{-170, 110, -70, 310}));
}

TEST(two_clips_leave_what_both_cover)
{
    CHECK((Shared(PixelEdges{0, 0, 100, 100}, PixelEdges{50, 20, 200, 80}) == PixelEdges{50, 20, 100, 80}));
    // Apart, they leave no pixel for the scissor to keep.
    const PixelEdges none = Shared(PixelEdges{0, 0, 10, 10}, PixelEdges{20, 20, 30, 30});
    CHECK(!ClampedRect(none.left, none.top, none.right, none.bottom, 80, 40));
}

namespace
{
    ScreenVertex At(float x, float y, uint32_t color = 0xFFFFFFFF)
    {
        return ScreenVertex{x, y, 0.0f, 1.0f, color, x / 1000.0f, y / 1000.0f};
    }

    // A box with its corners cut 10 px in, as RmlUi gives a rounded box to clip by: a fan of triangles around its
    // middle, each corner given twice, kept at the pixel centers as the renderer keeps every vertex.
    std::vector<ScreenVertex> CutBox(float left, float top, float right, float bottom)
    {
        const float c = 10.0f;
        const std::vector<std::pair<float, float>> rim = {{left + c, top}, {right - c, top}, {right, top + c},
            {right, bottom - c}, {right - c, bottom}, {left + c, bottom}, {left, bottom - c}, {left, top + c}};
        const ScreenVertex middle = At((left + right) / 2.0f - kPixelCenter, (top + bottom) / 2.0f - kPixelCenter);
        std::vector<ScreenVertex> fan;
        for (size_t i = 0; i < rim.size(); ++i)
        {
            const auto [x0, y0] = rim[i];
            const auto [x1, y1] = rim[(i + 1) % rim.size()];
            fan.insert(fan.end(), {middle, At(x0 - kPixelCenter, y0 - kPixelCenter), At(x1 - kPixelCenter, y1 - kPixelCenter)});
        }
        return fan;
    }

    float Area(const std::vector<ScreenVertex>& triangles)
    {
        float area = 0.0f;
        for (size_t i = 0; i + 2 < triangles.size(); i += 3)
        {
            const ScreenVertex &a = triangles[i], &b = triangles[i + 1], &c = triangles[i + 2];
            area += std::abs((b.x - a.x) * (c.y - a.y) - (b.y - a.y) * (c.x - a.x)) / 2.0f;
        }
        return area;
    }

    std::vector<ScreenVertex> Quad(float left, float top, float right, float bottom)
    {
        return {At(left, top), At(right, top), At(right, bottom), At(left, top), At(right, bottom), At(left, bottom)};
    }
}

TEST(a_rounded_clip_mask_is_the_outline_of_its_box_where_it_is_drawn)
{
    const std::vector<ScreenVertex> mask = CutBox(0.0f, 0.0f, 100.0f, 50.0f);
    const std::vector<ScreenVertex> shape = MaskShape(mask.data(), mask.size(), 10.0f, 20.0f, nullptr);
    // Its eight corners, not its middle nor a corner twice, moved by the translation as every vertex drawn is.
    CHECK_EQ(shape.size(), size_t{8});
    for (const ScreenVertex& v : shape)
    {
        CHECK(v.x >= 10.0f - kPixelCenter - 0.01f && v.x <= 110.0f - kPixelCenter + 0.01f);
        CHECK(v.y >= 20.0f - kPixelCenter - 0.01f && v.y <= 70.0f - kPixelCenter + 0.01f);
    }
    // Clipping a box to it leaves the cut box: 100 by 50 less four corners of 10 by 10 halved.
    std::vector<ScreenVertex> over = Quad(-100.0f, -100.0f, 300.0f, 300.0f);
    ClipToShape(over, shape);
    CHECK(std::abs(Area(over) - 4800.0f) < 0.5f);
    // A quarter turn about the origin turns the outline with it: x from -70 to -20, y from 10 to 110.
    const float turn[16] = {0, 1, 0, 0, -1, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1};
    for (const ScreenVertex& v : MaskShape(mask.data(), mask.size(), 10.0f, 20.0f, turn))
        CHECK(v.x >= -70.0f - kPixelCenter - 0.01f && v.x <= -20.0f - kPixelCenter + 0.01f && v.y >= 10.0f - kPixelCenter - 0.01f);
    // A plain box is the scissor's to clip: it has no outline of its own.
    const std::vector<ScreenVertex> box = Quad(0.0f, 0.0f, 100.0f, 50.0f);
    CHECK(MaskShape(box.data(), box.size(), 0.0f, 0.0f, nullptr).empty());
}

TEST(clipping_to_a_rounded_mask_cuts_only_what_crosses_its_outline_and_carries_each_vertex_along)
{
    const std::vector<ScreenVertex> mask  = CutBox(0.0f, 0.0f, 100.0f, 50.0f);
    const std::vector<ScreenVertex> shape = MaskShape(mask.data(), mask.size(), kPixelCenter, kPixelCenter, nullptr);
    // A box well inside is drawn as it was.
    const std::vector<ScreenVertex> inside = Quad(20.0f, 10.0f, 80.0f, 40.0f);
    std::vector<ScreenVertex> kept = inside;
    ClipToShape(kept, shape);
    CHECK_EQ(kept.size(), inside.size());
    for (size_t i = 0; i < kept.size(); ++i)
        CHECK(kept[i].x == inside[i].x && kept[i].y == inside[i].y && kept[i].u == inside[i].u);
    // The top left 20 by 20 loses its corner's 50 px; every new vertex keeps its texture where it lies.
    std::vector<ScreenVertex> corner = Quad(0.0f, 0.0f, 20.0f, 20.0f);
    ClipToShape(corner, shape);
    CHECK(std::abs(Area(corner) - 350.0f) < 0.5f);
    for (const ScreenVertex& v : corner)
        CHECK(std::abs(v.u - v.x / 1000.0f) < 1e-5f && std::abs(v.v - v.y / 1000.0f) < 1e-5f && v.x + v.y >= 10.0f - 0.01f);
    // Colors halfway along a cut edge are halfway between its ends: an edge from black to white cut at its middle.
    std::vector<ScreenVertex> fading = {At(-10.0f, 25.0f, 0xFF000000), At(10.0f, 25.0f, 0xFFFFFFFF), At(10.0f, 30.0f, 0xFFFFFFFF)};
    ClipToShape(fading, shape);
    CHECK(std::ranges::any_of(fading, [](const ScreenVertex& v) { return std::abs(v.x) < 1e-4f && (v.color & 0xFF) >= 0x7F && (v.color & 0xFF) <= 0x80; }));
    // Outside it altogether, nothing is left.
    std::vector<ScreenVertex> outside = Quad(200.0f, 200.0f, 220.0f, 220.0f);
    ClipToShape(outside, shape);
    CHECK(outside.empty());
}

TEST(two_rounded_masks_clip_to_what_both_cover)
{
    const std::vector<ScreenVertex> a = CutBox(0.0f, 0.0f, 100.0f, 50.0f), b = CutBox(50.0f, 0.0f, 150.0f, 50.0f);
    const std::vector<ScreenVertex> both = SharedShape(MaskShape(a.data(), a.size(), 0.0f, 0.0f, nullptr),
        MaskShape(b.data(), b.size(), 0.0f, 0.0f, nullptr));
    // x from 50 to 100 of the full height, less the two cut corners on each side that fall inside it: 2500 - 4 * 50.
    std::vector<ScreenVertex> over = Quad(-100.0f, -100.0f, 300.0f, 300.0f);
    ClipToShape(over, both);
    CHECK(std::abs(Area(over) - 2300.0f) < 0.5f);
}
