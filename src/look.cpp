#include "look.h"

#include "game_font.h"

namespace headsup
{
    LineStyle BadgeLineStyle(const Settings& settings)
    {
        return LineStyle{settings.badgeFontName.empty() ? settings.defaultFont : settings.badgeFontName, StyleBit(FontStyle::Bold),
            ToArgb(LineDesign{}.shadowColor.custom), 0.0f, true};
    }

    std::string CombatPopupFont(const Settings& settings)
    {
        if (!settings.combatPopupFont.empty()) return settings.combatPopupFont;
        // GDI sets the popups, and it has no game font.
        return GameFontVariantOf(settings.defaultFont) ? kDefaultFont : settings.defaultFont;
    }

    uint32_t BadgeColor(BadgeKind kind, const Settings& settings)
    {
        switch (kind)
        {
        case BadgeKind::Weak:
        case BadgeKind::WeakRank:
        case BadgeKind::StatusWeak: return ToArgb(settings.weakBadgeColor);
        case BadgeKind::Resist:
        case BadgeKind::ResistRank: return ToArgb(settings.resistBadgeColor);
        case BadgeKind::StatusRank: return ToArgb(settings.rankBadgeColor);
        case BadgeKind::Immune: break;
        }
        return ToArgb(settings.immuneBadgeColor);
    }
}
