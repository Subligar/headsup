#include "settings.h"

#include "argb.h"
#include "cursor_picture.h"
#include "default_settings.h"
#include "fonts.h"
#include "game_font.h"
#include "ini_text.h"
#include "pointer_keys.h"
#include "utf16.h"
#include "world_info.h"

#include <algorithm>
#include <cstdlib>
#include <sstream>
#include <cctype>
#include <climits>
#include <cmath>
#include <cstdio>
#include <iterator>
#include <string>
#include <unordered_map>
#include <utility>

namespace headsup
{
    namespace
    {
        struct BoolKey
        {
            const char* key;
            bool Settings::*field;
        };
        constexpr const char* kEnabledKey = "enabled"; // in every file any build saved: what tells a file from none
        constexpr BoolKey kBools[] = {{kEnabledKey, &Settings::enabled}, {"autoSave", &Settings::autoSave}, {"phTimers", &Settings::phTimers},
            {"scaleWithDistance", &Settings::scaleWithDistance},
            {"fixPointer", &Settings::fixPointer}, {"lockCursor", &Settings::lockCursor}, {"countYourAccuracy", &Settings::countAccuracy}, {"showBar", &Settings::showBar},
            {"hideHelpBar", &Settings::hideHelpBar}, {"hideNetstat", &Settings::hideNetstat},
            {"screenshotHidesGame", &Settings::screenshotHidesGame},
            {"stackNames", &Settings::stackNames}, {"stackLine", &Settings::stackLine}, {"hideInEvent", &Settings::hideInEvent},
            {"setupDone", &Settings::setupDone},
            {"debugMode", &Settings::debugMode},
            {"logging", &Settings::logging},
            {"nameplates", &Settings::nameplates}, {"outlines", &Settings::outlines},
            {"outlineFadeIn", &Settings::outlineFadeIn}, {"cursorFloat", &Settings::cursorFloat},
            {"autoCheck", &Settings::autoCheck}, {"lookUpPlayers", &Settings::lookUpPlayers},

            {"fishBarDraggable", &Settings::fishBarDraggable},
            {"fishColorByCatch", &Settings::fishColorByCatch}, {"blueMagic", &Settings::blueMagic},
            {"moveGameHelp", &Settings::moveGameHelp}, {"helpBarBackground", &Settings::helpBarBackground},
            {"helpBarFullWidth", &Settings::helpBarFullWidth}, {"widgetBackground", &Settings::widgetBackground}};

        // Whole numbers are stored as floats, like every other number in settings.ini.
        struct IntKey
        {
            const char* key;
            int Settings::*field;
            int lo, hi;
            int step = 1;
        };
        constexpr IntKey kInts[] = {{"smoothness", &Settings::smoothness, kMinSmoothness, kMaxSmoothness},
            {"outlineThickness", &Settings::outlineThickness, kMinOutlineThickness, kMaxOutlineThickness, kPercentStep},
            {"outlineOpacity", &Settings::outlineOpacity, kMinOpacity, kMaxOpacity, kPercentStep},
            {"menuOpacity", &Settings::menuOpacity, kMinMenuOpacity, kMaxOpacity, kPercentStep},
            {"pointerSize", &Settings::pointerSize, kMinPointerSize, kMaxPointerSize, kPercentStep},
            {"cursorRaise", &Settings::cursorRaise, -kMaxCursorRaise, kMaxCursorRaise},
            {"nameRaise", &Settings::nameRaise, kMinNameRaise, kMaxNameRaise},
            {"elementWeakRank", &Settings::elementWeakRank, kNoWeakRank, kMaxWeakRank},
            {"elementResistRank", &Settings::elementResistRank, kMinResistRank, kNoResistRank},
            {"maxElements", &Settings::maxElements, 0, kElementCount},
            {"barHeight", &Settings::barHeight, kMinBarHeight, kMaxBarHeight},
            {"barIconScale", &Settings::barIconScale, kMinIconScale, kMaxIconScale, kPercentStep},
            {"barWidgetSpacing", &Settings::barWidgetSpacing, 0, kMaxWidgetSpacing},
            {"barWidgetPadding", &Settings::barWidgetPadding, kThemePadding, kMaxWidgetPadding},
            {"widgetRounding", &Settings::widgetRounding, kThemeRounding, kMaxRounding},
            {"popupRounding", &Settings::popupRounding, kThemeRounding, kMaxRounding},
            {"sectionRounding", &Settings::sectionRounding, kThemeRounding, kMaxRounding},
            {"helpBarRounding", &Settings::helpBarRounding, kThemeRounding, kMaxRounding},
            {"panelSeparators", &Settings::panelSeparators, 0, kAllSeparators},
            {"helpBarTextSize", &Settings::helpBarTextSize, kAutoBarTextSize, kMaxBarTextSize},
            {"helpBarWidth", &Settings::helpBarWidth, kMinHelpBarWidth, kMaxHelpBarWidth},
            {"helpBarHeight", &Settings::helpBarHeight, kAutoHelpBarHeight, kMaxHelpBarHeight},
            {"fishBarHeight", &Settings::fishBarHeight, kMinFishBarHeight, kMaxFishBarHeight},
            {"fishBarTextSize", &Settings::fishBarTextSize, kMinFishTextSize, kMaxFishTextSize},
            {"baseSize", &Settings::baseSize, kMinTextSize, kMaxTextSize},
            {"baseIconSize", &Settings::baseIconSize, kMinTextSize, kMaxTextSize},
            {"pearlOutline", &Settings::pearlOutline, 0, kMaxPearlOutline, kPercentStep},
            {"scaleMin", &Settings::scaleMin, kMinSizePercent, kMaxSizePercent, kPercentStep},
            {"scaleMax", &Settings::scaleMax, kMinSizePercent, kMaxSizePercent, kPercentStep},
            {"timerPercent", &Settings::timerPercent, kMinSizePercent, kMaxSizePercent, kPercentStep},
            {"phUpMinutes", &Settings::phUpMinutes, 0, kMaxUpMinutes},
            {"screenshotKey", &Settings::screenshotKey, 0, UINT8_MAX},
            {"menuX", &Settings::menuX, -kMaxMenuSide, kMaxMenuSide}, // dragged partly off the screen's left
            {"menuY", &Settings::menuY, 0, kMaxMenuSide}, {"menuWidth", &Settings::menuWidth, 0, kMaxMenuSide},
            {"menuHeight", &Settings::menuHeight, 0, kMaxMenuSide},
            {"fishBarWidth", &Settings::fishBarWidth, kMinFishBarWidth, kMaxFishBarWidth},
            {"fishBarX", &Settings::fishBarX, kFishBarUnplaced, kMaxFishBarPlace},
            {"fishBarY", &Settings::fishBarY, kFishBarUnplaced, kMaxFishBarPlace},
            {"helpBarX", &Settings::helpBarX, -kMaxHelpBarOffset, kMaxHelpBarOffset},
            {"helpBarY", &Settings::helpBarY, -kMaxHelpBarOffset, kMaxHelpBarOffset}};

        // Numbers whose meaning changed and whose key changed with it: saved and bounded as the rest, and read through
        // Rebased.
        constexpr const char* kCursorWidthKey = "cursorWidth", *kCursorHeightKey = "cursorHeight";
        constexpr const char* kCursorEdgeKey  = "cursorOutlineThickness";
        constexpr const char* kMenuFontSizeKey = "menuFontSize";
        constexpr const char* kOldMenuScaleKey = "menuScale"; // a percent of Ashita's own font size
        constexpr float kOldMinMenuScale = 70.0f, kOldMaxMenuScale = 160.0f; // percent, the old key's range
        // Ashita's own font size, which the window's old size was a percent of: GetFontSize() in the game, 2026-10-14.
        constexpr float kAshitaFontPixels = 18.0f;
        constexpr IntKey kRebasedInts[] = {{kCursorWidthKey, &Settings::cursorWidth, kMinCursorSide, kMaxCursorSide},
            {kMenuFontSizeKey, &Settings::menuFontSize, kMinMenuFontSize, kMaxMenuFontSize},
            {kCursorHeightKey, &Settings::cursorHeight, kMinCursorSide, kMaxCursorSide},
            {kCursorEdgeKey, &Settings::cursorOutlineThickness, kMinCursorEdge, kMaxCursorEdge, kPercentStep}};
        constexpr const char* kCursorStyleKey = "cursorStyle";
        constexpr const char* kCursorFileKey  = "cursorFile";
        const char* const kCursorStyleNames[kCursorStyleCount] = {"vanilla", "triangle", "feather", "chocobo", "file"};
        constexpr const char* kPointerIconKey        = "pointerIcon";
        constexpr const char* kPointerFileKey        = "pointerFile";
        constexpr const char* kOldChocoboPointerKey  = "chocoboPointer";
        const char* const kPointerIconNames[kPointerIconCount] = {"vanilla", "chocobo", "file"};
        // Before it had a style the cursor was on or off, and Phoenix's feather or an arrow.
        constexpr const char* kOldReplaceCursorKey = "replaceCursor";
        constexpr const char* kOldCursorFeatherKey = "cursorFeather";
        constexpr const char* kOldCursorSizeKey    = "cursorSize";    // its height in pixels, its width its shape's
        constexpr const char* kOldCursorOutlineKey = "cursorOutline"; // percent, of twice the edge
        constexpr float kCursorEdgeRebase          = 2.0f;
        constexpr std::string_view kPathCharacters = "\\/:";

        struct FloatKey
        {
            const char* key;
            float Settings::*field;
            float lo, hi;
        };
        constexpr FloatKey kFloats[] = {{"maxDistance", &Settings::maxDistance, kMinOutlineDistance, kMaxOutlineDistance},
            {"barFontRatio", &Settings::barFontRatio, kMinFontRatio, kMaxFontRatio},
            {"popupTextScale", &Settings::popupTextScale, kMinPopupScale, kMaxPopupScale},
            {"popupWidthScale", &Settings::popupWidthScale, kMinPopupBoxScale, kMaxPopupScale},
            {"popupHeightScale", &Settings::popupHeightScale, kMinPopupBoxScale, kMaxPopupScale}};

        struct ColorKey
        {
            const char* key;
            Color Settings::*field;
        };
        constexpr ColorKey kColors[] = {{"weakBadgeColor", &Settings::weakBadgeColor},
            {"resistBadgeColor", &Settings::resistBadgeColor}, {"rankBadgeColor", &Settings::rankBadgeColor},
            {"immuneBadgeColor", &Settings::immuneBadgeColor}, {"fishSmallColor", &Settings::fishSmallColor},
            {"fishLargeColor", &Settings::fishLargeColor}, {"fishItemColor", &Settings::fishItemColor},
            {"fishMonsterColor", &Settings::fishMonsterColor}, {"fishLineColor", &Settings::fishLineColor},
            {"fishRodColor", &Settings::fishRodColor}, {"blueMagicColor", &Settings::blueMagicColor}};
        // The edge's custom color is under the keys the edge's one color had. The cursor's own cannot be under
        // cursorColor, where a file from before the designer holds the target's color.
        constexpr const char* kCursorEdgePickKey = "cursorEdgeColor";
        constexpr const char* kCursorPickKey     = "cursorPick";

        constexpr const char* kBadgeFontKey = "badgeFontName";
        constexpr const char* kRankDisplayKey = "rankDisplay";
        const char* const kRankDisplayNames[kRankDisplayCount] = {"rank", "increased", "total"};
        // Before the chances a badge's rank read as the skillchain table's percent unless this was on.
        constexpr const char* kOldShowRanksKey = "showRanks";
        constexpr const char* kBarTitleFontKey = "barTitleFont";
        constexpr const char* kBarTextFontKey  = "barTextFont";
        // Saved files from before the cursor scaled on its own, when it scaled with the names, take the names'.
        constexpr const char* kScaleCursorKey = "scaleCursor";
        constexpr const char* kKeepPointerKey = "keepPointerKeys"; // hex virtual-key codes, a space apart
        constexpr const char* kCurrencyFavoritesKey = "currencyFavorites"; // kCurrencyTable's keys, a space apart
        constexpr const char* kCollapsedKey = "popupCollapsed"; // the collapsed pop-out cards' keys, a space apart
        constexpr const char* kStatusFavoritesKey = "statusFavorites", *kStatusPinsKey = "statusPins"; // status ids

