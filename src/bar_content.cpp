#include "bar_content.h"

#include <algorithm>
#include <cmath>
#include <cstdio>

namespace headsup
{
    namespace
    {
        constexpr int kHalfDay = 12;
        constexpr std::string_view kDigits = "0123456789";
    }

    Span Text(std::string text, Tone tone) { return Span{std::move(text), tone, 0}; }

    void AddPart(std::vector<Span>& line, std::vector<Span> part)
    {
        if (part.empty()) return;
        constexpr const char* kPartGap = "  ";
        if (!line.empty())
        {
            if (!HasMark(part.front()))
                part.front().text = kPartGap + part.front().text;
            else if (!line.back().room.empty())
                line.back().text += kPartGap; // a gap of its own between two elements would not be drawn
            else
                line.push_back(Text(kPartGap));
        }
        for (Span& span : part)
            line.push_back(std::move(span));
    }

    Span Colored(std::string text, uint32_t argb) { return Span{std::move(text), Tone::Own, argb}; }

    PopupCell Cell(std::vector<Span> text, std::optional<Icon> icon) { return PopupCell{icon, false, std::move(text)}; }

    PopupCell Cell(Span text, std::optional<Icon> icon) { return Cell(ListOf(std::move(text)), icon); }

    PopupSection Section(CardKind card, std::string heading, std::vector<PopupRow> rows)
    {
        PopupSection section{std::move(heading), std::move(rows)};
        section.card = card;
        return section;
    }

    PopupRow Line(PopupCell cell) { return PopupRow{RowKind::Line, 0, ListOf(std::move(cell))}; }

    PopupRow LineOf(std::string text, Tone tone) { return Line(Cell(Text(std::move(text), tone))); }

    PopupRow Labeled(std::string label, Span value)
    {
        return Labeled(Text(std::move(label), Tone::Dim), std::move(value));
    }

    PopupRow Labeled(Span label, Span value)
    {
        label.text += " ";
        return PopupRow{RowKind::Pair, 0, ListOf(Cell(std::move(label)), Cell(std::move(value)))};
    }

    PopupRow Grid(int columns, size_t cells)
    {
        PopupRow row{RowKind::Grid, columns, {}};
        row.cells.reserve(cells);
        return row;
    }

    std::string SteadyFigures(std::string_view text, char digit)
    {
        std::string steady(text);
        for (char& c : steady)
            if (c >= '0' && c <= '9') c = digit;
        return steady;
    }

    char WidestDigit(FunctionRef<float(char)> measure)
    {
        char widest = '0';
        float most  = measure(widest);
        for (char digit = '1'; digit <= '9'; ++digit)
            if (const float width = measure(digit); width > most)
            {
                widest = digit;
                most   = width;
            }
        return widest;
    }

    std::vector<std::string_view> FigurePieces(std::string_view text)
    {
        std::vector<std::string_view> pieces;
        for (size_t at = 0; at < text.size();)
        {
            const size_t end = IsDigit(text[at]) ? at + 1 : std::min(text.find_first_of(kDigits, at), text.size());
            pieces.push_back(text.substr(at, end - at));
            at = end;
        }
        return pieces;
    }

    void SteadyBoxes(std::vector<Span>& line, float digit, FunctionRef<float(const std::string&)> width)
    {
        const auto drawn = [&](std::string_view text) {
            float total = 0.0f;
            for (const std::string_view piece : FigurePieces(text))
                total += IsDigit(piece.front()) ? digit : width(std::string(piece));
            return total;
        };
        for (Span& s : line)
        {
            if (s.room.empty()) continue;
            const size_t first = std::min(s.text.find_first_not_of(' '), s.text.size());
            const size_t last  = std::max(first, s.text.find_last_not_of(' ') + 1);
            const std::string lead = s.text.substr(0, first), trail = s.text.substr(last);
            s.digit = digit;
            s.trail = trail.empty() ? 0.0f : width(trail);
            s.box   = drawn(s.text);
            for (const std::string& widest : s.room)
                s.box = std::max(s.box, drawn(lead + widest + trail));
        }
    }

    std::string SpanText(double seconds)
    {
        const auto total = static_cast<long long>(std::ceil(std::max(0.0, seconds)));
        const long long days = total / kSecondsPerDay, hours = total % kSecondsPerDay / kSecondsPerHour;
        const long long minutes = total % kSecondsPerHour / kSecondsPerMinute, rest = total % kSecondsPerMinute;
        char text[32];
        if (days > 0)
            std::snprintf(text, sizeof(text), "%lldd %lldh", days, hours);
        else if (hours > 0)
            std::snprintf(text, sizeof(text), "%lldh %02lldm", hours, minutes);
        else if (minutes > 0)
            std::snprintf(text, sizeof(text), "%lldm %02llds", minutes, rest);
        else
            std::snprintf(text, sizeof(text), "%llds", rest);
        return text;
    }

    std::string ClockText(int hour, int minute, bool twentyFour, int second)
    {
        char seconds[16] = "";
        if (second >= 0) std::snprintf(seconds, sizeof(seconds), ":%02d", second);
        if (twentyFour) return HourMinute(hour, minute) + seconds;
        char text[40];
        std::snprintf(text, sizeof(text), "%d:%02d%s %s", hour % kHalfDay == 0 ? kHalfDay : hour % kHalfDay, minute,
            seconds, hour < kHalfDay ? "AM" : "PM");
        return text;
    }

    std::string HourMinute(int hour, int minute)
    {
        char text[8];
        std::snprintf(text, sizeof(text), "%02d:%02d", hour, minute);
        return text;
    }

    std::vector<Span> ClockSpans(int hour, int minute, bool twentyFour, int second)
    {
        const auto split = [&](int h) {
            std::string text = ClockText(h, minute, twentyFour, second);
            const size_t at  = std::min(text.find(' '), text.size());
            return std::pair{text.substr(0, at), text.substr(at)};
        };
        auto [time, meridiem] = split(hour);
        // Midnight reads with as many figures of hours as any time: 00 or 12.
        Span clock = Text(std::move(time));
        clock.room = {split(0).first};
        std::vector<Span> spans{std::move(clock)};
        if (meridiem.empty()) return spans;
        Span half = Text(std::move(meridiem));
        half.room = {split(0).second, split(kHalfDay).second};
        spans.push_back(std::move(half));
        return spans;
    }

    Span HourMinuteSpan(int hour, int minute)
    {
        Span span = Text(HourMinute(hour, minute));
        span.room = {HourMinute(0, 0)};
        return span;
    }
}
