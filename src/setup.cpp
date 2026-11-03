#include "setup.h"

#include "cursor_picture.h"

#include <algorithm>
#include <cstdio>
#include <initializer_list>
#include <iterator>
#include <string_view>
#include <vector>

namespace headsup
{
    namespace
    {
        KindSettings& KindOf(Settings& s, PlateKind kind) { return s.Kind(kind); }
        const KindSettings& DefaultKind(PlateKind kind)
        {
            static const std::array<KindSettings, kPlateKindCount> kDefaults = DefaultKinds();
            return kDefaults[static_cast<size_t>(kind)];
        }
        void EveryRow(Settings& s, PlateKind kind, PlateMode mode)
        {
            KindSettings& k = KindOf(s, kind);
            for (int row = 0; row < RowCountOf(kind); ++row)
                k.rows[row].plate = mode;
        }

        template <BarTheme T>
        void Theme(Settings& s)
        {
            s.barTheme = T;
        }
        const SetupOption kThemes[]{{"phoenix-gold", Theme<BarTheme::PhoenixGold>}, {"umbra-dark", Theme<BarTheme::UmbraDark>},
            {"phoenix-red-ornate", Theme<BarTheme::PhoenixRedOrnate>}, {"phoenix-red-pills", Theme<BarTheme::PhoenixRedPills>},
            {"aurora", Theme<BarTheme::Aurora>}, {"game-window", Theme<BarTheme::GameWindow>}};
        const SetupChoice kThemeChoices[]{{"theme", WindowWord::SetupChoiceTheme, WindowList::SetupTheme, kThemes}};
        const SettingPiece kThemeSettings[]{SettingPiece::WindowType, SettingPiece::DefaultFont,
            SettingPiece::BaseFontSize, SettingPiece::BaseIconSize,
            SettingPiece::TextFont, SettingPiece::TextSize,
            SettingPiece::Size};

        // The mob rows of what would attack you, notorious monsters and their placeholders, and claimed mobs.
        constexpr Category kAttackRows[]{Category::WillAttack, Category::NmWillAttack, Category::NmWontAttack, Category::Placeholder,
            Category::PartyClaim, Category::OtherClaim};
        void AttackersShown(Settings& s)
        {
            EveryRow(s, PlateKind::Mobs, PlateMode::WhenTargeted);
            for (Category row : kAttackRows)
                KindOf(s, PlateKind::Mobs).rows[CategoryIndex(row)].plate = PlateMode::Shown;
        }
        const SetupOption kMobPlates[]{{"all", [](Settings& s) { EveryRow(s, PlateKind::Mobs, PlateMode::Shown); }},
            {"attackers", AttackersShown}, {"target", [](Settings& s) { EveryRow(s, PlateKind::Mobs, PlateMode::WhenTargeted); }},
            {"game", [](Settings& s) { EveryRow(s, PlateKind::Mobs, PlateMode::Vanilla); }}};

        // A design of the default mob lines, the top ones of which show nothing.
        void MobLines(Settings& s, int emptyLines, OrnamentPlace ornament)
        {
            KindSettings& mobs = KindOf(s, PlateKind::Mobs);
            const KindSettings& defaults = DefaultKind(PlateKind::Mobs);
            for (int line = 0; line < kPlateLines; ++line)
            {
                mobs.lines[line] = defaults.lines[line];
                if (line < emptyLines) mobs.lines[line].text.clear();
            }
            mobs.ornament       = defaults.ornament;
            mobs.ornament.place = ornament;
        }
        const SetupOption kMobDesigns[]{{"full", [](Settings& s) { MobLines(s, 0, DefaultKind(PlateKind::Mobs).ornament.place); }},
            {"simple", [](Settings& s) { MobLines(s, 1, OrnamentPlace::Disabled); }},
            {"name", [](Settings& s) { MobLines(s, 2, OrnamentPlace::Disabled); }}};