        std::string SpaceApart(const std::vector<std::string>& words)
        {
            std::string text;
            for (const std::string& word : words)
                text += (text.empty() ? "" : " ") + word;
            return text;
        }
        std::string SpaceApart(const std::vector<uint16_t>& ids)
        {
            std::vector<std::string> words;
            for (const uint16_t id : ids)
                words.push_back(std::to_string(id));
            return SpaceApart(words);
        }
        // Status ids a space apart, as SpaceApart writes them: one Phoenix lacks, or one repeated, is left out.
        std::vector<uint16_t> StatusIds(const std::string& text)
        {
            std::vector<uint16_t> ids;
            std::istringstream words(text);
            for (std::string word; words >> word;)
            {
                char* end              = nullptr;
                const unsigned long id = std::strtoul(word.c_str(), &end, 10);
                if (*end == '\0' && id <= UINT16_MAX && *StatusName(static_cast<uint16_t>(id)) != '\0' &&
                    std::ranges::find(ids, static_cast<uint16_t>(id)) == ids.end())
                    ids.push_back(static_cast<uint16_t>(id));
            }
            return ids;
        }
        constexpr const char* kPinnedWindowsKey = "pinnedWindows"; // "equip:100,200 statcom:0,40"
        constexpr const char* kHiddenWindowsKey = "hiddenWindows"; // "statcom inventor"
        constexpr const char* kShiftVanillaUiKey = "shiftVanillaUi";
        // By HelpMode.
        constexpr const char* kPlacedUnderKeys[kHelpModes] = {"placedUnderHelpOff", "placedUnderHelpOn"};
        constexpr const char* kViewShiftsKeys[kHelpModes]  = {"viewShiftsHelpOff", "viewShiftsHelpOn"}; // "status:20"
        constexpr int kHex                    = 16;
        // A file's first save writes every key it lacks, so it keeps the defaults of its day, and each default that
        // changed is put right once in files from before. 1: the target cursor's became Phoenix's red. 2: the font's
        // became the bundled Marcellus SC. 3: the nameplate designer's settings took the place of the fixed plate's.
        // 4: the review's settings. A number whose meaning changed there has a new key, read through Rebased, and its
        // old key is never written again: the build before goes on reading its own.
        constexpr const char* kVersionKey = "settingsVersion";
        constexpr int kRedCursorVersion    = 1;
        constexpr int kBundledFontVersion  = 2;
        constexpr int kDesignerVersion     = 3;
        constexpr int kReviewVersion       = 4;
        constexpr int kSettingsVersion     = kReviewVersion;
        // Saved with the designer's keys. An older build's save takes the version back to its own and leaves this and
        // those keys in the file, so a file that has it is not migrated over them.
        constexpr const char* kDesignsSavedKey = "nameplateDesigns";
        constexpr const char* kBarThemeKey = "barTheme";
        // The Custom theme: this before each of kPaletteColors' names, and its shape and border.
        constexpr const char* kCustomThemeKey = "customTheme";
        constexpr const char* kShapeSuffix = "Shape";
        constexpr const char* kBorderSuffix = "Border";
        constexpr const char* kBackgroundSuffix = "Background";
        constexpr const char* kWindowTypeSuffix = "WindowType";
        constexpr const char* kRoundingSuffix = "Rounding";
        constexpr const char* kSeparatorSuffix = "Separator";
        constexpr const char* kCustomThemeNameKey = "customThemeName";
        constexpr const char* kBarCurrencyKey = "barCurrency";
        constexpr const char* kBarSecondCurrencyKey = "barCurrency2";
        constexpr const char* kBarBagKey      = "barBag";
        constexpr const char* kWeatherIconsKey = "weatherIcons"; // the Forecast widget's, by kCustomIconsNames
        const char* const kCustomIconsNames[kCustomIconsCount] = {"never", "missing", "always"};
        constexpr const char* kWorldRouteKey    = "barWorldRoute";
        constexpr const char* kWorldGuildKey    = "barWorldGuild";
        constexpr const char* kTrue                = "true";
        constexpr const char* kAshitaTrue          = "1";
        constexpr const char* kClockZoneKey       = "clockZone"; // the Clock widget's: a kClockZoneNames or kLocalZone
        constexpr const char* kClockSecondZoneKey = "clockSecondZone"; // its Secondary Timezone's, the same way
        constexpr const char* kLocalZone          = "local";
        constexpr const char* kTargetDetectionKey = "barTargetDetection";
        constexpr const char* kTargetShowsKey    = "barTargetShows";
        constexpr const char* kMenuFontKey      = "menuFont";
        constexpr const char* kBlueMagicLearnedKey = "blueMagicLearned";
        constexpr const char* kBlueLearnedNames[kBlueLearnedCount] = {"star", "plain"};
        constexpr const char* kFishBarFontKey = "fishBarFont", *kFishBarLineKey = "fishBarLine";
        constexpr const char* kDesignSuffix     = "Design";
        constexpr const char* kCombineSuffix    = "Combine";
        constexpr const char* kLinkPaddingSuffix = "LinkPadding";
        constexpr const char* kIconSideSuffix    = "Icon";
        constexpr const char* kAlignSuffix       = "Align";
        constexpr const char* kPopupSpotSuffix   = "PopupSpot"; // "left 10 top 200" or "right 120 bottom 80"
        constexpr const char* kSpotLeft = "left", *kSpotRight = "right", *kSpotTop = "top", *kSpotBottom = "bottom";
        const char* const kIconSideNames[kIconSideCount] = {"hide", "left", "right"};
        const char* const kWidgetAlignNames[kWidgetAlignCount] = {"location", "left", "center", "right"};
        // The EXP widget's: a place for each ExpText, and its bar's size.
        constexpr const char* kExpBarWidthKey = "barExpBarWidth", *kExpBarHeightKey = "barExpBarHeight";
        constexpr const char* kExpBarTextKey  = "barExpBarText";
        constexpr const char* kNoExpBarText   = "none";
        const char* const kExpTextNames[kExpTextCount] = {"earned", "toNext", "percent"};
        constexpr const char* kJobNamesKey  = "jobNames";
        constexpr const char* kZoneIdKey    = "zoneId";
        constexpr const char* kMemTextKey   = "memText";
        constexpr const char* kJobTextKey   = "jobText";
        const char* const kJobTextNames[kJobTextCount] = {"mainSub", "main", "levelFirst"};
        constexpr const char* kClock24Key   = "clock24";
        constexpr const char* kClockSecondsKey = "clockSeconds";
        constexpr const char* kDateFormatKey   = "clockDateFormat";
        const char* const kDateFormatNames[kDateFormatCount] = {"ymd", "monthDayYear", "dayMonthYear", "monthDay", "iso",
            "monthDayCommaYear", "shortMonthDay"};
        const char* const kClockZoneNames[kClockZoneCount] = {"japan", "utc", "usEastern", "usCentral", "usMountain",
            "usArizona", "usPacific", "usAlaska", "usHawaii", "ukIreland", "centralEurope", "easternEurope",
            "australiaEastern"};
        constexpr const char* kFishStaminaKey                   = "fishStamina";
        const char* const kFishStaminaNames[kFishStaminaCount]  = {"hide", "vanilla", "headsup"};
        constexpr const char* kHelpAnchorKey                    = "helpBarAnchor";
        const char* const kHelpAnchorNames[kHelpAnchorCount]    = {"left", "center", "right"};
        const char* const kBarWidgetKeys[] = {"barZone", "barWeather", "barTarget", "barGil", "barBags",
            "barEarthTime", "barGameHelp", "barPerformance", "barJob", "barParty", "barStatusTimers", "barWorld"};
        static_assert(std::size(kBarWidgetKeys) == kBarWidgetCount);
        const char* const kBarSectionKeys[] = {"barLeft", "barCenter", "barRight"};
        static_assert(std::size(kBarSectionKeys) == kBarSectionCount);
        const char* const kChannelKeys[3]               = {"R", "G", "B"};
        // By StatusRank, each followed by kStatusShowSuffix and the cutoffs' suffixes.
        const char* const kStatusKeys[kStatusRankCount] = {"paralyze", "bind", "silence", "slow", "poison", "lightSleep",
            "darkSleep", "blind", "stun", "gravity"};
        constexpr const char* kStatusShowSuffix   = "Badge";
        constexpr const char* kWeakRankSuffix     = "WeakRank";
        constexpr const char* kResistRankSuffix   = "ResistRank";
        const char* const kLabelShadeKeys[kLabelShadeCount] = {"labelUnknown", "labelTooWeak", "labelEasyPrey",
            "labelDecentChallenge", "labelEvenMatch", "labelTough", "labelVeryTough", "labelIncrediblyTough"};

        const char* const kPlateModeNames[kPlateModeCount]       = {"hidden", "shown", "whenTargeted", "vanilla"};
        // A mode that is gone: its name alone, and the whole nameplate for a target. Such a row is Shown.
        constexpr const char* kExpandingName = "expanding";
        const char* const kOutlineModeNames[kOutlineModeCount]   = {"disabled", "shown", "whenTargeted", "byNature"};
        const char* const kSourceNames[kColorSourceCount]        = {"custom", "game", "aggression", "toughness", "claim", "target", "type"};
        const char* const kAlignNames[kAlignCount]               = {"left", "center", "right"};
        const char* const kOrnamentPlaceNames[kOrnamentPlaceCount] = {"disabled", "top", "aboveMiddle", "belowMiddle", "bottom"};
        // Before it had a place over or under the plate, an ornament was a switch and one of two places between lines.
        constexpr const char* kOldOrnamentOnSuffix    = "On";
        constexpr const char* kOldOrnamentPlaceSuffix = "Place";
        constexpr const char* kOldOrnamentUpperPlace  = "lines12";
        const char* const kNatureKeys[kNatureCount] = {"passive", "aggressive", "passiveNm", "aggressiveNm", "placeholder", "noData"};
        const char* const kClaimKeys[kClaimStateCount]   = {"unclaimed", "others", "yours", "calledForHelp"};
        const char* const kTargetKeys[kTargetStateCount] = {"none", "targeted", "locked", "subTarget", "outOfRange"};
        const char* const kTypeKeys[kTargetTypeCount]    = {"mob", "player", "friend", "partyMember", "linkshell", "npc", "pet"};
        const char* const kPaletteKeys[]                 = {"paletteAggression", "paletteToughness", "paletteClaim", "paletteTarget", "paletteType"};
        constexpr const char* kLabelShadePrefix = "label"; // kLabelShadeKeys' prefix, which the palette's toughness keys drop
        constexpr const char* kRowInfix         = "Row";
        constexpr const char* kPlateOpacityBase = "plateOpacity";
        constexpr const char* kGlowOffSuffix    = "GlowOffInCombat";
        constexpr const char* kOldUntargetedKey = "untargetedOpacity";   // every mob's that was not your target
        constexpr const char* kOldGlowOffKey    = "glowOffWhenFighting"; // every line of a mob your party had claimed
        constexpr const char* kOldPetRow        = "Pet"; // the pets' one row of before, after kRowInfix
        constexpr const char* kLineInfix        = "Line";
        constexpr const char* kOrnamentInfix    = "Ornament";
        // A design's keys after their base, a row's, a line's, an ornament's or the opacity's: read and written by
        // these names alone.
        constexpr const char *kPlateKey = "Plate", *kOutlineKey = "Outline", *kColorKey = "Color";
        constexpr const char *kTextKey = "Text", *kSyncKey = "Sync", *kAlwaysKey = "Always", *kWhenKey = "When",
                             *kAlignKey = "Align", *kFontKey = "Font", *kStyleKey = "Style", *kBoldKey = "Bold",
                             *kTextSizeKey = "TextSize", *kIconScaleKey = "IconScale", *kIconSizeKey = "IconSize",
                             *kPaddingKey = "Padding", *kOffsetKey = "Offset", *kShadowStrengthKey = "ShadowStrength", *kShadowKey = "Shadow",
                             *kShadowColorKey = "ShadowColor", *kGlowKey = "Glow", *kGlowSizeKey = "GlowSize",
                             *kGlowColorKey = "GlowColor";
        constexpr const char *kVisibilityKey = "Visibility", *kWidthKey = "Width", *kHeightKey = "Height";
        constexpr const char *kNoTargetKey = "NoTarget", *kNotTargetedKey = "NotTargeted", *kTargetedKey = "Targeted",
                             *kFadeInKey = "FadeIn", *kRangeKey = "Range";
        constexpr const char* kMaxIconsSuffix   = "MaxIcons";
        constexpr const char* kDefaultFontKey   = "defaultFont";
        constexpr const char* kDetectionShownKey = "detectionShown";

        float ClampFloat(float v, float lo, float hi, float fallback)
        {
            return std::isfinite(v) ? std::clamp(v, lo, hi) : fallback;
        }

        Color ClampColor(Color c, const Color& fallback)
        {
            for (int i = 0; i < 3; ++i)
                c.v[i] = ClampFloat(c.v[i], 0.0f, 1.0f, fallback.v[i]);
            return c;
        }

        // What a key the store lacks reads as: no line of a file holds a line break, so it differs from every text.
        constexpr const char* kNoText = "\n";
        // A key's number as the store holds it; nothing for a key it lacks or that holds no number.
        std::optional<float> HeldNumber(SettingsStore& store, const std::string& key)
        {
            const std::string text = store.GetString(key.c_str(), kNoText);
            char* end              = nullptr;
            const float v          = std::strtof(text.c_str(), &end);
            return text == kNoText || end == text.c_str() || !std::isfinite(v) ? std::nullopt : std::optional<float>(v);
        }

        // A switch as the store holds it; nothing for a key it lacks. HeadsUp writes true or false, and Ashita wrote 1
        // or 0 for a key that the typed reads of builds before added.
        std::optional<bool> HeldSwitch(SettingsStore& store, const std::string& key)
        {
            const std::string text = store.GetString(key.c_str(), kNoText);
            if (text == kNoText) return std::nullopt;
            return text == kAshitaTrue || std::ranges::equal(text, std::string_view(kTrue), [](char a, char b) {
                return std::tolower(static_cast<unsigned char>(a)) == b;
            });
        }

        void LoadColor(SettingsStore& store, const std::string& base, Color& c)
        {
            for (int i = 0; i < 3; ++i)
                c.v[i] = HeldNumber(store, base + kChannelKeys[i]).value_or(c.v[i]);
        }

        // Saturates rather than overflowing, so Clamp then takes a huge value to the nearest limit.
        int Whole(float v) { return static_cast<int>(std::clamp<double>(std::round(v), INT_MIN, INT_MAX)); }

        int LoadInt(SettingsStore& store, const std::string& key, int fallback)
        {
            const std::optional<float> held = HeldNumber(store, key);
            return held ? Whole(*held) : fallback;
        }

        // A number whose meaning changed: the file's own under its new key, else what the value under its old key
        // comes to, so a look from before is kept, else the default.
        template <typename Convert>
        int Rebased(SettingsStore& store, const std::string& key, const std::string& oldKey, int fallback, Convert convert)
        {
            if (const std::optional<float> held = HeldNumber(store, key)) return Whole(*held);
            if (const std::optional<float> old = HeldNumber(store, oldKey)) return Whole(convert(*old));
            return fallback;
        }

