#include "notices.h"

#include <algorithm>
#include <cmath>

namespace headsup
{
    bool IsInvite(NoticeKind kind) { return kind == NoticeKind::PartyInvite || kind == NoticeKind::AllianceInvite; }

    float NoticeOpacity(float age, float life) { return std::clamp(1.0f - (age - life) / kNoticeFadeSeconds, 0.0f, 1.0f); }

    float NoticeBarShare(float age, float life) { return std::clamp(1.0f - age / life, 0.0f, 1.0f); }

    uint32_t NoticeStack::Add(NoticeKind kind, std::string name, bool test)
    {
        Notice notice{m_NextId++, kind, std::move(name)};
        if (IsInvite(kind)) (test ? m_TestInvite : m_Invite) = std::move(notice);
        else
        {
            m_Notices.push_back(std::move(notice));
            if (m_Notices.size() > static_cast<size_t>(kMaxNoticeMost)) m_Notices.pop_front();
        }
        return m_NextId - 1;
    }

    std::vector<ShownNotice> NoticeStack::Shown(float seconds, bool held, float life, int most, bool buttons)
    {
        const auto over = [](const Notice& notice, float lasts) { return notice.age >= lasts + kNoticeFadeSeconds; };
        if (m_Invite)
        {
            m_Invite->age += seconds; // the server's clock runs on, held or not
            if (over(*m_Invite, kInviteSeconds)) m_Invite.reset();
        }
        if (m_TestInvite)
        {
            if (!held) m_TestInvite->age += seconds;
            if (over(*m_TestInvite, life)) m_TestInvite.reset();
        }
        for (Notice& notice : m_Notices)
            if (!held) notice.age += seconds;
        std::erase_if(m_Notices, [&](const Notice& notice) { return over(notice, life); });
        while (m_Notices.size() > static_cast<size_t>(std::max(most, 0))) m_Notices.pop_front();

        std::vector<ShownNotice> shown;
        const auto show = [&](const Notice& notice, float lasts) {
            shown.push_back({notice.id, notice.kind, notice.name, NoticeOpacity(notice.age, lasts),
                NoticeBarShare(notice.age, lasts), buttons && IsInvite(notice.kind)});
        };
        if (m_Invite) show(*m_Invite, kInviteSeconds);
        if (m_TestInvite) show(*m_TestInvite, life);
        for (const Notice& notice : m_Notices) show(notice, life);
        return shown;
    }

    bool NoticeStack::Close(uint32_t id)
    {
        if (m_Invite && m_Invite->id == id)
        {
            m_Invite.reset();
            return true;
        }
        if (m_TestInvite && m_TestInvite->id == id) m_TestInvite.reset();
        std::erase_if(m_Notices, [&](const Notice& notice) { return notice.id == id; });
        return false;
    }

    void NoticeStack::EndInvite() { m_Invite.reset(); }

    std::vector<float> NoticeTops(std::span<const float> heights, float anchor, NoticeEdge edge)
    {
        const bool upward = edge == NoticeEdge::Bottom;
        std::vector<float> tops;
        float at = anchor;
        for (const float height : heights)
        {
            tops.push_back(upward ? at - height : at);
            at = upward ? at - height - kNoticeGap : at + height + kNoticeGap;
        }
        return tops;
    }

    float Slid(float at, float target, float seconds)
    {
        const float next = at + (target - at) * std::min(1.0f, kNoticeSlidePerSecond * seconds);
        return std::abs(target - next) <= kNoticeSlideSnap ? target : next;
    }

    float NoticeLeft(float width, HelpAnchor anchor)
    {
        switch (anchor)
        {
        case HelpAnchor::Left: return 0.0f;
        case HelpAnchor::Center: return width / 2.0f;
        case HelpAnchor::Right: return width;
        }
        return 0.0f;
    }

    NoticeSpot NoticeAnchor(int across, int down, HelpAnchor anchor, NoticeEdge edge, float screenWidth,
        float screenHeight, float barBottom, float wide, float firstHeight)
    {
        const auto share = [](int value, float screen) { return static_cast<float>(value) / kCasePlaceWhole * screen; };
        const bool upward = edge == NoticeEdge::Bottom;
        const float x = across != kFishBarUnplaced         ? share(across, screenWidth)
                        : anchor == HelpAnchor::Left   ? kNoticeStartLeft
                        : anchor == HelpAnchor::Right  ? screenWidth - kNoticeStartLeft
                                                       : screenWidth / 2.0f;
        const float y = down != kFishBarUnplaced ? share(down, screenHeight)
                        : upward                 ? screenHeight - kNoticeStartGap
                                                 : barBottom + kNoticeStartGap;
        const float lead   = NoticeLeft(std::min(wide, screenWidth), anchor);
        const float height = std::min(firstHeight, screenHeight);
        return {std::clamp(x, lead, std::max(lead, screenWidth - std::min(wide, screenWidth) + lead)),
            upward ? std::clamp(y, height, screenHeight) : std::clamp(y, 0.0f, screenHeight - height)};
    }

