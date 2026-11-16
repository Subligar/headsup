#include "markup.h"

#include "argb.h"

#include <algorithm>
#include <charconv>
#include <cstdio>

namespace headsup
{
    std::string EscapedRml(std::string_view text)
    {
        std::string out;
        out.reserve(text.size());
        for (const char c : text)
            switch (c)
            {
            case '&': out += "&amp;"; break;
            case '<': out += "&lt;"; break;
            case '>': out += "&gt;"; break;
            case '"': out += "&quot;"; break;
            default: out += c;
            }
        return out;
    }

    std::string RcssColor(uint32_t argb)
    {
        char text[16];
        std::snprintf(text, sizeof(text), "#%02x%02x%02x%02x", Channel(argb, kRedShift), Channel(argb, kGreenShift),
            Channel(argb, kBlueShift), Channel(argb, kAlphaShift));
        return text;
    }

    std::string Decimal(double value, int places)
    {
        constexpr size_t kLongest = 512; // the largest double's 309 digits, its sign, the point and any places asked for
        char text[kLongest];
        return std::string(text, std::to_chars(text, text + kLongest, value, std::chars_format::fixed, places).ptr);
    }

    std::optional<float> NumberIn(std::string_view text)
    {
        text.remove_prefix(std::min(text.find_first_not_of(" \t"), text.size()));
        if (text.starts_with('+')) text.remove_prefix(1);
        float value             = 0.0f;
        const auto [end, error] = std::from_chars(text.data(), text.data() + text.size(), value);
        return error == std::errc() ? std::optional<float>(value) : std::nullopt;
    }

    std::string RcssNumber(float value)
    {
        constexpr int kPlaces = 2;
        std::string out = Decimal(value, kPlaces);
        while (out.back() == '0')
            out.pop_back();
        if (out.back() == '.') out.pop_back();
        return out == "-0" ? "0" : out;
    }

    std::string Px(float value) { return RcssNumber(value) + "px"; }
    std::string Em(float value) { return RcssNumber(value) + "em"; }
}