        // A shadow of the unit before, where 100 was the mockup's shadow, as a strength of this one.
        float ShadowStrengthOf(float before) { return before / kShadowUnit; }

        std::string Capitalized(const char* key)
        {
            std::string text = key;
            if (!text.empty()) text[0] = static_cast<char>(std::toupper(static_cast<unsigned char>(text[0])));
            return text;
        }

        int NameIndex(const char* const* names, int count, const std::string& name, int fallback)
        {
            for (int i = 0; i < count; ++i)
                if (name == names[i]) return i;
            return fallback;
        }

        void StoreFloat(SettingsStore& store, const std::string& key, float v)
        {
            char text[32];
            std::snprintf(text, sizeof(text), "%.4f", v);
            // A value the four places do not hold, such as a color made from a hex code, keeps its precision.
            if (std::strtof(text, nullptr) != v) std::snprintf(text, sizeof(text), "%.9g", v);
            store.Set(key.c_str(), text);
        }

        void StoreColor(SettingsStore& store, const std::string& base, const Color& c)
        {
            for (int i = 0; i < 3; ++i)
                StoreFloat(store, base + kChannelKeys[i], c.v[i]);
        }

        void StoreBool(SettingsStore& store, const std::string& key, bool v) { store.Set(key.c_str(), v ? kTrue : "false"); }

        uint32_t LoadHex(SettingsStore& store, const std::string& key, uint32_t fallback)
        {
            const std::string text = store.GetString(key.c_str(), "");
            char* end              = nullptr;
            const unsigned long v  = std::strtoul(text.c_str(), &end, kHex);
            return text.empty() || *end != '\0' ? fallback : static_cast<uint32_t>(v);
        }

        void StoreHex(SettingsStore& store, const std::string& key, uint32_t v)
        {
            char text[16];
            std::snprintf(text, sizeof(text), "%X", v);
            store.Set(key.c_str(), text);
        }

        void LoadPick(SettingsStore& store, const std::string& base, ColorPick& pick)
        {
            pick.source = static_cast<ColorSource>(
                NameIndex(kSourceNames, kColorSourceCount, store.GetString((base + "Source").c_str(), ""), static_cast<int>(pick.source)));
            LoadColor(store, base, pick.custom);
        }

        void SavePick(SettingsStore& store, const std::string& base, const ColorPick& pick)
        {
            store.Set((base + "Source").c_str(), kSourceNames[static_cast<int>(pick.source)]);
            StoreColor(store, base, pick.custom);
        }

        // Where each palette's entries live in Settings::palette, by key.
        struct PaletteEntry
        {
            std::string key;
            Color* color;
        };
        std::vector<PaletteEntry> PaletteEntries(Palette& p)
        {
            std::vector<PaletteEntry> entries;
            auto add = [&](const char* palette, const std::string& entry, Color& color) {
                entries.push_back({std::string(palette) + entry, &color});
            };
            for (int i = 0; i < kNatureCount; ++i)
                add(kPaletteKeys[0], Capitalized(kNatureKeys[i]), p.aggression[i]);
            for (int i = 0; i < kLabelShadeCount; ++i)
                add(kPaletteKeys[1], std::string(kLabelShadeKeys[i]).substr(std::string(kLabelShadePrefix).size()), p.toughness[i]);
            for (int i = 0; i < kClaimStateCount; ++i)
                add(kPaletteKeys[2], Capitalized(kClaimKeys[i]), p.claim[i]);
            for (int i = 0; i < kTargetStateCount; ++i)
                add(kPaletteKeys[3], Capitalized(kTargetKeys[i]), p.target[i]);
            for (int i = 0; i < kTargetTypeCount; ++i)
                add(kPaletteKeys[4], Capitalized(kTypeKeys[i]), p.type[i]);
            return entries;
        }

        std::string LineBase(PlateKind kind, int line) { return std::string(KindKey(kind)) + kLineInfix + std::to_string(line + 1); }

        // iconShare: what a file from before, whose icon sizes were percents of its base font size, has them
        // multiplied by to be percents of the base icon size.
        void LoadLine(SettingsStore& store, const std::string& base, LineDesign& line, float iconShare)
        {
            auto key = [&](const char* name) { return base + name; };
            line.text     = store.GetString(key(kTextKey).c_str(), line.text.c_str());
            line.syncWith = LoadInt(store, key(kSyncKey), line.syncWith);
            line.show.always = HeldSwitch(store, key(kAlwaysKey)).value_or(line.show.always);
            line.show.when   = LoadHex(store, key(kWhenKey), line.show.when);
            line.align = static_cast<Align>(NameIndex(kAlignNames, kAlignCount, store.GetString(key(kAlignKey).c_str(), ""), static_cast<int>(line.align)));
            line.font     = store.GetString(key(kFontKey).c_str(), line.font.c_str());
            // Bold was a switch of its own before a line had styles.
            if (store.GetString(key(kStyleKey).c_str(), kNoText) != kNoText)
                line.style = static_cast<uint8_t>(LoadHex(store, key(kStyleKey), line.style));
            else if (HeldSwitch(store, key(kBoldKey)).value_or(false))
                line.style = StyleBit(FontStyle::Bold);
            line.textSize = LoadInt(store, key(kTextSizeKey), line.textSize);
            line.iconSize = Rebased(store, key(kIconScaleKey), key(kIconSizeKey), line.iconSize,
                [&](float before) { return before * iconShare; });
            line.padding = LoadInt(store, key(kPaddingKey), line.padding);
            line.offset  = LoadInt(store, key(kOffsetKey), line.offset);
            LoadPick(store, key(kColorKey), line.color);
            line.shadow = Rebased(store, key(kShadowStrengthKey), key(kShadowKey), line.shadow, ShadowStrengthOf);
            LoadPick(store, key(kShadowColorKey), line.shadowColor);
            line.glow     = LoadInt(store, key(kGlowKey), line.glow);
            line.glowSize = LoadInt(store, key(kGlowSizeKey), line.glowSize);
            LoadPick(store, key(kGlowColorKey), line.glowColor);
        }

        void SaveLine(SettingsStore& store, const std::string& base, const LineDesign& line)
        {
            auto key = [&](const char* name) { return base + name; };
            store.Set(key(kTextKey).c_str(), line.text.c_str());
            StoreFloat(store, key(kSyncKey), static_cast<float>(line.syncWith));
            StoreBool(store, key(kAlwaysKey), line.show.always);
            StoreHex(store, key(kWhenKey), line.show.when);
            store.Set(key(kAlignKey).c_str(), kAlignNames[static_cast<int>(line.align)]);
            store.Set(key(kFontKey).c_str(), line.font.c_str());
            StoreHex(store, key(kStyleKey), line.style);
            StoreFloat(store, key(kTextSizeKey), static_cast<float>(line.textSize));
            StoreFloat(store, key(kIconScaleKey), static_cast<float>(line.iconSize));
            StoreFloat(store, key(kPaddingKey), static_cast<float>(line.padding));
            StoreFloat(store, key(kOffsetKey), static_cast<float>(line.offset));
            SavePick(store, key(kColorKey), line.color);
            StoreFloat(store, key(kShadowStrengthKey), static_cast<float>(line.shadow));
            SavePick(store, key(kShadowColorKey), line.shadowColor);
            StoreFloat(store, key(kGlowKey), static_cast<float>(line.glow));
            StoreFloat(store, key(kGlowSizeKey), static_cast<float>(line.glowSize));
            SavePick(store, key(kGlowColorKey), line.glowColor);
            StoreBool(store, key(kGlowOffSuffix), line.glowOffInCombat);
        }

        std::string RowBase(PlateKind kind, int row)
        {
            return std::string(KindKey(kind)) + kRowInfix + Capitalized(RowKey(kind, row));
        }

        void LoadKind(SettingsStore& store, PlateKind kind, KindSettings& k, float iconShare)
        {
            for (int r = 0; r < RowCountOf(kind); ++r)
            {
                // Pets had one row before they had four: a row the file has no keys for takes that row's.
                std::string base = RowBase(kind, r);
                if (kind == PlateKind::Pets && store.GetString((base + kPlateKey).c_str(), kNoText) == kNoText)
                    base = std::string(KindKey(kind)) + kRowInfix + kOldPetRow;
                RowSettings& row       = k.rows[r];
                const std::string plate = store.GetString((base + kPlateKey).c_str(), "");
                row.plate = plate == kExpandingName ? PlateMode::Shown
                                                    : static_cast<PlateMode>(NameIndex(kPlateModeNames, kPlateModeCount, plate, static_cast<int>(row.plate)));
                row.outline = static_cast<OutlineMode>(NameIndex(kOutlineModeNames, kOutlineModeCount, store.GetString((base + kOutlineKey).c_str(), ""), static_cast<int>(row.outline)));
                LoadPick(store, base + kColorKey, row.outlineColor);
            }
            // Dead mobs had no row before: each went by the row it had alive. Where a file's rows all agree, as
            // with every mob on the game's own names, the dead keep that; rows that differ say nothing for them.
            const int dead = static_cast<int>(Category::Dead);
            if (kind == PlateKind::Mobs && store.GetString((RowBase(kind, dead) + kPlateKey).c_str(), kNoText) == kNoText)
            {
                const auto differs = [&](const RowSettings& row) { return row.plate != k.rows[0].plate; };
                if (std::none_of(k.rows, k.rows + dead, differs)) k.rows[dead].plate = k.rows[0].plate;
            }
            for (int n = 0; n < kPlateLines; ++n)
                LoadLine(store, LineBase(kind, n), k.lines[n], iconShare);
            const std::string base = std::string(KindKey(kind)) + kOrnamentInfix;
            OrnamentSettings& o    = k.ornament;
            const std::string visibility  = store.GetString((base + kVisibilityKey).c_str(), kNoText);
            const std::optional<bool> wasOn = HeldSwitch(store, base + kOldOrnamentOnSuffix);
            if (visibility != kNoText)
                o.place = static_cast<OrnamentPlace>(NameIndex(kOrnamentPlaceNames, kOrnamentPlaceCount, visibility, static_cast<int>(o.place)));
            else if (wasOn)
                o.place = !*wasOn ? OrnamentPlace::Disabled
                          : store.GetString((base + kOldOrnamentPlaceSuffix).c_str(), "") == kOldOrnamentUpperPlace ? OrnamentPlace::AboveMiddle
                                                                                                                     : OrnamentPlace::BelowMiddle;
            o.width  = LoadInt(store, base + kWidthKey, o.width);
            o.height = LoadInt(store, base + kHeightKey, o.height);
            LoadPick(store, base + kColorKey, o.color);
        }

        // The nameplates' opacities: their own keys, else for Not Targeted the one opacity of before.
        void LoadOpacity(SettingsStore& store, OpacitySettings& o)
        {
            const std::string own = kPlateOpacityBase;
            const auto percent = [&](const char* suffix, int fallback) {
                return Whole(HeldNumber(store, own + suffix).value_or(static_cast<float>(fallback)));
            };
            o.noTarget    = percent(kNoTargetKey, o.noTarget);
            o.notTargeted = Rebased(store, own + kNotTargetedKey, kOldUntargetedKey, o.notTargeted, [](float before) { return before; });
            o.targeted    = percent(kTargetedKey, o.targeted);
            o.range       = HeldNumber(store, own + kRangeKey).value_or(o.range);
            o.fadeIn      = HeldSwitch(store, own + kFadeInKey).value_or(o.fadeIn);
        }

        // A kind's lines' glow in combat, from a file of any age: the lines' own keys, else the one switch of before,
        // which was the mobs'.
        void LoadGlowOff(SettingsStore& store, PlateKind kind, KindSettings& k)
        {
            const std::optional<bool> before = kind == PlateKind::Mobs ? HeldSwitch(store, kOldGlowOffKey) : std::nullopt;
            for (int n = 0; n < kPlateLines; ++n)
            {
                LineDesign& line = k.lines[n];
                line.glowOffInCombat =
                    HeldSwitch(store, LineBase(kind, n) + kGlowOffSuffix).value_or(before.value_or(line.glowOffInCombat));
            }
        }

        void SaveKind(SettingsStore& store, PlateKind kind, const KindSettings& k)
        {
            for (int r = 0; r < RowCountOf(kind); ++r)
            {
                const std::string base = RowBase(kind, r);
                store.Set((base + kPlateKey).c_str(), kPlateModeNames[static_cast<int>(k.rows[r].plate)]);
                store.Set((base + kOutlineKey).c_str(), kOutlineModeNames[static_cast<int>(k.rows[r].outline)]);
                SavePick(store, base + kColorKey, k.rows[r].outlineColor);
            }
            for (int n = 0; n < kPlateLines; ++n)
                SaveLine(store, LineBase(kind, n), k.lines[n]);
            const std::string base = std::string(KindKey(kind)) + kOrnamentInfix;
            store.Set((base + kVisibilityKey).c_str(), kOrnamentPlaceNames[static_cast<int>(k.ornament.place)]);
            StoreFloat(store, base + kWidthKey, static_cast<float>(k.ornament.width));
            StoreFloat(store, base + kHeightKey, static_cast<float>(k.ornament.height));
            SavePick(store, base + kColorKey, k.ornament.color);
            if (HasStatusIcons(kind)) StoreFloat(store, std::string(KindKey(kind)) + kMaxIconsSuffix, static_cast<float>(k.maxIcons));
        }

        void LoadDesigner(SettingsStore& store, Settings& s)
        {
            const float iconShare = static_cast<float>(s.baseSize) / kDefaultIconSize;
            for (int k = 0; k < kPlateKindCount; ++k)
                LoadKind(store, static_cast<PlateKind>(k), s.kinds[k], iconShare);
            for (const PaletteEntry& e : PaletteEntries(s.palette))
                LoadColor(store, e.key, *e.color);
            s.defaultFont    = store.GetString(kDefaultFontKey, s.defaultFont.c_str());
            s.detectionShown = LoadHex(store, kDetectionShownKey, s.detectionShown);
        }

