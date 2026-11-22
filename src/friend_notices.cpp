#include "friend_notices.h"

#include <algorithm>
#include <cctype>
#include <charconv>

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
            const bool alt = !entry.character.empty() &&
                             !std::ranges::equal(entry.character, entry.added, [](char a, char b) {
                                 return std::tolower(static_cast<unsigned char>(a)) == std::tolower(static_cast<unsigned char>(b));
                             });
            news.push_back({entry.playing ? NoticeKind::Online : NoticeKind::Offline, entry.account, FriendName(entry),
                alt ? entry.added : std::string()});
        }
        m_Playing = std::move(playing);
        return news;
    }

    void FriendWatch::Forget() { m_Self.reset(); }

    std::string IncludedText(const FriendsIncluded& included)
    {
        std::string text = included.none ? std::string(kIncludedNoneLine) + '\n' : std::string();
        for (const uint32_t account : included.out) text += std::to_string(account) + '\n';
        return text;
    }

    FriendsIncluded IncludedFrom(std::string_view text)
    {
        constexpr std::string_view kBlank = " \t\r";
        FriendsIncluded included;
        while (!text.empty())
        {
            const size_t end      = text.find('\n');
            std::string_view line = text.substr(0, end);
            text                  = end == std::string_view::npos ? std::string_view{} : text.substr(end + 1);
            const size_t first    = line.find_first_not_of(kBlank);
            if (first == std::string_view::npos) continue;
            line = line.substr(first, line.find_last_not_of(kBlank) - first + 1);
            if (line == kIncludedNoneLine)
            {
                included.none = true;
                continue;
            }
            uint32_t account = 0;
            const auto [at, problem] = std::from_chars(line.data(), line.data() + line.size(), account);
            if (problem == std::errc{} && at == line.data() + line.size() && account > 0) included.out.insert(account);
        }
        return included;
    }

    bool Includes(const FriendsIncluded& included, uint32_t account) { return !included.none && !included.out.contains(account); }

    bool AllIncluded(const FriendsIncluded& included, std::span<const uint32_t> accounts)
    {
        return std::ranges::all_of(accounts, [&](uint32_t account) { return Includes(included, account); });
    }

    bool ToggleIncluded(FriendsIncluded& included, std::span<const uint32_t> accounts, int row)
    {
        constexpr int kAllRow = 0, kNoneRow = 1, kFirstFriendRow = 2;
        const FriendsIncluded was = included;
        if (row == kAllRow) included = {};
        else if (row == kNoneRow) included = {true, {}};
        else if (row >= kFirstFriendRow && row - kFirstFriendRow < static_cast<int>(accounts.size()))
        {
            const uint32_t account = accounts[static_cast<size_t>(row - kFirstFriendRow)];
            if (included.none) included = {false, {accounts.begin(), accounts.end()}};
            if (!included.out.erase(account)) included.out.insert(account);
        }
        return included != was;
    }

    bool NoticeWanted(const FriendNews& news, const Settings& settings, const FriendsIncluded& included)
    {
        const bool kindOn = news.kind == NoticeKind::Online ? settings.friendNoticeOnline : settings.friendNoticeOffline;
        return settings.enabled && kindOn && Includes(included, news.account);
    }
}