        // Outlines on some mob rows: the claim rows follow the row under them, the dead have none.
        void MobOutlines(Settings& s, std::span<const Category> rows, OutlineMode mode)
        {
            s.outlines         = true;
            KindSettings& mobs = KindOf(s, PlateKind::Mobs);
            for (int row = 0; row < kCategoryCount; ++row)
                mobs.rows[row].outline = IsClaimRow(static_cast<Category>(row)) ? OutlineMode::ByNature : OutlineMode::Disabled;
            for (Category row : rows)
                mobs.rows[CategoryIndex(row)].outline = mode;
        }
        constexpr Category kOutlinedByDefault[]{Category::WillAttack, Category::NmWillAttack, Category::NmWontAttack, Category::Placeholder};
        constexpr Category kLivingRows[]{Category::WillAttack, Category::WontAttack, Category::Unknown, Category::NmWillAttack,
            Category::NmWontAttack, Category::Placeholder};
        const SetupOption kOutlines[]{{"attackers", [](Settings& s) { MobOutlines(s, kOutlinedByDefault, OutlineMode::Shown); }},
            {"all", [](Settings& s) { MobOutlines(s, kLivingRows, OutlineMode::Shown); }},
            {"target", [](Settings& s) { MobOutlines(s, kLivingRows, OutlineMode::WhenTargeted); }},
            // The rows are kept, for outlines turned back on in the settings window to show as they did.
            {"none", [](Settings& s) { s.outlines = false; }}};
        template <int Percent>
        void Thickness(Settings& s)
        {
            s.outlineThickness = Percent;
        }
        const SetupOption kThicknesses[]{{"thin", Thickness<50>}, {"normal", Thickness<100>}, {"thick", Thickness<200>}};

        template <CursorStyle Style>
        void Cursor(Settings& s)
        {
            if (Style != CursorStyle::Vanilla)
                s.cursorWidth = std::clamp(NaturalCursorWidth(Style, s.cursorHeight), kMinCursorSide, kMaxCursorSide);
            s.cursorStyle = Style;
            s.cursorFile.clear();
        }
        const SetupOption kCursors[]{{"feather", Cursor<CursorStyle::Feather>}, {"arrow", Cursor<CursorStyle::Triangle>},
            {"chocobo", Cursor<CursorStyle::Chocobo>}, {"game", Cursor<CursorStyle::Vanilla>}};
        const SettingPiece kCursorSettings[]{SettingPiece::Color};

        template <PlateKind Kind, PlateMode Mode>
        void Plates(Settings& s)
        {
            EveryRow(s, Kind, Mode);
        }
        const SetupOption kPlayers[]{{"headsup", Plates<PlateKind::Players, PlateMode::Shown>}, {"game", Plates<PlateKind::Players, PlateMode::Vanilla>}};
        const SetupOption kSelf[]{{"shown", Plates<PlateKind::Self, PlateMode::Shown>}, {"hidden", Plates<PlateKind::Self, PlateMode::Hidden>},
            {"game", Plates<PlateKind::Self, PlateMode::Vanilla>}};
        const SetupOption kNpcs[]{{"headsup", Plates<PlateKind::Npcs, PlateMode::Shown>}, {"game", Plates<PlateKind::Npcs, PlateMode::Vanilla>}};
        const SetupOption kPets[]{{"targeted", Plates<PlateKind::Pets, PlateMode::WhenTargeted>}, {"shown", Plates<PlateKind::Pets, PlateMode::Shown>},
            {"game", Plates<PlateKind::Pets, PlateMode::Vanilla>}};

        // The player icons' tags, which a new install's player name line has before the name.
        constexpr std::string_view kPlayerName = kTagPlayerName;
        static_assert(kPlayerName.ends_with(kTagName));
        constexpr std::string_view kPlayerIcons = kPlayerName.substr(0, kPlayerName.size() - std::string_view(kTagName).size());
        // Yours and other players' name lines, with the icons in it or on the line above; that line is emptied of
        // them when they go back beside the name, and kept when it holds something else.
        void PlayerIcons(Settings& s, const std::string& name, bool above)
        {
            for (PlateKind kind : {PlateKind::Players, PlateKind::Self})
            {
                KindSettings& k = KindOf(s, kind);
                k.lines[kBottomLine].text = name;
                if (above) k.lines[kMiddleLine].text = kPlayerIcons;
                else if (k.lines[kMiddleLine].text == kPlayerIcons) k.lines[kMiddleLine].text.clear();
            }
        }
        const SetupOption kPlayerIconPlaces[]{
            {"left", [](Settings& s) { PlayerIcons(s, std::string(kPlayerIcons) + kTagName, false); }},
            {"right", [](Settings& s) { PlayerIcons(s, kTagName + std::string(kPlayerIcons), false); }},
            {"above", [](Settings& s) { PlayerIcons(s, kTagName, true); }}};