        void SaveDesigner(SettingsStore& store, const Settings& s)
        {
            for (int k = 0; k < kPlateKindCount; ++k)
                SaveKind(store, static_cast<PlateKind>(k), s.kinds[k]);
            const std::string opacity = kPlateOpacityBase;
            StoreFloat(store, opacity + kNoTargetKey, static_cast<float>(s.opacity.noTarget));
            StoreFloat(store, opacity + kNotTargetedKey, static_cast<float>(s.opacity.notTargeted));
            StoreFloat(store, opacity + kTargetedKey, static_cast<float>(s.opacity.targeted));
            StoreBool(store, opacity + kFadeInKey, s.opacity.fadeIn);
            StoreFloat(store, opacity + kRangeKey, s.opacity.range);
            Palette palette = s.palette;
            for (const PaletteEntry& e : PaletteEntries(palette))
                StoreColor(store, e.key, *e.color);
            store.Set(kDefaultFontKey, s.defaultFont.c_str());
            StoreHex(store, kDetectionShownKey, s.detectionShown);
            StoreBool(store, kDesignsSavedKey, true);
        }

        // A file from before the designer (kDesignerVersion): what the migration reads of it, each field with the default
        // of its day, under the key such a file carries.
        enum class OldIconSide : uint8_t { Left, Right, Hidden };
        const char* const kOldIconSideNames[] = {"left", "right", "hide"};
        constexpr int kOldSideCount           = static_cast<int>(std::size(kOldIconSideNames));
        enum class OldMobId : uint8_t { Off, LastThree, Full };
        const char* const kOldMobIdNames[] = {"off", "lastThree", "full"};
        // The player icons as such a file numbers them, GM first: each one's key and the tag that stands for it.
        struct OldPlayerIcon
        {
            const char* key;
            const char* tag;
        };
        constexpr OldPlayerIcon kOldPlayerIcons[] = {{"iconGm", kTagGm}, {"iconMentor", kTagMentor},
            {"iconNewAdventurer", kTagNew}, {"iconLevelSync", kTagSync}, {"iconAway", kTagAway},
            {"iconLinkshell", kTagLinkshell}, {"iconBazaar", kTagBazaar}, {"iconSeekingParty", kTagSeeking}};
        constexpr int kOldPlayerIconCount = static_cast<int>(std::size(kOldPlayerIcons));
        constexpr int kOldGmIcon          = 0;

        constexpr int kOldCategoryCount = static_cast<int>(Category::Placeholder) + 1;
        constexpr int kOldLabelShadeCount = static_cast<int>(LabelShade::VeryTough) + 1; // Incredibly Tough came later
        struct OldSettings
        {
            bool showLabels = true, replaceMobNames = true, replacePlayerNames = true, replaceNpcNames = true;
            bool showIcons = true;
            bool hideClaimedByParty = false, hideClaimed = false;
            OldMobId mobId          = OldMobId::Off;
            bool markPlaceholders   = true;
            std::string fontName = kDefaultFont, labelFontName = kDefaultFont;
            bool fontBold = false, labelFontBold = false;
            bool nameGlow = true, ownGlowColor = false;
            int glowStrength = 100, glowSize = 100;
            Color glowColor = ColorFromRgb(0xFFB45A);
            bool showOrnament = true, ownOrnamentColor = false;
            int ornamentWidth = 90, ornamentThickness = 6;
            Color ornamentColor = ColorFromRgb(0xD9B46A);
            int nameShadow = 100, labelShadow = 100; // of the mockup's shadow
            Color labelShadowColor = {{0.00f, 0.00f, 0.00f}};
            int mobNameSize = 15, playerNameSize = 15, selfNameSize = 15, npcNameSize = 15;
            int labelSize = 13, timerSize = 13, iconSize = 16;
            int playerIconSize   = 100; // percent of the name's letter height
            bool showPlayerIcons = true;
            OldIconSide playerIconSide[kOldPlayerIconCount] = {};
            Color cursorColor           = ColorFromRgb(kPhoenixRed);
            Color lockedCursorColor     = {{0.65f, 0.40f, 1.00f}};
            Color subCursorColor        = {{0.99f, 0.82f, 0.09f}};
            Color outOfRangeCursorColor = {{1.00f, 0.25f, 0.25f}};
            bool ownNameColor = false;
            Color nameColor   = {{1.00f, 1.00f, 1.00f}};
            Color labelColor[kOldLabelShadeCount] = {
                {{0.60f, 0.60f, 0.60f}}, // unknown
                {{0.60f, 0.60f, 0.60f}}, // Too Weak
                {{0.40f, 1.00f, 0.40f}}, // Easy Prey
                {{0.45f, 0.70f, 1.00f}}, // Decent Challenge
                {{1.00f, 1.00f, 1.00f}}, // Even Match
                {{1.00f, 1.00f, 0.35f}}, // Tough
                {{1.00f, 0.35f, 0.35f}}, // Very Tough
            };
            Color textOutline = {{0.00f, 0.00f, 0.00f}};
            // By Category, the six such a file knew: whether its mobs were outlined, and in what color.
            bool show[kOldCategoryCount]   = {true, false, false, true, true, true};
            Color color[kOldCategoryCount] = {
                {{1.00f, 0.15f, 0.15f}}, // aggressive
                {{0.20f, 1.00f, 0.30f}}, // passive
                {{0.70f, 0.70f, 0.70f}}, // unknown
                {{1.00f, 0.60f, 0.10f}}, // aggressive NM
                {{1.00f, 0.84f, 0.00f}}, // passive NM
                {{0.70f, 0.30f, 1.00f}}, // lottery placeholder
            };
        };

        struct OldBoolKey
        {
            const char* key;
            bool OldSettings::*field;
        };
        constexpr OldBoolKey kOldBools[] = {{"showLabels", &OldSettings::showLabels},
            {"replaceMobNames", &OldSettings::replaceMobNames}, {"showIcons", &OldSettings::showIcons},
            // The setting's first name, which saved files carry.
            {"hideInCombat", &OldSettings::hideClaimedByParty}, {"hideClaimed", &OldSettings::hideClaimed},
            {"markPlaceholders", &OldSettings::markPlaceholders}, {"fontBold", &OldSettings::fontBold},
            {"nameGlow", &OldSettings::nameGlow}, {"showOrnament", &OldSettings::showOrnament},
            {"ownGlowColor", &OldSettings::ownGlowColor}, {"ownOrnamentColor", &OldSettings::ownOrnamentColor},
            {"replacePlayerNames", &OldSettings::replacePlayerNames}, {"replaceNpcNames", &OldSettings::replaceNpcNames},
            {"showPlayerIcons", &OldSettings::showPlayerIcons}, {"ownNameColor", &OldSettings::ownNameColor}};

        struct OldIntKey
        {
            const char* key;
            int OldSettings::*field;
        };
        constexpr OldIntKey kOldInts[] = {{"mobNameSize", &OldSettings::mobNameSize},
            {"playerNameSize", &OldSettings::playerNameSize}, {"selfNameSize", &OldSettings::selfNameSize},
            {"npcNameSize", &OldSettings::npcNameSize}, {"labelSize", &OldSettings::labelSize},
            {"timerSize", &OldSettings::timerSize}, {"iconSize", &OldSettings::iconSize},
            {"playerIconSize", &OldSettings::playerIconSize}, {"glowStrength", &OldSettings::glowStrength},
            {"glowSize", &OldSettings::glowSize}, {"ornamentWidth", &OldSettings::ornamentWidth},
            {"ornamentThickness", &OldSettings::ornamentThickness}, {"nameShadow", &OldSettings::nameShadow},
            {"labelShadow", &OldSettings::labelShadow}};

        struct OldColorKey
        {
            const char* key;
            Color OldSettings::*field;
        };
        constexpr OldColorKey kOldColors[] = {{"cursorColor", &OldSettings::cursorColor},
            {"subCursorColor", &OldSettings::subCursorColor}, {"outOfRangeCursorColor", &OldSettings::outOfRangeCursorColor},
            {"lockedCursorColor", &OldSettings::lockedCursorColor}, {"nameColor", &OldSettings::nameColor},
            {"textOutline", &OldSettings::textOutline}, {"glowColor", &OldSettings::glowColor},
            {"ornamentColor", &OldSettings::ornamentColor}};

        constexpr const char* kOldFontKey = "fontName";
        // Files from before the level line had its own font and shadow take the name's.
        constexpr const char* kOldLabelFontKey        = "labelFontName";
        constexpr const char* kOldLabelBoldKey        = "labelFontBold";
        constexpr const char* kOldLabelShadowColorKey = "labelShadowColor";
        // And from before each kind of name had its own size, one for all of them.
        constexpr const char* kOldOneNameSizeKey = "nameSize";
        constexpr Color kOldCursorColor          = {{1.0f, 1.0f, 1.0f}};
        constexpr const char* kOldFont           = "Trebuchet MS";
        constexpr const char* kOldShowSuffix     = "Show";
        const char* const kOldCategoryKeys[kOldCategoryCount] = {"willAttack", "wontAttack", "unknown", "nmWillAttack",
            "nmWontAttack", "placeholder"};
        constexpr const char* kOldMobIdKey = "mobId";

        // saved is the file's version: the defaults that changed before it are put right, as kVersionKey says.
        OldSettings LoadOld(SettingsStore& store, float saved)
        {
            OldSettings o;
            for (const OldBoolKey& b : kOldBools)
                o.*b.field = HeldSwitch(store, b.key).value_or(o.*b.field);
            o.mobNameSize = o.playerNameSize = o.selfNameSize = o.npcNameSize = LoadInt(store, kOldOneNameSizeKey, o.mobNameSize);
            for (const OldIntKey& i : kOldInts)
                o.*i.field = LoadInt(store, i.key, o.*i.field);
            for (const OldColorKey& c : kOldColors)
                LoadColor(store, c.key, o.*c.field);
            o.fontName         = store.GetString(kOldFontKey, kDefaultFont);
            o.labelFontName    = store.GetString(kOldLabelFontKey, o.fontName.c_str());
            o.labelFontBold    = HeldSwitch(store, kOldLabelBoldKey).value_or(o.fontBold);
            o.labelShadowColor = o.textOutline;
            LoadColor(store, kOldLabelShadowColorKey, o.labelShadowColor);
            for (int k = 0; k < kOldLabelShadeCount; ++k)
                LoadColor(store, kLabelShadeKeys[k], o.labelColor[k]);
            for (int c = 0; c < kOldCategoryCount; ++c)
            {
                const std::string base = kOldCategoryKeys[c];
                o.show[c] = HeldSwitch(store, base + kOldShowSuffix).value_or(o.show[c]);
                LoadColor(store, base, o.color[c]);
            }
            for (int i = 0; i < kOldPlayerIconCount; ++i)
                o.playerIconSide[i] = static_cast<OldIconSide>(NameIndex(kOldIconSideNames, kOldSideCount,
                    store.GetString(kOldPlayerIcons[i].key, ""), static_cast<int>(o.playerIconSide[i])));
            o.mobId = static_cast<OldMobId>(NameIndex(kOldMobIdNames, static_cast<int>(std::size(kOldMobIdNames)),
                store.GetString(kOldMobIdKey, ""), static_cast<int>(o.mobId)));
            if (saved < kRedCursorVersion && o.cursorColor == kOldCursorColor) o.cursorColor = OldSettings{}.cursorColor;
            if (saved < kBundledFontVersion)
            {
                if (o.fontName == kOldFont) o.fontName = kDefaultFont;
                if (o.labelFontName == kOldFont) o.labelFontName = kDefaultFont;
            }
            return o;
        }

        // The icons beside a name, as the fixed plate laid them: those on the left in their order, GM always last
        // of them and so nearest the name whatever its side says, then the name, then those on the right.
        std::string PlayerNameText(const OldSettings& o)
        {
            if (!o.showPlayerIcons) return kTagName;
            std::string left, right;
            for (int icon = 0; icon < kOldPlayerIconCount; ++icon)
            {
                if (icon == kOldGmIcon) continue;
                if (o.playerIconSide[icon] == OldIconSide::Left) left += kOldPlayerIcons[icon].tag;
                else if (o.playerIconSide[icon] == OldIconSide::Right) right += kOldPlayerIcons[icon].tag;
            }
            return left + kTagGm + kTagName + right;
        }

