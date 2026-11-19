#include "notices.h"
#include "test.h"

#include <cmath>
#include <string>
#include <vector>

using namespace headsup;

namespace
{
    constexpr float kFade = 0.5f; // Fade Speed, in seconds

    std::vector<std::string> Names(const std::vector<ShownNotice>& shown)
    {
        std::vector<std::string> names;
        for (const ShownNotice& notice : shown) names.push_back(notice.name);
        return names;
    }

    bool Within(const std::vector<ShownNotice>& shown)
    {
        for (const ShownNotice& notice : shown)
            if (notice.opacity < 0.0f || notice.opacity > 1.0f || notice.share < 0.0f || notice.share > 1.0f) return false;
        return true;
    }
}

TEST(a_notice_is_solid_until_its_time_is_up_then_fades_for_the_fade_speed)
{
    CHECK_EQ(NoticeOpacity(7.9f, 8.0f, 3.0f), 1.0f);
    CHECK_EQ(NoticeOpacity(8.0f, 8.0f, 3.0f), 1.0f);
    CHECK(std::abs(NoticeOpacity(9.5f, 8.0f, 3.0f) - 0.5f) < 0.001f);
    CHECK_EQ(NoticeOpacity(11.0f, 8.0f, 3.0f), 0.0f);
    CHECK(std::abs(NoticeOpacity(8.25f, 8.0f, 0.5f) - 0.5f) < 0.001f);
    CHECK_EQ(NoticeOpacity(20.0f, 8.0f, 3.0f), 0.0f);
    CHECK_EQ(NoticeBarShare(8.0f, 8.0f), 0.0f);
    CHECK_EQ(NoticeBarShare(2.0f, 8.0f), 0.75f);
}

TEST(with_no_fade_a_notice_vanishes_the_moment_its_time_is_up)
{
    CHECK_EQ(NoticeOpacity(7.99f, 8.0f, 0.0f), 1.0f);
    CHECK_EQ(NoticeOpacity(8.0f, 8.0f, 0.0f), 0.0f);
    NoticeStack stack;
    stack.Add(NoticeKind::Online, "A");
    CHECK(stack.Shown(7.9f, false, 8.0f, 0.0f, 5, true).size() == 1);
    CHECK(stack.Shown(0.1f, false, 8.0f, 0.0f, 5, true).empty());
}

TEST(a_notice_goes_half_a_second_after_its_time)
{
    NoticeStack stack;
    stack.Add(NoticeKind::Online, "A");
    std::vector<ShownNotice> shown = stack.Shown(8.4f, false, 8.0f, kFade, 5, true);
    CHECK(shown.size() == 1 && shown[0].share == 0.0f && shown[0].opacity > 0.0f);
    CHECK(stack.Shown(0.2f, false, 8.0f, kFade, 5, true).empty());
}

// An invite's clock is the server's limit, so the pointer holds only the friends' notices; a test invite acts the same.
TEST(the_pointer_over_a_notice_holds_the_friends_clocks_and_never_an_invites)
{
    NoticeStack stack;
    stack.Add(NoticeKind::Online, "A");
    stack.Add(NoticeKind::PartyInvite, "B");
    stack.Add(NoticeKind::PartyInvite, kInviteSampleName, true);
    const std::vector<ShownNotice> shown = stack.Shown(9.0f, true, 8.0f, kFade, 5, true);
    CHECK((Names(shown) == std::vector<std::string>{"B", kInviteSampleName, "A"}));
    CHECK_EQ(shown[2].share, 1.0f);
    CHECK(std::abs(shown[0].share - 0.9f) < 0.001f); // 9 s of its 90
    CHECK(std::abs(shown[1].share - 0.9f) < 0.001f);
}

TEST(an_invite_keeps_the_first_place_and_is_never_pushed_out)
{
    NoticeStack stack;
    stack.Add(NoticeKind::Online, "A");
    stack.Add(NoticeKind::PartyInvite, "L");
    for (const char* name : {"B", "C", "D", "E", "F"}) stack.Add(NoticeKind::Online, name);
    std::vector<ShownNotice> shown = stack.Shown(0.0f, false, 8.0f, kFade, 5, true);
    CHECK((Names(shown) == std::vector<std::string>{"L", "C", "D", "E", "F"})); // the invite counts in the 5
    CHECK(shown[0].buttons && !shown[1].buttons && !shown[4].buttons);
    shown = stack.Shown(0.0f, false, 8.0f, kFade, 2, true);
    CHECK((Names(shown) == std::vector<std::string>{"L", "F"}));
    CHECK(!stack.Shown(0.0f, false, 8.0f, kFade, 2, false)[0].buttons);
}