        const SetupChoice kNameplateParts[]{{"mob-plates", WindowWord::SetupChoiceMobPlates, WindowList::SetupMobPlates, kMobPlates},
            {"mob-design", WindowWord::SetupChoiceMobDesign, WindowList::SetupMobDesign, kMobDesigns},
            {"outlines", WindowWord::SetupChoiceOutlines, WindowList::SetupOutlines, kOutlines},
            {"outline-thickness", WindowWord::SetupChoiceOutlineThickness, WindowList::SetupOutlineThickness, kThicknesses},
            {"players", WindowWord::SetupChoicePlayers, WindowList::SetupPlayers, kPlayers}, {"self", WindowWord::SetupChoiceSelf, WindowList::SetupSelf, kSelf},
            {"npcs", WindowWord::SetupChoiceNpcs, WindowList::SetupNpcs, kNpcs}, {"pets", WindowWord::SetupChoicePets, WindowList::SetupPets, kPets},
            {"player-icons", WindowWord::SetupChoicePlayerIcons, WindowList::SetupPlayerIcons, kPlayerIconPlaces}};

        // A preset picks an option of each of its parts, by key, in the parts' order.
        void Preset(Settings& s, std::span<const SetupChoice> parts, std::initializer_list<std::string_view> keys)
        {
            const std::string_view* key = keys.begin();
            for (const SetupChoice& part : parts)
            {
                for (const SetupOption& option : part.options)
                    if (option.key == *key) option.apply(s);
                ++key;
            }
        }
        // In kNameplateParts' order: which mobs, what it shows, outlines, thickness, players, you, NPCs, pets, player icons.
        const SetupOption kNameplatePresets[]{
            {"full", [](Settings& s) { Preset(s, kNameplateParts, {"all", "full", "attackers", "normal", "headsup", "shown", "headsup", "targeted", "left"}); }},
            {"simple", [](Settings& s) { Preset(s, kNameplateParts, {"attackers", "simple", "attackers", "normal", "headsup", "hidden", "headsup", "targeted", "left"}); }},
            {"minimal", [](Settings& s) { Preset(s, kNameplateParts, {"target", "name", "target", "normal", "game", "game", "game", "game", "left"}); }}};
        const SetupChoice kNameplateChoices[]{{"nameplates", WindowWord::SetupChoiceNameplates, WindowList::SetupNameplates, kNameplatePresets}};

        // The panel shown with these widgets and no others; their copies go with the others.
        void PanelOf(Settings& s, std::span<const BarWidget> shown)
        {
            s.showBar = true;
            for (int w = 0; w < kBarWidgetCount; ++w)
                SetWidgetShown(s, static_cast<BarWidget>(w), std::ranges::find(shown, static_cast<BarWidget>(w)) != shown.end());
        }
        void DefaultPanel(Settings& s)
        {
            std::vector<BarWidget> shown;
            for (int w = 0; w < kBarWidgetCount; ++w)
                if (kBarWidgetDefaults[static_cast<size_t>(w)]) shown.push_back(static_cast<BarWidget>(w));
            PanelOf(s, shown);
        }
        constexpr BarWidget kSimplePanel[]{BarWidget::Zone, BarWidget::VanaTime, BarWidget::Job, BarWidget::Target, BarWidget::Currency, BarWidget::Bags};
        constexpr BarWidget kMinimalPanel[]{BarWidget::Zone, BarWidget::VanaTime, BarWidget::Target};
        // The widgets are kept, for the panel turned back on in the settings window to show as it did.
        const SetupOption kPanels[]{{"full", DefaultPanel}, {"simple", [](Settings& s) { PanelOf(s, kSimplePanel); }},
            {"minimal", [](Settings& s) { PanelOf(s, kMinimalPanel); }}, {"hidden", [](Settings& s) { s.showBar = false; }}};
        const SetupChoice kPanelChoices[]{{"panel", WindowWord::SetupChoicePanel, WindowList::SetupPanel, kPanels}};

