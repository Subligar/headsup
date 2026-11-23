#pragma once

#include "settings.h"

#include <cstdint>
#include <deque>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace headsup
{
    enum class NoticeKind : uint8_t
    {
        Online,
        Offline,
        PartyInvite,
        AllianceInvite
    };
    bool IsInvite(NoticeKind kind);

    constexpr float kInviteSeconds = 90.0f; // the time the server gives you to answer an invite
    // Max Notifications counts the invites and the page's test notices too.
    constexpr int kNoticeBoxes            = kMaxNoticeMost;
    constexpr float kNoticeGap = 10.0f, kNoticeBarHeight = 4.0f;
    constexpr float kMinNoticeText        = 6.0f; // pixels: Shrink goes no smaller
    constexpr float kNoticeSlidePerSecond = 8.0f, kNoticeSlideSnap = 0.5f;
    constexpr float kNoticeStartLeft      = 10.0f, kNoticeStartGap = 10.0f; // unplaced: from the left, and the panel
    constexpr double kFriendSoundWait     = 10.0;                          // seconds
    constexpr uint32_t kOnlineColor = 0xFF33FF33, kOfflineColor = 0xFFCCCCCC, kInviteColor = 0xFF66CCFF;
    constexpr const char* kFriendSampleName = "Test Friend", *kInviteSampleName = "Test Leader";
    constexpr const char* kDeclineWord = "Decline", *kAcceptWord = "Accept";

    // 1 until its time is up, then down to 0 over fade seconds; with no fade, 0 the moment it is up.
    float NoticeOpacity(float age, float life, float fade);
    // 1 down to 0 over its life.
    float NoticeBarShare(float age, float life);

    struct ShownNotice
    {
        uint32_t id = 0; // its own for as long as it shows
        NoticeKind kind = NoticeKind::Online;
        std::string name;
        float opacity = 1.0f, share = 1.0f;
        bool buttons = false; // an invite's Decline and Accept
        std::string aka{};    // a friend on an alt: the name they are on your friend list by
    };

    class NoticeStack
    {
    public:
        // test: the page's Test made it. An invite takes the place of the invite held of its own sort (real or
        // test), so a Test never costs you a real invite. Its id.
        uint32_t Add(NoticeKind kind, std::string name, bool test = false, std::string aka = {});
        // seconds: the frame's. held: the pointer is over a notification, which stops the friends' clocks; an
        // invite's runs on for kInviteSeconds, whatever Duration (life) says. fade: Fade Speed. most: Max
        // Notifications, the invites counted: friends' go oldest first to make room, and a test invite before a
        // real one. buttons: Show Buttons. The real invite first, then a test one, then the rest oldest first.
        std::vector<ShownNotice> Shown(float seconds, bool held, float life, float fade, int most, bool buttons);
        // This one goes now. True when it was a real invite, which the game is then to answer.
        bool Close(uint32_t id);
        // The real invite goes: it was answered, or you zoned or signed off.
        void EndInvite();

    private:
        struct Notice
        {
            uint32_t id;
            NoticeKind kind;
            std::string name;
            std::string aka;
            float age = 0.0f;
        };
        std::optional<Notice> m_Invite, m_TestInvite;
        std::deque<Notice> m_Notices; // the rest, oldest first
        uint32_t m_NextId = 1;
    };

    // Each notice's top, oldest first. Top: from the anchor, each under the one before. Bottom: the anchor is the
    // stack's bottom edge, the oldest sits on it and each newer one goes above.
    std::vector<float> NoticeTops(std::span<const float> heights, float anchor, NoticeEdge edge);
    // One frame of a notice moving toward its place.
    float Slid(float at, float target, float seconds);

    struct NoticeSpot
    {
        float x = 0.0f, y = 0.0f;
    };
    struct NoticeRect
    {
        float left = 0.0f, top = 0.0f, width = 0.0f, height = 0.0f;
        bool operator==(const NoticeRect&) const = default;
    };
    // The spot is one point: the stack's left edge, middle or right edge by Horizontal Anchor, and its top or bottom
    // edge by Vertical Anchor. A box's left from the spot: 0, half its width or its whole width to the left.
    float NoticeLeft(float width, HelpAnchor anchor);
    // The spot, kept so the stack is whole across the screen (every box lines up on the anchor's side, so the widest,
    // wide, bounds it) and its first box (firstHeight tall) is whole down it. Unplaced: kNoticeStartLeft
    // from the left or the right, or the screen's middle; kNoticeStartGap under the top panel, or over the screen's
    // bottom.
    NoticeSpot NoticeAnchor(int across, int down, HelpAnchor anchor, NoticeEdge edge, float screenWidth,
        float screenHeight, float barBottom, float wide, float firstHeight);
    // Whether the point is inside any of the boxes.
    bool OverAny(std::span<const NoticeRect> boxes, float x, float y);
    // The spot that leaves a stack drawn in bounds where it is, for these anchors.
    NoticeSpot Reanchored(const NoticeRect& bounds, HelpAnchor anchor, NoticeEdge edge);
    struct FittedText
    {
        float title = 0.0f, message = 0.0f; // font sizes
    };
    // Shrink: each line's size brought down until it is no wider than room, then both by the same share until their
    // heights fit tallRoom, never under kMinNoticeText. A room of 0 is any (Flexible).
    FittedText FittedSizes(float titleSize, float messageSize, float titleWide, float messageWide, float room,
        float titleTall, float messageTall, float tallRoom);
    // Corner Rounding: its own pixels, or under Set by Theme the theme's Window Rounding, or its shape's own when the
    // theme has none.
    float NoticeRadius(int setting, int themeRounding, float shapeRadius);
    // Ten-thousandths of the screen, as the spot's two keys hold it.
    int SpotShare(float pixels, float screen);

    class SoundWaits
    {
    public:
        // True starts the kind's wait: kFriendSoundWait for Online and for Offline, each its own; none for an invite.
        bool Due(NoticeKind kind, double now);

    private:
        std::optional<double> m_LastOnline, m_LastOffline;
    };

    // The page's test notices: one of each kind that is on (Show Online, Show Offline, Show Invites as a party
    // invite), an online one when none is; the first room of them, the room Max Notifications leaves.
    std::vector<ShownNotice> NoticeSamples(const Settings& settings, int room);
    // Invite Position: the invites moved to the start of the stack's order or its end, the rest kept in theirs.
    void PlaceInvites(std::vector<ShownNotice>& shown, const Settings& settings);
    // The frame's notices: the stack's shown, then on the Notifications page (page) the samples in the room Max
    // Notifications leaves, the invites among them all put by Invite Position.
    std::vector<ShownNotice> NoticesToShow(std::vector<ShownNotice> shown, const Settings& settings, bool page);
    // The Notifications card's Test: of the kinds that are on, in the enum's order, the one at the seed's remainder
    // by their count; nothing when none is.
    std::optional<NoticeKind> TestKind(uint32_t seed, const Settings& settings);
    // The kind's Sound Effect (a choice of settings.h) and its volume in percent: Master Volume times its Alert Volume.
    int NoticeSoundOf(const Settings& settings, NoticeKind kind);
    int NoticeVolumeOf(const Settings& settings, NoticeKind kind);

    uint32_t NoticeColor(NoticeKind kind);
    // A friend's heading says who logged in or out; the line under it is only for an alt (aka).
    std::string NoticeTitle(NoticeKind kind, std::string_view name);
    std::string NoticeMessage(NoticeKind kind, std::string_view name, std::string_view aka);
}
