#include "bar_content.h"
#include "test.h"

#include <string>
#include <vector>

using namespace headsup;

TEST(a_labeled_row_is_a_pair_of_its_label_dim_and_its_value)
{
    const PopupRow row = Labeled("Latency", Text("48 ms"));
    CHECK(row.kind == RowKind::Pair && row.cells.size() == 2);
    CHECK(row.cells[0].text == (std::vector<Span>{Span{"Latency ", Tone::Dim, 0}}));
    CHECK(row.cells[1].text == (std::vector<Span>{Span{"48 ms", Tone::Normal, 0}}));
}

TEST(a_span_of_time_reads_as_its_two_largest_units_rounded_up)
{
    CHECK_EQ(SpanText(2 * 86400.0 + 8.5 * 3600.0), std::string("2d 8h"));
    CHECK_EQ(SpanText(8.5 * 3600.0), std::string("8h 30m"));
    CHECK_EQ(SpanText(3600.0), std::string("1h 00m"));
    CHECK_EQ(SpanText(309.0), std::string("5m 09s"));
    CHECK_EQ(SpanText(8.2), std::string("9s"));
    CHECK_EQ(SpanText(-1.0), std::string("0s"));
    CHECK_EQ(HourMinute(9, 5), std::string("09:05"));
}

TEST(printed_text_is_one_value_in_a_printf_format)
{
    CHECK_EQ(Printed("%.1f ms", 16.66), std::string("16.7 ms"));
    CHECK_EQ(Printed("%02d", 7), std::string("07"));
    CHECK_EQ(Printed("%03X", 0x2Au), std::string("02A"));
}

TEST(a_lines_figures_are_measured_as_its_fonts_widest_digit_so_a_ticking_clock_keeps_one_width)
{
    CHECK_EQ(SteadyFigures("Lightsday 14:43", '8'), std::string("Lightsday 88:88"));
    CHECK_EQ(SteadyFigures("S 184 R 85", '0'), std::string("S 000 R 00")); // as many figures as it has
    CHECK_EQ(SteadyFigures("Waning Crescent 38%", '4'), std::string("Waning Crescent 44%"));
    CHECK_EQ(SteadyFigures("No Target", '0'), std::string("No Target"));
    CHECK_EQ(SteadyFigures("Zulkheim \xC2\xB7 Bastok", '0'), std::string("Zulkheim \xC2\xB7 Bastok")); // bytes past ASCII stay
    // A font's widest digit, by what each measures: the first of them when several are as wide.
    const auto old = [](char digit) { return digit == '1' ? 4.0f : digit == '4' ? 9.5f : digit == '7' ? 6.0f : 8.0f; };
    CHECK_EQ(WidestDigit(old), '4');
    CHECK_EQ(WidestDigit([](char) { return 7.0f; }), '0'); // tabular figures
    CHECK_EQ(WidestDigit([](char digit) { return digit == '9' ? 7.5f : 7.0f; }), '9');
}
