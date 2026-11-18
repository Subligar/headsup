#include "friend_notices.h"

#include <charconv>

namespace headsup
{
    std::vector<FriendNews> FriendWatch::Read(uint32_t self, const std::vector<FriendEntry>& entries, double now)
    {
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
}
