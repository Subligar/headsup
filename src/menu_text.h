#pragma once

#include "bar_placement.h"
#include "bar_types.h"
#include "native_locate.h"
#include "outline_math.h"
#include "plate_settings.h"

#include <algorithm>
#include <cctype>
#include <cstddef>
#include <iterator>
#include <string>
#include <string_view>
#include <utility>

// What the settings window's pages say and the small questions they ask.
namespace headsup
{
    // Whether text holds part, in any case.
    inline bool Matches(std::string_view text, const char* part)
    {
        const std::string_view p(part);
        if (p.empty()) return true;
        const auto lower = [](char c) { return static_cast<char>(std::tolower(static_cast<unsigned char>(c))); };
        for (size_t at = 0; at + p.size() <= text.size(); ++at)
            if (std::equal(p.begin(), p.end(), text.begin() + static_cast<std::ptrdiff_t>(at),
                    [&](char a, char b) { return lower(a) == lower(b); }))
                return true;
        return false;
    }

    // A window as the menu names it: HeadsUp's name for it, or the game's.
    inline std::string WindowName(const std::string& window)
    {
        const char* label = WindowLabel(window);
        return label != nullptr ? label : window;
    }

    // The Replace Dashes list shows the marks themselves, as the window's style draws them (menu_document).
    constexpr const char* kDotMark     = "<div class=\"dash dot\"></div>";
    constexpr const char* kSquareMark  = "<div class=\"dash square\"></div>";
    constexpr const char* kDiamondMark = "<div class=\"dash diamond\"></div>";
}
