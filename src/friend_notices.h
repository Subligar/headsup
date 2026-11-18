#pragma once

#include "friend_list.h"
#include "settings.h"

#include <cstdint>
#include <map>
#include <optional>
#include <deque>
#include <set>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace headsup
{
    enum class NoticeKind : uint8_t
    {
        Online,
        Offline
    };

    struct FriendNews
    {
        NoticeKind kind = NoticeKind::Online;
        uint32_t account = 0;
        std::string name;
        bool operator==(const FriendNews&) const = default;
    };

    // At your sign-on the list shows a friend already online as not playing for a read, then playing: a first read
    // is followed by this long of reads that only record.
    constexpr double kSignOnQuietSeconds = 5.0;

    // Compares each read of the friend list with the one before, by account.
    class FriendWatch
    {
    public:
        // self: your character's server id; now in seconds. A read for another character than the last, the first
        // read after Forget, and every read for kSignOnQuietSeconds after either only record the list. A self of 0
        // (not known yet) is skipped.
        std::vector<FriendNews> Read(uint32_t self, const std::vector<FriendEntry>& entries, double now);
        // Signed off: the next read is a first read.
        void Forget();

    private:
        std::optional<uint32_t> m_Self;
        double m_QuietUntil = 0.0;
        std::map<uint32_t, bool> m_Playing; // by account, as the last read had it
    };

    using FriendMutes = std::set<uint32_t>; // accounts
    // One account a line, ascending.
    std::string FriendMutesText(const FriendMutes& mutes);
    // The lines that are a whole account number; anything else is passed over.
    FriendMutes FriendMutesFrom(std::string_view text);
    // HeadsUp on, the kind's switch on and the friend not muted.
    bool NoticeWanted(const FriendNews& news, const Settings& settings, const FriendMutes& mutes);

    constexpr int kMaxNotices             = 5;
    constexpr float kNoticeWidth          = 300.0f, kNoticeGap = 10.0f, kNoticeBarHeight = 4.0f;
    constexpr float kNoticeFadeShare      = 0.2f; // the last of its time, fading
    constexpr float kNoticeSlidePerSecond = 8.0f, kNoticeSlideSnap = 0.5f;
    constexpr float kNoticeStartLeft      = 10.0f, kNoticeStartGap = 10.0f; // unplaced: from the left, and the panel
    constexpr double kOnlineSoundWait     = 10.0;                          // seconds
    constexpr uint32_t kOnlineColor = 0xFF33FF33, kOfflineColor = 0xFFCCCCCC;

    // 1 for the first 80% of its life, down to 0 at its end.
    float NoticeOpacity(float age, float life);
    // 1 down to 0 over its life.
    float NoticeBarShare(float age, float life);

    struct ShownNotice
    {
        uint32_t id = 0; // its own for as long as it shows
        NoticeKind kind = NoticeKind::Online;
        std::string name;
        float opacity = 1.0f, share = 1.0f;
    };

    class NoticeStack
    {
    public:
        // A sixth pushes the oldest out.
        void Add(NoticeKind kind, std::string name, double now);
        // Oldest first; drops those past their life.
        std::vector<ShownNotice> Shown(double now, float life);

    private:
        struct Notice
        {
            uint32_t id;
            NoticeKind kind;
            std::string name;
            double added;
        };
        std::deque<Notice> m_Notices;
        uint32_t m_NextId = 1;
    };

    // Each notice's top, oldest first. Downward: from the anchor, each under the one before. Upward: the anchor is
    // the stack's bottom edge, the oldest sits on it and each newer one goes above.
    std::vector<float> NoticeTops(std::span<const float> heights, float anchor, bool upward);
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
        // True starts that kind's wait. An offline notice has no sound.
        bool Due(NoticeKind kind, double now);

    private:
        std::optional<double> m_LastOnline;
    };

    constexpr const char* kNoticeSampleName = "Friend"; // the name Move's and Test's notices show
    // What Move shows to drag: a notice of each kind that is on, or an online one when neither is.
    std::vector<ShownNotice> NoticeSamples(const Settings& settings);

    uint32_t NoticeColor(NoticeKind kind);
    const char* NoticeTitle(NoticeKind kind);
    std::string NoticeMessage(NoticeKind kind, std::string_view name);
}
