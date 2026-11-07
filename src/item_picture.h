#pragma once

#include "plate_image.h"

#include <cstddef>
#include <cstdint>
#include <optional>

namespace headsup
{
    // An item's or a status icon's picture from the game's own resources (Ashita's IItem::Bitmap, IStatusIcon::Bitmap):
    // an 8-bit bitmap with its palette or a 32-bit one, with or without the file header, its black clear. None for
    // anything else, or one cut short.
    std::optional<Image> ResourcePicture(const uint8_t* bmp, size_t size);
}