        // The look such a file had, as the designer's settings.
        void MigrateDesigner(const OldSettings& o, Settings& s)
        {
            KindSettings& mobs      = s.Kind(PlateKind::Mobs);
            KindSettings& players   = s.Kind(PlateKind::Players);
            KindSettings& self      = s.Kind(PlateKind::Self);
            KindSettings& npcs      = s.Kind(PlateKind::Npcs);
            KindSettings& pets      = s.Kind(PlateKind::Pets); // drawn as mobs then: the mobs' keys covered them
            s.detectionShown        = kAllDetection;           // such a file showed every icon, the aggression one too
            LineDesign& mobIcons    = mobs.lines[kTopLine];
            LineDesign& mobLevel    = mobs.lines[kMiddleLine];
            LineDesign& mobName     = mobs.lines[kBottomLine];
            const int base          = std::clamp(o.mobNameSize, kMinTextSize, kMaxTextSize);
            auto percent            = [&](int px) {
                const float pixels = static_cast<float>(std::clamp(px, kMinTextSize, kMaxTextSize));
                return static_cast<int>(std::lround(kPercent * pixels / static_cast<float>(base)));
            };
            // An icon's pixels as a percent of the base icon size, which such a file leaves at its default.
            auto iconScale          = [](float pixels) { return static_cast<int>(std::lround(kPercent * pixels / kDefaultIconSize)); };
            // The icons are playerIconSize percent of the name they stand beside.
            auto IconPercent        = [&](int nameSize) {
                const float pixels = static_cast<float>(std::clamp(nameSize, kMinTextSize, kMaxTextSize));
                return iconScale(static_cast<float>(o.playerIconSize) / kPercent * pixels);
            };
            auto fontOf             = [](const std::string& font) { return font == kDefaultFont ? std::string() : font; };
            auto boldOf             = [](bool bold) { return bold ? StyleBit(FontStyle::Bold) : uint8_t{0}; };
            auto setVanilla         = [](KindSettings& k, PlateKind kind) {
                for (int r = 0; r < RowCountOf(kind); ++r)
                    k.rows[r].plate = PlateMode::Vanilla;
            };
            // Such a file drew a pet's name as any mob's, where a pet row of today shows it when targeted.
            for (int r = 0; r < RowCountOf(PlateKind::Pets); ++r)
                pets.rows[r].plate = PlateMode::Shown;
            if (!o.replaceMobNames)
            {
                setVanilla(mobs, PlateKind::Mobs);
                setVanilla(pets, PlateKind::Pets);
            }
            if (!o.replacePlayerNames)
            {
                setVanilla(players, PlateKind::Players);
                setVanilla(self, PlateKind::Self);
            }
            if (!o.replaceNpcNames) setVanilla(npcs, PlateKind::Npcs);

            const Category natureCategory[kNatureCount] = {Category::WontAttack, Category::WillAttack, Category::NmWontAttack,
                Category::NmWillAttack, Category::Placeholder, Category::Unknown};
            for (int n = 0; n < kNatureCount; ++n)
            {
                const int c = CategoryIndex(natureCategory[n]);
                const bool steps = CanOutlineByNature(PlateKind::Mobs, c);
                mobs.rows[c].outline = o.show[c] ? OutlineMode::Shown : steps ? OutlineMode::ByNature : OutlineMode::Disabled;
                s.palette.aggression[n] = o.color[c];
            }
            std::ranges::copy(o.labelColor, s.palette.toughness);
            s.palette.target[static_cast<int>(TargetState::Targeted)]   = o.cursorColor;
            s.palette.target[static_cast<int>(TargetState::Locked)]     = o.lockedCursorColor;
            s.palette.target[static_cast<int>(TargetState::SubTarget)]  = o.subCursorColor;
            s.palette.target[static_cast<int>(TargetState::OutOfRange)] = o.outOfRangeCursorColor;
            s.cursorEdgeColor.custom = o.textOutline;
            // Every shadow such a file drew was of the unit before, a line's it never set included.
            auto strength = [](int before) { return Whole(ShadowStrengthOf(static_cast<float>(before))); };
            for (KindSettings& k : s.kinds)
                for (LineDesign& line : k.lines)
                {
                    line.shadowColor.custom = o.textOutline;
                    line.shadow             = strength(OldSettings{}.nameShadow);
                }
            mobLevel.shadowColor.custom = o.labelShadowColor;
            mobLevel.shadow             = strength(o.labelShadow);

            mobIcons.font = mobName.font = pets.lines[kBottomLine].font = fontOf(o.fontName);
            mobIcons.style = mobName.style = pets.lines[kBottomLine].style = boldOf(o.fontBold);
            mobLevel.font = fontOf(o.labelFontName);
            mobLevel.style = boldOf(o.labelFontBold);
            for (const PlateKind kind : {PlateKind::Players, PlateKind::Self})
            {
                LineDesign& line = s.Kind(kind).lines[kBottomLine];
                line.font        = fontOf(o.fontName);
                line.style       = boldOf(o.fontBold);
                line.text        = PlayerNameText(o);
            }
            players.lines[kBottomLine].iconSize = IconPercent(o.playerNameSize);
            self.lines[kBottomLine].iconSize    = IconPercent(o.selfNameSize);
            npcs.lines[kBottomLine].font = fontOf(o.fontName);
            npcs.lines[kBottomLine].style = boldOf(o.fontBold);

            s.baseSize                         = base;
            mobLevel.textSize                  = percent(o.labelSize);
            mobIcons.iconSize                  = iconScale(static_cast<float>(std::clamp(o.iconSize, kMinTextSize, kMaxTextSize)));
            players.lines[kBottomLine].textSize  = percent(o.playerNameSize);
            self.lines[kBottomLine].textSize     = percent(o.selfNameSize);
            npcs.lines[kBottomLine].textSize     = percent(o.npcNameSize);
            s.timerPercent                     = percent(o.timerSize);

            std::string level;
            auto append = [](std::string& text, const std::string& part) { text += (text.empty() || part.empty() ? "" : " ") + part; };
            if (o.showLabels) append(level, kTagLevelCon);
            if (o.mobId == OldMobId::LastThree) append(level, kTagId);
            if (o.mobId == OldMobId::Full) append(level, std::string(kTagServerId) + " " + kTagId);
            if (o.mobId != OldMobId::Off && o.markPlaceholders) append(level, kTagPlaceholder);
            mobLevel.text = level;
            mobIcons.text = o.showIcons ? kTagDetect : "";
            auto condition = [&]() -> LineShow {
                if (o.hideClaimed) return LineShow{false, ConditionBit(ShowCondition::Unclaimed)};
                if (o.hideClaimedByParty)
                    return LineShow{false, ConditionBit(ShowCondition::Unclaimed) | ConditionBit(ShowCondition::OthersClaim)};
                return LineShow{};
            };
            mobLevel.show = condition();
            if (!mobIcons.text.empty()) mobIcons.show = mobLevel.show;

            for (KindSettings& k : s.kinds) // such a file glowed the mobs' names alone
                for (LineDesign& line : k.lines)
                    line.glow = 0;
            mobName.glow     = o.nameGlow ? o.glowStrength : 0;
            mobName.glowSize = o.glowSize;
            if (o.ownGlowColor) mobName.glowColor = ColorPick{ColorSource::Custom, o.glowColor};
            mobs.ornament.place  = o.showOrnament ? OrnamentPlace::BelowMiddle : OrnamentPlace::Disabled;
            mobs.ornament.width  = o.ornamentWidth;
            mobs.ornament.height = o.ornamentThickness;
            if (o.ownOrnamentColor) mobs.ornament.color = ColorPick{ColorSource::Custom, o.ornamentColor};
            for (KindSettings& k : s.kinds)
            {
                k.lines[kBottomLine].shadow = strength(o.nameShadow);
                if (o.ownNameColor) k.lines[kBottomLine].color = ColorPick{ColorSource::Custom, o.nameColor};
            }
        }
    }

    bool NameplatesOn(const Settings& s)
    {
        return s.enabled && (s.nameplates || s.cursorStyle != CursorStyle::Vanilla || s.phTimers);
    }

    void UseHeadsUpWindowDefaults(Settings& s)
    {
        const Settings d;
        s.shiftVanillaUi = true;
        s.placedUnder   = d.placedUnder;
        s.viewShifts    = d.viewShifts;
        s.pinnedWindows = {};
    }

    void UseGameWindowDefaults(Settings& s)
    {
        s.shiftVanillaUi = false;
        s.moveGameHelp  = false;
        s.pinnedWindows = {};
    }

    void UseHelpTextDefaults(Settings& s)
    {
        UseHeadsUpWindowDefaults(s);
        s.moveGameHelp = false;
        s.hideHelpBar  = true;
        s.Widget(BarWidget::GameHelp).on = true;
    }

    HelpMode WindowHelpMode(const Settings& s)
    {
        return ModeOf(s.showBar && s.Widget(BarWidget::GameHelp).on, s.hideHelpBar);
    }

    void AutoAdjustWindows(Settings& s, int band, int barOnly)
    {
        if (band == 0) return;
        const auto place = [&](HelpMode mode, int under) {
            s.placedUnder[static_cast<size_t>(mode)] = std::clamp(under, 0, kMaxWindowOffset);
            s.viewShifts[static_cast<size_t>(mode)].clear();
        };
        place(HelpMode::Off, barOnly);
        place(WindowHelpMode(s), band);
    }

    bool BarOn(const Settings& s)
    {
        return s.enabled && s.showBar;
    }

    bool BarRuns(const Settings& s, uint8_t loginStatus)
    {
        return BarOn(s) && loginStatus == kLoginInGame;
    }

    std::vector<std::string> FontChoices(std::vector<std::string> installed, const std::string& current)
    {
        installed.push_back(current);
        installed.erase(std::remove_if(installed.begin(), installed.end(),
                            [](const std::string& name) { return name.empty() || name[0] == '@' || GameFontVariantOf(name); }),
            installed.end());
        auto lower = [](const std::string& name) {
            std::string l = name;
            for (char& ch : l)
                ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
            return l;
        };
        std::sort(installed.begin(), installed.end(), [&](const std::string& a, const std::string& b) { return lower(a) < lower(b); });
        installed.erase(std::unique(installed.begin(), installed.end(), [](const std::string& a, const std::string& b) { return SameFamily(a, b); }), installed.end());
        std::vector<std::string> choices = BundledFamilies();
        for (const std::string& name : installed)
            if (std::none_of(choices.begin(), choices.end(), [&](const std::string& had) { return SameFamily(had, name); }))
                choices.push_back(name);
        return choices;
    }

    std::vector<std::string> NameplateFontChoices(const std::vector<std::string>& fonts)
    {
        std::vector<std::string> choices{kGameFontName, kGameFontNarrowName};
        choices.insert(choices.end(), fonts.begin(), fonts.end());
        return choices;
    }

    const BarPalette& OwnPalette(const Settings& s)
    {
        return s.barTheme == BarTheme::Custom ? s.customPalette : PaletteOf(s.barTheme);
    }

    bool UsesGameFont(const Settings& s)
    {
        const auto game = [](const std::string& font) { return GameFontVariantOf(font).has_value(); };
        return game(s.defaultFont) || game(s.badgeFontName) || std::ranges::any_of(s.kinds, [&](const KindSettings& k) {
            return std::ranges::any_of(k.lines, [&](const LineDesign& line) { return game(line.font); });
        });
    }

    namespace
    {
        // A source the pick's own dropdown does not offer, as a file from before may hold, goes back to the default.
        void ClampPick(ColorPick& pick, const ColorPick& fallback, const std::vector<ColorSource>& offered)
        {
            if (std::ranges::find(offered, pick.source) == offered.end()) pick.source = fallback.source;
            pick.custom = ClampColor(pick.custom, fallback.custom);
        }

        // conditions holds the bits of the conditions the line's kind offers: one it does not could show the line
        // while the menu, which lists only those, reads Never.
        void ClampLine(LineDesign& line, const LineDesign& d, const std::vector<ColorSource>& offered, uint32_t conditions)
        {
            std::ranges::replace(line.text, '\n', ' ');
            if (line.text.size() > kMaxTemplate)
            {
                size_t end = kMaxTemplate;
                while (end > 0 && ContinuesCharacter(line.text[end])) --end;
                line.text.resize(end);
            }
            if (line.font.size() > kMaxFontName) line.font.clear();
            if (static_cast<int>(line.align) >= kAlignCount) line.align = d.align;
            line.style &= kAllFontStyles;
            line.show.when &= conditions;
            line.textSize  = std::clamp(Stepped(line.textSize, kPercentStep), kMinSizePercent, kMaxSizePercent);
            line.iconSize  = std::clamp(Stepped(line.iconSize, kPercentStep), kMinSizePercent, kMaxSizePercent);
            line.padding   = std::clamp(line.padding, kMinLinePadding, kMaxLinePadding);
            line.offset    = std::clamp(line.offset, kMinLineOffset, kMaxLineOffset);
            line.shadow    = std::clamp(Stepped(line.shadow, kPercentStep), kMinShadow, kMaxShadow);
            line.glow      = std::clamp(Stepped(line.glow, kPercentStep), kMinGlow, kMaxGlow);
            line.glowSize  = std::clamp(Stepped(line.glowSize, kPercentStep), kMinGlowSize, kMaxGlowSize);
            ClampPick(line.color, d.color, offered);
            ClampPick(line.shadowColor, d.shadowColor, offered);
            ClampPick(line.glowColor, d.glowColor, offered);
            if (line.syncWith < 0 || line.syncWith >= kPlateLines) line.syncWith = kNoSync;
        }

        // A cycle of syncs is cut at its highest-numbered line, so the lower line keeps its source.
        void BreakSyncCycles(LineDesign (&lines)[kPlateLines])
        {
            for (int start = 0; start < kPlateLines; ++start)
            {
                int at = start, highest = start;
                for (int step = 0; step < kPlateLines; ++step)
                {
                    at = lines[at].syncWith;
                    if (at == kNoSync) break;
                    highest = std::max(highest, at);
                    if (at == start)
                    {
                        lines[highest].syncWith = kNoSync;
                        break;
                    }
                }
            }
        }