    bool OverAny(std::span<const NoticeRect> boxes, float x, float y)
    {
        return std::ranges::any_of(boxes, [&](const NoticeRect& box) {
            return x >= box.left && x < box.left + box.width && y >= box.top && y < box.top + box.height;
        });
    }

    NoticeSpot Reanchored(const NoticeRect& bounds, HelpAnchor anchor, NoticeEdge edge)
    {
        return {bounds.left + NoticeLeft(bounds.width, anchor),
            edge == NoticeEdge::Top ? bounds.top : bounds.top + bounds.height};
    }

    FittedText FittedSizes(float titleSize, float messageSize, float titleWide, float messageWide, float room,
        float titleTall, float messageTall, float tallRoom)
    {
        const auto narrowed = [&](float size, float wide) { return room > 0.0f && wide > room ? size * room / wide : size; };
        FittedText fit{std::max(narrowed(titleSize, titleWide), kMinNoticeText),
            std::max(narrowed(messageSize, messageWide), kMinNoticeText)};
        const float tall = titleTall * fit.title / titleSize + messageTall * fit.message / messageSize;
        if (tallRoom > 0.0f && tall > tallRoom)
        {
            const float share = tallRoom / tall;
            fit = {std::max(fit.title * share, kMinNoticeText), std::max(fit.message * share, kMinNoticeText)};
        }
        return fit;
    }

    float NoticeRadius(int setting, int themeRounding, float shapeRadius)
    {
        if (setting != kThemeRounding) return static_cast<float>(setting);
        return themeRounding != kShapeRounding ? static_cast<float>(themeRounding) : shapeRadius;
    }

    int SpotShare(float pixels, float screen) { return static_cast<int>(std::lround(pixels / screen * kCasePlaceWhole)); }

    bool SoundWaits::Due(NoticeKind kind, double now)
    {
        if (IsInvite(kind)) return true;
        std::optional<double>& last = kind == NoticeKind::Online ? m_LastOnline : m_LastOffline;
        if (last && now - *last < kFriendSoundWait) return false;
        last = now;
        return true;
    }

    uint32_t NoticeColor(NoticeKind kind)
    {
        if (IsInvite(kind)) return kInviteColor;
        return kind == NoticeKind::Online ? kOnlineColor : kOfflineColor;
    }

    const char* NoticeTitle(NoticeKind kind)
    {
        switch (kind)
        {
        case NoticeKind::Online: return "Friend Online";
        case NoticeKind::Offline: return "Friend Offline";
        case NoticeKind::PartyInvite: return "Party Invite";
        case NoticeKind::AllianceInvite: return "Alliance Invite";
        }
        return "";
    }

    std::string NoticeMessage(NoticeKind kind, std::string_view name)
    {
        switch (kind)
        {
        case NoticeKind::Online: return std::string(name) + " is now online";
        case NoticeKind::Offline: return std::string(name) + " went offline";
        case NoticeKind::PartyInvite: return std::string(name) + " invited you to a party";
        case NoticeKind::AllianceInvite: return std::string(name) + " invited you to an alliance";
        }
        return std::string(name);
    }

    std::vector<ShownNotice> NoticeSamples(const Settings& settings)
    {
        // Ids no stack reaches, so a sample never takes a real notice's place as it slides.
        constexpr uint32_t kOnlineSample = UINT32_MAX, kOfflineSample = UINT32_MAX - 1, kInviteSample = UINT32_MAX - 2;
        std::vector<ShownNotice> samples;
        const bool any = settings.friendNoticeOnline || settings.friendNoticeOffline || settings.partyNotice;
        if (settings.friendNoticeOnline || !any) samples.push_back({kOnlineSample, NoticeKind::Online, kFriendSampleName});
        if (settings.friendNoticeOffline) samples.push_back({kOfflineSample, NoticeKind::Offline, kFriendSampleName});
        if (settings.partyNotice)
            samples.push_back({kInviteSample, NoticeKind::PartyInvite, kInviteSampleName, 1.0f, 1.0f,
                settings.partyNoticeButtons});
        return samples;
    }

    std::optional<NoticeKind> TestKind(uint32_t seed, const Settings& settings)
    {
        std::vector<NoticeKind> on;
        if (settings.friendNoticeOnline) on.push_back(NoticeKind::Online);
        if (settings.friendNoticeOffline) on.push_back(NoticeKind::Offline);
        if (settings.partyNotice) on.push_back(NoticeKind::PartyInvite);
        if (on.empty()) return std::nullopt;
        return on[seed % on.size()];
    }

    int NoticeSoundOf(const Settings& settings, NoticeKind kind)
    {
        if (IsInvite(kind)) return settings.partyNoticeSound;
        return kind == NoticeKind::Online ? settings.friendNoticeOnlineSound : settings.friendNoticeOfflineSound;
    }

    int NoticeVolumeOf(const Settings& settings, NoticeKind kind)
    {
        const int alert = IsInvite(kind) ? settings.partyNoticeVolume : settings.friendNoticeVolume;
        return settings.noticeVolume * alert / kMaxNoticeVolume; // percent of a percent
    }
}
