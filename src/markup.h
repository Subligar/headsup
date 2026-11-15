#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

namespace headsup
{
    // Text as markup takes it: the characters it reads specially become entities.
    std::string EscapedRml(std::string_view text);
    // A color as RCSS reads it: #rrggbbaa.
    std::string RcssColor(uint32_t argb);
    // HeadsUp's own numbers go through these, never printf's %f or strtof: an addon can set the game's C locale to one
    // whose decimals take a comma (LESSONS.md, the CRT locale), and std::to_chars and from_chars follow none.
    // value rounded to places decimals after a point.
    std::string Decimal(double value, int places);
    // The number at text's start, after any spaces and a plus, as strtof read it; nothing for none.
    std::optional<float> NumberIn(std::string_view text);
    // A number as RCSS reads it: two decimals at most, none it does not need.
    std::string RcssNumber(float value);
    std::string Px(float value);
    std::string Em(float value);
}