        void ClampDesigner(Settings& s, const Settings& d)
        {
            s.scaleMax = std::max(s.scaleMax, s.scaleMin);
            for (int k = 0; k < kPlateKindCount; ++k)
            {
                KindSettings& kind = s.kinds[k];
                const KindSettings& defaults = d.kinds[k];
                const std::vector<ColorSource> text = SourcesFor(static_cast<PlateKind>(k), true);
                const std::vector<ColorSource> plain = SourcesFor(static_cast<PlateKind>(k), false);
                uint32_t conditions = 0;
                for (const ShowCondition condition : ConditionsFor(static_cast<PlateKind>(k)))
                    conditions |= ConditionBit(condition);
                for (int r = 0; r < kMaxRows; ++r)
                {
                    RowSettings& row = kind.rows[r];
                    if (static_cast<int>(row.plate) >= kPlateModeCount) row.plate = defaults.rows[r].plate;
                    if (static_cast<int>(row.outline) >= kOutlineModeCount ||
                        (row.outline == OutlineMode::ByNature && !CanOutlineByNature(static_cast<PlateKind>(k), r)))
                        row.outline = defaults.rows[r].outline;
                    ClampPick(row.outlineColor, defaults.rows[r].outlineColor, plain);
                }
                for (int n = 0; n < kPlateLines; ++n)
                    ClampLine(kind.lines[n], defaults.lines[n], text, conditions);
                BreakSyncCycles(kind.lines);
                OrnamentSettings& o = kind.ornament;
                if (static_cast<int>(o.place) >= kOrnamentPlaceCount) o.place = defaults.ornament.place;
                o.width  = std::clamp(o.width, kMinOrnamentWidth, kMaxOrnamentWidth);
                o.height = std::clamp(o.height, kMinOrnamentThickness, kMaxOrnamentThickness);
                ClampPick(o.color, defaults.ornament.color, plain);
                kind.maxIcons = std::clamp(kind.maxIcons, 1, kCountedIcons);
            }
            for (int OpacitySettings::*percent : {&OpacitySettings::noTarget, &OpacitySettings::notTargeted, &OpacitySettings::targeted})
                s.opacity.*percent = std::clamp(Stepped(s.opacity.*percent, kPercentStep), kMinOpacity, kMaxOpacity);
            s.opacity.range = ClampFloat(s.opacity.range, kMinOutlineDistance, kMaxOutlineDistance, d.opacity.range);
            Palette fallback = d.palette;
            const std::vector<PaletteEntry> entries = PaletteEntries(s.palette), defaults = PaletteEntries(fallback);
            for (size_t i = 0; i < entries.size(); ++i)
                *entries[i].color = ClampColor(*entries[i].color, *defaults[i].color);
            if (s.defaultFont.empty() || s.defaultFont.size() > kMaxFontName) s.defaultFont = d.defaultFont;
            s.detectionShown &= kAllDetection;
        }
    }

    int DefaultCursorWidth() { return NaturalCursorWidth(kDefaultCursorStyle, kDefaultCursorHeight); }

    const char* CursorStyleName(CursorStyle style) { return kCursorStyleNames[static_cast<int>(style)]; }

    bool IsFolderFileName(std::string_view name)
    {
        return !name.empty() && name.size() <= kMaxFileName && name.find_first_of(kPathCharacters) == std::string_view::npos;
    }

    Settings Clamp(Settings s)
    {
        const Settings d;
        for (const FloatKey& f : kFloats)
            s.*f.field = ClampFloat(s.*f.field, f.lo, f.hi, d.*f.field);
        for (const IntKey& i : kInts)
            s.*i.field = std::clamp(Stepped(s.*i.field, i.step), i.lo, i.hi);
        for (const IntKey& i : kRebasedInts)
            s.*i.field = std::clamp(Stepped(s.*i.field, i.step), i.lo, i.hi);
        s.smoothness = SmoothnessCopies(SmoothnessLevelOf(s.smoothness));
        if (s.badgeFontName.size() > kMaxFontName) s.badgeFontName = d.badgeFontName;
        if (s.barTitleFont.size() > kMaxFontName) s.barTitleFont = d.barTitleFont;
        if (s.barTextFont.size() > kMaxFontName) s.barTextFont = d.barTextFont;
        std::vector<uint8_t> keys;
        for (const uint8_t vk : s.keepPointerKeys)
        {
            const uint8_t key = BindableKey(vk);
            if (key != 0 && std::ranges::find(keys, key) == keys.end() && keys.size() < kMaxKeptKeys) keys.push_back(key);
        }
        s.keepPointerKeys = std::move(keys);
        s.screenshotKey   = BindableKey(s.screenshotKey);
        s.pinnedWindows   = WindowPlacesFrom(WindowPlaceText(s.pinnedWindows));
        for (size_t m = 0; m < kHelpModes; ++m)
        {
            s.placedUnder[m] = std::clamp(s.placedUnder[m], 0, kMaxWindowOffset);
            s.viewShifts[m]  = ViewShiftsFrom(ViewShiftsText(s.viewShifts[m]), WindowViews());
        }
        for (const ColorKey& c : kColors)
            s.*c.field = ClampColor(s.*c.field, d.*c.field);
        const bool named = IsFolderFileName(s.cursorFile);
        if (!named) s.cursorFile.clear();
        if (static_cast<int>(s.cursorStyle) >= kCursorStyleCount || (s.cursorStyle == CursorStyle::File && !named))
            s.cursorStyle = d.cursorStyle;
        const bool pointerNamed = IsFolderFileName(s.pointerFile);
        if (!pointerNamed) s.pointerFile.clear();
        if (static_cast<int>(s.pointerIcon) >= kPointerIconCount || (s.pointerIcon == PointerIcon::File && !pointerNamed))
            s.pointerIcon = d.pointerIcon;
        ClampPick(s.cursorColor, d.cursorColor, CursorSources());
        ClampPick(s.cursorEdgeColor, d.cursorEdgeColor, CursorSources());
        if (static_cast<int>(s.barTheme) >= kBarThemeCount) s.barTheme = d.barTheme;
        if (static_cast<int>(s.customPalette.shape) >= kBarShapeCount) s.customPalette.shape = d.customPalette.shape;
        s.customPalette.border = std::clamp(s.customPalette.border, 0, kMaxStripBorder);
        s.customPalette.rounding = std::clamp(s.customPalette.rounding, kShapeRounding, kMaxRounding);
        s.customPalette.windowType = std::clamp(s.customPalette.windowType, kFollowGameWindow, kWindowTypes);
        if (!IsFolderFileName(s.customThemeName)) s.customThemeName = kUnnamedTheme;
        for (int slot = 0; slot < kPanelSlots; ++slot)
        {
            WidgetOptions& w = s.widgets[slot];
            const WidgetOptions fresh;
            w.design = Whole(WidgetInSlot(slot).kind, std::move(w.design));
            for (int WidgetOptions::* const which : {&WidgetOptions::currency, &WidgetOptions::secondCurrency})
                if (w.*which < kYourConquestCurrency || w.*which >= kCurrencyCount) w.*which = fresh.*which;
            if (w.bag < 0 || w.bag >= kContainerCount) w.bag = fresh.bag;
            if (w.clockZone && static_cast<int>(*w.clockZone) >= kClockZoneCount) w.clockZone = fresh.clockZone;
            if (static_cast<int>(w.weatherIcons) >= kCustomIconsCount) w.weatherIcons = fresh.weatherIcons;
            if (static_cast<int>(w.iconSide) >= kIconSideCount) w.iconSide = fresh.iconSide;
            if (static_cast<int>(w.align) >= kWidgetAlignCount) w.align = fresh.align;
            if (w.clockSecondZone && static_cast<int>(*w.clockSecondZone) >= kClockZoneCount) w.clockSecondZone = fresh.clockSecondZone;
            w.linkPadding  = std::clamp(w.linkPadding, 0, kMaxWidgetSpacing);
            w.expBarWidth  = std::clamp(w.expBarWidth, kMinExpBarWidth, kMaxExpBarWidth);
            w.expBarHeight = std::clamp(w.expBarHeight, kMinExpBarHeight, kMaxExpBarHeight);
            w.targetDetection &= kAllDetection;
            if (w.worldRoute < 0 || w.worldRoute >= static_cast<int>(TravelRoutes().size())) w.worldRoute = fresh.worldRoute;
            if (w.worldGuild < 0 || w.worldGuild >= static_cast<int>(GuildShops().size())) w.worldGuild = fresh.worldGuild;
        }
        if (MenuFontNamed(s.menuFont) == nullptr) s.menuFont = d.menuFont;
        if (static_cast<int>(s.helpBarAnchor) >= kHelpAnchorCount) s.helpBarAnchor = d.helpBarAnchor;
        ClampDesigner(s, d);
        for (StatusBadgeRule& status : s.statusBadges)
        {
            status.weakRank   = std::clamp(status.weakRank, kNoWeakRank, kMaxWeakRank);
            status.resistRank = std::clamp(status.resistRank, kMinResistRank, kNoResistRank);
        }
        // A linked widget sits just after its own in the layout too, as the panel draws it.
        s.barSections = Panel(s).placement;
        return s;
    }

    namespace
    {
        // A copy's key: the first copy's as files have always had it, the others' after it with their number.
        std::string CopyKey(const std::string& key, PanelWidget w)
        {
            return w.copy == 0 ? key : key + "_" + std::to_string(w.copy + 1);
        }

        void LoadWidget(SettingsStore& store, PanelWidget w, WidgetOptions& o)
        {
            const std::string widget = kBarWidgetKeys[static_cast<int>(w.kind)];
            const auto flag = [&](const std::string& key, bool fallback) { return HeldSwitch(store, CopyKey(key, w)).value_or(fallback); };
            const auto text = [&](const char* key) { return store.GetString(CopyKey(key, w).c_str(), ""); };
            const auto number = [&](const std::string& key, int fallback) { return LoadInt(store, CopyKey(key, w), fallback); };
            o.on         = flag(widget, o.on);
            o.design     = DesignFrom(w.kind, text((widget + kDesignSuffix).c_str()));
            o.combine    = PanelWidgetNamed(text((widget + kCombineSuffix).c_str()));
            o.linkPadding = number(widget + kLinkPaddingSuffix, o.linkPadding);
            {
                std::istringstream words(text((widget + kPopupSpotSuffix).c_str()));
                std::string across, down;
                PopupSpot spot;
                if (words >> across >> spot.x >> down >> spot.y && (across == kSpotLeft || across == kSpotRight) &&
                    (down == kSpotTop || down == kSpotBottom))
                {
                    spot.fromRight  = across == kSpotRight;
                    spot.fromBottom = down == kSpotBottom;
                    o.popupSpot     = spot;
                }
            }
            if (IconPartOf(w.kind))
                o.iconSide = static_cast<IconSide>(NameIndex(kIconSideNames, kIconSideCount,
                    text((widget + kIconSideSuffix).c_str()), static_cast<int>(o.iconSide)));
            if (w.kind != BarWidget::GameHelp)
                o.align = static_cast<WidgetAlign>(NameIndex(kWidgetAlignNames, kWidgetAlignCount,
                    text((widget + kAlignSuffix).c_str()), static_cast<int>(o.align)));
            switch (w.kind)
            {
            case BarWidget::Currency:
                for (const auto& [key, which] : {std::pair{kBarCurrencyKey, &WidgetOptions::currency},
                         std::pair{kBarSecondCurrencyKey, &WidgetOptions::secondCurrency}})
                {
                    const std::string held = text(key);
                    if (held == kYourConquestPoints) o.*which = kYourConquestCurrency;
                    else if (const std::optional<int> currency = CurrencyOfKey(held)) o.*which = *currency;
                }
                break;
            case BarWidget::Bags: o.bag = number(kBarBagKey, o.bag); break;
            case BarWidget::Weather:
                o.weatherIcons = static_cast<CustomIcons>(
                    NameIndex(kCustomIconsNames, kCustomIconsCount, text(kWeatherIconsKey), static_cast<int>(o.weatherIcons)));
                break;
            case BarWidget::World:
            {
                const std::string route = text(kWorldRouteKey), guild = text(kWorldGuildKey);
                for (size_t r = 0; r < TravelRoutes().size(); ++r)
                    if (route == TravelRoutes()[r].name) o.worldRoute = static_cast<int>(r);
                for (size_t g = 0; g < GuildShops().size(); ++g)
                    if (guild == GuildShops()[g].shops) o.worldGuild = static_cast<int>(g);
                break;
            }
            case BarWidget::Zone:
                o.zoneId = flag(kZoneIdKey, o.zoneId);
                break;
            case BarWidget::Performance:
                o.memText = flag(kMemTextKey, o.memText);
                break;
            case BarWidget::EarthTime:
            {
                // kClockZoneCount for a name of none of them, as kLocalZone is.
                if (const int zone = NameIndex(kClockZoneNames, kClockZoneCount, text(kClockZoneKey), kClockZoneCount); zone < kClockZoneCount)
                    o.clockZone = static_cast<ClockZone>(zone);
                const std::string second = text(kClockSecondZoneKey);
                if (const int zone = NameIndex(kClockZoneNames, kClockZoneCount, second, kClockZoneCount); zone < kClockZoneCount)
                    o.clockSecondZone = static_cast<ClockZone>(zone);
                else if (second == kLocalZone)
                    o.clockSecondZone.reset();
                o.clock24      = flag(kClock24Key, o.clock24);
                o.clockSeconds = flag(kClockSecondsKey, o.clockSeconds);
                o.dateFormat   = static_cast<DateFormat>(
                    NameIndex(kDateFormatNames, kDateFormatCount, text(kDateFormatKey), static_cast<int>(o.dateFormat)));
                break;
            }
            case BarWidget::Job:
            {
                o.jobNames = flag(kJobNamesKey, o.jobNames);
                o.jobText  = static_cast<JobText>(NameIndex(kJobTextNames, kJobTextCount, text(kJobTextKey), static_cast<int>(o.jobText)));
                o.expBarWidth  = number(kExpBarWidthKey, o.expBarWidth);
                o.expBarHeight = number(kExpBarHeightKey, o.expBarHeight);
                if (const std::string barText = text(kExpBarTextKey); barText == kNoExpBarText)
                    o.expBarText.reset();
                else if (const int at = NameIndex(kExpTextNames, kExpTextCount, barText, -1); at >= 0)
                    o.expBarText = static_cast<ExpText>(at);
                break;
            }
            case BarWidget::Target:
                o.targetDetection = static_cast<uint32_t>(
                    std::clamp(number(kTargetDetectionKey, static_cast<int>(o.targetDetection)), 0, static_cast<int>(kAllDetection)));
                o.targetShows    = static_cast<uint8_t>(
                    std::clamp(number(kTargetShowsKey, o.targetShows), 0, static_cast<int>(kAllTargetShows)));
                break;
            default: break;
            }
        }

