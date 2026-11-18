#include "friend_notices.h"
#include "test.h"

#include <cmath>
#include <string>
#include <vector>

using namespace headsup;

namespace
{
    FriendEntry E(uint32_t account, const char* name, bool playing) { return FriendEntry{account, name, name, playing}; }

    // Reads after the first come this far after it, past the sign-on's quiet spell.
    constexpr double kAfter = kSignOnQuietSeconds + 1.0;
}

TEST(a_friend_whose_playing_flag_flips_is_news_and_nothing_else_is)
{
    FriendWatch watch;
    CHECK(watch.Read(65001, {E(1001, "Two", true), E(1002, "Three", false)}, 0.0).empty()); // the first read only records
    CHECK(watch.Read(65001, {E(1001, "Two", true), E(1002, "Three", false)}, kAfter).empty());
    CHECK((watch.Read(65001, {E(1001, "Two", false), E(1002, "Three", false)}, kAfter + 1.0) ==
           std::vector<FriendNews>{{NoticeKind::Offline, 1001, "Two"}}));
    CHECK((watch.Read(65001, {E(1001, "Two", true), E(1002, "Three", true)}, kAfter + 2.0) ==
           std::vector<FriendNews>{{NoticeKind::Online, 1001, "Two"}, {NoticeKind::Online, 1002, "Three"}}));
    // Another character with no change of the flag, a friend gone, a friend who appears already playing.
    CHECK(watch.Read(65001, {E(1001, "Alt", true), E(1003, "Four", true)}, kAfter + 3.0).empty());
    // Back after being gone: they appear, which is no news, whatever they were before.
    CHECK(watch.Read(65001, {E(1001, "Alt", true), E(1002, "Three", true), E(1003, "Four", true)}, kAfter + 4.0).empty());
}

TEST(a_friend_is_followed_by_account_whatever_slot_they_are_in)
{
    FriendWatch watch;
    watch.Read(65001, {E(1001, "Two", false), E(1002, "Three", true)}, 0.0);
    CHECK((watch.Read(65001, {E(1002, "Three", true), E(1001, "Two", true)}, kAfter) ==
           std::vector<FriendNews>{{NoticeKind::Online, 1001, "Two"}}));
}

TEST(a_sign_on_or_another_character_only_records_the_list)
{
    FriendWatch watch;
    watch.Read(65001, {E(1001, "Two", false)}, 0.0);
    CHECK(watch.Read(65002, {E(1001, "Two", true)}, kAfter).empty()); // another character of yours
    CHECK((watch.Read(65002, {E(1001, "Two", false)}, 2 * kAfter) ==
           std::vector<FriendNews>{{NoticeKind::Offline, 1001, "Two"}}));
    watch.Forget();
    CHECK(watch.Read(65002, {E(1001, "Two", true)}, 3 * kAfter).empty());
}

TEST(the_list_filling_after_a_sign_on_is_no_news)
{
    // On the test client the list shows a friend already online as not playing in the first read in game, and
    // playing a second later.
    FriendWatch watch;
    CHECK(watch.Read(65001, {E(1001, "Two", false)}, 100.0).empty());
    CHECK(watch.Read(65001, {E(1001, "Two", true)}, 100.0 + kSignOnQuietSeconds - 0.5).empty());
    CHECK((watch.Read(65001, {E(1001, "Two", false)}, 100.0 + kSignOnQuietSeconds + 0.5) ==
           std::vector<FriendNews>{{NoticeKind::Offline, 1001, "Two"}}));
}

TEST(a_friend_who_came_online_while_no_read_was_taken_is_news_at_the_next_read)
{
    // Zoning: the plugin takes no read, and the watch keeps the last one.
    FriendWatch watch;
    watch.Read(65001, {E(1001, "Two", false)}, 0.0);
    CHECK_EQ(watch.Read(65001, {E(1001, "Two", true)}, 60.0).size(), size_t{1});
}

TEST(a_notice_is_wanted_only_with_headsup_on_its_kind_on_and_the_friend_not_muted)
{
    Settings s;
    const FriendNews online{NoticeKind::Online, 1001, "Two"}, offline{NoticeKind::Offline, 1001, "Two"};
    CHECK(NoticeWanted(online, s, {}) && NoticeWanted(offline, s, {}));
    CHECK(!NoticeWanted(online, s, {1001}) && NoticeWanted(online, s, {1002}));
    s.friendNoticeOffline = false;
    CHECK(NoticeWanted(online, s, {}) && !NoticeWanted(offline, s, {}));
    s.friendNoticeOnline = false;
    CHECK(!NoticeWanted(online, s, {}));
    s.friendNoticeOnline = true;
    s.enabled            = false;
    CHECK(!NoticeWanted(online, s, {}));
}

