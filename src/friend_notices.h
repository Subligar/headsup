#pragma once

#include "friend_list.h"
#include "notices.h"
#include "settings.h"

#include <cstdint>
#include <map>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <vector>

namespace headsup
{
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
}
