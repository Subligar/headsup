#include "argb.h"
#include "game_glyphs.h"
#include "test.h"

using namespace headsup;

TEST(vertex_counts_by_primitive_type)
{
    CHECK_EQ(VertexCount(kTriangleStrip, 2), 4u); // one glyph quad
    CHECK_EQ(VertexCount(kTriangleList, 2), 6u);
    CHECK_EQ(VertexCount(kTriangleFan, 3), 5u);
    CHECK_EQ(VertexCount(kLineList, 3), 6u);
    CHECK_EQ(VertexCount(kLineStrip, 3), 4u);
    CHECK_EQ(VertexCount(kPointList, 7), 7u);
    CHECK_EQ(VertexCount(0, 2), 0u);
}

TEST(a_letters_shown_color_applies_the_modulate_scale_and_is_opaque)
{
    CHECK_EQ(ShownColor(0x80604020u, 2), 0xFFC08040u);
    CHECK_EQ(ShownColor(0x80808040u, 2), 0xFFFFFF80u); // 0x80 is full intensity
    CHECK_EQ(ShownColor(0x80402010u, 4), 0xFFFF8040u);
    CHECK_EQ(ShownColor(0x80FFFF80u, 1), 0xFFFFFF80u);
}