TEST(a_new_invite_takes_the_place_of_the_one_held_of_its_sort)
{
    NoticeStack stack;
    stack.Add(NoticeKind::PartyInvite, "First");
    stack.Add(NoticeKind::AllianceInvite, "Second");
    std::vector<ShownNotice> shown = stack.Shown(0.0f, false, 8.0f, kFade, 5, true);
    CHECK(shown.size() == 1 && shown[0].name == "Second" && shown[0].kind == NoticeKind::AllianceInvite);
    stack.Add(NoticeKind::PartyInvite, kInviteSampleName, true);
    shown = stack.Shown(0.0f, false, 8.0f, kFade, 5, true);
    CHECK((Names(shown) == std::vector<std::string>{"Second", kInviteSampleName}));
}

TEST(closing_tells_a_real_invite_from_a_test_and_a_friend)
{
    NoticeStack stack;
    const uint32_t real    = stack.Add(NoticeKind::PartyInvite, "Leader");
    const uint32_t test    = stack.Add(NoticeKind::PartyInvite, kInviteSampleName, true);
    const uint32_t friend_ = stack.Add(NoticeKind::Online, "Pal");
    CHECK(real != test && test != friend_);
    CHECK(!stack.Close(test));
    CHECK(!stack.Close(friend_));
    CHECK(!stack.Close(friend_ + 100));
    CHECK((Names(stack.Shown(0.0f, false, 8.0f, kFade, 5, true)) == std::vector<std::string>{"Leader"}));
    CHECK(stack.Close(real));
    CHECK(!stack.Close(real));
    CHECK(stack.Shown(0.0f, false, 8.0f, kFade, 5, true).empty());
    stack.Add(NoticeKind::PartyInvite, "Leader");
    stack.Add(NoticeKind::PartyInvite, kInviteSampleName, true);
    stack.EndInvite();
    CHECK((Names(stack.Shown(0.0f, false, 8.0f, kFade, 5, true)) == std::vector<std::string>{kInviteSampleName}));
}

TEST(every_invite_stays_ninety_seconds_whatever_the_duration)
{
    NoticeStack stack;
    stack.Add(NoticeKind::PartyInvite, "Leader");
    stack.Add(NoticeKind::PartyInvite, kInviteSampleName, true);
    std::vector<ShownNotice> shown = stack.Shown(8.6f, false, 8.0f, kFade, 5, true);
    CHECK((Names(shown) == std::vector<std::string>{"Leader", kInviteSampleName}));
    CHECK(std::abs(shown[1].share - (1.0f - 8.6f / 90.0f)) < 0.001f);
    CHECK(stack.Shown(81.0f, false, 8.0f, kFade, 5, true).size() == 2); // 89.6 s in
    CHECK(stack.Shown(1.0f, false, 8.0f, kFade, 5, true).empty());      // 90.6 s: their fade is over
}

TEST(max_notifications_of_one_leaves_a_real_invite_alone)
{
    NoticeStack stack;
    stack.Add(NoticeKind::Online, "A");
    stack.Add(NoticeKind::PartyInvite, kInviteSampleName, true);
    stack.Add(NoticeKind::PartyInvite, "Leader");
    CHECK((Names(stack.Shown(0.0f, false, 8.0f, kFade, 1, true)) == std::vector<std::string>{"Leader"}));
    stack.Add(NoticeKind::PartyInvite, kInviteSampleName, true);
    CHECK((Names(stack.Shown(0.0f, false, 8.0f, kFade, 2, true)) == std::vector<std::string>{"Leader", kInviteSampleName}));
}

TEST(the_duration_shortened_drops_the_notices_older_than_it)
{
    NoticeStack stack;
    stack.Add(NoticeKind::Online, "A");
    std::vector<ShownNotice> shown = stack.Shown(5.0f, false, 8.0f, kFade, 5, true);
    CHECK(shown.size() == 1 && Within(shown));
    shown = stack.Shown(0.0f, false, 3.0f, kFade, 5, true);
    CHECK(shown.empty());
    stack.Add(NoticeKind::Online, "B");
    shown = stack.Shown(3.2f, false, 3.0f, kFade, 5, true); // into its fade
    CHECK(shown.size() == 1 && Within(shown));
}

TEST(each_friend_sound_waits_ten_seconds_of_its_own_and_an_invite_never)
{
    SoundWaits waits;
    CHECK(waits.Due(NoticeKind::Online, 0.0));
    CHECK(!waits.Due(NoticeKind::Online, 9.0));
    CHECK(waits.Due(NoticeKind::Offline, 9.0));
    CHECK(waits.Due(NoticeKind::Online, 10.0));
    CHECK(waits.Due(NoticeKind::PartyInvite, 0.0));
    CHECK(waits.Due(NoticeKind::PartyInvite, 0.1));
}