TEST(the_mute_file_holds_one_account_a_line_and_survives_a_hand_edit)
{
    CHECK_EQ(FriendMutesText({1003, 1001}), "1001\n1003\n");
    CHECK_EQ(FriendMutesText({}), "");
    CHECK((FriendMutesFrom("1001\n1003\n") == FriendMutes{1001, 1003}));
    CHECK((FriendMutesFrom("abc\r\n\r\n 1002 \r\n1002\n-5\n12x\n0\n99999999999\n7") == FriendMutes{7, 1002}));
    CHECK(FriendMutesFrom("").empty());
}

TEST(a_notice_is_solid_for_four_fifths_of_its_time_then_fades)
{
    CHECK_EQ(NoticeOpacity(0.0f, 8.0f), 1.0f);
    CHECK_EQ(NoticeOpacity(6.4f, 8.0f), 1.0f);
    CHECK(std::abs(NoticeOpacity(7.2f, 8.0f) - 0.5f) < 0.001f); // halfway through its last 1.6 s
    CHECK_EQ(NoticeOpacity(8.0f, 8.0f), 0.0f);
    CHECK_EQ(NoticeOpacity(9.0f, 8.0f), 0.0f);
    CHECK_EQ(NoticeBarShare(2.0f, 8.0f), 0.75f);
    CHECK_EQ(NoticeBarShare(9.0f, 8.0f), 0.0f);
}

TEST(the_stack_keeps_five_notices_oldest_first_and_drops_each_at_its_time)
{
    NoticeStack stack;
    for (int n = 0; n < 6; ++n) stack.Add(NoticeKind::Online, std::string(1, static_cast<char>('A' + n)), 10.0 + n);
    std::vector<ShownNotice> shown = stack.Shown(15.0, 8.0f);
    CHECK_EQ(shown.size(), size_t{5});
    CHECK_EQ(shown.front().name, "B"); // A, the oldest, was pushed out by the sixth
    CHECK_EQ(shown.back().name, "F");
    CHECK_EQ(shown.front().share, 0.5f); // B is 4 s into 8
    shown = stack.Shown(20.5, 8.0f);     // B (11) and C (12) are past 8 s; D is 7.5 s in, E 6.5, F 5.5
    CHECK_EQ(shown.size(), size_t{3});
    CHECK(std::abs(shown[0].opacity - 0.3125f) < 0.001f); // D: 1.1 s into its 1.6 s fade
    CHECK(std::abs(shown[1].opacity - 0.9375f) < 0.001f); // E: 0.1 s into it
    CHECK_EQ(shown[2].opacity, 1.0f);
    const uint32_t d = shown[0].id;
    CHECK(shown[1].id != d && stack.Shown(20.6, 8.0f)[0].id == d);
    // The Duration turned down to 6 s while they show: D and E are older than it and go. F, 5.6 s in, is 0.8 s
    // into a 1.2 s fade.
    shown = stack.Shown(20.6, 6.0f);
    CHECK_EQ(shown.size(), size_t{1});
    CHECK(shown[0].name == "F" && std::abs(shown[0].opacity - 0.3333f) < 0.001f);
}

TEST(notices_stack_ten_pixels_apart_down_from_the_anchor_or_up_from_it)
{
    const float heights[] = {50.0f, 50.0f, 60.0f};
    CHECK((NoticeTops(heights, 200.0f, false) == std::vector<float>{200.0f, 260.0f, 320.0f}));
    CHECK((NoticeTops(heights, 500.0f, true) == std::vector<float>{450.0f, 390.0f, 320.0f}));
    CHECK(NoticeTops({}, 200.0f, false).empty());
}

TEST(a_notice_slides_to_its_place_and_lands_on_it)
{
    CHECK(std::abs(Slid(100.0f, 0.0f, 0.016f) - 87.2f) < 0.01f); // 8 a second of what is left: 12.8% of 100
    CHECK_EQ(Slid(100.0f, 0.0f, 0.5f), 0.0f);                    // a long frame never overshoots
    CHECK_EQ(Slid(0.4f, 0.0f, 0.016f), 0.0f);                    // within half a pixel it is there
    CHECK_EQ(Slid(7.0f, 7.0f, 0.016f), 7.0f);
}

