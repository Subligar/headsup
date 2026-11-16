#include "markup.h"
#include "test.h"

#include <string>

using namespace headsup;

TEST(decimals_are_written_and_read_with_a_point_whatever_the_c_locale)
{
    CHECK_EQ(Decimal(2.5, 1), "2.5");
    CHECK_EQ(Decimal(33.333, 1), "33.3");
    CHECK_EQ(Decimal(0.125, 0), "0");
    CHECK_EQ(Decimal(1.0 / 3.0, 4), "0.3333");
    CHECK(NumberIn("2.5") == 2.5f && NumberIn("0.5000") == 0.5f && NumberIn("-1") == -1.0f);
    // As strtof read them: spaces and a plus before it, and whatever follows it.
    CHECK(NumberIn("  +3") == 3.0f && NumberIn("12.5 px") == 12.5f);
    CHECK(!NumberIn("") && !NumberIn("px") && !NumberIn("."));
    CHECK_EQ(RcssNumber(0.5f), "0.5");
    CHECK_EQ(Em(1.25f), "1.25em");
}