        void SaveWidget(SettingsStore& store, PanelWidget w, const WidgetOptions& o)
        {
            const std::string widget = kBarWidgetKeys[static_cast<int>(w.kind)];
            const auto flag = [&](const std::string& key, bool v) { StoreBool(store, CopyKey(key, w), v); };
            const auto text = [&](const char* key, const char* v) { store.Set(CopyKey(key, w).c_str(), v); };
            flag(widget, o.on);
            if (!PartsOf(w.kind).empty()) text((widget + kDesignSuffix).c_str(), DesignText(o.design).c_str());
            text((widget + kCombineSuffix).c_str(), o.combine ? PanelWidgetName(*o.combine).c_str() : "");
            StoreFloat(store, CopyKey(widget + kLinkPaddingSuffix, w), static_cast<float>(o.linkPadding));
            if (IconPartOf(w.kind)) text((widget + kIconSideSuffix).c_str(), kIconSideNames[static_cast<int>(o.iconSide)]);
            if (w.kind != BarWidget::GameHelp) text((widget + kAlignSuffix).c_str(), kWidgetAlignNames[static_cast<int>(o.align)]);
            const std::optional<PopupSpot>& spot = o.popupSpot;
            text((widget + kPopupSpotSuffix).c_str(),
                spot ? (std::string(spot->fromRight ? kSpotRight : kSpotLeft) + " " + std::to_string(spot->x) + " " +
                        (spot->fromBottom ? kSpotBottom : kSpotTop) + " " + std::to_string(spot->y)).c_str()
                     : "");
            switch (w.kind)
            {
            case BarWidget::Currency:
                for (const auto& [key, which] : {std::pair{kBarCurrencyKey, o.currency}, std::pair{kBarSecondCurrencyKey, o.secondCurrency}})
                    text(key, which == kYourConquestCurrency ? kYourConquestPoints : kCurrencyTable[which].key);
                break;
            case BarWidget::Bags: StoreFloat(store, CopyKey(kBarBagKey, w), static_cast<float>(o.bag)); break;
            case BarWidget::Weather: text(kWeatherIconsKey, kCustomIconsNames[static_cast<int>(o.weatherIcons)]); break;
            case BarWidget::World:
                text(kWorldRouteKey, TravelRoutes()[static_cast<size_t>(o.worldRoute)].name);
                text(kWorldGuildKey, GuildShops()[static_cast<size_t>(o.worldGuild)].shops);
                break;
            case BarWidget::Zone:
                flag(kZoneIdKey, o.zoneId);
                break;
            case BarWidget::Performance:
                flag(kMemTextKey, o.memText);
                break;
            case BarWidget::EarthTime:
                text(kClockZoneKey, o.clockZone ? kClockZoneNames[static_cast<int>(*o.clockZone)] : kLocalZone);
                text(kClockSecondZoneKey, o.clockSecondZone ? kClockZoneNames[static_cast<int>(*o.clockSecondZone)] : kLocalZone);
                flag(kClock24Key, o.clock24);
                flag(kClockSecondsKey, o.clockSeconds);
                text(kDateFormatKey, kDateFormatNames[static_cast<int>(o.dateFormat)]);
                break;
            case BarWidget::Job:
                flag(kJobNamesKey, o.jobNames);
                text(kJobTextKey, kJobTextNames[static_cast<int>(o.jobText)]);
                StoreFloat(store, CopyKey(kExpBarWidthKey, w), static_cast<float>(o.expBarWidth));
                StoreFloat(store, CopyKey(kExpBarHeightKey, w), static_cast<float>(o.expBarHeight));
                text(kExpBarTextKey, o.expBarText ? kExpTextNames[static_cast<int>(*o.expBarText)] : kNoExpBarText);
                break;
            case BarWidget::Target:
                StoreFloat(store, CopyKey(kTargetDetectionKey, w), static_cast<float>(o.targetDetection));
                StoreFloat(store, CopyKey(kTargetShowsKey, w), static_cast<float>(o.targetShows));
                break;
            default: break;
            }
        }
    }

    WidgetOptions NewWidget(PanelWidget widget)
    {
        WidgetOptions o;
        o.on         = widget.copy > 0 || kBarWidgetDefaults[static_cast<int>(widget.kind)];
        o.design     = DefaultDesign(widget.kind);
        return o;
    }

    std::array<WidgetOptions, kPanelSlots> DefaultWidgets()
    {
        std::array<WidgetOptions, kPanelSlots> widgets;
        for (int slot = 0; slot < kPanelSlots; ++slot)
            widgets[slot] = NewWidget(WidgetInSlot(slot));
        return widgets;
    }

    bool Shows(const Settings& s, PanelWidget widget)
    {
        return s.Widget(widget).on && (widget.copy == 0 || Placed(s.barSections, widget));
    }

    bool AnyCopyShown(const Settings& s, BarWidget widget)
    {
        for (int copy = 0; copy < kMostCopies; ++copy)
            if (Shows(s, PanelWidget{widget, copy})) return true;
        return false;
    }

    std::optional<PanelWidget> CloneWidget(Settings& s, PanelWidget widget)
    {
        const std::optional<PanelWidget> added = AddCopy(s.barSections, widget.kind);
        if (!added) return added;
        WidgetOptions& clone = s.Widget(*added) = s.Widget(widget);
        clone.on             = true;
        clone.combine.reset();
        return added;
    }

    void RemoveWidgetCopy(Settings& s, PanelWidget widget)
    {
        if (!RemoveCopy(s.barSections, widget)) return;
        s.Widget(widget) = NewWidget(widget);
        for (WidgetOptions& o : s.widgets)
            if (o.combine == widget) o.combine.reset();
    }

    std::vector<PanelWidget> CombineChoices(const Settings& s, PanelWidget widget)
    {
        std::vector<PanelWidget> choices;
        if (!Combines(widget.kind)) return choices;
        const auto taken = [&](PanelWidget other) {
            for (int slot = 0; slot < kPanelSlots; ++slot)
                if (WidgetInSlot(slot) != widget && s.widgets[slot].combine == other) return true;
            return false;
        };
        const auto leadsBack = [&](PanelWidget other) {
            int steps = 0;
            for (std::optional<PanelWidget> on = other; on && steps <= kPanelSlots; on = s.Widget(*on).combine, ++steps)
                if (*on == widget) return true;
            return false;
        };
        for (const std::vector<PanelWidget>& section : s.barSections)
            for (const PanelWidget other : section)
                if (other != widget && Combines(other.kind) && !taken(other) && !leadsBack(other)) choices.push_back(other);
        return choices;
    }

    PanelGroups Panel(const Settings& s)
    {
        CombineLinks links{};
        for (int slot = 0; slot < kPanelSlots; ++slot)
            links[slot] = s.widgets[slot].combine;
        return Combine(s.barSections, links);
    }

    std::vector<PanelWidget> PillSettings(const Settings& s, PanelWidget head)
    {
        std::vector<PanelWidget> on;
        for (const PanelWidget w : Members(Panel(s), head))
            if (s.Widget(w).on) on.push_back(w);
        return on.empty() ? std::vector<PanelWidget>{head} : on;
    }

    namespace
    {
        constexpr const char* kDefaultsSection = "settings";

        // A settings file over HeadsUp's defaults (kDefaultSettingsText): a key the file lacks reads as they hold it.
        class DefaultedStore final : public SettingsStore
        {
        public:
            explicit DefaultedStore(SettingsStore& file)
                : m_File(file)
            {}
            std::string GetString(const char* key, const char* fallback) override
            {
                const auto d = Defaults().find(key);
                return m_File.GetString(key, d != Defaults().end() ? d->second.c_str() : fallback);
            }
            void Set(const char* key, const char* value) override { m_File.Set(key, value); }

        private:
            // kDefaultSettingsText's values by key, read once.
            static const std::unordered_map<std::string, std::string>& Defaults()
            {
                static const std::unordered_map<std::string, std::string> values = [] {
                    std::unordered_map<std::string, std::string> read;
                    std::istringstream lines(kDefaultSettingsText);
                    for (std::string line; std::getline(lines, line);)
                        if (const size_t equals = line.find('='); equals != std::string::npos)
                        {
                            const std::string key = line.substr(0, equals);
                            read.try_emplace(key, *IniValue(kDefaultSettingsText, kDefaultsSection, key));
                        }
                    return read;
                }();
                return values;
            }
            SettingsStore& m_File;
        };

        class EmptyStore final : public SettingsStore
        {
        public:
            std::string GetString(const char*, const char* fallback) override { return fallback; }
            void Set(const char*, const char*) override {}
        };
    }

    Settings DefaultSettings()
    {
        EmptyStore none;
        return LoadSettings(none);
    }

    namespace
    {
        // Every build's file holds these two, and a store with neither is a new install, which has today's defaults and
        // no look from before to keep.
        bool HoldsFile(SettingsStore& store)
        {
            return store.GetString(kVersionKey, kNoText) != kNoText || store.GetString(kEnabledKey, kNoText) != kNoText;
        }

