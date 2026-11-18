#include "friend_notices.h"
#include "test.h"

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