TEST(the_test_picks_only_among_the_kinds_that_are_on)
{
    Settings s;
    CHECK(TestKind(0, s) == NoticeKind::Online);
    CHECK(TestKind(1, s) == NoticeKind::Offline);
    CHECK(TestKind(2, s) == NoticeKind::PartyInvite);
    CHECK(TestKind(3, s) == NoticeKind::Online);
    s.friendNoticeOnline = s.friendNoticeOffline = false;
    for (uint32_t seed = 0; seed < 5; ++seed) CHECK(TestKind(seed, s) == NoticeKind::PartyInvite);
    s.partyNotice = false;
    CHECK(!TestKind(0, s));
}

TEST(a_notice_plays_its_kinds_sound_at_master_times_alert)
{
    Settings s;
    s.noticeVolume       = 50;
    s.friendNoticeVolume = 60;
    s.partyNoticeVolume  = 100;
    CHECK_EQ(NoticeVolumeOf(s, NoticeKind::Online), 30);
    CHECK_EQ(NoticeVolumeOf(s, NoticeKind::Offline), 30);
    CHECK_EQ(NoticeVolumeOf(s, NoticeKind::PartyInvite), 50);
    CHECK_EQ(NoticeVolumeOf(s, NoticeKind::AllianceInvite), 50);
    s.friendNoticeOnlineSound  = kFirstCallChoice + 3;
    s.friendNoticeOfflineSound = kChimeSound;
    s.partyNoticeSound         = kFirstCallChoice + 7;
    CHECK_EQ(NoticeSoundOf(s, NoticeKind::Online), kFirstCallChoice + 3);
    CHECK_EQ(NoticeSoundOf(s, NoticeKind::Offline), kChimeSound);
    CHECK_EQ(NoticeSoundOf(s, NoticeKind::PartyInvite), kFirstCallChoice + 7);
    CHECK_EQ(NoticeSoundOf(s, NoticeKind::AllianceInvite), kFirstCallChoice + 7);
}

TEST(the_samples_are_a_test_notice_of_each_kind_that_is_on)
{
    Settings s;
    std::vector<ShownNotice> samples = NoticeSamples(s, kMaxNoticeMost);
    CHECK(samples.size() == 3);
    CHECK(samples[0].kind == NoticeKind::Online && samples[1].kind == NoticeKind::Offline);
    CHECK(samples[2].kind == NoticeKind::PartyInvite);
    CHECK(samples[0].name == kFriendSampleName && samples[1].name == kFriendSampleName);
    CHECK(samples[2].name == kInviteSampleName && samples[2].buttons && !samples[0].buttons);
    CHECK(samples[0].opacity == 1.0f && samples[0].share == 1.0f);
    CHECK(samples[0].id != samples[1].id && samples[1].id != samples[2].id);
    // Max Notifications counts them, after the real ones.
    CHECK((NoticeSamples(s, 2).size() == 2 && NoticeSamples(s, 2)[1].kind == NoticeKind::Offline));
    CHECK(NoticeSamples(s, 0).empty());
    s.partyNoticeButtons = false;
    CHECK(!NoticeSamples(s, kMaxNoticeMost)[2].buttons);
    s.friendNoticeOnline = false;
    s.partyNotice        = false;
    samples              = NoticeSamples(s, kMaxNoticeMost);
    CHECK(samples.size() == 1 && samples[0].kind == NoticeKind::Offline);
    s.friendNoticeOffline = false;
    samples               = NoticeSamples(s, kMaxNoticeMost);
    CHECK(samples.size() == 1 && samples[0].kind == NoticeKind::Online); // something to see and drag
}

TEST(notices_stack_ten_pixels_apart_down_from_the_anchor_or_up_from_it)
{
    const float heights[] = {50.0f, 50.0f, 60.0f};
    CHECK((NoticeTops(heights, 200.0f, NoticeEdge::Top) == std::vector<float>{200.0f, 260.0f, 320.0f}));
    CHECK((NoticeTops(heights, 500.0f, NoticeEdge::Bottom) == std::vector<float>{450.0f, 390.0f, 320.0f}));
    CHECK(NoticeTops({}, 200.0f, NoticeEdge::Top).empty());
}

