#include "case_pictures.h"
#include "test.h"

#include <cstdlib>

using namespace headsup;

namespace
{
    int AlphaAt(const Image& image, int x, int y) { return static_cast<int>(image.argb[static_cast<size_t>(y * image.width + x)] >> 24); }
    uint32_t RgbAt(const Image& image, int x, int y) { return image.argb[static_cast<size_t>(y * image.width + x)] & 0xFFFFFF; }
}

TEST(a_glow_is_bright_in_the_middle_and_gone_at_the_edge)
{
    const Image glow = CaseImage(CasePicture::Glow);
    CHECK(glow.width == 128 && glow.height == 128);
    CHECK(AlphaAt(glow, 64, 64) > 250);                 // a pixel off the middle: nearly full
    CHECK_EQ(AlphaAt(glow, 0, 64), 0);                  // the edge stays clear, so a stretched glow shows no square
    // Half way out: between the stops at 0.45 (0.22) and 1 (0): 0.22 - 0.22 * (0.5 - 0.45) / 0.55 = 0.2, of 255.
    CHECK(std::abs(AlphaAt(glow, 64 + 31, 64) - 51) <= 1);
}

TEST(a_tile_is_opaque_and_lighter_at_its_top_than_its_foot)
{
    const Image tile = CaseImage(CasePicture::Tile);
    CHECK_EQ(RgbAt(tile, 0, 0), 0xE4E7ECu);
    CHECK_EQ(RgbAt(tile, 0, tile.height - 1), 0x8E949Du);
    CHECK(AlphaAt(tile, 0, 0) == 255 && AlphaAt(tile, 0, tile.height - 1) == 255);
}

TEST(a_rise_is_clear_at_its_top_and_solid_white_at_its_foot)
{
    const Image rise = CaseImage(CasePicture::Rise);
    CHECK(AlphaAt(rise, 0, 0) == 0 && AlphaAt(rise, 0, rise.height - 1) == 255 && RgbAt(rise, 0, 0) == 0xFFFFFFu);
}

TEST(a_band_fades_out_to_both_ends)
{
    const Image band = CaseImage(CasePicture::Band);
    CHECK(AlphaAt(band, 0, 0) == 0 && AlphaAt(band, band.width - 1, 0) == 0);
    const int full = static_cast<int>(256.0f * 170.0f / 1040.0f) + 1; // the strip's own fade, 170 of its 1040 pixels
    CHECK(AlphaAt(band, full, 0) == 255 && AlphaAt(band, full - 2, 0) < 255);
}

TEST(a_vignette_is_clear_in_the_middle_and_dark_in_its_corners)
{
    const Image vignette = CaseImage(CasePicture::Vignette);
    CHECK_EQ(AlphaAt(vignette, vignette.width / 2, vignette.height / 2), 0);
    // The corner pixel is 146.2 from the middle: (146.2 - 0.3 * 144) / (0.75 * 256 - 0.3 * 144) of the way to dark.
    CHECK(std::abs(AlphaAt(vignette, 0, 0) - 176) <= 1);
}

TEST(a_text_is_cut_after_its_last_ink_and_keeps_its_line)
{
    Coverage text{60, 36, std::vector<uint8_t>(60 * 36, 0)};
    for (int y = 12; y <= 20; ++y)
        for (int x = 8; x <= 21; ++x) text.alpha[static_cast<size_t>(y * 60 + x)] = static_cast<uint8_t>(100 + x);
    const Image cut = CaseTextImage(text, 8);
    CHECK(cut.width == 2 + 14 + 2 && cut.height == 20 + 4);
    CHECK(cut.argb[static_cast<size_t>((12 - 6) * cut.width + (8 - 6))] == (uint32_t{108} << 24 | 0xFFFFFFu));
    CHECK_EQ(AlphaAt(cut, 0, 0), 0);
    for (int y = 12; y <= 20; ++y) text.alpha[static_cast<size_t>(y * 60 + 54)] = 200; // 3 pixels into the right margin
    CHECK_EQ(CaseTextImage(text, 8).width, 2 + 47 + 2);
    CHECK(CaseTextImage(Coverage{60, 36, std::vector<uint8_t>(60 * 36, 0)}, 8).argb.empty());
}

TEST(a_text_is_drawn_near_the_size_it_shows)
{
    CHECK_EQ(CaseFontPixels(100.0f), 80); // the reveal's name on a 1440-high screen, as it was drawn
    CHECK_EQ(CaseFontPixels(60.0f), 48);  // the title
    CHECK_EQ(CaseFontPixels(0.2f), 1);
}
