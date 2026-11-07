#pragma once

#include "bar_content.h"

#include <string>
#include <string_view>

namespace headsup
{
    // How one icon is drawn differently from the rest, wherever it is drawn: moved by x and y pixels (right and
    // down) and sized by scale around its middle, inside the room Icon Scale and the rest give it.
    struct IconAdjust
    {
        float x = 0.0f, y = 0.0f, scale = 1.0f;
        bool operator==(const IconAdjust&) const = default;
    };
    // The settings cog's name in the table: it is painted in code (Cog), not read from a file.
    constexpr std::string_view kCogName = "cog";
    // An icon's adjustment by its file name (an icon's PNG, or a crest's chunk in 51.DAT); none for one not adjusted.
    IconAdjust AdjustOf(std::string_view file);
    IconAdjust AdjustOf(Icon icon);
    IconAdjust AdjustOf(GamePicture picture);
    // A pack's status icon is named by its pack's folder and its file, as "HD/253.png"; the game's own have none.
    IconAdjust AdjustOf(const StatusMark& mark);
    // An adjustment as an element's style takes it, which moves and sizes what is drawn without moving what is
    // beside it: empty for none.
    std::string AdjustRules(const IconAdjust& adjust);
    // The same as a style attribute, with the space before it: empty for none.
    std::string AdjustStyle(const IconAdjust& adjust);
}