        template <PointerIcon Icon>
        void Pointer(Settings& s)
        {
            s.pointerIcon = Icon;
            s.pointerFile.clear();
        }
        const SetupOption kPointers[]{{"chocobo", Pointer<PointerIcon::Chocobo>}, {"game", Pointer<PointerIcon::Vanilla>}};
        const SetupChoice kOptionChoices[]{{"cursor", WindowWord::SetupChoiceCursor, WindowList::SetupCursor, kCursors},
            {"pointer", WindowWord::SetupChoicePointer, WindowList::SetupPointer, kPointers}};
        const SettingPiece kOptionSettings[]{SettingPiece::AutoCheck,
            SettingPiece::LookUpPlayers, SettingPiece::StackNames,
            SettingPiece::HideDuringConversation};

        const SetupPage kPages[]{
            {SetupStep::Welcome, "welcome", WindowWord::SetupStepWelcome, {}, {}, {}, {}},
            {SetupStep::Theme, "theme", WindowWord::SetupStepTheme, kThemeChoices, {}, {}, kThemeSettings},
            {SetupStep::Nameplates, "nameplates", WindowWord::SetupStepNameplates, kNameplateChoices, {}, kNameplateParts, {}},
            {SetupStep::TopPanel, "top-panel", WindowWord::SetupStepTopPanel, kPanelChoices, {}, {}, {}},
            {SetupStep::Options, "options", WindowWord::SetupStepOptions, kOptionChoices, kOptionSettings, {}, kCursorSettings},
        };
        static_assert(std::size(kPages) == kSetupSteps);

        constexpr BarWidget kWhere[]{BarWidget::Zone, BarWidget::Weather, BarWidget::World};
        constexpr BarWidget kTime[]{BarWidget::VanaTime, BarWidget::EarthTime};
        constexpr BarWidget kYou[]{BarWidget::Job, BarWidget::Currency, BarWidget::Bags, BarWidget::Party, BarWidget::Target};
        constexpr BarWidget kSystem[]{BarWidget::Performance, BarWidget::GameHelp, BarWidget::Signet};
        const SetupWidgetGroup kWidgetGroups[]{{"where", kWhere}, {"time", kTime}, {"you", kYou}, {"system", kSystem}};

        constexpr const char* kBackupExtension = ".ini";
        constexpr int kFirstAgain              = 2; // the number a second backup of the same second takes
    }

    std::span<const SetupPage> SetupPages() { return kPages; }
    const SetupPage& SetupPageOf(SetupStep step) { return kPages[static_cast<size_t>(step)]; }

    std::optional<size_t> PickedOption(const SetupChoice& choice, const Settings& s)
    {
        for (size_t i = 0; i < choice.options.size(); ++i)
        {
            Settings picked = s;
            choice.options[i].apply(picked);
            if (picked == s) return i;
        }
        return std::nullopt;
    }

    std::span<const SetupWidgetGroup> SetupWidgetGroups() { return kWidgetGroups; }

    void SetWidgetShown(Settings& s, BarWidget widget, bool shown)
    {
        if (shown)
        {
            s.Widget(PanelWidget{widget}).on = true;
            return;
        }
        s.Widget(PanelWidget{widget}).on = false;
        for (int copy = 1; copy < kMostCopies; ++copy)
            RemoveWidgetCopy(s, PanelWidget{widget, copy});
    }

    std::string SetupBackupName(const SetupTime& at, const std::function<bool(const std::string&)>& taken)
    {
        char stem[64];
        std::snprintf(stem, sizeof(stem), "settings-before-setup-%04d-%02d-%02d-%02d%02d%02d", at.year, at.month, at.day, at.hour,
            at.minute, at.second);
        std::string name = std::string(stem) + kBackupExtension;
        for (int again = kFirstAgain; taken(name); ++again)
            name = std::string(stem) + "-" + std::to_string(again) + kBackupExtension;
        return name;
    }
}
