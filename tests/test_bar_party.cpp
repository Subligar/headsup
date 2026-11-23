#include "bar_party.h"
#include "test.h"

using namespace headsup;

TEST(only_the_jobs_phoenix_grades_with_mp_have_it)
{
    for (const uint8_t job : {3, 4, 5, 7, 8, 15, 16, 20, 21, 22}) // WHM BLM RDM PLD DRK SMN BLU SCH GEO RUN
        CHECK(JobHasMp(job));
    for (const uint8_t job : {0, 1, 2, 6, 9, 10, 11, 12, 13, 14, 17, 18, 19, 23}) // BRD is 10
        CHECK(!JobHasMp(job));
}

TEST(a_member_uses_mp_from_the_main_job_or_a_leveled_subjob)
{
    PartyMember war;
    war.mainJob = 1, war.subJob = 3, war.subLevel = 37; // WAR/WHM
    CHECK(UsesMp(war));
    war.subLevel = 0; // the subjob not unlocked
    CHECK(!UsesMp(war));
    PartyMember blm;
    blm.mainJob = 4;
    CHECK(UsesMp(blm));
}

// The server sends a member's HP and MP with their percent, floor(100 * now / max) in floats but at least 1 while above
// 0, and never their max.
TEST(a_members_max_is_their_figure_at_full_and_otherwise_kept_while_it_still_fits_the_percent)
{
    CHECK_EQ(PoolMax(1500, 100, 0), 1500u);
    CHECK_EQ(PoolMax(1500, 100, 1400), 1500u); // a max that grew, read at full
    CHECK_EQ(PoolMax(750, 50, 1500), 1500u);
    CHECK_EQ(PoolMax(749, 49, 1500), 1500u);
    CHECK_EQ(PoolMax(5, 1, 1500), 1500u);      // floor(0.33) is 0, raised to 1
    CHECK_EQ(PoolMax(530, 52, 1000), 1000u);   // 530 / 1000 is 0.53 in a float, and that times 100 is 52.999996
    CHECK_EQ(PoolMax(0, 0, 1500), 1500u);      // dead
    // Not seen at full, or the kept max no longer fits: the largest max the percent allows.
    CHECK_EQ(PoolMax(750, 50, 0), 1500u);
    CHECK_EQ(PoolMax(520, 42, 0), 1238u);
    CHECK_EQ(PoolMax(750, 50, 1200), 1500u);   // 750 of 1200 would read 62
    CHECK_EQ(PoolMax(1300, 90, 1200), 1444u);  // more than the kept max
    CHECK_EQ(PoolMax(0, 0, 0), 0u);            // dead and never seen: unknown
}
