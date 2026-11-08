#pragma once

#include "argb.h"
#include "bar_design.h"
#include "bar_placement.h"
#include "bar_server_time.h"
#include "bar_statuses.h"
#include "bar_theme.h"
#include "bar_types.h"
#include "color.h"
#include "currencies.h"
#include "dash_mark.h"
#include "fonts.h"
#include "icons.h"
#include "ini_text.h"
#include "magic_chance.h"
#include "menu_document.h"
#include "menu_fonts.h"
#include "native_locate.h"
#include "outline_math.h"
#include "plate_settings.h"
#include "window_skin.h"
#include "window_views.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace headsup
{
    constexpr int kMinOutlineThickness = 25, kMaxOutlineThickness = 400; // percent of kOutlineUnitPx
    constexpr int kMinSmoothness = SmoothnessCopies(SmoothnessLevel::Low), kMaxSmoothness = SmoothnessCopies(SmoothnessLevel::Ultra);
    constexpr int kMinTextSize = 8, kMaxTextSize = 48;   // pixels: the base font and base icon sizes
    constexpr int kDefaultIconSize = 16;                 // pixels: the base icon size
    constexpr int kMaxPearlOutline = 300;                // percent of XIUI's: the linkpearl's outline
    constexpr int kMinNameRaise = 0, kMaxNameRaise = 40; // pixels
    constexpr const char* kDefaultFont = kMontserratSemiBold;
    constexpr const char* kBadgeFont   = kBarlowCondensed; // what badges were set in before they took the default font
    constexpr size_t kMaxFontName      = 31; // a Windows font name's longest, without its terminator

    constexpr int kDefaultWidgetSpacing = 2, kMaxWidgetSpacing = 40; // pixels
    constexpr int kDefaultWidgetPadding = 8, kMaxWidgetPadding = 40; // pixels
    constexpr int kThemeRounding = -1; // a corner's radius in pixels up to kMaxRounding, or the theme's own
    // Where the Zone widget shows who holds its region: not at all, before or after the zone's name, or under it.
    // The Target widget's Visibility: the targets it shows for.
    enum class TargetShow : uint8_t { NpcsObjects, Players, Mobs, Pets, NoTarget };
    constexpr int kTargetShowCount = 5;
    constexpr uint8_t TargetShowBit(TargetShow show) { return static_cast<uint8_t>(1u << static_cast<int>(show)); }
    constexpr uint8_t kAllTargetShows = (1u << kTargetShowCount) - 1;
    // Its Show Icons: the detection icons, then a mob's weaknesses after its level.

    // The Conquest widget's nations, when it shows several: two lines of two, or one line of all.
    // One copy of a widget on the panel: whether it shows, what of it, and its own settings.
    struct WidgetOptions
    {
        bool on         = false;
        WidgetDesign design; // what it shows on its two lines (NewWidget gives each kind its default)
        int currency    = kGilCurrency;             // Gil: by kCurrencyTable, or kYourConquestCurrency
        int secondCurrency = kYourConquestCurrency; // its Currency 2 buttons', the same way
        int bag         = 0;                        // Bags: the container of its Bag 1 buttons, by Ashita's number
        int secondBag   = kMogSatchel;              // its Bag 2 buttons', the same way
        CustomIcons weatherIcons = CustomIcons::Always; // Forecast: its weather pictures
        bool zoneId = true;                         // Zone: its number after its name in the pop-out's header
        bool memText = false;                       // Performance: "Mem" before its Memory button's figures
        StatusDisplay statusDisplay = StatusDisplay::Both; // Status Timers: how each pinned status shows
        std::optional<ClockZone> clockZone;         // Clock: none for the computer's own
        std::optional<ClockZone> clockSecondZone = ClockZone::Japan; // its Secondary Timezone part's, the same way
        bool clock24         = true;                // its two lines
        bool clockSeconds    = false;               // its own line's seconds
        DateFormat dateFormat = DateFormat::MonthDayYear; // how its Local Date button writes the date
        int expBarWidth      = kDefaultExpBarWidth;  // Job & EXP: its EXP Bar's
        int expBarHeight     = kDefaultExpBarHeight;
        std::optional<ExpText> expBarText;           // what its EXP Bar says; none for nothing
        Align expBarTextAlign = Align::Center;       // where on the bar it says it
        std::optional<uint32_t> expBarBack, expBarFill; // its EXP Bar's colors (ARGB); none for the theme's
        JobText jobText      = JobText::MainSub;     // how its Job Info says the jobs
        // Where on its line each of its texts goes, against the widget's other lines.
        Align jobInfoAlign = Align::Left, earnedAlign = Align::Left, toNextAlign = Align::Left, percentAlign = Align::Left;
        uint32_t targetDetection = kDefaultDetection; // Target: the senses its Detection part shows, by DetectionBit
        uint8_t targetShows  = kAllTargetShows; // the targets it shows for, by TargetShowBit
        int worldRoute       = 0;                    // World Info: its Tracked Ship, by TravelRoutes
        int worldGuild       = 0;                    // its Tracked Guild, by GuildShops
        std::optional<PanelWidget> combine;          // shown right after it in its pill, with one popup (Combine)
        int linkPadding = 0; // pixels between it and the widget it combines with
        IconSide iconSide = IconSide::Left; // its icon's, for a widget with one (IconPartOf)
        WidgetAlign align = WidgetAlign::Location; // all but Game Help's, whose text is the help bar's
        std::optional<PopupSpot> popupSpot; // where its pop-out shows while pinned; none for under its pill
        bool operator==(const WidgetOptions&) const = default;
    };
    // A copy as a new one is: shown, and the first copies as kBarWidgetDefaults has them.
    WidgetOptions NewWidget(PanelWidget widget);
    std::array<WidgetOptions, kPanelSlots> DefaultWidgets();

    // The target cursor: the game's own, one of HeadsUp's shapes or pictures, or a cursor file of the player's.
    enum class CursorStyle : uint8_t { Vanilla, Triangle, Feather, Chocobo, File };
    constexpr int kCursorStyleCount = 5;
    constexpr CursorStyle kDefaultCursorStyle = CursorStyle::Feather;
    const char* CursorStyleName(CursorStyle style); // as the settings file has it
    // A picture keeps its own colors; a shape is filled with the cursor's.
    constexpr bool CursorIsPicture(CursorStyle style) { return style == CursorStyle::Chocobo || style == CursorStyle::File; }
    // A cursor or theme file is kept by its name, which goes into a path under its folder: not empty, not over this
    // long, and with nothing in it that would lead out of the folder.
    constexpr size_t kMaxFileName = 64;
    bool IsFolderFileName(std::string_view name);
    constexpr const char* kUnnamedTheme = "Custom"; // a custom theme's name where a file holds none
    // The mouse pointer: the game's own, PlayOnline's chocobo, or a cursor file of the player's.
    // Which fish stamina bar shows while a fish is hooked: none, the game's, or HeadsUp's in place of the game's.
    enum class FishBarShown : uint8_t { Hidden, Vanilla, HeadsUp };
    constexpr int kFishStaminaCount = 3;
    enum class PointerIcon : uint8_t { Vanilla, Chocobo, File };
    constexpr int kPointerIconCount = 3;
    constexpr int kMinCursorSide = 4, kMaxCursorSide = 64; // pixels: the target cursor's width and height
    constexpr int kDefaultCursorHeight = 20;
    int DefaultCursorWidth(); // the default style's at that height
    constexpr int kMinCursorEdge = 50, kMaxCursorEdge = 400; // percent
    constexpr int kMaxCursorRaise = 20; // pixels, up or down
    constexpr int kMinOpacity = 10, kMaxOpacity = 100; // percent

    constexpr int kMinMenuFontSize = 10, kMaxMenuFontSize = 40, kDefaultMenuFontSize = 18; // pixels
    constexpr int kMinPointerSize = 100, kMaxPointerSize = 300; // percent of the pointer's own size
    constexpr int kMinMenuOpacity = 30; // percent: fainter, the window could not be found to set it back
    constexpr int kMaxMenuSide  = 16384; // pixels: more than any screen is across
    // How the Target popup marks a TP move whose blue magic you have learned: a star after it, or plain text.
    enum class BlueLearned : uint8_t { Star, Plain };
    constexpr int kBlueLearnedCount = 2;
    constexpr int kFishBarUnplaced = -1; // the fishing bar's X and Y until it is dragged somewhere
    constexpr int kMaxFishBarPlace = 7680; // pixels: across an 8K screen
    constexpr int kMinFishBarWidth = 100, kMaxFishBarWidth = 1000; // pixels
    constexpr int kMinFishBarHeight = 6, kDefaultFishBarHeight = 18, kMaxFishBarHeight = 80; // pixels, its frame counted
    constexpr int kMinFishTextSize = 8, kDefaultFishTextSize = 15, kMaxFishTextSize = 40;    // pixels

    constexpr int kMaxHelpBarOffset = 4000; // pixels either way
    constexpr int kMinHelpBarWidth = 10, kDefaultHelpBarWidth = 50, kMaxHelpBarWidth = 100; // percent of the screen
    constexpr int kAutoHelpBarHeight = 8, kMaxHelpBarHeight = 200;  // pixels, or Auto: as tall as its text
    constexpr int kMaxUpMinutes = 60, kDefaultUpMinutes = 5; // how long a respawn timer reads "up" before it goes

    struct Settings
    {
        // The Currency popup's favorites at its top, by kCurrencyTable's key or kYourConquestPoints, in this order.
        std::vector<std::string> currencyFavorites{kCurrencyTable[kGilCurrency].key, kYourConquestPoints};
        std::vector<std::string> collapsedSections; // the pop-out cards collapsed, by CollapseKey
        // The Status Timers popup's favorites at its top, and those of them it shows on the panel, by status id.
        std::vector<uint16_t> statusFavorites{kSignetEffect, kSanctionEffect};
        std::vector<uint16_t> statusPins{kSignetEffect};
        bool enabled       = true;
        bool autoSave      = true; // off, changes wait for Save in the settings window, and Revert drops them
        int outlineThickness = 100; // percent of kOutlineUnitPx
        int outlineOpacity   = 60;  // percent
        bool outlineFadeIn   = false; // fainter with distance, down to almost nothing at the range
        int smoothness     = kMinSmoothness; // shifted copies per mesh: a SmoothnessLevel's
        float maxDistance  = 50.0f; // yalms
        // Which elements and statuses get badges, as BadgeRules has them.
        int elementWeakRank   = BadgeRules{}.elementWeakRank;
        int elementResistRank = BadgeRules{}.elementResistRank;
        int maxElements       = BadgeRules{}.maxElements;
        StatusBadgeRule statusBadges[kStatusRankCount] = {}; // by StatusRank
        RankDisplay rankDisplay = RankDisplay::IncreasedChance; // what a rank badge's number says
        bool countAccuracy      = false; // Increased Chance with your own magic accuracy, not the mob's evasion alone
        std::string badgeFontName; // badges' numbers; empty for the default font
        bool showBar       = true;
        BarTheme barTheme  = BarTheme::PhoenixGold;
        // The custom theme in use, or the last one, kept while another is picked: its colors and looks as they are now,
        // and the name of its file in the themes folder.
        BarPalette customPalette = PaletteOf(BarTheme::PhoenixGold);
        std::string customThemeName = kUnnamedTheme;
        int panelSeparators    = kAllSeparators; // by SeparatorBit, whatever the theme
        bool widgetBackground   = false; // every widget's pill or tint shown while not hovered, over the theme's look
        int barHeight      = 45; // pixels: the least at which titles are 13 px over 11 px detail lines
        float barFontRatio = kDefaultFontRatio; // a widget's top line's size over its second line's
        int barIconScale   = kDefaultIconScale; // its icons, in percent of the most it holds
        std::string barTitleFont; // widget titles and popup headings; empty for the theme's
        std::string barTextFont;  // details, figures and popup text; empty for the theme's
        int barWidgetSpacing = kDefaultWidgetSpacing; // between neighbouring widgets
        int barWidgetPadding = kDefaultWidgetPadding; // inside every widget, from its edges to its content
        float popupTextScale   = kDefaultPopupScale; // the pop-outs' text sizes, times their own
        float popupWidthScale  = kDefaultPopupScale; // each pop-out's sections, and so the pop-out, this much wider
        float popupHeightScale = kDefaultPopupScale; // and taller
        int widgetRounding  = kThemeRounding; // the widgets' boxes' corners
        int popupRounding   = kThemeRounding; // the pop-outs'
        int sectionRounding = kThemeRounding; // their sections'
        int helpBarRounding = kThemeRounding; // the help bar's
        std::array<WidgetOptions, kPanelSlots> widgets = DefaultWidgets(); // by SlotOf; a copy not placed has none
        std::vector<WindowPlace> pinnedWindows;  // game windows the player put at places of their own, never shifted
        std::vector<std::string> hiddenWindows;  // game windows drawn as nothing, by short name
        bool shiftVanillaUi = true; // the game's windows that hang from the top go under the panel, a view at a time
        // By HelpMode: where a view's highest window goes, in the game's menu pixels, and the player's own shifts.
        std::array<int, kHelpModes> placedUnder = {ReferenceBand(HelpMode::Off), ReferenceBand(HelpMode::On)};
        std::array<std::vector<ViewShift>, kHelpModes> viewShifts;
        bool moveGameHelp = false; // the game's own help bar goes under the top bar, and the windows under it
        int helpBarX = 0, helpBarY = 0;          // back-buffer pixels from the help bar's place under the top bar
        bool helpBarBackground = true;           // off, the help text alone
        int helpBarTextSize = kAutoBarTextSize;  // its text's pixels, or Auto: the bar's title size
        int helpBarWidth    = kDefaultHelpBarWidth;
        int helpBarHeight   = kAutoHelpBarHeight;
        HelpAnchor helpBarAnchor = HelpAnchor::Left;
        bool helpBarFullWidth    = false; // the whole screen's width, whatever the text's
        std::string menuFont = kDefaultMenuFont; // the settings window's text, by its kMenuFonts name
        int menuFontSize     = kDefaultMenuFontSize; // pixels: the settings window's text and everything sized by it
        int menuOpacity      = 100;              // percent
        // Where the player left the settings window, in pixels; no width for one never moved, which opens in the
        // screen's middle.
        int menuX = 0, menuY = 0, menuWidth = 0, menuHeight = 0;
        FishBarShown fishStamina = FishBarShown::HeadsUp;
        int fishBarWidth = 300;
        int fishBarX = kFishBarUnplaced, fishBarY = kFishBarUnplaced;
        int fishBarHeight   = kDefaultFishBarHeight;
        int fishBarTextSize = kDefaultFishTextSize;
        std::string fishBarFont;     // its line's font, or the theme's text font
        bool fishBarDraggable = true; // off, the pointer goes through it to the game
        std::string fishBarLine = "${size|fish} $feeling";
        bool blueMagic = true; // the Target popup's TP moves a blue mage can learn, in blueMagicColor
        Color blueMagicColor = {{0.40f, 0.65f, 1.00f}};
        BlueLearned blueMagicLearned = BlueLearned::Star;
        bool fishColorByCatch  = true; // off, the theme's fill whatever is on the line
        Color fishSmallColor   = {{0.31f, 0.71f, 0.94f}};
        Color fishLargeColor   = {{0.18f, 0.48f, 0.84f}};
        Color fishItemColor    = {{0.66f, 0.60f, 0.49f}};
        Color fishMonsterColor = {{0.61f, 0.31f, 0.85f}};
        Color fishLineColor    = {{1.00f, 0.55f, 0.10f}};
        Color fishRodColor     = {{0.88f, 0.19f, 0.19f}};
        BarPlacement barSections = DefaultPlacement(); // where each widget sits, by BarSection
        bool hideHelpBar = false; // skip the game's own help bar across the top of the screen
        bool hideNetstat = false; // and its connection readout (S, R and %) at the top right
        int screenshotKey        = 0;    // the virtual-key code that turns screenshot mode on and off, 0 for none
        bool screenshotHidesGame = true; // screenshot mode takes the game's own windows and chat too
        bool phTimers          = false; // above your name, a respawn timer for each placeholder or NM that dies near you
        bool scaleWithDistance = true;  // sizes follow how far a name's owner is from you
        bool scaleCursor       = false; // the target cursor's size follows the distance too, apart from the names
        std::vector<uint8_t> keepPointerKeys; // keys that set off a command without hiding the game's pointer
        CursorStyle cursorStyle = kDefaultCursorStyle;
        std::string cursorFile; // a File cursor's name in the cursors folder, without its extension
        PointerIcon pointerIcon = PointerIcon::Vanilla;
        std::string pointerFile; // a File pointer's name in the cursors folder, without its extension
        int pointerSize = kMinPointerSize; // percent, the game's own pointer included
        bool fixPointer        = true;  // the game reads the mouse in its true client area and syncs its pointer before clicks
        bool lockCursor        = false; // Windows' pointer kept inside the game's window while it has the focus
        int cursorWidth        = DefaultCursorWidth(); // pixels
        int cursorHeight       = kDefaultCursorHeight;
        int cursorOutlineThickness = 100; // percent: its edge, a twentieth of its height at 100
        bool cursorFloat       = true;  // it bobs up and down
        int cursorRaise        = 5;     // pixels over its place
        int nameRaise          = 6;     // pixels the names HeadsUp draws sit above the game's, with all above them
        bool stackNames        = false; // names HeadsUp draws that would cover each other rise over one another
        bool stackLine         = true;  // and a risen one has a line down to where it stood
        bool hideInEvent       = false; // no nameplate, HeadsUp's or the game's, while you are in a conversation
        bool setupDone         = false; // Setup was finished, skipped or closed, and opens by itself no more
        bool debugMode         = false; // the HeadsUp Config page shows its readings and reports
        bool logging           = false; // with debugMode, notes go to a log of their own
        Color weakBadgeColor   = {{0.40f, 1.00f, 0.40f}};
        Color resistBadgeColor = {{1.00f, 0.35f, 0.35f}};
        Color rankBadgeColor   = {{1.00f, 0.85f, 0.30f}};
        Color immuneBadgeColor = {{1.00f, 0.25f, 0.25f}};
        bool nameplates = true;
        bool outlines   = true;
        int baseSize    = 15; // pixels: a line's text at 100 percent
        int baseIconSize = kDefaultIconSize; // and its icons
        int pearlOutline = 100;              // the linkpearl's outline, percent of XIUI's; 0 for none
        bool autoCheck    = false; // every mob that comes near is checked by HeadsUp, for its exact level
        bool lookUpPlayers = false; // every player the server shows you is looked up by the search server, for their jobs
        std::string defaultFont = kDefaultFont;
        int scaleMin = 50, scaleMax = 250; // percent: the distance scale's limits
        int timerPercent = 85;
        int phUpMinutes  = kDefaultUpMinutes;
        std::array<KindSettings, kPlateKindCount> kinds = DefaultKinds();
        OpacitySettings opacity; // every kind's nameplates
        Palette palette = DefaultPalette();
        uint32_t detectionShown = kDefaultDetection;
        ColorPick cursorColor{ColorSource::Target};
        ColorPick cursorEdgeColor{ColorSource::Custom, ColorFromRgb(0x000000)};
        const WidgetOptions& Widget(PanelWidget w) const { return widgets[SlotOf(w)]; }
        WidgetOptions& Widget(PanelWidget w) { return widgets[SlotOf(w)]; }
        const KindSettings& Kind(PlateKind k) const { return kinds[static_cast<int>(k)]; }
        KindSettings& Kind(PlateKind k) { return kinds[static_cast<int>(k)]; }
        bool operator==(const Settings&) const = default;
    };

    bool NameplatesOn(const Settings& s);
    // How game windows move: HeadsUp's way, every view under the panel at its reference place with nothing of the
    // player's, or the game's, where it puts them. Each clears the windows' own offsets.
    void UseHeadsUpWindowDefaults(Settings& s);
    void UseGameWindowDefaults(Settings& s);
    // HeadsUp's help text in place of the game's help bar, which is hidden, and the windows as the HeadsUp defaults
    // have them.
    void UseHelpTextDefaults(Settings& s);
    bool BarOn(const Settings& s);
    // A copy is on the panel and switched on; AnyCopyShown asks whether any copy of a widget is.
    bool Shows(const Settings& s, PanelWidget widget);
    bool AnyCopyShown(const Settings& s, BarWidget widget);
    // Another copy of a widget, after the last of its copies, shown and with that copy's settings but its Combine;
    // none when it has no more. Taking a copy off forgets its settings.
    std::optional<PanelWidget> CloneWidget(Settings& s, PanelWidget widget);
    void RemoveWidgetCopy(Settings& s, PanelWidget widget);
    // The panel as it is drawn, each widget's combine pulling its partner after it.
    PanelGroups Panel(const Settings& s);
    // The widgets whose settings a pill's popup shows: those of its widgets that are on, or its first alone.
    std::vector<PanelWidget> PillSettings(const Settings& s, PanelWidget head);
    // What a widget can be combined with, in the panel's order: every placed widget that combines, but itself, one
    // another widget has, and one whose own links lead back to it.
    std::vector<PanelWidget> CombineChoices(const Settings& s, PanelWidget widget);
    // Whether the game's help shows under the panel, for the windows: the panel's help line or the game's own strip.
    HelpMode WindowHelpMode(const Settings& s);
    // Places every view under the band of now (in the game's menu pixels; 0 while the panel is off): band for the
    // mode the help is in, and barOnly, the panel's own, for the mode without help. The player's shifts for each mode
    // placed go.
    void AutoAdjustWindows(Settings& s, int band, int barOnly);
    // IPlayer::GetLoginStatus() once the character is in the world; 0 and 1 are the title screen and character select.
    inline constexpr uint8_t kLoginInGame = 2;
    // The bar's settings are on and the character is in game.
    bool BarRuns(const Settings& s, uint8_t loginStatus);

    // The fonts to offer: the installed families (as GDI lists them, once per character set and with vertical variants
    // named @...), each once and sorted, with current added when it is not installed, and never the game's own two.
    std::vector<std::string> FontChoices(std::vector<std::string> installed, const std::string& current);
    // The fonts a nameplate can be set in: the game's own two, then the font list.
    std::vector<std::string> NameplateFontChoices(const std::vector<std::string>& fonts);
    // Whether a nameplate is set in the game's own font: the default, a line's or the badges'.
    bool UsesGameFont(const Settings& s);
    // The theme picked, as far as the settings know it: Custom's palette, or a built-in one's (Vanilla's without the
    // game's art, which PaletteFor in bar_look.h adds). Its background, window type, frame and separators are its own.
    const BarPalette& OwnPalette(const Settings& s);

    // Key/value persistence: Ashita's configuration manager in the plugin, an in-memory map in tests.
    class SettingsStore
    {
    public:
        virtual ~SettingsStore()                                             = default;
        virtual std::string GetString(const char* key, const char* fallback) = 0;
        virtual void Set(const char* key, const char* value)                 = 0;
    };

    // Out-of-range values become the nearest limit; NaN and infinities become the default.
    Settings Clamp(Settings s);
    // Whether HeadsUp shows a pointer of its own in place of the game's: another icon, or the game's at another size.
    bool PointerSwapped(const Settings& s);
    // The file's settings; a new install's are HeadsUp's defaults (default_settings.h).
    Settings LoadSettings(SettingsStore& store);
    // What a new install has: Settings' own with default_settings.h over them.
    Settings DefaultSettings();
    void SaveSettings(const Settings& s, SettingsStore& store);

    // What a save has to tell the player: a failure once for a run of them, and the save that ends such a run.
    enum class SaveNotice : uint8_t { None, Failed, Saved };

    // A store that passes a Set on only when the key's text is not the text the store holds: a Set is Ashita writing
    // the key into the file, about a third of a millisecond, and a save makes one for every key.
    class ChangedKeys final : public SettingsStore
    {
    public:
        explicit ChangedKeys(SettingsStore& store)
            : m_Store(store)
        {}
        std::string GetString(const char* key, const char* fallback) override { return m_Store.GetString(key, fallback); }
        void Set(const char* key, const char* value) override;
        // LoadSettings, after asking the store for the text it holds for every key a save writes, so the next save
        // passes on exactly the keys the file lacks or holds otherwise than a save writes them.
        Settings Load();
        // Whether the file loaded is owed a save without waiting for a change: it holds settings, and lacks the
        // designs' marker or has an older build's version.
        bool Outdated() const { return m_Outdated; }
        // SaveSettings. Returns every key passed on that the file is not known to hold, with its latest text: those
        // of this save, and of earlier ones that ReadBack was not told the file holds.
        const std::vector<KeyText>& Save(const Settings& s);
        // After a save, whether the file as read back holds the keys Save returned: only then are they owed no more.
        SaveNotice ReadBack(bool fileHoldsThem);
        bool Unwritten() const { return !m_Unwritten.empty(); }

    private:
        SettingsStore& m_Store;
        std::unordered_map<std::string, std::string> m_Held; // each key's text as the store holds it
        std::vector<KeyText> m_Unwritten;
        bool m_Passing  = true; // off while Load lists the keys of a save
        bool m_Outdated = false;
        bool m_Failing  = false; // the last read-back did not hold the keys, which the player was told
    };

    // Opaque D3DCOLOR (A8R8G8B8) for D3DRS_TEXTUREFACTOR, from a color Clamp has kept in 0-1.
    uint32_t ToArgb(const Color& c);
}