TEST(a_notice_slides_to_its_place_and_lands_on_it)
{
    CHECK(std::abs(Slid(100.0f, 0.0f, 0.016f) - 87.2f) < 0.01f); // 8 a second of what is left: 12.8% of 100
    CHECK_EQ(Slid(100.0f, 0.0f, 0.5f), 0.0f);                    // a long frame never overshoots
    CHECK_EQ(Slid(0.4f, 0.0f, 0.016f), 0.0f);                    // within half a pixel it is there
    CHECK_EQ(Slid(7.0f, 7.0f, 0.016f), 7.0f);
}

TEST(a_notice_says_who_and_what_in_its_kinds_color)
{
    // A friend's second line is only for an alt: the name they are on your friend list by.
    CHECK_EQ(NoticeTitle(NoticeKind::Online, "Two"), "Two logged in");
    CHECK_EQ(NoticeTitle(NoticeKind::Offline, "Two"), "Two logged out");
    CHECK_EQ(NoticeMessage(NoticeKind::Online, "Two", ""), "");
    CHECK_EQ(NoticeMessage(NoticeKind::Offline, "Alt", "Two"), "Also known as Two");
    CHECK_EQ(NoticeMessage(NoticeKind::PartyInvite, "Lead", ""), "Lead invited you to a party");
    CHECK_EQ(NoticeMessage(NoticeKind::AllianceInvite, "Lead", ""), "Lead invited you to an alliance");
    CHECK_EQ(NoticeTitle(NoticeKind::PartyInvite, "Lead"), "Party Invite");
    CHECK_EQ(NoticeTitle(NoticeKind::AllianceInvite, "Lead"), "Alliance Invite");
    CHECK(NoticeColor(NoticeKind::Online) != NoticeColor(NoticeKind::Offline));
    CHECK(NoticeColor(NoticeKind::PartyInvite) == NoticeColor(NoticeKind::AllianceInvite));
    CHECK(NoticeColor(NoticeKind::PartyInvite) != NoticeColor(NoticeKind::Online));
    CHECK(IsInvite(NoticeKind::AllianceInvite) && !IsInvite(NoticeKind::Offline));
}

// A 1000 by 800 screen, the panel's bottom at 45 and a first box 300 by 60.
TEST(the_spot_is_the_stacks_side_and_edge_by_its_anchors)
{
    NoticeSpot at = NoticeAnchor(-1, -1, HelpAnchor::Left, NoticeEdge::Top, 1000.0f, 800.0f, 45.0f, 300.0f, 60.0f);
    CHECK(at.x == 10.0f && at.y == 55.0f);
    at = NoticeAnchor(-1, -1, HelpAnchor::Center, NoticeEdge::Top, 1000.0f, 800.0f, 45.0f, 300.0f, 60.0f);
    CHECK(at.x == 500.0f && at.y == 55.0f);
    at = NoticeAnchor(-1, -1, HelpAnchor::Right, NoticeEdge::Bottom, 1000.0f, 800.0f, 45.0f, 300.0f, 60.0f);
    CHECK(at.x == 990.0f && at.y == 790.0f);
    CHECK_EQ(NoticeLeft(300.0f, HelpAnchor::Left), 0.0f);
    CHECK_EQ(NoticeLeft(300.0f, HelpAnchor::Center), 150.0f);
    CHECK_EQ(NoticeLeft(300.0f, HelpAnchor::Right), 300.0f);
    at = NoticeAnchor(2500, 5000, HelpAnchor::Left, NoticeEdge::Top, 1000.0f, 800.0f, 45.0f, 300.0f, 60.0f);
    CHECK(at.x == 250.0f && at.y == 400.0f);
    CHECK_EQ(SpotShare(250.0f, 1000.0f), 2500);
}

TEST(a_spot_keeps_the_first_notice_whole_on_the_screen)
{
    const auto x = [](int across, HelpAnchor anchor) {
        return NoticeAnchor(across, 100, anchor, NoticeEdge::Top, 1000.0f, 800.0f, 45.0f, 300.0f, 60.0f).x;
    };
    CHECK_EQ(x(9900, HelpAnchor::Left), 700.0f);
    CHECK_EQ(x(9900, HelpAnchor::Center), 850.0f);
    CHECK_EQ(x(9900, HelpAnchor::Right), 990.0f);
    CHECK_EQ(x(0, HelpAnchor::Left), 0.0f);
    CHECK_EQ(x(0, HelpAnchor::Center), 150.0f);
    CHECK_EQ(x(0, HelpAnchor::Right), 300.0f);
    CHECK_EQ(NoticeAnchor(9900, 100, HelpAnchor::Left, NoticeEdge::Top, 1000.0f, 800.0f, 45.0f, 300.0f, 60.0f).y, 8.0f);
    CHECK_EQ(NoticeAnchor(9900, 100, HelpAnchor::Left, NoticeEdge::Bottom, 1000.0f, 800.0f, 45.0f, 300.0f, 60.0f).y, 60.0f);
}

