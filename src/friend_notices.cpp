#include "friend_notices.h"

#include <algorithm>
#include <charconv>
#include <cmath>

namespace headsup
{
    std::vector<FriendNews> FriendWatch::Read(uint32_t self, const std::vector<FriendEntry>& entries, double now)
    {
        if (self == 0) return {};
        if (m_Self != self)
        {
            m_Self       = self;
            m_QuietUntil = now + kSignOnQuietSeconds;
            m_Playing.clear();
        }
        const bool quiet = now < m_QuietUntil;
        std::vector<FriendNews> news;
        std::map<uint32_t, bool> playing;
        for (const FriendEntry& entry : entries)
        {
            playing[entry.account] = entry.playing;
            const auto before      = m_Playing.find(entry.account);
            if (quiet || before == m_Playing.end() || before->second == entry.playing) continue;
            news.push_back({entry.playing ? NoticeKind::Online : NoticeKind::Offline, entry.account, FriendName(entry)});
        }
        m_Playing = std::move(playing);
        return news;
    }

    void FriendWatch::Forget() { m_Self.reset(); }

    std::string FriendMutesText(const FriendMutes& mutes)
    {
        std::string text;
        for (const uint32_t account : mutes) text += std::to_string(account) + '\n';
        return text;
    }

    FriendMutes FriendMutesFrom(std::string_view text)
    {
        constexpr std::string_view kBlank = " \t\r";
        FriendMutes mutes;
        while (!text.empty())
        {
            const size_t end      = text.find('\n');
            std::string_view line = text.substr(0, end);
            text                  = end == std::string_view::npos ? std::string_view{} : text.substr(end + 1);
            const size_t first    = line.find_first_not_of(kBlank);
            if (first == std::string_view::npos) continue;
            line = line.substr(first, line.find_last_not_of(kBlank) - first + 1);
            uint32_t account = 0;
            const auto [at, problem] = std::from_chars(line.data(), line.data() + line.size(), account);
            if (problem == std::errc{} && at == line.data() + line.size() && account > 0) mutes.insert(account);
        }
        return mutes;
    }

    bool NoticeWanted(const FriendNews& news, const Settings& settings, const FriendMutes& mutes)
    {
        const bool kindOn = news.kind == NoticeKind::Online ? settings.friendNoticeOnline : settings.friendNoticeOffline;
        return settings.enabled && kindOn && !mutes.contains(news.account);
    }

    float NoticeOpacity(float age, float life)
    {
        const float fadeStart = life * (1.0f - kNoticeFadeShare);
        if (age <= fadeStart) return 1.0f;
        if (age >= life) return 0.0f;
        return 1.0f - (age - fadeStart) / (life * kNoticeFadeShare);
    }

    float NoticeBarShare(float age, float life) { return std::clamp(1.0f - age / life, 0.0f, 1.0f); }

    void NoticeStack::Add(NoticeKind kind, std::string name, double now)
    {
        m_Notices.push_back({m_NextId++, kind, std::move(name), now});
        if (m_Notices.size() > static_cast<size_t>(kMaxNotices)) m_Notices.pop_front();
    }

    std::vector<ShownNotice> NoticeStack::Shown(double now, float life)
    {
        std::erase_if(m_Notices, [&](const Notice& notice) { return now - notice.added >= life; });
        std::vector<ShownNotice> shown;
        for (const Notice& notice : m_Notices)
        {
            const auto age = static_cast<float>(now - notice.added);
            shown.push_back({notice.id, notice.kind, notice.name, NoticeOpacity(age, life), NoticeBarShare(age, life)});
        }
        return shown;
    }

    std::vector<float> NoticeTops(std::span<const float> heights, float anchor, bool upward)
    {
        std::vector<float> tops;
        float edge = anchor;
        for (const float height : heights)
        {
            tops.push_back(upward ? edge - height : edge);
            edge = upward ? edge - height - kNoticeGap : edge + height + kNoticeGap;
        }
        return tops;
    }

    float Slid(float at, float target, float seconds)
    {
        const float next = at + (target - at) * std::min(1.0f, kNoticeSlidePerSecond * seconds);
        return std::abs(target - next) <= kNoticeSlideSnap ? target : next;
    }

    NoticeSpot NoticeAnchor(int across, int down, bool upward, float screenWidth, float screenHeight, float barBottom,
        float firstHeight)
    {
        const auto share = [](int value, float screen) { return static_cast<float>(value) / kCasePlaceWhole * screen; };
        const float x = across == kFishBarUnplaced ? kNoticeStartLeft : share(across, screenWidth);
        const float y = down != kFishBarUnplaced  ? share(down, screenHeight)
                        : upward                  ? screenHeight - kNoticeStartGap
                                                  : barBottom + kNoticeStartGap;
        const float height = std::min(firstHeight, screenHeight);
        return {std::clamp(x, 0.0f, std::max(0.0f, screenWidth - kNoticeWidth)),
            upward ? std::clamp(y, height, screenHeight) : std::clamp(y, 0.0f, screenHeight - height)};
    }

    int SpotShare(float pixels, float screen) { return static_cast<int>(std::lround(pixels / screen * kCasePlaceWhole)); }

    bool SoundWaits::Due(NoticeKind kind, double now)
    {
        if (kind != NoticeKind::Online || (m_LastOnline && now - *m_LastOnline < kOnlineSoundWait)) return false;
        m_LastOnline = now;
        return true;
    }

    uint32_t NoticeColor(NoticeKind kind) { return kind == NoticeKind::Online ? kOnlineColor : kOfflineColor; }

    const char* NoticeTitle(NoticeKind kind) { return kind == NoticeKind::Online ? "Friend Online" : "Friend Offline"; }

    std::string NoticeMessage(NoticeKind kind, std::string_view name)
    {
        return std::string(name) + (kind == NoticeKind::Online ? " is now online" : " went offline");
    }
}