        Settings LoadFrom(SettingsStore& store, bool file)
        {
            Settings s;
            for (const BoolKey& b : kBools)
                s.*b.field = HeldSwitch(store, b.key).value_or(s.*b.field);
            for (const IntKey& i : kInts)
                s.*i.field = LoadInt(store, i.key, s.*i.field);
            // The shape a file from before had, which also gave its one size a width.
            const bool wasArrow = !HeldSwitch(store, kOldCursorFeatherKey).value_or(true);
            const std::string style = store.GetString(kCursorStyleKey, kNoText);
            if (style != kNoText)
                s.cursorStyle = static_cast<CursorStyle>(NameIndex(kCursorStyleNames, kCursorStyleCount, style, static_cast<int>(s.cursorStyle)));
            else if (!HeldSwitch(store, kOldReplaceCursorKey).value_or(true))
                s.cursorStyle = CursorStyle::Vanilla;
            else if (wasArrow)
                s.cursorStyle = CursorStyle::Triangle;
            s.cursorFile   = store.GetString(kCursorFileKey, "");
            const std::string icon = store.GetString(kPointerIconKey, kNoText);
            if (icon != kNoText)
                s.pointerIcon = static_cast<PointerIcon>(NameIndex(kPointerIconNames, kPointerIconCount, icon, static_cast<int>(s.pointerIcon)));
            else if (HeldSwitch(store, kOldChocoboPointerKey).value_or(false))
                s.pointerIcon = PointerIcon::Chocobo;
            s.pointerFile = store.GetString(kPointerFileKey, "");
            const std::string display = store.GetString(kRankDisplayKey, kNoText);
            if (display != kNoText)
                s.rankDisplay = static_cast<RankDisplay>(
                    NameIndex(kRankDisplayNames, kRankDisplayCount, display, static_cast<int>(s.rankDisplay)));
            else if (HeldSwitch(store, kOldShowRanksKey).value_or(false))
                s.rankDisplay = RankDisplay::Rank;
            s.cursorHeight = Rebased(store, kCursorHeightKey, kOldCursorSizeKey, s.cursorHeight, [](float size) { return size; });
            s.cursorWidth  = Rebased(store, kCursorWidthKey, kOldCursorSizeKey, s.cursorWidth, [&](float size) {
                return static_cast<float>(NaturalCursorWidth(wasArrow ? CursorStyle::Triangle : CursorStyle::Feather, Whole(size)));
            });
            s.cursorOutlineThickness = Rebased(store, kCursorEdgeKey, kOldCursorOutlineKey, s.cursorOutlineThickness,
                [](float before) { return before * kCursorEdgeRebase; });
            for (const FloatKey& f : kFloats)
                s.*f.field = HeldNumber(store, f.key).value_or(s.*f.field);
            for (const ColorKey& c : kColors)
                LoadColor(store, c.key, s.*c.field);
            LoadPick(store, kCursorPickKey, s.cursorColor);
            LoadPick(store, kCursorEdgePickKey, s.cursorEdgeColor);
            // A file from before badges had a font of their own to name keeps the one they were set in.
            s.badgeFontName = store.GetString(kBadgeFontKey, file ? kBadgeFont : s.badgeFontName.c_str());
            s.barTitleFont  = store.GetString(kBarTitleFontKey, "");
            s.barTextFont   = store.GetString(kBarTextFontKey, "");
            s.scaleCursor   = HeldSwitch(store, kScaleCursorKey).value_or(file ? s.scaleWithDistance : s.scaleCursor);
            {
                // An unknown or repeated key is left out.
                std::istringstream words(store.GetString(kCurrencyFavoritesKey, SpaceApart(s.currencyFavorites).c_str()));
                s.currencyFavorites.clear();
                for (std::string key; words >> key;)
                    if ((CurrencyOfKey(key) || key == kYourConquestPoints) && std::ranges::find(s.currencyFavorites, key) ==
                                                                                 s.currencyFavorites.end())
                        s.currencyFavorites.push_back(key);
            }
            {
                std::istringstream words(store.GetString(kCollapsedKey, ""));
                s.collapsedSections.clear();
                for (std::string key; words >> key;)
                    if (std::ranges::find(s.collapsedSections, key) == s.collapsedSections.end()) s.collapsedSections.push_back(key);
            }
            s.statusFavorites = StatusIds(store.GetString(kStatusFavoritesKey, SpaceApart(s.statusFavorites).c_str()));
            s.statusPins      = StatusIds(store.GetString(kStatusPinsKey, SpaceApart(s.statusPins).c_str()));
            std::istringstream keys(store.GetString(kKeepPointerKey, ""));
            for (std::string key; keys >> key;)
            {
                char* end        = nullptr;
                const long value = std::strtol(key.c_str(), &end, kHex);
                if (*end == '\0' && value > 0 && value <= UINT8_MAX) s.keepPointerKeys.push_back(static_cast<uint8_t>(value));
            }
            for (int i = 0; i < kStatusRankCount; ++i)
            {
                const std::string base = kStatusKeys[i];
                StatusBadgeRule& rule  = s.statusBadges[i];
                rule.show              = HeldSwitch(store, base + kStatusShowSuffix).value_or(rule.show);
                rule.weakRank          = LoadInt(store, base + kWeakRankSuffix, rule.weakRank);
                rule.resistRank        = LoadInt(store, base + kResistRankSuffix, rule.resistRank);
            }
            s.barTheme = static_cast<BarTheme>(
                NameIndex(kBarThemeNames, kBarThemeCount, store.GetString(kBarThemeKey, ""), static_cast<int>(s.barTheme)));
            const std::string custom = kCustomThemeKey;
            for (const PaletteColor& color : kPaletteColors)
                s.customPalette.*color.field = LoadHex(store, custom + color.name, s.customPalette.*color.field);
            s.customPalette.shape = static_cast<BarShape>(NameIndex(kBarShapeNames, kBarShapeCount,
                store.GetString((custom + kShapeSuffix).c_str(), ""), static_cast<int>(s.customPalette.shape)));
            s.customPalette.border = LoadInt(store, custom + kBorderSuffix, s.customPalette.border);
            s.customPalette.background = static_cast<BarBackground>(NameIndex(kBarBackgroundNames, kBarBackgroundCount,
                store.GetString((custom + kBackgroundSuffix).c_str(), ""), static_cast<int>(s.customPalette.background)));
            s.customPalette.windowType   = LoadInt(store, custom + kWindowTypeSuffix, s.customPalette.windowType);
            s.customPalette.rounding     = LoadInt(store, custom + kRoundingSuffix, s.customPalette.rounding);
            s.customPalette.separator    = static_cast<DashMark>(NameIndex(kDashMarkNames, kDashMarkCount,
                store.GetString((custom + kSeparatorSuffix).c_str(), ""), static_cast<int>(s.customPalette.separator)));
            s.customThemeName = store.GetString(kCustomThemeNameKey, "");
            s.menuFont         = store.GetString(kMenuFontKey, s.menuFont.c_str());
            // By the font named: one whose Windows file was missing was drawn in Barlow Condensed, which nothing here knows.
            s.menuFontSize     = Rebased(store, kMenuFontSizeKey, kOldMenuScaleKey, s.menuFontSize, [&](float percent) {
                const MenuFont* font = MenuFontNamed(s.menuFont);
                return kAshitaFontPixels * std::clamp(percent, kOldMinMenuScale, kOldMaxMenuScale) / kPercent *
                       (font != nullptr ? font->oldScale : 1.0f);
            });
            s.fishBarFont  = store.GetString(kFishBarFontKey, s.fishBarFont.c_str());
            s.fishBarLine  = store.GetString(kFishBarLineKey, s.fishBarLine.c_str());
            s.blueMagicLearned = static_cast<BlueLearned>(NameIndex(kBlueLearnedNames, kBlueLearnedCount,
                store.GetString(kBlueMagicLearnedKey, ""), static_cast<int>(s.blueMagicLearned)));
            s.helpBarAnchor = static_cast<HelpAnchor>(NameIndex(kHelpAnchorNames, kHelpAnchorCount,
                store.GetString(kHelpAnchorKey, ""), static_cast<int>(s.helpBarAnchor)));
            s.fishStamina = static_cast<FishBarShown>(NameIndex(kFishStaminaNames, kFishStaminaCount,
                store.GetString(kFishStaminaKey, ""), static_cast<int>(s.fishStamina)));
            std::array<std::string, kBarSectionCount> sections;
            for (int i = 0; i < kBarSectionCount; ++i)
                sections[i] = store.GetString(kBarSectionKeys[i], "");
            s.barSections = PlacementFrom(sections);
            for (int slot = 0; slot < kPanelSlots; ++slot)
                if (const PanelWidget w = WidgetInSlot(slot); Placed(s.barSections, w) || w.kind == BarWidget::GameHelp)
                    LoadWidget(store, w, s.widgets[slot]);
            s.pinnedWindows = WindowPlacesFrom(store.GetString(kPinnedWindowsKey, ""));
            s.hiddenWindows = HiddenWindowsFrom(store.GetString(kHiddenWindowsKey, ""));
            s.shiftVanillaUi = HeldSwitch(store, kShiftVanillaUiKey).value_or(s.shiftVanillaUi);
            for (size_t m = 0; m < kHelpModes; ++m)
            {
                s.placedUnder[m] = LoadInt(store, kPlacedUnderKeys[m], s.placedUnder[m]);
                s.viewShifts[m]  = ViewShiftsFrom(store.GetString(kViewShiftsKeys[m], ""), WindowViews());
            }
            const float saved = HeldNumber(store, kVersionKey).value_or(0.0f);
            if (saved >= kDesignerVersion || HeldSwitch(store, kDesignsSavedKey).value_or(false)) LoadDesigner(store, s);
            else if (file) MigrateDesigner(LoadOld(store, saved), s);
            for (int k = 0; k < kPlateKindCount; ++k)
            {
                const auto kind = static_cast<PlateKind>(k);
                LoadGlowOff(store, kind, s.kinds[k]);
                if (HasStatusIcons(kind))
                    s.kinds[k].maxIcons = LoadInt(store, std::string(KindKey(kind)) + kMaxIconsSuffix, s.kinds[k].maxIcons);
            }
            LoadOpacity(store, s.opacity);
            return Clamp(s);
        }
    }

    Settings LoadSettings(SettingsStore& fileStore)
    {
        if (HoldsFile(fileStore)) return LoadFrom(fileStore, true);
        DefaultedStore store(fileStore);
        return LoadFrom(store, false);
    }

    void SaveSettings(const Settings& s, SettingsStore& store)
    {
        for (const BoolKey& b : kBools)
            StoreBool(store, b.key, s.*b.field);
        for (const IntKey& i : kInts)
            StoreFloat(store, i.key, static_cast<float>(s.*i.field));
        for (const IntKey& i : kRebasedInts)
            StoreFloat(store, i.key, static_cast<float>(s.*i.field));
        for (const FloatKey& f : kFloats)
            StoreFloat(store, f.key, s.*f.field);
        for (const ColorKey& c : kColors)
            StoreColor(store, c.key, s.*c.field);
        store.Set(kCursorStyleKey, CursorStyleName(s.cursorStyle));
        store.Set(kCursorFileKey, s.cursorFile.c_str());
        store.Set(kPointerIconKey, kPointerIconNames[static_cast<int>(s.pointerIcon)]);
        store.Set(kPointerFileKey, s.pointerFile.c_str());
        store.Set(kRankDisplayKey, kRankDisplayNames[static_cast<int>(s.rankDisplay)]);
        SavePick(store, kCursorPickKey, s.cursorColor);
        SavePick(store, kCursorEdgePickKey, s.cursorEdgeColor);
        store.Set(kBadgeFontKey, s.badgeFontName.c_str());
        store.Set(kBarTitleFontKey, s.barTitleFont.c_str());
        store.Set(kBarTextFontKey, s.barTextFont.c_str());
        StoreBool(store, kScaleCursorKey, s.scaleCursor);
        std::string keys;
        for (const uint8_t vk : s.keepPointerKeys)
        {
            char hex[4];
            std::snprintf(hex, sizeof(hex), "%02X", vk);
            keys += (keys.empty() ? "" : " ") + std::string(hex);
        }
        store.Set(kKeepPointerKey, keys.c_str());
        store.Set(kCurrencyFavoritesKey, SpaceApart(s.currencyFavorites).c_str());
        store.Set(kCollapsedKey, SpaceApart(s.collapsedSections).c_str());
        store.Set(kStatusFavoritesKey, SpaceApart(s.statusFavorites).c_str());
        store.Set(kStatusPinsKey, SpaceApart(s.statusPins).c_str());
        for (int i = 0; i < kStatusRankCount; ++i)
        {
            const std::string base = kStatusKeys[i];
            StoreBool(store, base + kStatusShowSuffix, s.statusBadges[i].show);
            StoreFloat(store, base + kWeakRankSuffix, static_cast<float>(s.statusBadges[i].weakRank));
            StoreFloat(store, base + kResistRankSuffix, static_cast<float>(s.statusBadges[i].resistRank));
        }
        store.Set(kBarThemeKey, kBarThemeNames[static_cast<int>(s.barTheme)]);
        for (const PaletteColor& color : kPaletteColors)
            StoreHex(store, kCustomThemeKey + std::string(color.name), s.customPalette.*color.field);
        store.Set((kCustomThemeKey + std::string(kShapeSuffix)).c_str(), kBarShapeNames[static_cast<int>(s.customPalette.shape)]);
        StoreFloat(store, kCustomThemeKey + std::string(kBorderSuffix), static_cast<float>(s.customPalette.border));
        store.Set((kCustomThemeKey + std::string(kBackgroundSuffix)).c_str(),
            kBarBackgroundNames[static_cast<int>(s.customPalette.background)]);
        StoreFloat(store, kCustomThemeKey + std::string(kWindowTypeSuffix), static_cast<float>(s.customPalette.windowType));
        StoreFloat(store, kCustomThemeKey + std::string(kRoundingSuffix), static_cast<float>(s.customPalette.rounding));
        store.Set((kCustomThemeKey + std::string(kSeparatorSuffix)).c_str(), kDashMarkNames[static_cast<int>(s.customPalette.separator)]);
        store.Set(kCustomThemeNameKey, s.customThemeName.c_str());
        store.Set(kHelpAnchorKey, kHelpAnchorNames[static_cast<int>(s.helpBarAnchor)]);
        store.Set(kFishStaminaKey, kFishStaminaNames[static_cast<int>(s.fishStamina)]);
        store.Set(kMenuFontKey, s.menuFont.c_str());
        store.Set(kFishBarFontKey, s.fishBarFont.c_str());
        store.Set(kFishBarLineKey, s.fishBarLine.c_str());
        store.Set(kBlueMagicLearnedKey, kBlueLearnedNames[static_cast<int>(s.blueMagicLearned)]);
        // A copy's keys are written while it is on the panel; the first copy's always.
        for (int slot = 0; slot < kPanelSlots; ++slot)
            if (const PanelWidget w = WidgetInSlot(slot); w.copy == 0 || Placed(s.barSections, w))
                SaveWidget(store, w, s.widgets[slot]);
        for (int i = 0; i < kBarSectionCount; ++i)
            store.Set(kBarSectionKeys[i], PlacementNames(s.barSections[i]).c_str());
        store.Set(kPinnedWindowsKey, WindowPlaceText(s.pinnedWindows).c_str());
        store.Set(kHiddenWindowsKey, HiddenWindowsText(s.hiddenWindows).c_str());
        StoreBool(store, kShiftVanillaUiKey, s.shiftVanillaUi);
        for (size_t m = 0; m < kHelpModes; ++m)
        {
            StoreFloat(store, kPlacedUnderKeys[m], static_cast<float>(s.placedUnder[m]));
            store.Set(kViewShiftsKeys[m], ViewShiftsText(s.viewShifts[m]).c_str());
        }
        SaveDesigner(store, s);
        StoreFloat(store, kVersionKey, static_cast<float>(kSettingsVersion));
    }

    void ChangedKeys::Set(const char* key, const char* value)
    {
        const auto [held, added] = m_Held.try_emplace(key, value);
        if (!added)
        {
            if (held->second == value) return;
            held->second = value;
        }
        if (!m_Passing) return;
        m_Store.Set(key, value);
        if (const auto waiting = std::ranges::find(m_Unwritten, key, &KeyText::key); waiting != m_Unwritten.end())
            waiting->text = value;
        else
            m_Unwritten.push_back(KeyText{key, value});
    }

    Settings ChangedKeys::Load()
    {
        m_Held.clear();
        m_Unwritten.clear();
        m_Passing = false;
        SaveSettings(Settings{}, *this);
        m_Passing = true;
        const std::string marker = m_Held.at(kDesignsSavedKey);
        bool any = false;
        for (auto& [key, text] : m_Held)
        {
            text = m_Store.GetString(key.c_str(), kNoText);
            any  = any || text != kNoText;
        }
        m_Outdated = any && (m_Held.at(kDesignsSavedKey) != marker ||
                                std::strtof(m_Held.at(kVersionKey).c_str(), nullptr) < kSettingsVersion);
        return LoadSettings(m_Store);
    }

    const std::vector<KeyText>& ChangedKeys::Save(const Settings& s)
    {
        SaveSettings(s, *this);
        return m_Unwritten;
    }

    SaveNotice ChangedKeys::ReadBack(bool fileHoldsThem)
    {
        if (fileHoldsThem) m_Unwritten.clear();
        const bool wasFailing = std::exchange(m_Failing, !fileHoldsThem);
        if (wasFailing == m_Failing) return SaveNotice::None;
        return m_Failing ? SaveNotice::Failed : SaveNotice::Saved;
    }

    uint32_t ToArgb(const Color& c)
    {
        auto byte = [](float f) { return static_cast<uint8_t>(std::lround(f * 255.0f)); };
        return Opaque(byte(c.v[0]), byte(c.v[1]), byte(c.v[2]));
    }

    bool PointerSwapped(const Settings& s)
    {
        return s.enabled && (s.pointerIcon != PointerIcon::Vanilla || s.pointerSize != kMinPointerSize);
    }
}