TEST(changing_an_anchor_keeps_the_stack_where_it_is)
{
    const NoticeRect bounds{100.0f, 200.0f, 300.0f, 150.0f};
    NoticeSpot at = Reanchored(bounds, HelpAnchor::Left, NoticeEdge::Top);
    CHECK(at.x == 100.0f && at.y == 200.0f);
    at = Reanchored(bounds, HelpAnchor::Center, NoticeEdge::Bottom);
    CHECK(at.x == 250.0f && at.y == 350.0f);
    at = Reanchored(bounds, HelpAnchor::Right, NoticeEdge::Top);
    CHECK(at.x == 400.0f && at.y == 200.0f);
}

TEST(shrink_brings_each_line_down_to_the_width_then_both_to_the_height)
{
    FittedText fit = FittedSizes(16.0f, 13.0f, 400.0f, 200.0f, 280.0f, 20.0f, 16.0f, 0.0f);
    CHECK(std::abs(fit.title - 11.2f) < 0.001f && fit.message == 13.0f);
    // At those sizes the lines stand 14 and 16 tall, 30 in all: both take two thirds to fit 20.
    fit = FittedSizes(16.0f, 13.0f, 400.0f, 200.0f, 280.0f, 20.0f, 16.0f, 20.0f);
    CHECK(std::abs(fit.title - 7.4667f) < 0.001f && std::abs(fit.message - 8.6667f) < 0.001f);
    fit = FittedSizes(16.0f, 13.0f, 4000.0f, 200.0f, 280.0f, 20.0f, 16.0f, 0.0f);
    CHECK(fit.title == kMinNoticeText && fit.message == 13.0f);
    fit = FittedSizes(16.0f, 13.0f, 400.0f, 200.0f, 0.0f, 20.0f, 16.0f, 0.0f);
    CHECK(fit.title == 16.0f && fit.message == 13.0f);
}

TEST(set_by_theme_takes_the_themes_window_rounding_or_its_shapes)
{
    CHECK_EQ(NoticeRadius(-1, -1, 9.0f), 9.0f);
    CHECK_EQ(NoticeRadius(-1, 12, 9.0f), 12.0f);
    CHECK_EQ(NoticeRadius(4, 12, 9.0f), 4.0f);
    CHECK_EQ(NoticeRadius(0, 12, 9.0f), 0.0f);
}

TEST(a_point_is_over_a_notice_only_inside_a_box)
{
    const NoticeRect boxes[] = {{10.0f, 55.0f, 300.0f, 60.0f}, {10.0f, 125.0f, 300.0f, 60.0f}};
    CHECK(OverAny(boxes, 10.0f, 55.0f));
    CHECK(OverAny(boxes, 309.0f, 184.0f));
    CHECK(!OverAny(boxes, 310.0f, 60.0f));
    CHECK(!OverAny(boxes, 100.0f, 120.0f)); // the gap
    CHECK(!OverAny(boxes, 9.0f, 60.0f));
    CHECK(!OverAny({}, 100.0f, 80.0f));
}

TEST(invite_position_puts_invites_at_the_anchor_or_the_top_or_the_bottom)
{
    const auto order = [](InvitePlace place, NoticeEdge edge) {
        Settings settings;
        settings.partyNoticePlace = place;
        settings.noticeEdge       = edge;
        std::vector<ShownNotice> shown = {ShownNotice{1, NoticeKind::PartyInvite, "A"},
            ShownNotice{2, NoticeKind::Online, "B"}, ShownNotice{3, NoticeKind::Offline, "C"},
            ShownNotice{4, NoticeKind::AllianceInvite, "D"}};
        PlaceInvites(shown, settings);
        std::vector<uint32_t> ids;
        for (const ShownNotice& notice : shown) ids.push_back(notice.id);
        return ids;
    };
    // The stack is laid out from its anchor: first is the top when it grows down, the bottom when it grows up.
    const std::vector<uint32_t> first = {1, 4, 2, 3}, last = {2, 3, 1, 4};
    CHECK(order(InvitePlace::Auto, NoticeEdge::Top) == first);
    CHECK(order(InvitePlace::Auto, NoticeEdge::Bottom) == first);
    CHECK(order(InvitePlace::Top, NoticeEdge::Top) == first);
    CHECK(order(InvitePlace::Top, NoticeEdge::Bottom) == last);
    CHECK(order(InvitePlace::Bottom, NoticeEdge::Top) == last);
    CHECK(order(InvitePlace::Bottom, NoticeEdge::Bottom) == first);
}
