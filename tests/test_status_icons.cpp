#include "status_icons.h"
#include "test.h"

#include <algorithm>
#include <iterator>
#include <string_view>

using namespace headsup;

namespace
{
    int PackNamed(std::string_view name)
    {
        return static_cast<int>(std::ranges::find(kStatusPackNames, name) - std::begin(kStatusPackNames));
    }

    uint32_t At(const Image& image, int x, int y) { return image.argb[static_cast<size_t>(y * image.width + x)]; }
}

TEST(a_packs_icon_is_its_png_pixels_and_a_status_it_lacks_or_the_vanilla_pack_has_none)
{
    // Read from the PNGs with another decoder: Signet's middle, its clear corner, and Tetsouou's RGB Sleep II.
    const std::optional<Image> hd = PackIcon(PackNamed("HD"), 253), xiview = PackIcon(PackNamed("XIView"), 253);
    CHECK(hd && hd->width == 32 && hd->height == 32 && xiview);
    CHECK_EQ(At(*hd, 16, 16), 0xFF64A8B6u);
    CHECK_EQ(At(*hd, 0, 0), 0u);
    CHECK_EQ(At(*xiview, 16, 16), 0xFF3D4743u);
    CHECK_EQ(At(*xiview, 5, 9), 0xFF2E3132u);
    const std::optional<Image> rgb = PackIcon(PackNamed("Tetsouou"), 24);
    CHECK(rgb && At(*rgb, 16, 16) == 0xFFF8F840u && At(*rgb, 0, 0) == 0xFF000000u);
    CHECK(!PackIcon(PackNamed("HD"), 650).has_value()); // no file of its own: the game's shows
    CHECK(!PackIcon(kVanillaStatusIcons, 253).has_value() && !PackIcon(kStatusPackCount, 253).has_value());
}

TEST(debuffs_are_xiuis_and_every_other_status_is_a_buff)
{
    CHECK(IsDebuff(2) && IsDebuff(19) && IsDebuff(134) && IsDebuff(805)); // Sleep, Sleep II, Dia, Tomahawk
    CHECK(!IsDebuff(40) && !IsDebuff(253) && !IsDebuff(33));               // Protect, Signet, Haste
    CHECK(!IsDebuff(777));                                                 // a Prowess: not in XIUI's table
    CHECK(IsDebuff(0) && IsDebuff(793) && IsDebuff(802) && IsDebuff(804)); // KO, Ninjutsu Ele Debuff, Ele. Res. Down, Hysteria
}
