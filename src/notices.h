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

    constexpr float kInviteSeconds     = 90.0f; // the time the server gives you to answer an invite
    constexpr float kNoticeFadeSeconds = 0.5f;  // after its time is up
    constexpr int kNoticeSampleCount   = 3;     // the page's test notices: online, offline, invite
    // A real invite and a test one are not counted in Max Notifications.
    constexpr int kNoticeBoxes            = kMaxNoticeMost + 2 + kNoticeSampleCount;
    constexpr float kNoticeWidth          = 300.0f, kNoticeGap = 10.0f, kNoticeBarHeight = 4.0f;
    constexpr float kNoticeSlidePerSecond = 8.0f, kNoticeSlideSnap = 0.5f;
    constexpr float kNoticeStartLeft      = 10.0f, kNoticeStartGap = 10.0f; // unplaced: from the left, and the panel
    constexpr double kFriendSoundWait     = 10.0;                          // seconds
    constexpr uint32_t kOnlineColor = 0xFF33FF33, kOfflineColor = 0xFFCCCCCC, kInviteColor = 0xFF66CCFF;
    constexpr const char* kFriendSampleName = "Test Friend", *kInviteSampleName = "Test Leader";
    constexpr const char* kDeclineWord = "Decline", *kAcceptWord = "Accept";

    // 1 until its time is up, then down to 0 over kNoticeFadeSeconds.
    float NoticeOpacity(float age, float life);
    // 1 down to 0 over its life.
    float NoticeBarShare(float age, float life);

    struct ShownNotice
    {
        uint32_t id = 0; // its own for as long as it shows
        NoticeKind kind = NoticeKind::Online;
        std::string name;
        float opacity = 1.0f, share = 1.0f;
        bool buttons = false; // an invite's Decline and Accept
    };

    class NoticeStack
    {
    public:
        // test: the page's Test made it. An invite takes the place of the invite held of its own sort (real or
        // test), so a Test never costs you a real invite. Its id.
        uint32_t Add(NoticeKind kind, std::string name, bool test = false);
        // seconds: the frame's. held: the pointer is over a notification, which stops every clock but a real
        // invite's. life: Duration, which a real invite swaps for kInviteSeconds. most: Max Notifications, the
        // invites apart. buttons: Show Buttons. The real invite first, then a test one, then the rest oldest first.
        std::vector<ShownNotice> Shown(float seconds, bool held, float life, int most, bool buttons);
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
    // The stack's left and its top edge (bottom edge when upward), kept so the first box, firstHeight tall, is whole
    // on the screen. Unplaced: at the left, just under the top panel, or just over the screen's bottom when upward.
    NoticeSpot NoticeAnchor(int across, int down, bool upward, float screenWidth, float screenHeight, float barBottom,
        float firstHeight);
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
    // invite), an online one when none is.
    std::vector<ShownNotice> NoticeSamples(const Settings& settings);
    // The Notifications card's Test: of the kinds that are on, in the enum's order, the one at the seed's remainder
    // by their count; nothing when none is.
    std::optional<NoticeKind> TestKind(uint32_t seed, const Settings& settings);
    // The kind's Sound Effect (a choice of settings.h) and its volume in percent: Master Volume times its Alert Volume.
    int NoticeSoundOf(const Settings& settings, NoticeKind kind);
    int NoticeVolumeOf(const Settings& settings, NoticeKind kind);

    uint32_t NoticeColor(NoticeKind kind);
    const char* NoticeTitle(NoticeKind kind);
    std::string NoticeMessage(NoticeKind kind, std::string_view name);
}