TEST(the_stack_starts_under_the_panel_and_a_placed_one_keeps_its_share_of_the_screen)
{
    NoticeSpot at = NoticeAnchor(-1, -1, false, 1920.0f, 1080.0f, 45.0f, 60.0f);
    CHECK(at.x == 10.0f && at.y == 55.0f);
    at = NoticeAnchor(-1, -1, true, 1920.0f, 1080.0f, 45.0f, 60.0f);
    CHECK(at.x == 10.0f && at.y == 1070.0f);
    at = NoticeAnchor(2500, 5000, false, 1920.0f, 1080.0f, 45.0f, 60.0f);
    CHECK(at.x == 480.0f && at.y == 540.0f);
    CHECK_EQ(SpotShare(480.0f, 1920.0f), 2500);
    // Saved at the bottom right of a wider screen: the first 300 by 60 px box stays whole on this one.
    CHECK_EQ(NoticeAnchor(10000, 10000, false, 1280.0f, 720.0f, 45.0f, 60.0f).x, 980.0f);
    CHECK_EQ(NoticeAnchor(10000, 10000, false, 1280.0f, 720.0f, 45.0f, 60.0f).y, 660.0f);
}

TEST(a_spot_placed_for_one_way_of_stacking_keeps_the_first_notice_on_the_screen_the_other_way)
{
    // Dragged to the bottom with Stack Upward on, then switched off: the first box sits 60 px over the bottom.
    CHECK_EQ(NoticeAnchor(2500, 10000, false, 1920.0f, 1080.0f, 45.0f, 60.0f).y, 1020.0f);
    // Dragged to the top with it off, then switched on: the first box hangs from 60 px down, not off the top.
    CHECK_EQ(NoticeAnchor(2500, 0, true, 1920.0f, 1080.0f, 45.0f, 60.0f).y, 60.0f);
}

TEST(an_online_sound_waits_ten_seconds_and_an_offline_has_none)
{
    SoundWaits waits;
    CHECK(waits.Due(NoticeKind::Online, 100.0));
    CHECK(!waits.Due(NoticeKind::Online, 106.0)); // a silent one does not start the wait again
    CHECK(!waits.Due(NoticeKind::Online, 109.9) && waits.Due(NoticeKind::Online, 110.0));
    CHECK(!waits.Due(NoticeKind::Offline, 500.0));
}

TEST(a_notice_says_who_and_what_in_its_kinds_color)
{
    CHECK_EQ(NoticeMessage(NoticeKind::Online, "Two"), "Two is now online");
    CHECK_EQ(NoticeMessage(NoticeKind::Offline, "Two"), "Two went offline");
    CHECK_EQ(std::string(NoticeTitle(NoticeKind::Online)), "Friend Online");
    CHECK(NoticeColor(NoticeKind::Online) != NoticeColor(NoticeKind::Offline));
}

TEST(move_shows_a_sample_of_each_kind_that_is_on_and_one_online_when_none_is)
{
    Settings s;
    std::vector<ShownNotice> samples = NoticeSamples(s);
    CHECK(samples.size() == 2 && samples[0].kind == NoticeKind::Online && samples[1].kind == NoticeKind::Offline);
    CHECK(samples[0].name == kNoticeSampleName && samples[0].opacity == 1.0f && samples[0].share == 1.0f);
    CHECK(samples[0].id != samples[1].id);
    s.friendNoticeOnline = false;
    samples              = NoticeSamples(s);
    CHECK(samples.size() == 1 && samples[0].kind == NoticeKind::Offline);
    s.friendNoticeOffline = false;
    samples               = NoticeSamples(s);
    CHECK(samples.size() == 1 && samples[0].kind == NoticeKind::Online); // something to drag
}

TEST(a_read_while_your_character_is_not_known_is_skipped)
{
    // The party's first member reads 0 for a moment: neither a character change nor a read.
    FriendWatch watch;
    watch.Read(65001, {E(1001, "Two", false)}, 0.0);
    CHECK(watch.Read(0, {E(1001, "Two", true)}, kAfter).empty());
    CHECK((watch.Read(65001, {E(1001, "Two", true)}, kAfter + 1.0) ==
           std::vector<FriendNews>{{NoticeKind::Online, 1001, "Two"}}));
}
