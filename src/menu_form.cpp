#include "menu_form.h"

#include "bar_document.h"
#include "bar_look.h"
#include "bar_theme.h"
#include "icon_adjust.h"
#include "cursor_picture.h"
#include "icons.h"
#include "menu_text.h"
#include "world_info.h"
#include "native_locate.h"
#include "plate_settings.h"
#include "plate_template.h"
#include "pointer_keys.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <initializer_list>
#include <optional>
#include <span>
#include <utility>

namespace headsup
{
    namespace
    {
        constexpr const char* kChip        = "btn raised chip";
        constexpr const char* kQuietChip   = "btn flat chip";
        constexpr const char* kResetPill   = "btn wash reset-all"; // a card's width, under the cards
        constexpr const char* kSendPill    = "btn wash wide";
        constexpr const char* kErrorNote   = "error";
        constexpr const char* kFeedbackOtherId = "other";
        constexpr const char* kNavStart    = "nav-";
        constexpr const char* kSetupPageKey = "setup", *kSetupButtons = "buttons";
        constexpr int kNoOption = -1; // a choice whose settings no option holds
        constexpr const char* kWidgetChip  = "chip "; // a tray chip's name in its card, before its widget's number
        constexpr const char* kChipToggle  = " on";   // after it, its toggle's
        constexpr const char* kChipClone   = " clone"; // and its clone button's
        constexpr const char* kChipRemove  = " remove"; // and a copy's remove button's
        // A designer chip's name on a line and, for a created widget, in the lists, before its widget's slot and its part.
        constexpr const char* kPartChip = "part ", *kListedPartChip = "lpart ";
        // A Pop-out Designer chip's name in a row and in the lists, before its widget's slot and its card.
        constexpr const char* kCardChip = "card ", *kListedCardChip = "pcard ";
        constexpr int kFirstSense = static_cast<int>(DetectionIcon::Link); // Detection Icons' first, after the aggression

        // The picture a designer chip shows before its name, or none.
        std::string PartPicture(const PartMark& mark)
        {
            if (mark.icon) return IconSource(*mark.icon);
            if (mark.picture != GamePicture::None) return GamePictureSource(mark.picture);
            return {};
        }
        // A currency as its dropdown's row shows it: its picture, or the room of one, then its name.
        std::string CurrencyRow(int currency, Nation yours)
        {
            const Span mark = CurrencyMark(currency, yours);
            const std::string source = mark.coin                        ? IconSource(Icon::Gil)
                                       : mark.picture != GamePicture::None ? GamePictureSource(mark.picture)
                                       : mark.item != 0                    ? ItemSource(mark.item)
                                                                           : std::string();
            const IconAdjust adjust   = mark.coin ? AdjustOf(Icon::Gil) : AdjustOf(mark.picture);
            const std::string picture = source.empty() ? "<div class=\"omark\"></div>"
                                                       : "<img class=\"omark\" src=\"" + source + "\"" +
                                                             AdjustStyle(adjust) + "/>";
            return picture + EscapedRml(CurrencyName(currency, yours));
        }
        constexpr const char* kWidgetNameClass = "caption widget-name"; // over each widget's settings in a combined popup
        constexpr float kAddWindowShare    = 0.8f;    // of a dropdown's width: a group's "+ Add Window"
        constexpr size_t kMaxFindText      = 31;      // letters of a window's name to look for

        const std::vector<std::string>& PlateFonts(MenuFormState& state, const std::vector<std::string>& fonts)
        {
            if (state.plateFontsFrom != fonts)
            {
                state.plateFontsFrom = fonts;
                state.plateFonts     = NameplateFontChoices(fonts);
            }
            return state.plateFonts;
        }

        // What a page is drawn with and from.
        struct Context
        {
            Form& form;
            Settings& s;
            const MenuReadings& readings;
            MenuFormState& state;
            const std::vector<std::string>& fonts;
            const std::vector<std::string>& menuFonts;
            const WindowLayout& layout;
            std::optional<PlateKind> kind; // an entity page's
            bool plates        = false;    // the kind has a row that draws a nameplate
            bool outlineColors = false;    // and one outlined in its own color pick
            const std::vector<std::string>& plateFonts = PlateFonts(state, fonts);
        };

        std::string Words(const char* text) { return std::string(Said(text)); }

        // A label that shows name and names its control's id by key, and scope where one control is drawn for
        // several lines or widgets, whatever name says.
        std::string Labeled(const char* name, std::string_view key, std::string_view scope = {})
        {
            return Words(name) + std::string(kIdMark) + std::string(key) + std::string(scope);
        }
        const char* Word(const Context& c, WindowWord word) { return c.layout.WordOf(word).name; }
        const char* WordTip(const Context& c, WindowWord word) { return c.layout.WordOf(word).tip; }
        std::string_view FileName(std::string_view path) { return path.substr(path.find_last_of("\\/") + 1); }

        // A cutoff as its dropdown shows it: the rank, which no chance stands for without a mob, or Off past the end.
        std::string CutoffText(const Context& c, int rank)
        {
            return rank == kNoWeakRank || rank == kNoResistRank ? Words(Word(c, WindowWord::CutoffOff)) : std::to_string(rank);
        }
        // weak: the cutoff is a weakness's; a resistance's otherwise.
        std::string CutoffTip(const Context& c, int rank, bool weak)
        {
            if (rank == kNoWeakRank || rank == kNoResistRank) return Words(WordTip(c, weak ? WindowWord::WeakOff : WindowWord::ResistOff));
            return Filled(Said(WordTip(c, weak ? WindowWord::WeakRank : WindowWord::ResistRank)), kCountMark, std::to_string(rank));
        }

        std::string Pixels(int px) { return Formatted("%d px", px); }
        // A size as pixels, but the word at lowest, where the slider's lowest stands for no size of its own.
        auto PixelsOr(const char* word, int lowest)
        {
            return [=](int px) { return px == lowest ? Words(word) : Pixels(px); };
        }

        // A tip that says how near a fade begins.
        std::string Near(const char* tip) { return Filled(Said(tip), kNearMark, Formatted("%.0f", kFadeNearYalms)); }

        // A list's names, for the kit's rows of choices and named steps.
        std::vector<const char*> NamesOf(std::span<const LayoutOption> options)
        {
            std::vector<const char*> names;
            for (const LayoutOption& option : options)
                names.push_back(option.name != nullptr ? option.name : "");
            return names;
        }

        // A dropdown of a list's options for a value of the enum they name, in its order.
        template <typename Enum>
        void OptionsDropdown(Form& form, const std::string& label, Enum& value, std::span<const LayoutOption> options,
            const char* tip)
        {
            int picked = static_cast<int>(value);
            form.NumberDropdown(label.c_str(), picked, 0, static_cast<int>(options.size()) - 1,
                [&](int i) { return Words(options[static_cast<size_t>(i)].name); }, tip, 0.0f,
                [&](int i) { return Words(options[static_cast<size_t>(i)].tip); });
            value = static_cast<Enum>(picked);
        }

        // Whether a mob's design holds one of these tags, which the settings of a card show through.
        bool Uses(const Settings& s, std::initializer_list<Tag> tags)
        {
            return std::ranges::any_of(tags, [&](Tag tag) { return UsesTag(s.Kind(PlateKind::Mobs), tag); });
        }
        // What a card says while no line of a design holds one of these tags.
        std::string TagsNote(const Context& c, const KindSettings& kind, std::initializer_list<Tag> tags, WindowWord sentence)
        {
            return UnusedTagsNote(kind, tags, Word(c, sentence), c.layout.OptionsOf(WindowList::Tags), Word(c, WindowWord::Or));
        }

        // The pages as pills, the picked one filled; a child page is indented.
        void Sidebar(Context& c)
        {
            Form& form = c.form;
            form.BeginBlock(kSidebarBlock);
            const bool kindPages = KindPagesShown(c.s);
            for (const LayoutPage& page : c.layout.pages)
            {
                if (!kindPages && KindOfPage(page.page)) continue;
                const std::string id = kNavStart + std::string(page.key);
                if (form.Clicked(id) && c.state.page != page.page)
                {
                    c.state.Leave();
                    c.state.page     = page.page;
                    form.scrollToTop = true;
                }
                form.Raw(std::string("<div class=\"nav") + (page.child ? " child" : "") + "\" id=\"" + id + "\">" +
                         EscapedRml(page.name) + "</div>");
            }
            for (const LayoutPage& page : c.layout.pages)
                if (kindPages || !KindOfPage(page.page))
                    form.Set(kNavStart + std::string(page.key), FormValueKind::Class, kPickedClass, {}, page.page == c.state.page);
        }

        // A row of keys: each a chip that a click removes, and while there are fewer than most, a chip that takes
        // the next key pressed that was not already held.
        void Keys(Context& c, const std::string& label, const char* tip, std::vector<uint8_t>& keys, size_t most,
            KeyCapture& capture)
        {
            Form& form = c.form;
            form.Row(label.c_str(), tip);
            form.EndRow();
            form.Line("line chips");
            std::optional<size_t> removed;
            for (size_t i = 0; i < keys.size(); ++i)
                if (form.Button((KeyName(keys[i]) + "  x").c_str(), (label + " key " + std::to_string(i)).c_str(), kChip))
                    removed = i;
            if (capture.on)
            {
                if (form.Button(Word(c, WindowWord::KeyWaiting), (label + " cancel").c_str(), "btn flat chip waiting"))
                    capture.on = false;
                else if (form.keyDown)
                    for (const uint8_t key : HeldKeys(form.keyDown))
                    {
                        if (std::ranges::find(capture.held, key) != capture.held.end()) continue;
                        if (std::ranges::find(keys, key) == keys.end())
                        {
                            keys.push_back(key);
                            form.changed = true;
                        }
                        capture.on = false;
                        break;
                    }
            }
            else if (keys.size() < most && form.Button(Word(c, WindowWord::KeyAdd), (label + " add").c_str(), kQuietChip))
                capture = KeyCapture{true, form.keyDown ? HeldKeys(form.keyDown) : std::vector<uint8_t>{}};
            form.EndLine();
            if (removed)
            {
                keys.erase(keys.begin() + static_cast<std::ptrdiff_t>(*removed));
                form.changed = true;
            }
        }

        // Every setting back to HeadsUp's defaults, after a second press.
        void ResetAll(Context& c, const LayoutSetting& e)
        {
            Form& form = c.form;
            form.Fade(false);
            if (!c.state.confirmReset)
            {
                const std::string label = Labeled(e.name, e.key);
                if (form.Button(label.c_str(), label.c_str(), kResetPill, e.tip)) c.state.confirmReset = true;
                return;
            }
            form.Note(Word(c, WindowWord::ResetAllSettingsNote));
            form.Line();
            if (form.Button(Word(c, WindowWord::ResetAllSettingsResetEverything), "reset everything", kAccentPill))
            {
                c.s                  = DefaultSettings();
                form.changed         = true;
                c.state.confirmReset = false;
            }
            if (form.Button(Word(c, WindowWord::ResetAllSettingsKeepThem), "keep them", kRaisedPill)) c.state.confirmReset = false;
            form.EndLine();
        }

        // The views of the game's windows for one of the help's modes, the one it is in at first: where a view's
        // highest window goes, then each view with its shift (the default unless the player typed another) and the
        // windows it holds.
        void WindowGroups(Context& c)
        {
            Form& form  = c.form;
            Settings& s = c.s;
            form.Fade(!s.shiftVanillaUi);
            form.Note(Word(c, WindowWord::GroupsNote));
            const HelpMode mode = c.state.shiftsFor.value_or(WindowHelpMode(s));
            form.Line();
            for (const auto& [shown, word] : {std::pair{HelpMode::On, WindowWord::GroupsHelpOn},
                     std::pair{HelpMode::Off, WindowWord::GroupsHelpOff}})
                if (form.Button(Word(c, word), word == WindowWord::GroupsHelpOn ? "help on" : "help off",
                        shown == mode ? kRaisedPill : kFlatPill, WordTip(c, word)))
                    c.state.shiftsFor = shown;
            form.EndLine();
            const auto m = static_cast<size_t>(mode);
            form.Row(Labeled(Word(c, WindowWord::GroupsStartsAt), "starts-at").c_str(),
                WordTip(c, WindowWord::GroupsStartsAt));
            form.NumberAt("Y", "starts at", s.placedUnder[m], kMaxWindowOffset);
            s.placedUnder[m] = std::max(s.placedUnder[m], 0);
            form.EndRow();
            std::vector<ViewShift>& shifts = s.viewShifts[m];
            for (const WindowView& view : WindowViews())
            {
                const std::string id = std::string(view.id);
                const int fallback   = s.placedUnder[m] - view.top;
                const auto own       = std::ranges::find(shifts, id, &ViewShift::id);
                int dy               = own != shifts.end() ? own->dy : fallback;
                const int was        = dy;
                std::string windows;
                for (const WindowKey& key : view.windows)
                    windows += (windows.empty() ? "" : ", ") + WindowName(ShortName(key));
                form.Row(Labeled(std::string(view.name).c_str(), "view " + id).c_str(), WordTip(c, WindowWord::GroupsRowTip));
                // Named after the view: a box's own label is the box's id and -name, which "y search" would give
                // the box of the view "search/name".
                const bool reset = form.ResetAt((id + " reset").c_str(), own != shifts.end());
                form.NumberAt("Y", (id + " shift").c_str(), dy, kMaxWindowOffset);
                form.Caption(windows.c_str());
                form.EndRow();
                if (reset || (dy != was && dy == fallback))
                {
                    shifts.erase(own);
                    form.changed = true;
                }
                else if (dy != was)
                {
                    if (own == shifts.end())
                        shifts.push_back(ViewShift{id, dy});
                    else
                        own->dy = dy;
                }
            }
        }

        // Three ways to set every game window at once.
        void WindowDefaults(Context& c)
        {
            Form& form = c.form;
            form.Fade(false);
            form.Line();
            const auto button = [&](WindowWord word, const char* id, const char* look) {
                return form.Button(Word(c, word), id, look, WordTip(c, word));
            };
            if (button(WindowWord::WindowDefaultsHeadsupDefaults, "headsup defaults", kRaisedPill))
            {
                UseHeadsUpWindowDefaults(c.s);
                form.changed = true;
            }
            if (button(WindowWord::WindowDefaultsHeadsupHelp, "headsup help", kRaisedPill))
            {
                UseHelpTextDefaults(c.s);
                form.changed = true;
            }
            if (button(WindowWord::WindowDefaultsGameDefaults, "game defaults", kFlatPill))
            {
                UseGameWindowDefaults(c.s);
                form.changed = true;
            }
            form.EndLine();
        }

        // The game's windows HeadsUp moves out from under the help bar, and those it has seen there.
        void WindowList(Context& c)
        {
            Form& form         = c.form;
            Settings& s        = c.s;
            WindowFilter& list = c.state.windows;
            form.Fade(false);
            form.Note(Word(c, WindowWord::WindowListNote));
            {
                form.Row(Labeled(Word(c, WindowWord::WindowListFind), "find").c_str(), nullptr);
                form.TextAt("find window", list.text, Word(c, WindowWord::WindowListNameHint), kFilterWidth, true, kMaxFindText);
                // Which windows are listed is the window's state, and no setting to save.
                const bool was = form.changed;
                form.CheckAt("all windows", list.all, WordTip(c, WindowWord::WindowListAllWindows));
                form.changed = was;
                form.EndRow();
            }
            // The named windows and the player's, or every one, less those the filter leaves out: in one order
            // whatever is switched, so a row stays put when it changes.
            std::vector<std::string> listed;
            const auto has = [](const auto& names, const std::string& name) {
                return std::ranges::find(names, name) != names.end();
            };
            for (const std::string& name : AllWindows())
            {
                const char* label = WindowLabel(name);
                if (!(list.all || label != nullptr ||
                        std::ranges::any_of(s.pinnedWindows, [&](const WindowPlace& p) { return p.name == name; }) ||
                        has(s.hiddenWindows, name) ||
                        (c.readings.seenWindows != nullptr && has(*c.readings.seenWindows, name))))
                    continue;
                if (Matches(name, list.text.c_str()) || (label != nullptr && Matches(label, list.text.c_str())))
                    listed.push_back(name);
            }
            for (const std::string& window : listed)
            {
                const char* label      = WindowLabel(window);
                const std::string name = TitleCaseText(label != nullptr ? std::string(label) : window); // the game's own
                form.Row(Labeled(name.c_str(), "window " + window).c_str(), WordTip(c, WindowWord::WindowListRowTip));
                const auto hidden = std::ranges::find(s.hiddenWindows, window);
                bool kept         = hidden == s.hiddenWindows.end();
                form.ToggleAt(("keep " + window).c_str(), kept, Word(c, WindowWord::WindowListHide),
                    Word(c, WindowWord::WindowListKeep));
                if (kept && hidden != s.hiddenWindows.end()) s.hiddenWindows.erase(hidden);
                if (!kept)
                {
                    if (hidden == s.hiddenWindows.end()) s.hiddenWindows.push_back(window);
                    form.EndRow();
                    continue;
                }
                // The player's place for it, else where it stands now, muted: typing either number puts it there.
                const auto pin = std::ranges::find(s.pinnedWindows, window, &WindowPlace::name);
                const bool pinned = pin != s.pinnedWindows.end();
                WindowPlace at    = pinned ? *pin : WindowPlace{window};
                if (const auto* now = c.readings.windowPlaces; !pinned && now != nullptr)
                    if (const auto seen = std::ranges::find(*now, window, &WindowPlace::name); seen != now->end()) at = *seen;
                const WindowPlace was = at;
                form.NumberAt("X", ("x " + window).c_str(), at.x, kMaxWindowOffset, !pinned);
                form.NumberAt("Y", ("y " + window).c_str(), at.y, kMaxWindowOffset, !pinned);
                const bool unpin = form.ResetAt(("place " + window).c_str(), pinned);
                form.EndRow();
                if (unpin)
                {
                    s.pinnedWindows.erase(pin);
                    form.changed = true;
                }
                if (unpin || at == was) continue;
                at.x = std::max(at.x, 0);
                at.y = std::max(at.y, 0);
                if (pinned)
                    *pin = at;
                else
                    s.pinnedWindows.push_back(at);
            }
        }

        // The palette Customize Appearance shows, as the panel draws it (Vanilla's in the game's art's colors). Taken
        // from the settings as they are now: a theme picked this frame must not show the last one's looks.
        const BarPalette& ShownPalette(const Context& c) { return PaletteFor(c.s, c.readings.skin); }

        const std::vector<std::string>& ThemeFiles(const Context& c)
        {
            static const std::vector<std::string> kNoThemes;
            return c.readings.themeFiles != nullptr ? *c.readings.themeFiles : kNoThemes;
        }

        // Whether a theme's file has this name already: Windows does not tell the cases apart.
        bool ThemeTaken(const Context& c, const std::string& name)
        {
            const auto lower = [](std::string text) {
                std::ranges::transform(text, text.begin(), [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
                return text;
            };
            return std::ranges::any_of(ThemeFiles(c), [&](const std::string& file) { return lower(file) == lower(name); });
        }

        // A built-in theme's copy's name: "Aurora (custom)", or with the first number free after it.
        std::string CopyName(const Context& c, BarTheme theme)
        {
            const std::string base(Said(c.layout.OptionsOf(WindowList::BarTheme)[static_cast<size_t>(theme)].name));
            std::string name = Filled(Said(Word(c, WindowWord::ThemeCopy)), kFileMark, base);
            for (int n = 2; ThemeTaken(c, name); ++n)
                name = Filled(Filled(Said(Word(c, WindowWord::ThemeCopyAgain)), kFileMark, base), kCountMark, std::to_string(n));
            return name;
        }

        // A change to the theme's looks: a custom theme takes it, and the plugin saves its file with the settings; a
        // built-in one becomes a new custom theme, its copy with the change, and that is picked.
        void EditTheme(Context& c, const BarPalette& edited)
        {
            if (edited == ShownPalette(c)) return;
            BarPalette kept = edited;
            if (c.s.barTheme != BarTheme::Custom)
            {
                c.s.customThemeName = CopyName(c, c.s.barTheme);
                kept.parent         = c.s.barTheme;
            }
            c.s.customPalette = kept;
            c.s.barTheme      = BarTheme::Custom;
        }

        // The rest of the palette, by ThemePaletteRow.
        constexpr uint32_t BarPalette::*kThemeRowFields[] = {&BarPalette::underLine, &BarPalette::below,
            &BarPalette::accent, &BarPalette::accentEnd, &BarPalette::pill, &BarPalette::pillBorder, &BarPalette::title,
            &BarPalette::subtitle, &BarPalette::divider, &BarPalette::panelTop, &BarPalette::panelBottom,
            &BarPalette::panelBorder, &BarPalette::panelOuter, &BarPalette::text, &BarPalette::heading,
            &BarPalette::tagFill, &BarPalette::tagBorder, &BarPalette::tagText, &BarPalette::highlight,
            &BarPalette::bottomLine};
        static_assert(std::size(kThemeRowFields) == static_cast<size_t>(ThemePaletteRow::BorderColor) + 1);

        void ThemePalette(Context& c, const LayoutSetting& e)
        {
            Form& form = c.form;
            form.Fade(!BarOn(c.s));
            BarPalette edited = ShownPalette(c);
            const auto rows   = c.layout.RowsOf(e);
            const size_t first = (rows.size() + 1) / 2;
            form.Raw("<div class=\"columns\"><div class=\"column\">");
            for (size_t i = 0; i < rows.size(); ++i)
            {
                if (i == first) form.Raw("</div><div class=\"column second\">");
                const LayoutRow& row = rows[i];
                // The art's frame is in the window type's own color.
                const bool art = edited.background == BarBackground::Window;
                if (art && row.row == static_cast<uint16_t>(ThemePaletteRow::BorderColor)) continue;
                form.ArgbSwatch(Labeled(row.name, row.key, std::string(" ") + e.key).c_str(), edited.*kThemeRowFields[row.row],
                    row.tip);
            }
            form.Raw("</div></div>");
            EditTheme(c, edited);
        }

        // Set only when moved: a percent cannot say every alpha exactly.
        void ThemeOpacity(Context& c, const std::string& label, std::initializer_list<uint32_t BarPalette::*> colors, const char* tip)
        {
            BarPalette edited = ShownPalette(c);
            if (edited.background == BarBackground::Window) return; // the art has no colors of the theme's
            constexpr float kFullAlpha = static_cast<float>(kOpaqueAlpha);
            int percent = static_cast<int>(std::lround(Channel(edited.**colors.begin(), kAlphaShift) * kPercent / kFullAlpha));
            const int was = percent;
            // By whole percents: RmlUi would move a value between its steps onto one and say so, as if dragged.
            c.form.SliderInt(label.c_str(), percent, 0, kMaxOpacity, [](int shown) { return Formatted("%d%%", shown); }, tip);
            if (percent == was) return;
            const auto alpha = static_cast<uint32_t>(std::lround(static_cast<float>(percent) / kPercent * kFullAlpha));
            for (uint32_t BarPalette::* const color : colors)
                edited.*color = (edited.*color & ~kAlphaMask) | alpha << kAlphaShift;
            EditTheme(c, edited);
        }

        // The theme picked: the built-in ones, then the player's own from the themes folder, the one in use among them
        // even before the folder is read again.
        void ThemeDropdown(Context& c, const std::string& label, const char* tip)
        {
            Settings& s = c.s;
            const auto options = c.layout.OptionsOf(WindowList::BarTheme);
            Form::ListGroup builtIn{std::string(Said(Word(c, WindowWord::ThemeBuiltIn))), {}, {}};
            std::vector<BarTheme> themes;
            for (int t = 0; t < kBarThemeCount; ++t)
                if (static_cast<BarTheme>(t) != BarTheme::Custom)
                {
                    themes.push_back(static_cast<BarTheme>(t));
                    builtIn.rows.push_back(Words(options[static_cast<size_t>(t)].name));
                    builtIn.tips.push_back(Words(options[static_cast<size_t>(t)].tip));
                }
            Form::ListGroup custom{std::string(Said(Word(c, WindowWord::ThemeCustom))), ThemeFiles(c), {}};
            if (s.barTheme == BarTheme::Custom && std::ranges::find(custom.rows, s.customThemeName) == custom.rows.end())
                custom.rows.push_back(s.customThemeName);
            const int customAt = static_cast<int>(themes.size());
            int picked = s.barTheme == BarTheme::Custom
                             ? customAt + static_cast<int>(std::ranges::find(custom.rows, s.customThemeName) - custom.rows.begin())
                             : static_cast<int>(std::ranges::find(themes, s.barTheme) - themes.begin());
            const std::string shown = s.barTheme == BarTheme::Custom ? s.customThemeName : builtIn.rows[static_cast<size_t>(picked)];
            const Form::ListGroup groups[] = {builtIn, custom};
            const int was = picked;
            c.form.GroupedDropdown(label.c_str(), picked, groups, shown, tip);
            if (c.form.Opened(label)) c.state.themesRequested = true;
            if (picked == was) return;
            if (picked < customAt)
                s.barTheme = themes[static_cast<size_t>(picked)];
            else if (const std::string& name = custom.rows[static_cast<size_t>(picked - customAt)]; name != s.customThemeName || s.barTheme != BarTheme::Custom)
                c.state.themeLoad = name;
        }

        // The custom theme's name, renamed once the box is left: a name another theme has, or no file could take, is
        // put back.
        void ThemeName(Context& c, const std::string& label, const char* tip)
        {
            MenuFormState& st = c.state;
            if (st.themeNameOf != c.s.customThemeName) st.themeName = st.themeNameOf = c.s.customThemeName;
            c.form.Row(label.c_str(), tip);
            if (c.form.TextAt(label.c_str(), st.themeName, nullptr, kControlWidth, false, kMaxFileName))
            {
                if (IsFolderFileName(st.themeName) && !ThemeTaken(c, st.themeName))
                    st.themeRename = st.themeName;
                else
                    st.themeName = c.s.customThemeName;
            }
            c.form.EndRow();
        }

        // The topic, with a box beside it for what an Other topic is.
        void FeedbackTopicRow(Context& c, const LayoutSetting& e, const std::string& label, const char* tip)
        {
            FeedbackDraft& draft = c.state.feedback;
            const auto options   = c.layout.OptionsOf(e);
            int picked           = static_cast<int>(draft.topic);
            c.form.Row(label.c_str(), tip);
            c.form.DropdownAt(label.c_str(), picked, 0, kFeedbackTopicCount - 1,
                [&](int i) { return Words(options[static_cast<size_t>(i)].name); });
            draft.topic = static_cast<FeedbackTopic>(picked);
            if (draft.topic == FeedbackTopic::Other)
                c.form.TextAt(kFeedbackOtherId, draft.other, nullptr, kControlWidth, true, kMostFeedbackOther);
            c.form.EndRow();
        }

        // Send Feedback, across the card, with what became of the last press under it.
        void SendFeedback(Context& c, const std::string& label, const char* tip)
        {
            MenuFormState& st = c.state;
            c.form.Line();
            if (c.form.Button(label.c_str(), label.c_str(), kSendPill, tip) && st.feedbackStatus != FeedbackStatus::Sending)
            {
                const bool filled = FeedbackFilled(st.feedback);
                st.feedbackStatus = filled ? FeedbackStatus::Sending : FeedbackStatus::Missing;
                st.feedbackRequested = filled;
            }
            c.form.EndLine();
            switch (st.feedbackStatus)
            {
            case FeedbackStatus::None: break;
            case FeedbackStatus::Missing: c.form.Note(Word(c, WindowWord::SendFeedbackMissing), kErrorNote); break;
            case FeedbackStatus::Sending: c.form.Note(Word(c, WindowWord::SendFeedbackSending)); break;
            case FeedbackStatus::Sent: c.form.Note(Word(c, WindowWord::SendFeedbackSent)); break;
            case FeedbackStatus::Failed: c.form.Note(Word(c, WindowWord::SendFeedbackFailed), kErrorNote); break;
            }
        }

        // The profile in use, the others to use or delete, and a name to make a new one under or rename this one to.
        void ProfileFiles(Context& c)
        {
            Form& form = c.form;
            form.Fade(false);
            static const std::vector<std::string> kNone;
            static const std::string kNoProfile;
            const std::vector<std::string>& names = c.readings.profiles != nullptr ? *c.readings.profiles : kNone;
            const std::string& inUse = c.readings.profile != nullptr ? *c.readings.profile : kNoProfile;
            form.Row(Labeled(Word(c, WindowWord::ProfileFilesProfile), "profile").c_str(), WordTip(c, WindowWord::ProfileFilesProfile));
            if (!names.empty())
            {
                if (c.state.profilePicked.empty()) c.state.profilePicked = inUse;
                const auto known = std::ranges::find(names, c.state.profilePicked);
                int picked       = known != names.end() ? static_cast<int>(known - names.begin()) : 0;
                form.DropdownAt("profile file", picked, 0, static_cast<int>(names.size()) - 1,
                    [&](int i) { return names[static_cast<size_t>(i)] + (names[static_cast<size_t>(i)] == inUse ? " *" : ""); },
                    kFilterWidth);
                c.state.profilePicked = names[static_cast<size_t>(picked)];
                if (form.Button(Word(c, WindowWord::ProfileFilesUse), "use profile", kRaisedPill,
                        WordTip(c, WindowWord::ProfileFilesUse)) && c.state.profilePicked != inUse)
                    c.state.profileUse = c.state.profilePicked;
                if (form.Button(Word(c, WindowWord::ProfileFilesDelete), "delete profile", kRaisedPill,
                        WordTip(c, WindowWord::ProfileFilesDelete)) && c.state.profilePicked != inUse)
                    c.state.profileDelete = c.state.profilePicked;
            }
            if (form.Opened("profile file") || names.empty()) c.state.profilesRequested = true;
            form.EndRow();
            form.Row(Labeled(Word(c, WindowWord::ProfileFilesNew), "new").c_str(), WordTip(c, WindowWord::ProfileFilesNew));
            form.TextAt("profile name", c.state.profileName, Word(c, WindowWord::ProfileFilesNameHint), kFilterWidth, true,
                kMaxFileName);
            const bool named = IsFolderFileName(c.state.profileName) && std::ranges::find(names, c.state.profileName) == names.end();
            if (form.Button(Word(c, WindowWord::ProfileFilesCreate), "create profile") && named)
                c.state.profileNew = c.state.profileName;
            if (form.Button(Word(c, WindowWord::ProfileFilesRename), "rename profile", kRaisedPill,
                    WordTip(c, WindowWord::ProfileFilesRename)) && named && !inUse.empty())
                c.state.profileRename = c.state.profileName;
            form.EndRow();
        }

        // A copy's name as its tab and chip say it: the widget's, and after the first copy its number; a created
        // widget's own Name.
        std::string CopyName(const Context& c, PanelWidget widget)
        {
            if (widget.kind == BarWidget::Custom) return c.s.Widget(widget).name;
            const char* name = c.layout.OptionsOf(WindowList::BarWidgets)[static_cast<size_t>(widget.kind)].name;
            const std::string said(Said(name));
            return widget.copy == 0 ? said : said + " " + std::to_string(widget.copy + 1);
        }

        // A widget's parts as chips on its two lines, each with a toggle and its icon where it has one: a chip drags
        // within its line or to the other, before the chip it is dropped on or to the end. The last part on cannot be
        // switched off; trying says so under the lines until the next toggle or move. A created widget's lines take
        // any widget's parts from the lists under them, by widget, and a chip let go off the lines goes back there.
        void Designer(Context& c, PanelWidget widget)
        {
            Form& form          = c.form;
            WidgetDesign& d     = c.s.Widget(widget).design;
            const auto names    = c.layout.OptionsOf(WindowList::WidgetParts);
            const auto widgets  = c.layout.OptionsOf(WindowList::BarWidgets);
            const bool created  = widget.kind == BarWidget::Custom;
            const std::string base = kPartChip + std::to_string(SlotOf(widget)), list = kListedPartChip + std::to_string(SlotOf(widget));
            const std::string chipStart = form.Named(base) + "-", listedStart = form.Named(list) + "-";
            struct Move
            {
                int line;
                size_t index;
                int toLine;
                size_t at;
            };
            struct Put
            {
                WidgetPart part;
                int line;
                size_t at;
            };
            std::optional<Move> move; // applied after the lines are written, as the trays' drop is
            std::optional<Put> put;   // a listed chip dropped on a line
            std::optional<WidgetPart> taken; // a line's chip let go
            bool toggled = false, refused = false;
            const auto dropped = [&](const std::string& target, int toLine, size_t at) {
                const FormEvent* event = form.Took(target, kDropEvent);
                if (event == nullptr) return;
                if (event->value.compare(0, listedStart.size(), listedStart) == 0)
                {
                    const int part = std::atoi(event->value.c_str() + listedStart.size());
                    if (part >= 0 && part < kWidgetPartCount) put = Put{static_cast<WidgetPart>(part), toLine, at};
                    return;
                }
                if (event->value.compare(0, chipStart.size(), chipStart) != 0) return;
                const int part = std::atoi(event->value.c_str() + chipStart.size());
                for (int line = 0; line < kDesignLines; ++line)
                    for (size_t i = 0; i < d.lines[static_cast<size_t>(line)].size(); ++i)
                        if (static_cast<int>(d.lines[static_cast<size_t>(line)][i].part) == part) move = Move{line, i, toLine, at};
            };
            const auto picture = [&](size_t part) {
                const PartMark& mark = c.readings.partMarks[part];
                const std::string source = PartPicture(mark);
                return source.empty() ? source
                                      : "<img class=\"dmark\" src=\"" + source + "\"" +
                                            AdjustStyle(mark.icon ? AdjustOf(*mark.icon) : AdjustOf(mark.picture)) + "/>";
            };
            const auto widgetName = [&](BarWidget kind) { return std::string(Said(widgets[static_cast<size_t>(kind)].name)); };
            form.Raw("<div class=\"design\">");
            for (int line = 0; line < kDesignLines; ++line)
            {
                std::vector<DesignChip>& chips = d.lines[static_cast<size_t>(line)];
                form.Raw(std::string("<div class=\"tray ") + kDesignLineClass + "\">");
                for (size_t i = 0; i < chips.size(); ++i)
                {
                    DesignChip& chip       = chips[i];
                    const auto part        = static_cast<size_t>(chip.part);
                    const std::string name = base + " " + std::to_string(part);
                    const std::string id   = form.Id(name);
                    form.Raw(std::string("<div class=\"btn raised wchip ") + kDesignChipClass + " drag\" id=\"" + id + "\"" + TipAttribute(names[part].tip) + ">");
                    // A Pinned Status button is on while a status is pinned at its rank, which its pin sets.
                    const auto pin = static_cast<size_t>(chip.part) - static_cast<size_t>(WidgetPart::StatusPin1);
                    const bool pinned = IsPinnedStatus(chip.part);
                    if (pinned) chip.on = pin < c.s.statusPins.size();
                    bool on = chip.on;
                    if (!pinned) form.CheckAt((name + kChipToggle).c_str(), on);
                    if (on != chip.on)
                    {
                        toggled = true;
                        if (on)
                            chip.on = true;
                        else if (!TurnOff(d, chip.part, PillShowsOther(c.s, Panel(c.s), widget)))
                        {
                            refused = true;
                            form.Set(form.Named(name + kChipToggle), FormValueKind::Class, "on", {}, true);
                        }
                    }
                    std::string text(Said(names[part].name));
                    if (created && WidgetOfPart(chip.part) != BarWidget::Custom) text += " (" + widgetName(WidgetOfPart(chip.part)) + ")";
                    form.Raw(picture(part) + EscapedRml(text) + "</div>");
                    form.Set(id, FormValueKind::Class, "on", {}, chip.on);
                    form.Set(id, FormValueKind::Class, kOffClass, {}, form.Off());
                    dropped(id, line, i);
                    if (created && form.Took(id, kDragEndEvent) != nullptr) taken = chip.part;
                }
                const std::string end = form.Id(base + " end " + std::to_string(line));
                form.Raw("<div class=\"tray-end\" id=\"" + end + "\"></div></div>");
                dropped(end, line, chips.size());
            }
            form.Raw("</div>");
            // A drop also ends its drag: the drop wins.
            bool changed = false;
            if (move)
                changed = MovePart(d, move->line, move->index, move->toLine, move->at);
            else if (put)
                changed = PutPart(d, put->part, put->line, put->at);
            else if (taken && !(changed = TakePart(d, *taken, PillShowsOther(c.s, Panel(c.s), widget))))
                refused = true;
            if (changed) form.changed = true;
            if (refused)
                c.state.partRefused = widget;
            else if ((toggled || changed) && c.state.partRefused == widget)
                c.state.partRefused.reset();
            if (c.state.partRefused == widget)
                form.Raw(std::string("<div class=\"error\">") + EscapedRml(Said(Word(c, WindowWord::WidgetTabsDesignLastOn))) +
                         "</div>");
            if (!created) return;
            form.Note(Word(c, WindowWord::WidgetTabsDesignAdd));
            std::optional<BarWidget> group;
            for (const WidgetPart part : PartsOf(BarWidget::Custom))
            {
                if (HoldsPart(d, part)) continue;
                if (const BarWidget of = WidgetOfPart(part); of != group)
                {
                    if (group) form.Raw("</div>");
                    form.Raw("<div class=\"tray-name\">" + EscapedRml(of == BarWidget::Custom ? CopyName(c, widget) : widgetName(of)) + "</div>");
                    form.Raw(std::string("<div class=\"tray ") + kPaletteClass + "\">");
                    group = of;
                }
                const auto at        = static_cast<size_t>(part);
                const std::string id = form.Id(list + " " + std::to_string(at));
                form.Raw(std::string("<div class=\"btn raised wchip ") + kDesignChipClass + " drag\" id=\"" + id + "\"" +
                         TipAttribute(names[at].tip) + ">" + picture(at) + EscapedRml(Said(names[at].name)) + "</div>");
                form.Set(id, FormValueKind::Class, kOffClass, {}, form.Off());
            }
            if (group) form.Raw("</div>");
        }

        // A widget's pop-out as rows of chips, a card each, then the lists of the cards it does not show, by widget, its
        // own first: a chip drags within its row, to another, or onto the empty row after them for a new one; a row's chip
        // let go off every row leaves the pop-out. A row of kMostCardsInRow takes no other card; trying says so under the
        // rows until the next change.
        void PopupDesigner(Context& c, PanelWidget widget)
        {
            Form& form            = c.form;
            PopupDesign& d        = c.s.Widget(widget).popup;
            const auto names      = c.layout.OptionsOf(WindowList::PopupCards);
            const auto widgets    = c.layout.OptionsOf(WindowList::BarWidgets);
            const std::string row = kCardChip + std::to_string(SlotOf(widget)), list = kListedCardChip + std::to_string(SlotOf(widget));
            const std::string rowStart = form.Named(row) + "-", listStart = form.Named(list) + "-";
            const auto widgetName = [&](BarWidget kind) { return std::string(Said(widgets[static_cast<size_t>(kind)].name)); };
            const auto cardIn     = [&](const std::string& id) -> std::optional<CardKind> {
                for (const std::string* start : {&rowStart, &listStart})
                    if (id.compare(0, start->size(), *start) == 0)
                    {
                        const int card = std::atoi(id.c_str() + start->size());
                        if (card >= 0 && card < kCardKindCount) return static_cast<CardKind>(card);
                    }
                return std::nullopt;
            };
            struct Drop
            {
                CardKind card;
                size_t row, at;
            };
            std::optional<Drop> drop; // applied after the rows are written, as the Widget Designer's move
            std::optional<CardKind> taken; // a row's chip let go
            const auto dropped = [&](const std::string& target, size_t r, size_t at) {
                const FormEvent* event = form.Took(target, kDropEvent);
                if (event == nullptr) return;
                if (const std::optional<CardKind> card = cardIn(event->value)) drop = Drop{*card, r, at};
            };
            const auto chip = [&](const std::string& name, CardKind card, bool shown) {
                const std::string id      = form.Id(name);
                const LayoutOption& said  = names[static_cast<size_t>(card)];
                std::string text(Said(said.name));
                if (shown && WidgetOfCard(card) != widget.kind) text += " (" + widgetName(WidgetOfCard(card)) + ")";
                form.Raw(std::string("<div class=\"btn raised wchip ") + kDesignChipClass + " drag\" id=\"" + id + "\"" +
                         TipAttribute(said.tip) + ">" + EscapedRml(text) + "</div>");
                form.Set(id, FormValueKind::Class, "on", {}, shown);
                form.Set(id, FormValueKind::Class, kOffClass, {}, form.Off());
                return id;
            };
            form.Raw("<div class=\"design\">");
            for (size_t r = 0; r <= d.rows.size(); ++r)
            {
                const bool last = r == d.rows.size();
                form.Raw(std::string("<div class=\"tray ") + kDesignLineClass + "\">");
                for (size_t i = 0; !last && i < d.rows[r].size(); ++i)
                {
                    const CardKind card  = d.rows[r][i];
                    const std::string id = chip(row + " " + std::to_string(static_cast<int>(card)), card, true);
                    dropped(id, r, i);
                    if (form.Took(id, kDragEndEvent) != nullptr) taken = card;
                }
                const std::string end = form.Id(row + " end " + std::to_string(r));
                form.Raw(std::string("<div class=\"tray-end") + (last ? std::string(" ") + kHintClass : "") + "\" id=\"" + end + "\">" +
                         (last ? EscapedRml(Said(Word(c, WindowWord::WidgetTabsPopupNewRow))) : "") + "</div></div>");
                dropped(end, r, last ? 0 : d.rows[r].size());
            }
            form.Raw("</div>");
            const bool refused = drop && !RowTakes(d, drop->row, drop->card);
            const bool changed = drop ? !refused && PlaceCard(d, drop->card, drop->row, drop->at) : taken && RemoveCard(d, *taken);
            if (changed) form.changed = true;
            if (refused)
                c.state.popupRefused = widget;
            else if (changed && c.state.popupRefused == widget)
                c.state.popupRefused.reset();
            if (c.state.popupRefused == widget)
                form.Raw(std::string("<div class=\"error\">") + EscapedRml(Said(Word(c, WindowWord::WidgetTabsPopupRowFull))) +
                         "</div>");
            form.Note(Word(c, WindowWord::WidgetTabsPopupAdd));
            std::vector<BarWidget> order{widget.kind};
            for (int k = 0; k < kBarWidgetCount; ++k)
                if (static_cast<BarWidget>(k) != widget.kind) order.push_back(static_cast<BarWidget>(k));
            for (const BarWidget kind : order)
            {
                std::vector<CardKind> cards = CardsOf(kind);
                std::erase_if(cards, [&](CardKind card) { return HoldsCard(d, card); });
                if (cards.empty()) continue;
                form.Raw("<div class=\"tray-name\">" + EscapedRml(widgetName(kind)) + "</div>");
                form.Raw(std::string("<div class=\"tray ") + kPaletteClass + "\">");
                for (const CardKind card : cards)
                    chip(list + " " + std::to_string(static_cast<int>(card)), card, false);
                form.Raw("</div>");
            }
        }

        // One widget's options: what it shows and its own settings, as the layout orders them, a row the widget has
        // nothing for left out; then its Clone and, on a copy, its Remove.
        void WidgetOptions(Context& c, const LayoutSetting& e, PanelWidget widget)
        {
            Form& form  = c.form;
            Settings& s = c.s;
            headsup::WidgetOptions& o = s.Widget(widget);
            const BarWidget kind      = widget.kind;
            const bool jobs = kind == BarWidget::Job && PartOn(o.design, WidgetPart::JobJobs);
            const bool bar  = kind == BarWidget::Job && PartOn(o.design, WidgetPart::JobBar);
            const bool off = !BarOn(s) || !o.on;
            // Its pill's spacing, background, rounding and pop-out are its first widget's, set there alone.
            const bool pillLooks = HasOwnLooks(kind) && Panel(s).group[SlotOf(widget)] == widget;
            const char* topPanel = Word(c, WindowWord::WidgetTabsTopPanel);
            // Each copy's rows have ids of their own: nothing open or typed is carried to another tab.
            const std::string scope = std::string(" ") + BarWidgetName(kind) + (widget.copy > 0 ? " " + std::to_string(widget.copy + 1) : "");
            for (const LayoutRow& row : c.layout.RowsOf(e))
            {
                const std::string label = Labeled(row.name, row.key, scope);
                // A tip shared by every widget, naming this one's default.
                const auto ownDefault = [&] {
                    return Filled(Said(row.tip), kDefaultMark, WidgetDefaultText(c.layout, static_cast<WidgetTabsRow>(row.row), widget));
                };
                // A look of its own on a slider whose step under lowest is the Top Panel's.
                const auto ownSlider = [&](std::optional<int>& own, int lowest, int highest, FunctionRef<std::string(int)> shown) {
                    const int panel = lowest - 1;
                    int at          = own.value_or(panel);
                    form.SliderInt(label.c_str(), at, panel, highest,
                        [&](int v) { return v == panel ? Words(topPanel) : shown(v); }, row.tip);
                    own = at == panel ? std::nullopt : std::optional<int>(at);
                };
                // A Job & EXP alignment, shown while the part it places is on.
                const auto alignRow = [&](WidgetPart part, Align& align) {
                    if (kind != BarWidget::Job || !PartOn(o.design, part)) return;
                    const std::vector<const char*> names = NamesOf(c.layout.OptionsOf(WindowList::Align));
                    form.Segments(label.c_str(), align, names.data(), kAlignCount, row.tip);
                };
                form.Fade(off);
                switch (static_cast<WidgetTabsRow>(row.row))
                {
                case WidgetTabsRow::Combine:
                {
                    const std::vector<PanelWidget> choices = CombineChoices(s, widget);
                    if (choices.empty()) break;
                    // 0 is None, then each choice from 1.
                    const auto at = std::ranges::find(choices, o.combine.value_or(widget));
                    int picked    = at == choices.end() ? 0 : static_cast<int>(at - choices.begin()) + 1;
                    form.NumberDropdown(label.c_str(), picked, 0, static_cast<int>(choices.size()),
                        [&](int i) {
                            if (i == 0) return std::string(Word(c, WindowWord::WidgetTabsCombineNone));
                            const PanelWidget choice = choices[static_cast<size_t>(i - 1)];
                            return CopyName(c, choice) +
                                   (s.Widget(choice).on ? "" : " " + std::string(Word(c, WindowWord::WidgetTabsLinkDisabled)));
                        },
                        row.tip);
                    o.combine = picked == 0 ? std::nullopt : std::optional<PanelWidget>(choices[static_cast<size_t>(picked - 1)]);
                    break;
                }
                case WidgetTabsRow::LinkedPadding:
                    if (!o.combine || CombineChoices(s, widget).empty()) break;
                    form.SliderInt(label.c_str(), o.linkPadding, 0, kMaxWidgetSpacing, Pixels, ownDefault().c_str());
                    break;
                case WidgetTabsRow::Visibility:
                    if (kind == BarWidget::Target)
                    {
                        uint32_t shows = o.targetShows;
                        form.BitsDropdown(label.c_str(), shows, c.layout.OptionsOf(WindowList::TargetShows), row.tip);
                        o.targetShows = static_cast<uint8_t>(shows);
                    }
                    break;
                case WidgetTabsRow::DetectionIcons:
                    if (kind == BarWidget::Target)
                    {
                        // The list leaves out the aggression, which is a part of its own.
                        uint32_t senses = o.targetDetection >> kFirstSense;
                        form.BitsDropdown(label.c_str(), senses, c.layout.OptionsOf(WindowList::DetectionIcons), row.tip);
                        o.targetDetection = senses << kFirstSense;
                    }
                    break;
                case WidgetTabsRow::BlueMagic:
                    if (kind == BarWidget::Target) form.Switch(label.c_str(), s.blueMagic, row.tip);
                    break;
                case WidgetTabsRow::BlueMagicColor:
                    if (kind != BarWidget::Target) break;
                {
                    static const Settings kDefaults;
                    form.Fade(off || !s.blueMagic);
                    form.Swatch(label.c_str(), s.blueMagicColor, row.tip, &kDefaults.blueMagicColor);
                    break;
                }
                case WidgetTabsRow::BlueMagicLearned:
                    if (kind == BarWidget::Target)
                    {
                        form.Fade(off || !s.blueMagic);
                        const std::vector<const char*> names = NamesOf(c.layout.OptionsOf(WindowList::BlueLearned));
                        form.Segments(label.c_str(), s.blueMagicLearned, names.data(), kBlueLearnedCount, row.tip);
                    }
                    break;
                case WidgetTabsRow::Currency1:
                case WidgetTabsRow::Currency2:
                    if (kind == BarWidget::Currency)
                    {
                        const auto which   = static_cast<WidgetTabsRow>(row.row);
                        int& currency      = which == WidgetTabsRow::Currency1 ? o.currency : o.secondCurrency;
                        const Nation yours = c.readings.nation;
                        const std::vector<int> choices = CurrencyChoices(yours);
                        // Your nation's points, picked by their name, are its own choice.
                        const int shown = NationPoints(yours) == currency ? kYourConquestCurrency : currency;
                        int picked      = static_cast<int>(std::ranges::find(choices, shown) - choices.begin());
                        const int was   = picked;
                        std::vector<std::string> rows;
                        for (const int choice : choices)
                            rows.push_back(CurrencyRow(choice, yours));
                        form.MarkupDropdown(label.c_str(), picked, rows, row.tip);
                        if (picked != was) currency = choices[static_cast<size_t>(picked)];
                    }
                    break;
                case WidgetTabsRow::CustomIcons:
                    if (kind == BarWidget::Weather)
                    {
                        int use = static_cast<int>(o.weatherIcons);
                        form.NumberDropdown(label.c_str(), use, 0, kCustomIconsCount - 1,
                            [&](int i) { return std::string(c.layout.OptionsOf(WindowList::CustomIcons)[static_cast<size_t>(i)].name); },
                            row.tip);
                        o.weatherIcons = static_cast<CustomIcons>(use);
                    }
                    break;
                case WidgetTabsRow::Ship:
                    if (kind == BarWidget::World && PartOn(o.design, WidgetPart::WorldShip))
                        form.NumberDropdown(label.c_str(), o.worldRoute, 0, static_cast<int>(TravelRoutes().size()) - 1,
                            [](int r) { return std::string(TravelRoutes()[static_cast<size_t>(r)].name); }, row.tip);
                    break;
                case WidgetTabsRow::Guild:
                    if (kind == BarWidget::World && PartOn(o.design, WidgetPart::WorldGuild))
                        form.NumberDropdown(label.c_str(), o.worldGuild, 0, static_cast<int>(GuildShops().size()) - 1,
                            [](int g) {
                                const GuildShop& shop = GuildShops()[static_cast<size_t>(g)];
                                return std::string(shop.guild) + ": " + shop.shops + " (" + shop.zone + ")";
                            },
                            row.tip);
                    break;
                case WidgetTabsRow::Bag1:
                case WidgetTabsRow::Bag2:
                    if (kind == BarWidget::Bags)
                    {
                        int& bag = static_cast<WidgetTabsRow>(row.row) == WidgetTabsRow::Bag1 ? o.bag : o.secondBag;
                        const std::vector<int>& bags = BagChoices();
                        const auto at = std::ranges::find(bags, bag);
                        int choice    = at != bags.end() ? static_cast<int>(at - bags.begin()) : 0;
                        form.NumberDropdown(label.c_str(), choice, 0, static_cast<int>(bags.size()) - 1,
                            [&](int i) { return ContainerName(bags[static_cast<size_t>(i)]); }, row.tip);
                        bag = bags[static_cast<size_t>(choice)];
                    }
                    break;
                case WidgetTabsRow::LocalTimezone:
                case WidgetTabsRow::SecondaryTimezone:
                    if (kind == BarWidget::EarthTime)
                    {
                        const bool local = static_cast<WidgetTabsRow>(row.row) == WidgetTabsRow::LocalTimezone;
                        if (!PartOn(o.design, local ? WidgetPart::ClockTime : WidgetPart::ClockServer)) break;
                        std::optional<ClockZone>& picked = local ? o.clockZone : o.clockSecondZone;
                        int zone = ClockZoneChoice(picked);
                        form.NumberDropdown(label.c_str(), zone, 0, kClockZoneChoices - 1,
                            [&](int choice) { return ClockZoneChoiceName(choice, c.layout, c.readings.localZone); }, row.tip);
                        picked = ClockZoneOfChoice(zone);
                    }
                    break;
                case WidgetTabsRow::N24Hour:
                    if (kind == BarWidget::EarthTime) form.Switch(label.c_str(), o.clock24, row.tip);
                    break;
                case WidgetTabsRow::Seconds:
                    if (kind == BarWidget::EarthTime) form.Switch(label.c_str(), o.clockSeconds, row.tip);
                    break;
                case WidgetTabsRow::DateFormat:
                    if (kind == BarWidget::EarthTime && PartOn(o.design, WidgetPart::ClockDate))
                    {
                        // Each format as today reads in it.
                        int format = static_cast<int>(o.dateFormat);
                        form.NumberDropdown(label.c_str(), format, 0, kDateFormatCount - 1,
                            [&](int i) { return DateText(static_cast<DateFormat>(i), c.readings.today); }, row.tip);
                        o.dateFormat = static_cast<DateFormat>(format);
                    }
                    break;
                case WidgetTabsRow::ShowZoneId:
                    if (kind == BarWidget::Zone) form.Switch(label.c_str(), o.zoneId, row.tip);
                    break;
                case WidgetTabsRow::Icon:
                    // Named after its widget's icon, as the Widget Designer's button for it was.
                    if (const std::optional<WidgetPart> icon = IconPartOf(kind))
                    {
                        const LayoutOption& part = c.layout.OptionsOf(WindowList::WidgetParts)[static_cast<size_t>(*icon)];
                        const std::vector<const char*> names = NamesOf(c.layout.OptionsOf(WindowList::IconSides));
                        form.Segments(Labeled(part.name, row.key, scope).c_str(), o.iconSide, names.data(), kIconSideCount,
                            ownDefault().c_str());
                    }
                    break;
                case WidgetTabsRow::Alignment:
                    if (Aligns(kind))
                    {
                        int picked = static_cast<int>(o.align);
                        form.NumberDropdown(label.c_str(), picked, 0, kWidgetAlignCount - 1,
                            [&](int i) { return std::string(c.layout.OptionsOf(WindowList::WidgetAligns)[static_cast<size_t>(i)].name); },
                            ownDefault().c_str());
                        o.align = static_cast<WidgetAlign>(picked);
                    }
                    break;
                case WidgetTabsRow::Name:
                    if (kind != BarWidget::Custom) break;
                    form.Row(label.c_str(), row.tip);
                    if (form.TextAt(label.c_str(), o.name, nullptr, kControlWidth, false, kMaxWidgetName)) form.changed = true;
                    form.EndRow();
                    break;
                case WidgetTabsRow::Design:
                    // the Widget Designer's description
                    form.Note(kind == BarWidget::StatusTimers ? Word(c, WindowWord::WidgetTabsDesignStatusTimers) : row.tip);
                    Designer(c, widget);
                    break;
                case WidgetTabsRow::PopupDesign:
                    if (kind != BarWidget::Custom) break;
                    form.Caption(row.name);
                    form.Note(row.tip);
                    PopupDesigner(c, widget);
                    break;
                case WidgetTabsRow::BarText:
                    if (bar)
                    {
                        // 0 is None, then each text from 1.
                        int text = o.expBarText ? static_cast<int>(*o.expBarText) + 1 : 0;
                        form.NumberDropdown(label.c_str(), text, 0, kExpTextCount,
                            [&](int i) { return std::string(c.layout.OptionsOf(WindowList::ExpBarText)[static_cast<size_t>(i)].name); },
                            row.tip);
                        o.expBarText = text == 0 ? std::nullopt : std::optional(static_cast<ExpText>(text - 1));
                    }
                    break;
                case WidgetTabsRow::JobText:
                    if (jobs || (bar && o.expBarText == ExpText::Jobs))
                    {
                        int text = static_cast<int>(o.jobText);
                        form.NumberDropdown(label.c_str(), text, 0, kJobTextCount - 1,
                            [&](int i) { return std::string(c.layout.OptionsOf(WindowList::JobText)[static_cast<size_t>(i)].name); },
                            row.tip);
                        o.jobText = static_cast<JobText>(text);
                    }
                    break;
                case WidgetTabsRow::BarTextAlign: alignRow(WidgetPart::JobBar, o.expBarTextAlign); break;
                case WidgetTabsRow::JobTextAlign: alignRow(WidgetPart::JobJobs, o.jobInfoAlign); break;
                case WidgetTabsRow::EarnedAlign: alignRow(WidgetPart::JobEarned, o.earnedAlign); break;
                case WidgetTabsRow::TnlAlign: alignRow(WidgetPart::JobToNext, o.toNextAlign); break;
                case WidgetTabsRow::PercentAlign: alignRow(WidgetPart::JobPercent, o.percentAlign); break;
                case WidgetTabsRow::BarBack:
                case WidgetTabsRow::BarColor:
                    if (bar)
                    {
                        const bool back           = static_cast<WidgetTabsRow>(row.row) == WidgetTabsRow::BarBack;
                        const BarPalette& palette = ShownPalette(c);
                        const char* const words[] = {Word(c, WindowWord::Themes), Word(c, WindowWord::CustomColor)};
                        form.ThemeColorRow(label.c_str(), back ? o.expBarBack : o.expBarFill,
                            back ? ExpBarBack(palette) : palette.accent, words, row.tip);
                    }
                    break;
                case WidgetTabsRow::BarWidth:
                    if (bar)
                        form.SliderInt(label.c_str(), o.expBarWidth, kMinExpBarWidth, kMaxExpBarWidth, Pixels, row.tip);
                    break;
                case WidgetTabsRow::BarHeight:
                    if (bar)
                        form.SliderInt(label.c_str(), o.expBarHeight, kMinExpBarHeight, kMaxExpBarHeight, Pixels, row.tip);
                    break;
                case WidgetTabsRow::MemText:
                    if (kind == BarWidget::Performance && PartOn(o.design, WidgetPart::PerfMemory))
                        form.Switch(label.c_str(), o.memText, row.tip);
                    break;
                case WidgetTabsRow::OwnLooks:
                    if (HasOwnLooks(kind)) form.Caption(label.c_str());
                    break;
                case WidgetTabsRow::WidgetSpacing:
                    if (pillLooks) ownSlider(o.spacing, 0, kMaxWidgetSpacing, Pixels);
                    break;
                case WidgetTabsRow::WidgetPadding:
                    if (HasOwnLooks(kind)) ownSlider(o.padding, 0, kMaxWidgetPadding, Pixels);
                    break;
                case WidgetTabsRow::ShowBackground:
                    if (pillLooks)
                    {
                        const std::vector<const char*> names = NamesOf(c.layout.OptionsOf(WindowList::OwnBackground));
                        int picked = !o.background ? 0 : *o.background ? 1 : 2; // as the list has them
                        form.Segments(label.c_str(), picked, names.data(), static_cast<int>(names.size()), row.tip);
                        o.background = picked == 0 ? std::nullopt : std::optional<bool>(picked == 1);
                    }
                    break;
                case WidgetTabsRow::CornerRounding:
                    if (pillLooks)
                        ownSlider(o.rounding, kThemeRounding, kMaxRounding, PixelsOr(Word(c, WindowWord::Themes), kThemeRounding));
                    break;
                case WidgetTabsRow::PrimaryFont:
                    if (HasOwnLooks(kind)) form.FontDropdown(label.c_str(), o.titleFont, c.fonts, row.tip, topPanel);
                    break;
                case WidgetTabsRow::SecondaryFont:
                    if (HasOwnLooks(kind)) form.FontDropdown(label.c_str(), o.textFont, c.fonts, row.tip, topPanel);
                    break;
                case WidgetTabsRow::FontRatio:
                    if (HasOwnLooks(kind))
                    {
                        // By the Top Panel's slider's positions, set only when moved, so a ratio between them is kept.
                        std::optional<int> at = o.fontRatio ? std::optional<int>(FontRatioPosition(*o.fontRatio)) : std::nullopt;
                        const std::optional<int> before = at;
                        ownSlider(at, 0, 2 * kFontRatioSteps, [](int p) { return Printed("%.2fx", FontRatioAt(p)); });
                        if (at != before) o.fontRatio = at ? std::optional<float>(FontRatioAt(*at)) : std::nullopt;
                    }
                    break;
                case WidgetTabsRow::IconScale:
                    if (HasOwnLooks(kind))
                    {
                        std::optional<int> at =
                            o.iconScale ? std::optional<int>((*o.iconScale - kMinIconScale) / kPercentStep) : std::nullopt;
                        ownSlider(at, 0, (kMaxIconScale - kMinIconScale) / kPercentStep,
                            [](int p) { return Formatted("%d%%", kMinIconScale + p * kPercentStep); });
                        o.iconScale = at ? std::optional<int>(kMinIconScale + *at * kPercentStep) : std::nullopt;
                    }
                    break;
                case WidgetTabsRow::PopupTextScale:
                    if (pillLooks)
                    {
                        const auto step = [](float scale) { return static_cast<int>(std::lround((scale - kMinPopupScale) / kPopupScaleStep)); };
                        std::optional<int> at = o.popupTextScale ? std::optional<int>(step(*o.popupTextScale)) : std::nullopt;
                        const std::optional<int> before = at;
                        ownSlider(at, 0, step(kMaxPopupScale),
                            [](int p) { return Printed("%.1fx", kMinPopupScale + static_cast<float>(p) * kPopupScaleStep); });
                        if (at != before)
                            o.popupTextScale = at ? std::optional<float>(kMinPopupScale + static_cast<float>(*at) * kPopupScaleStep)
                                                  : std::nullopt;
                    }
                    break;
                case WidgetTabsRow::StatusDisplay:
                    if (kind == BarWidget::StatusTimers)
                    {
                        int display = static_cast<int>(o.statusDisplay);
                        form.NumberDropdown(label.c_str(), display, 0, kStatusDisplayCount - 1,
                            [&](int i) { return std::string(c.layout.OptionsOf(WindowList::StatusDisplay)[static_cast<size_t>(i)].name); },
                            row.tip);
                        o.statusDisplay = static_cast<StatusDisplay>(display);
                    }
                    break;
                }
            }
        }

        // The settings of each widget the panel's popup shows; more than one, each in a box under its name, up to
        // kSettingsAcross a row.
        void PopupSettings(Context& c, const LayoutSetting& tabs)
        {
            const bool boxed = c.readings.settingsOf.size() > 1;
            if (boxed) c.form.Raw("<div class=\"wcols\">");
            for (const PanelWidget widget : c.readings.settingsOf)
            {
                c.form.Fade(false);
                if (boxed)
                {
                    c.form.Raw("<div class=\"wbox\">");
                    c.form.Caption(CopyName(c, widget).c_str(), kWidgetNameClass);
                }
                WidgetOptions(c, tabs, widget);
                if (boxed) c.form.Raw("</div>");
            }
            if (boxed) c.form.Raw("</div>");
        }

        // Left, Center and Right as trays sunk into the card, each holding its widgets as raised chips in bar order:
        // a chip drags to another place or tray, and a click on one that is on opens its popup on the panel on its
        // settings, or closes it when it shows them already. Its toggle turns it on and off, the button at its right
        // clones it, and on a copy the one left of that takes it off.
        void Trays(Context& c)
        {
            struct Drop
            {
                BarSection from;
                size_t index;
                BarSection to;
                size_t at;
            };
            Form& form  = c.form;
            Settings& s = c.s;
            form.Fade(!BarOn(s));
            std::optional<Drop> drop; // applied after the trays are written, so no list changes under its own loop
            std::optional<PanelWidget> clone, remove; // the same
            const std::string chipStart = form.Named(kWidgetChip) + "-";
            const auto sections         = c.layout.OptionsOf(WindowList::BarSections);
            const auto widgets          = c.layout.OptionsOf(WindowList::BarWidgets);
            // The chip a drop names, as the section it is in and its place there.
            const auto dropped = [&](const std::string& target, BarSection to, size_t at) {
                const FormEvent* event = form.Took(target, kDropEvent);
                if (event == nullptr || event->value.compare(0, chipStart.size(), chipStart) != 0) return;
                const int w = std::atoi(event->value.c_str() + chipStart.size());
                if (w < 0 || w >= kPanelSlots || !Placed(s.barSections, WidgetInSlot(w))) return;
                const auto [section, index] = PlaceOf(s.barSections, WidgetInSlot(w));
                drop = Drop{static_cast<BarSection>(section), index, to, at};
            };
            form.Line("line trays");
            for (int section = 0; section < kBarSectionCount; ++section)
            {
                const auto here = static_cast<BarSection>(section);
                form.Raw(std::string("<div class=\"tray ") + kWidgetTrayClass + "\"><div class=\"tray-name\">" +
                         EscapedRml(Said(sections[static_cast<size_t>(section)].name)) + "</div>");
                const std::vector<PanelWidget>& held = s.barSections[section];
                for (size_t i = 0; i < held.size(); ++i)
                {
                    const int w          = SlotOf(held[i]);
                    const std::string id = form.Id(kWidgetChip + std::to_string(w));
                    // Two lines: its toggle with its clone and, on a copy, its remove at the right; then its name.
                    form.Raw(std::string("<div class=\"btn raised wchip ") + kTrayChipClass + " drag\" id=\"" + id + "\"" +
                             TipAttribute(widgets[static_cast<size_t>(held[i].kind)].tip) + "><div class=\"wtop\">");
                    const bool was = s.widgets[w].on;
                    form.CheckAt((kWidgetChip + std::to_string(w) + kChipToggle).c_str(), s.widgets[w].on);
                    const auto action = [&](const char* name, Icon icon, WindowWord word) {
                        const std::string actionId = form.Id(kWidgetChip + std::to_string(w) + name);
                        form.Raw("<div class=\"wclone\" id=\"" + actionId + "\"" + TipAttribute(WordTip(c, word)) +
                                 "><img src=\"" + IconSource(icon) + "\"" + AdjustStyle(AdjustOf(icon)) + "/></div>");
                        return !form.Off() && form.Clicked(actionId);
                    };
                    // Floated right, so the first is the rightmost.
                    if (HasCopies(held[i].kind) && action(kChipClone, Icon::Clone, WindowWord::WidgetTabsClone)) clone = held[i];
                    if (!IsFirstCopy(held[i]) && action(kChipRemove, Icon::Delete, WindowWord::WidgetTabsRemoveCopy)) remove = held[i];
                    form.Raw("</div><div class=\"wname\">" + EscapedRml(CopyName(c, held[i])) + "</div></div>");
                    form.Set(id, FormValueKind::Class, kOffClass, {}, form.Off());
                    if (!form.Off() && was && s.widgets[w].on && held[i].kind != BarWidget::Settings && form.Clicked(id))
                        c.state.settingsAsked = held[i];
                    form.Set(id, FormValueKind::Class, "on", {}, s.widgets[w].on);
                    form.Set(id, FormValueKind::Class, kPickedClass, {},
                        std::ranges::find(c.readings.settingsOf, held[i]) != c.readings.settingsOf.end());
                    dropped(id, here, i); // dropped on a chip: before it
                }
                // The rest of the tray takes a drop at the section's end.
                const std::string end = form.Id("tray end " + std::to_string(section));
                form.Raw("<div class=\"tray-end\" id=\"" + end + "\"></div></div>");
                dropped(end, here, held.size());
            }
            form.EndLine();
            if (drop) form.changed |= MoveWidget(s.barSections, drop->from, drop->index, drop->to, drop->at);
            if (const std::optional<PanelWidget> added = clone ? CloneWidget(s, *clone) : std::nullopt)
            {
                c.state.settingsAsked = *added;
                form.changed          = true;
            }
            if (remove)
            {
                // Its settings, if they showed, give way to the first copy's; a created widget's close with it.
                if (remove->kind != BarWidget::Custom &&
                    std::ranges::find(c.readings.settingsOf, *remove) != c.readings.settingsOf.end())
                    c.state.settingsAsked = PanelWidget{remove->kind};
                RemoveWidgetCopy(s, *remove);
                form.changed = true;
            }
        }

        // A palette's colors in two columns, the first the longer, each with an arrow back to its default.
        void PaletteSwatches(Context& c, const LayoutSetting& e, Color* colors, const Color* defaults)
        {
            Form& form = c.form;
            form.Fade(!c.s.enabled);
            const auto rows = c.layout.RowsOf(e);
            const int count = static_cast<int>(rows.size());
            const int first = (count + 1) / 2;
            form.Raw("<div class=\"columns\"><div class=\"column\">");
            for (int i = 0; i < count; ++i)
            {
                if (i == first) form.Raw("</div><div class=\"column second\">");
                const LayoutRow& row = rows[static_cast<size_t>(i)];
                form.Swatch(Labeled(row.name, row.key, std::string(" ") + e.key).c_str(), colors[row.row], row.tip,
                    &defaults[row.row]);
            }
            form.Raw("</div></div>");
        }

        // The settings row a visibility row stands for, among its kind's.
        int RowIndexOf(VisibilityRow row)
        {
            switch (row)
            {
            case VisibilityRow::Aggressive: return CategoryIndex(Category::WillAttack);
            case VisibilityRow::Passive: return CategoryIndex(Category::WontAttack);
            case VisibilityRow::NmAggressive: return CategoryIndex(Category::NmWillAttack);
            case VisibilityRow::NmPassive: return CategoryIndex(Category::NmWontAttack);
            case VisibilityRow::NmPlaceholder: return CategoryIndex(Category::Placeholder);
            case VisibilityRow::ClaimedYours: return CategoryIndex(Category::PartyClaim);
            case VisibilityRow::ClaimedOthers: return CategoryIndex(Category::OtherClaim);
            case VisibilityRow::Dead: return CategoryIndex(Category::Dead);
            case VisibilityRow::NoData: return CategoryIndex(Category::Unknown);
            case VisibilityRow::Players: return static_cast<int>(PlayerRow::Player);
            case VisibilityRow::PartyMembers: return static_cast<int>(PlayerRow::PartyMember);
            case VisibilityRow::Friends: return static_cast<int>(PlayerRow::Friend);
            case VisibilityRow::LinkshellMembers: return static_cast<int>(PlayerRow::LinkshellMember);
            case VisibilityRow::Self:
            case VisibilityRow::Npcs: return 0;
            case VisibilityRow::YourPets: return static_cast<int>(PetRow::Yours);
            case VisibilityRow::PartyPets: return static_cast<int>(PetRow::Party);
            case VisibilityRow::FriendPets: return static_cast<int>(PetRow::Friend);
            case VisibilityRow::OthersPets: return static_cast<int>(PetRow::Others);
            }
            return 0;
        }

        // The rows of the kind's page, in the layout's order, each with its settings row.
        template <typename Visit>
        void ForEachVisibilityRow(const Context& c, Visit visit)
        {
            const LayoutSetting* visibility = c.layout.SettingOf(SettingPiece::Visibility);
            if (visibility == nullptr) return;
            for (const LayoutRow& row : c.layout.RowsOf(*visibility))
                if ((row.kinds & KindBit(*c.kind)) != 0) visit(row, RowIndexOf(static_cast<VisibilityRow>(row.row)));
        }

        void Visibility(Context& c)
        {
            Form& form           = c.form;
            Settings& s          = c.s;
            const PlateKind kind = *c.kind;
            KindSettings& k      = s.Kind(kind);
            const bool plates = s.enabled && s.nameplates, outlines = s.enabled && s.outlines;
            const auto modes        = c.layout.OptionsOf(WindowList::PlateMode);
            const auto outlineModes = c.layout.OptionsOf(WindowList::OutlineMode);
            form.Fade(!s.enabled);
            form.Row(Labeled(nullptr, "visibility heads").c_str(), nullptr);
            form.Fade(!plates);
            form.Caption(Labeled(Word(c, WindowWord::VisibilityNameplates), "nameplates").c_str(), "head", kModeWidth);
            form.Fade(!outlines);
            form.Caption(Labeled(Word(c, WindowWord::VisibilityOutlines), "outlines").c_str(), "head", kModeWidth);
            form.EndRow();
            ForEachVisibilityRow(c, [&](const LayoutRow& shown, int r) {
                RowSettings& row = k.rows[r];
                form.Fade(!s.enabled);
                form.Row(Labeled(NameOf(shown, kind), shown.key).c_str(), TipOf(shown, kind));
                form.Fade(!plates);
                int plate = static_cast<int>(row.plate);
                form.DropdownAt((std::string("plate ") + RowKey(kind, r)).c_str(), plate, 0, kPlateModeCount - 1,
                    [&](int m) { return Words(modes[static_cast<size_t>(m)].name); }, kModeWidth, nullptr,
                    [&](int m) { return Words(modes[static_cast<size_t>(m)].tip); });
                row.plate = static_cast<PlateMode>(plate);
                form.Fade(!outlines);
                int outline         = static_cast<int>(row.outline);
                const bool claimRow = kind == PlateKind::Mobs && IsClaimRow(static_cast<Category>(r));
                form.DropdownAt((std::string("outline ") + RowKey(kind, r)).c_str(), outline, 0, OutlineModesFor(kind, r) - 1,
                    [&](int m) { return Words(outlineModes[static_cast<size_t>(m)].name); }, kModeWidth, nullptr, [&](int m) {
                        const LayoutOption& mode = outlineModes[static_cast<size_t>(m)];
                        return Words(claimRow && mode.otherTip != nullptr ? mode.otherTip : mode.tip);
                    });
                row.outline = static_cast<OutlineMode>(outline);
                form.EndRow();
            });
        }

        void OutlineColors(Context& c, const LayoutSetting& e)
        {
            Form& form      = c.form;
            KindSettings& k = c.s.Kind(*c.kind);
            form.Fade(!c.s.enabled || !c.s.outlines);
            const std::vector<ColorSource> sources = SourcesFor(*c.kind, false);
            ForEachVisibilityRow(c, [&](const LayoutRow& shown, int r) {
                RowSettings& row = k.rows[r];
                if (!HasOutlineColor(row.outline)) return;
                form.ColorPickRow(Labeled(NameOf(shown, c.kind), shown.key, " outline").c_str(), row.outlineColor, sources,
                    TipOf(e, c.kind));
            });
        }

        // The three lines' boxes, each named, then the kind's tags as chips to drag into them.
        void Design(Context& c)
        {
            Form& form           = c.form;
            const PlateKind kind = *c.kind;
            KindSettings& k      = c.s.Kind(kind);
            DesignEditor& editor = c.state.design;
            const auto lines     = c.layout.OptionsOf(WindowList::PlateLines);
            form.Fade(!c.s.enabled || !c.s.nameplates || !c.plates);
            std::optional<int> swapped; // the line that trades places with the one under it, once the lines are written
            for (int i = 0; i < kPlateLines; ++i)
            {
                const std::string scope = std::string(" ") + KindKey(kind) + " " + std::to_string(i);
                form.Line("line design");
                form.Caption(lines[static_cast<size_t>(i)].name, "line-name");
                // An end line's missing arrow keeps its room, so the three boxes start together.
                if (i == 0)
                    form.Raw(std::string("<div class=\"") + kArrowRoomClass + "\"></div>");
                else if (form.ArrowAt(("move up" + scope).c_str(), false, WordTip(c, WindowWord::DesignMoveUp)))
                    swapped = i - 1;
                if (i == kPlateLines - 1)
                    form.Raw(std::string("<div class=\"") + kArrowRoomClass + "\"></div>");
                else if (form.ArrowAt(("move down" + scope).c_str(), true, WordTip(c, WindowWord::DesignMoveDown)))
                    swapped = i;
                form.LineBox(("design" + scope).c_str(), k.lines[i].text, editor.drafts[i]);
                form.EndLine();
            }
            if (swapped)
            {
                SwapLines(k, *swapped, *swapped + 1);
                std::swap(editor.drafts[*swapped], editor.drafts[*swapped + 1]);
                form.changed = true;
            }
            form.Line("line chips");
            for (const Tag tag : TagsFor(kind))
                form.TagChip(tag);
            form.EndLine();
        }

        // A line's place: how far it stands off the next line, and across. False for any other row.
        bool LinePlaceRow(Form& form, const LayoutRow& row, PlateKind kind, int n, const std::string& label, LineDesign& line)
        {
            switch (static_cast<LineStylesRow>(row.row))
            {
            case LineStylesRow::Padding:
                form.SliderInt(label.c_str(), line.padding, kMinLinePadding, kMaxLinePadding, Pixels,
                    n < kPlateLines - 1 ? TipOf(row, kind) : row.otherTip);
                return true;
            case LineStylesRow::HorizontalOffset:
                form.SliderInt(label.c_str(), line.offset, kMinLineOffset, kMaxLineOffset, Pixels, TipOf(row, kind));
                return true;
            default: return false;
            }
        }

        // Every tab shows the same rows; a synced line's tab alone shows only its Sync row and what it uses.
        void LineStyles(Context& c, const LayoutSetting& e)
        {
            Form& form           = c.form;
            const PlateKind kind = *c.kind;
            KindSettings& k      = c.s.Kind(kind);
            DesignEditor& editor = c.state.design;
            const bool off       = !c.s.enabled || !c.s.nameplates || !c.plates;
            const auto lines     = c.layout.OptionsOf(WindowList::PlateLines);
            form.Fade(off);
            const std::vector<const char*> tabs = NamesOf(lines);
            editor.styleTab  = form.Tabs("line style", editor.styleTab, tabs.data(), kPlateLines);
            const int n      = editor.styleTab;
            LineDesign& line = k.lines[n];
            // Each line's controls have ids of their own: one held, open or typed in is not carried to another tab.
            const std::string scope = std::string(" ") + KindKey(kind) + std::to_string(n);
            const std::vector<ColorSource> sources = SourcesFor(kind, true);
            for (const LayoutRow& row : c.layout.RowsOf(e))
            {
                const auto r = static_cast<LineStylesRow>(row.row);
                if (r != LineStylesRow::SyncLineStyle && line.syncWith != kNoSync) continue;
                const std::string label = Labeled(NameOf(row, kind), row.key, scope);
                const char* tip         = TipOf(row, kind);
                form.Fade(off);
                switch (r)
                {
                case LineStylesRow::SyncLineStyle:
                {
                    const std::vector<int> targets = SyncTargets(k, n);
                    const int synced               = SyncEntry(targets, line.syncWith);
                    int sync                       = synced;
                    form.NumberDropdown(label.c_str(), sync, 0, static_cast<int>(targets.size()),
                        [&](int entry) { return SyncEntryLabel(targets, entry, c.layout); }, tip, 0.0f,
                        [&](int entry) { return SyncEntryTip(targets, entry, c.layout); });
                    if (sync != synced) line.syncWith = SyncOfEntry(targets, sync);
                    if (line.syncWith != kNoSync)
                        form.Note(Filled(Said(Word(c, WindowWord::LineStylesUsesStyle)), kLineMark,
                            Said(lines[static_cast<size_t>(line.syncWith)].name)).c_str());
                    break;
                }
                case LineStylesRow::LineVisibility: form.ChecksDropdown(label.c_str(), line.show, ConditionsFor(kind), tip); break;
                case LineStylesRow::Alignment:
                    // The bottom line is what the others are set against.
                    if (n < kPlateLines - 1)
                    {
                        const std::vector<const char*> names = NamesOf(c.layout.OptionsOf(WindowList::Align));
                        form.Segments(label.c_str(), line.align, names.data(), kAlignCount, tip);
                    }
                    break;
                case LineStylesRow::Font:
                    form.FontDropdown(label.c_str(), line.font, c.plateFonts, tip, Word(c, WindowWord::Default));
                    break;
                case LineStylesRow::FontStyle:
                {
                    const std::vector<const char*> names = NamesOf(c.layout.OptionsOf(WindowList::FontStyle));
                    form.Toggles(label.c_str(), line.style, names.data(), kFontStyleCount, tip);
                    break;
                }
                case LineStylesRow::TextSize:
                    form.SliderPercent(label.c_str(), line.textSize, kMinSizePercent, kMaxSizePercent, tip);
                    break;
                case LineStylesRow::IconSize:
                    form.SliderPercent(label.c_str(), line.iconSize, kMinSizePercent, kMaxSizePercent, tip);
                    break;
                case LineStylesRow::Padding:
                case LineStylesRow::HorizontalOffset: LinePlaceRow(form, row, kind, n, label, line); break;
                case LineStylesRow::TextColor: form.ColorPickRow(label.c_str(), line.color, sources, tip); break;
                case LineStylesRow::ShadowStrength:
                    form.SliderPercent(label.c_str(), line.shadow, kMinShadow, kMaxShadow, tip);
                    break;
                case LineStylesRow::ShadowColor:
                    form.Fade(off || line.shadow == 0);
                    form.ColorPickRow(label.c_str(), line.shadowColor, sources, tip);
                    break;
                case LineStylesRow::GlowStrength: form.SliderPercent(label.c_str(), line.glow, kMinGlow, kMaxGlow, tip); break;
                case LineStylesRow::GlowSize:
                    form.Fade(off || line.glow == 0);
                    form.SliderPercent(label.c_str(), line.glowSize, kMinGlowSize, kMaxGlowSize, tip);
                    break;
                case LineStylesRow::GlowColor:
                    form.Fade(off || line.glow == 0);
                    form.ColorPickRow(label.c_str(), line.glowColor, sources, tip);
                    break;
                case LineStylesRow::DisableGlowInCombat:
                    form.Fade(off || line.glow == 0);
                    form.Switch(label.c_str(), line.glowOffInCombat, tip);
                    break;
                }
            }
        }

        void Ornament(Context& c, const LayoutSetting& e)
        {
            Form& form              = c.form;
            const PlateKind kind    = *c.kind;
            OrnamentSettings& o     = c.s.Kind(kind).ornament;
            const bool off          = !c.s.enabled || !c.s.nameplates || !c.plates;
            const std::string scope = std::string(" ") + KindKey(kind) + " ornament";
            for (const LayoutRow& row : c.layout.RowsOf(e))
            {
                const std::string label = Labeled(NameOf(row, kind), row.key, scope);
                const char* tip         = TipOf(row, kind);
                form.Fade(off || o.place == OrnamentPlace::Disabled);
                switch (static_cast<OrnamentRow>(row.row))
                {
                case OrnamentRow::Visibility:
                    form.Fade(off);
                    OptionsDropdown(form, label, o.place, c.layout.OptionsOf(WindowList::OrnamentPlace), tip);
                    break;
                case OrnamentRow::Width:
                    form.SliderInt(label.c_str(), o.width, kMinOrnamentWidth, kMaxOrnamentWidth, Pixels, tip);
                    break;
                case OrnamentRow::Height:
                    form.SliderInt(label.c_str(), o.height, kMinOrnamentThickness, kMaxOrnamentThickness, Pixels, tip);
                    break;
                case OrnamentRow::Color: form.ColorPickRow(label.c_str(), o.color, SourcesFor(kind, false), tip); break;
                }
            }
        }

        // Which of the Detection tag's icons show: aggression and links, then what the mob senses you by.
        void Detection(Context& c, const LayoutSetting& e)
        {
            Form& form  = c.form;
            Settings& s = c.s;
            form.Fade(!s.enabled || !s.nameplates || !Uses(s, {Tag::Detect}));
            const auto rows = c.layout.RowsOf(e);
            for (int d = 0; d < kDetectionIconCount; ++d)
            {
                if (static_cast<DetectionIcon>(d) == DetectionIcon::Sight)
                {
                    form.Caption(Labeled(Word(c, WindowWord::DetectionSenses), "senses").c_str(), "line-name");
                    form.Indent(true);
                }
                const LayoutRow& row = rows[static_cast<size_t>(d)];
                const uint32_t bit   = DetectionBit(static_cast<DetectionIcon>(d));
                bool on              = (s.detectionShown & bit) != 0;
                form.Switch(Labeled(row.name, row.key).c_str(), on, row.tip, Word(c, WindowWord::DetectionHide),
                    Word(c, WindowWord::DetectionShow));
                s.detectionShown = on ? s.detectionShown | bit : s.detectionShown & ~bit;
            }
            form.Indent(false);
        }

        // Each status's switch, then the weakest and the least resisted rank that gets a badge.
        void StatusTable(Context& c, const LayoutSetting& e)
        {
            Form& form           = c.form;
            Settings& s          = c.s;
            const bool dim       = !s.enabled || !s.nameplates || !Uses(s, {Tag::Status});
            const auto cutoff    = [&](int rank) { return CutoffText(c, rank); };
            const auto weakTip   = [&](int rank) { return CutoffTip(c, rank, true); };
            const auto resistTip = [&](int rank) { return CutoffTip(c, rank, false); };
            form.Fade(dim);
            form.Row(Labeled(nullptr, "status heads").c_str(), nullptr);
            form.Caption("", "head", kToggleWidth);
            form.Caption(Labeled(Word(c, WindowWord::StatusTableWeak), "weak").c_str(), "head", kCutoffWidth);
            form.Caption(Labeled(Word(c, WindowWord::StatusTableResist), "resist").c_str(), "head", kCutoffWidth);
            form.EndRow();
            for (const LayoutRow& row : c.layout.RowsOf(e))
            {
                StatusBadgeRule& rule = s.statusBadges[row.row];
                const std::string key = row.key;
                form.Fade(dim);
                form.Row(Labeled(row.name, key).c_str(), row.tip);
                form.Raw("<div class=\"switch-cell\">");
                form.CheckAt(("status " + key).c_str(), rule.show);
                form.Raw("</div>");
                form.Fade(dim || !rule.show);
                form.DropdownAt(("weak " + key).c_str(), rule.weakRank, kMaxWeakRank, kNoWeakRank, cutoff, kCutoffWidth, nullptr,
                    weakTip);
                form.DropdownAt(("resist " + key).c_str(), rule.resistRank, kMinResistRank, kNoResistRank, cutoff,
                    kCutoffWidth, nullptr, resistTip);
                form.EndRow();
            }
        }

        // A list of HeadsUp's own pictures, named by the layout's options, then the cursors folder's files by name.
        template <typename Style>
        PictureChoice<Style> PictureDropdown(Context& c, const LayoutSetting& e, const std::string& label, const char* tip,
            Style style, const std::string& file)
        {
            static const std::vector<std::string> kNoFiles;
            const auto options = c.layout.OptionsOf(e);
            const std::vector<PictureChoice<Style>> choices =
                PictureChoices(c.readings.cursorFiles != nullptr ? *c.readings.cursorFiles : kNoFiles, style, file);
            const auto said = [&](int choice) { return options[static_cast<size_t>(choices[static_cast<size_t>(choice)].style)]; };
            int picked      = PictureChoiceOf(choices, style, file);
            c.form.NumberDropdown(label.c_str(), picked, 0, static_cast<int>(choices.size()) - 1,
                [&](int choice) {
                    const PictureChoice<Style>& shown = choices[static_cast<size_t>(choice)];
                    return shown.style == Style::File ? shown.file : Words(said(choice).name);
                },
                tip, 0.0f, [&](int choice) { return Words(said(choice).tip); });
            if (c.form.Opened(label)) c.state.cursorsRequested = true;
            return choices[static_cast<size_t>(picked)];
        }

        void TargetCursorStyle(Context& c, const LayoutSetting& e, const std::string& label, const char* tip)
        {
            Settings& s = c.s;
            c.form.Fade(!s.enabled);
            const PictureChoice<CursorStyle> choice = PictureDropdown(c, e, label, tip, s.cursorStyle, s.cursorFile);
            // Another style comes at its own width, or it would be squeezed into the one before's.
            if (choice.style != s.cursorStyle && choice.style != CursorStyle::Vanilla)
                s.cursorWidth = std::clamp(NaturalCursorWidth(choice.style, s.cursorHeight), kMinCursorSide, kMaxCursorSide);
            s.cursorStyle = choice.style;
            s.cursorFile  = choice.file;
        }

        // A setting, wherever the layout puts it. Each says when it shows, its card's condition included, as it may
        // be moved out of that card.
        void DrawSetting(Context& c, const LayoutSetting& e)
        {
            Form& form              = c.form;
            Settings& s             = c.s;
            const MenuReadings& r   = c.readings;
            const std::string label = Labeled(NameOf(e, c.kind), e.key);
            const char* tip         = TipOf(e, c.kind);
            const bool on = s.enabled, plates = s.enabled && s.nameplates, outlines = s.enabled && s.outlines;
            const bool cursor = s.enabled && s.cursorStyle != CursorStyle::Vanilla;
            const bool shown  = plates && c.plates; // a kind's design, on a page whose rows draw a nameplate
            static const Settings kDefaults;
            static const Palette kPalette = DefaultPalette();
            const auto cutoff             = [&](int rank) { return CutoffText(c, rank); };
            switch (e.piece)
            {
            // Entities
            case SettingPiece::BaseFontSize:
                form.Fade(!plates);
                form.SliderInt(label.c_str(), s.baseSize, kMinTextSize, kMaxTextSize, Pixels, tip);
                break;
            case SettingPiece::DefaultFont:
                form.Fade(!plates);
                form.FontDropdown(label.c_str(), s.defaultFont, c.plateFonts, tip);
                break;
            case SettingPiece::BaseIconSize:
                form.Fade(!plates);
                form.SliderInt(label.c_str(), s.baseIconSize, kMinTextSize, kMaxTextSize, Pixels, tip);
                break;
            case SettingPiece::LinkpearlOutline:
                form.Fade(!plates);
                form.SliderPercent(label.c_str(), s.pearlOutline, 0, kMaxPearlOutline, tip);
                break;
            case SettingPiece::AutoCheck:
                form.Fade(!plates);
                form.Switch(label.c_str(), s.autoCheck, tip);
                break;
            case SettingPiece::LookUpPlayers:
                form.Fade(!plates);
                form.Switch(label.c_str(), s.lookUpPlayers, tip);
                break;
            case SettingPiece::ScaleWithDistance:
                form.Fade(!plates);
                form.Switch(label.c_str(), s.scaleWithDistance, tip);
                break;
            case SettingPiece::MinSize:
            case SettingPiece::MaxSize:
                form.Fade(!plates || !s.scaleWithDistance);
                form.SliderPercent(label.c_str(), e.piece == SettingPiece::MinSize ? s.scaleMin : s.scaleMax,
                    kMinSizePercent, kMaxSizePercent, tip);
                break;
            case SettingPiece::VerticalOffset:
                form.Fade(!plates);
                form.SliderInt(label.c_str(), s.nameRaise, kMinNameRaise, kMaxNameRaise, Pixels, tip);
                break;
            case SettingPiece::StackNames:
                form.Fade(!plates);
                form.Switch(label.c_str(), s.stackNames, tip);
                break;
            case SettingPiece::StackLineToEntity:
                form.Fade(!plates || !s.stackNames);
                form.Switch(label.c_str(), s.stackLine, tip);
                break;
            case SettingPiece::HideDuringConversation:
                form.Fade(!plates);
                form.Switch(label.c_str(), s.hideInEvent, tip);
                break;
            case SettingPiece::Thickness:
                form.Fade(!outlines);
                form.SliderPercent(label.c_str(), s.outlineThickness, kMinOutlineThickness, kMaxOutlineThickness, tip);
                break;
            case SettingPiece::Opacity:
                form.Fade(!outlines);
                form.SliderPercent(label.c_str(), s.outlineOpacity, kMinOpacity, kMaxOpacity, tip);
                break;
            case SettingPiece::Smoothness:
            {
                form.Fade(!outlines);
                const std::vector<const char*> names = NamesOf(c.layout.OptionsOf(WindowList::Smoothness));
                int smoothness = static_cast<int>(SmoothnessLevelOf(s.smoothness));
                form.SliderNamed(label.c_str(), smoothness, names.data(), kSmoothnessLevelCount, tip);
                s.smoothness = SmoothnessCopies(static_cast<SmoothnessLevel>(smoothness));
                break;
            }
            case SettingPiece::OutlineRange:
                form.Fade(!outlines);
                form.SliderFloat(label.c_str(), s.maxDistance, kMinOutlineDistance, kMaxOutlineDistance, "%.0f yalms", tip);
                break;
            case SettingPiece::OutlineFadeIn:
                form.Fade(!outlines);
                form.Switch(label.c_str(), s.outlineFadeIn, Near(tip).c_str());
                break;
            case SettingPiece::StencilNote:
                form.Fade(!outlines);
                if (!r.stencilAvailable) form.Note(NameOf(e, c.kind));
                break;
            case SettingPiece::CursorStyle: TargetCursorStyle(c, e, label, tip); break;
            case SettingPiece::Width:
                form.Fade(!cursor);
                form.SliderInt(label.c_str(), s.cursorWidth, kMinCursorSide, kMaxCursorSide, Pixels, tip);
                break;
            case SettingPiece::CursorHeight:
                form.Fade(!cursor);
                form.SliderInt(label.c_str(), s.cursorHeight, kMinCursorSide, kMaxCursorSide, Pixels, tip);
                break;
            case SettingPiece::OutlineThickness:
                form.Fade(!cursor);
                form.SliderPercent(label.c_str(), s.cursorOutlineThickness, kMinCursorEdge, kMaxCursorEdge, tip);
                break;
            case SettingPiece::Scale:
                form.Fade(!cursor);
                form.Switch(label.c_str(), s.scaleCursor, tip);
                break;
            case SettingPiece::Float:
                form.Fade(!cursor);
                form.Switch(label.c_str(), s.cursorFloat, tip);
                break;
            case SettingPiece::CursorVerticalOffset:
                form.Fade(!cursor);
                form.SliderInt(label.c_str(), s.cursorRaise, -kMaxCursorRaise, kMaxCursorRaise, Pixels, tip);
                break;
            case SettingPiece::Color:
                form.Fade(!cursor || CursorIsPicture(s.cursorStyle));
                form.ColorPickRow(label.c_str(), s.cursorColor, CursorSources(), tip);
                break;
            case SettingPiece::EdgeColor:
                form.Fade(!cursor);
                form.ColorPickRow(label.c_str(), s.cursorEdgeColor, CursorSources(), tip);
                break;
            case SettingPiece::AggressionColors: PaletteSwatches(c, e, s.palette.aggression, kPalette.aggression); break;
            case SettingPiece::ToughnessColors: PaletteSwatches(c, e, s.palette.toughness, kPalette.toughness); break;
            case SettingPiece::ClaimColors: PaletteSwatches(c, e, s.palette.claim, kPalette.claim); break;
            case SettingPiece::TargetingColors: PaletteSwatches(c, e, s.palette.target, kPalette.target); break;
            case SettingPiece::EntityTypeColors: PaletteSwatches(c, e, s.palette.type, kPalette.type); break;
            // The entity pages
            case SettingPiece::Visibility: Visibility(c); break;
            case SettingPiece::OutlineColors: OutlineColors(c, e); break;
            case SettingPiece::Design: Design(c); break;
            case SettingPiece::MaxIconsShown:
                form.Fade(!shown);
                if (HasStatusIcons(*c.kind))
                    form.NumberDropdown(label.c_str(), s.Kind(*c.kind).maxIcons, 1, kCountedIcons,
                        [&](int most) {
                            return most == kCountedIcons ? Words(Word(c, WindowWord::MaxIconsShownAll)) : std::to_string(most);
                        },
                        tip);
                break;
            case SettingPiece::UnusedTags:
                form.Fade(!shown);
                // Under the chips, so nothing above moves under the mouse when it comes and goes.
                if (const std::string note = TagsNote(c, s.Kind(*c.kind), {Tag::Name}, WindowWord::UnusedTagsName); !note.empty())
                    form.Note(note.c_str());
                break;
            case SettingPiece::LineStyles: LineStyles(c, e); break;
            case SettingPiece::Ornament: Ornament(c, e); break;
            case SettingPiece::NoTarget:
                form.Fade(!plates);
                form.SliderPercent(label.c_str(), s.opacity.noTarget, kMinOpacity, kMaxOpacity, tip);
                break;
            case SettingPiece::NotTargeted:
                form.Fade(!plates);
                form.SliderPercent(label.c_str(), s.opacity.notTargeted, kMinOpacity, kMaxOpacity, tip);
                break;
            case SettingPiece::IsTargeted:
                form.Fade(!plates);
                form.SliderPercent(label.c_str(), s.opacity.targeted, kMinOpacity, kMaxOpacity, tip);
                break;
            case SettingPiece::FadeIn:
                form.Fade(!plates);
                form.Switch(label.c_str(), s.opacity.fadeIn, Near(tip).c_str());
                break;
            case SettingPiece::Range:
                form.Fade(!plates || !s.opacity.fadeIn);
                form.SliderFloat(label.c_str(), s.opacity.range, kMinOutlineDistance, kMaxOutlineDistance, "%.0f yalms", tip);
                break;
            case SettingPiece::Detection: Detection(c, e); break;
            // The Target widget's popup reads these too, but they are set with the nameplates.
            case SettingPiece::BadgeFont:
                form.Fade(!plates);
                form.FontDropdown(label.c_str(), s.badgeFontName, c.plateFonts, tip, Word(c, WindowWord::Default));
                break;
            case SettingPiece::BadgeNumbers:
                form.Fade(!plates);
                OptionsDropdown(form, label, s.rankDisplay, c.layout.OptionsOf(e), tip);
                break;
            case SettingPiece::CountYourAccuracy:
                form.Fade(!plates || s.rankDisplay != RankDisplay::IncreasedChance);
                form.Switch(label.c_str(), s.countAccuracy, tip);
                break;
            case SettingPiece::WeakColor:
                form.Fade(!plates);
                form.Swatch(label.c_str(), s.weakBadgeColor, tip, &kDefaults.weakBadgeColor);
                break;
            case SettingPiece::ResistColor:
                form.Fade(!plates);
                form.Swatch(label.c_str(), s.resistBadgeColor, tip, &kDefaults.resistBadgeColor);
                break;
            case SettingPiece::StatusResistColor:
                form.Fade(!plates);
                form.Swatch(label.c_str(), s.rankBadgeColor, tip, &kDefaults.rankBadgeColor);
                break;
            case SettingPiece::ImmuneColor:
                form.Fade(!plates);
                form.Swatch(label.c_str(), s.immuneBadgeColor, tip, &kDefaults.immuneBadgeColor);
                break;
            case SettingPiece::WeakFrom:
                form.Fade(!plates || !Uses(s, {Tag::Weak, Tag::Resist}));
                form.NumberDropdown(label.c_str(), s.elementWeakRank, kMaxWeakRank, kNoWeakRank, cutoff, tip, 0.0f,
                    [&](int rank) { return CutoffTip(c, rank, true); });
                break;
            case SettingPiece::ResistFrom:
                form.Fade(!plates || !Uses(s, {Tag::Weak, Tag::Resist}));
                form.NumberDropdown(label.c_str(), s.elementResistRank, kMinResistRank, kNoResistRank, cutoff, tip, 0.0f,
                    [&](int rank) { return CutoffTip(c, rank, false); });
                break;
            case SettingPiece::MaxElements:
            {
                form.Fade(!plates || !Uses(s, {Tag::Weak, Tag::Resist}));
                const auto word = [&](int count) {
                    return c.layout.WordOf(count == 0               ? WindowWord::MaxElementsNone
                                           : count == kElementCount ? WindowWord::MaxElementsAll
                                                                    : WindowWord::MaxElementsSome);
                };
                form.NumberDropdown(label.c_str(), s.maxElements, kElementCount, 0,
                    [&](int count) { return word(count).name != nullptr ? Words(word(count).name) : std::to_string(count); }, tip,
                    0.0f, [&](int count) { return Filled(Said(word(count).tip), kCountMark, std::to_string(count)); });
                break;
            }
            case SettingPiece::StatusTable: StatusTable(c, e); break;
            case SettingPiece::RespawnTimers:
                form.Fade(!on);
                form.Switch(label.c_str(), s.phTimers, tip);
                break;
            case SettingPiece::TimerSize:
                form.Fade(!on || !s.phTimers);
                form.SliderPercent(label.c_str(), s.timerPercent, kMinSizePercent, kMaxSizePercent, tip);
                break;
            case SettingPiece::UpTime:
                form.Fade(!on || !s.phTimers);
                form.SliderInt(label.c_str(), s.phUpMinutes, 0, kMaxUpMinutes, [](int minutes) { return Formatted("%d min", minutes); }, tip);
                break;
            // User Interface
            case SettingPiece::Key:
            {
                form.Fade(false);
                std::vector<uint8_t> key;
                if (s.screenshotKey != 0) key.push_back(static_cast<uint8_t>(s.screenshotKey));
                Keys(c, label, tip, key, 1, c.state.shotKeyCapture);
                s.screenshotKey = key.empty() ? 0 : key.front();
                break;
            }
            case SettingPiece::HideGameWindows:
                form.Fade(false);
                form.Switch(label.c_str(), s.screenshotHidesGame, tip);
                break;
            case SettingPiece::FishingBar:
                form.Fade(false);
                OptionsDropdown(form, label, s.fishStamina, c.layout.OptionsOf(e), tip);
                break;
            case SettingPiece::FishingBarWidth:
                form.Fade(false);
                form.SliderInt(label.c_str(), s.fishBarWidth, kMinFishBarWidth, kMaxFishBarWidth, Pixels, tip);
                break;
            case SettingPiece::FishingBarHeight:
                form.Fade(false);
                form.SliderInt(label.c_str(), s.fishBarHeight, kMinFishBarHeight, kMaxFishBarHeight, Pixels, tip);
                break;
            case SettingPiece::FishingBarX:
            case SettingPiece::FishingBarY:
            {
                form.Fade(false);
                const bool across = e.piece == SettingPiece::FishingBarX;
                int& side         = across ? s.fishBarX : s.fishBarY;
                const int screen  = across ? r.screenWidth : r.screenHeight;
                form.SliderInt(label.c_str(), side, kFishBarUnplaced, screen > 0 ? screen : kMaxFishBarPlace,
                    PixelsOr(Word(c, WindowWord::Auto), kFishBarUnplaced), tip);
                break;
            }
            case SettingPiece::FishingBarDraggable:
                form.Fade(false);
                form.Switch(label.c_str(), s.fishBarDraggable, tip);
                break;
            case SettingPiece::FishingBarFont:
                form.Fade(false);
                form.FontDropdown(label.c_str(), s.fishBarFont, c.fonts, tip, Word(c, WindowWord::Themes));
                break;
            case SettingPiece::FishingBarTextSize:
                form.Fade(false);
                form.SliderInt(label.c_str(), s.fishBarTextSize, kMinFishTextSize, kMaxFishTextSize, Pixels, tip);
                break;
            case SettingPiece::FishingBarLine:
                form.Fade(false);
                form.Line("line design");
                form.Caption(label.c_str(), "line-name");
                form.LineBox("fish line", s.fishBarLine, c.state.fishLine);
                form.EndLine();
                form.Line("line chips");
                for (const Tag tag : FishBarTags())
                    form.TagChip(tag);
                form.EndLine();
                break;
            case SettingPiece::FishColorByCatch:
                form.Fade(false);
                form.Switch(label.c_str(), s.fishColorByCatch, tip);
                break;
            case SettingPiece::FishSmallColor:
            case SettingPiece::FishLargeColor:
            case SettingPiece::FishItemColor:
            case SettingPiece::FishMonsterColor:
            case SettingPiece::FishLineColor:
            case SettingPiece::FishRodColor:
            {
                form.Fade(!s.fishColorByCatch);
                const auto colorOf = [](SettingPiece piece) {
                    switch (piece)
                    {
                    case SettingPiece::FishSmallColor: return &Settings::fishSmallColor;
                    case SettingPiece::FishLargeColor: return &Settings::fishLargeColor;
                    case SettingPiece::FishItemColor: return &Settings::fishItemColor;
                    case SettingPiece::FishMonsterColor: return &Settings::fishMonsterColor;
                    case SettingPiece::FishLineColor: return &Settings::fishLineColor;
                    default: return &Settings::fishRodColor;
                    }
                };
                Color Settings::* const color = colorOf(e.piece);
                form.Swatch(label.c_str(), s.*color, tip, &(kDefaults.*color));
                break;
            }
            case SettingPiece::Groups: WindowGroups(c); break;
            case SettingPiece::AutoAdjust:
                form.Fade(!s.shiftVanillaUi || r.windowsBand == 0);
                if (form.Action(label.c_str(), tip))
                {
                    AutoAdjustWindows(s, r.windowsBand, r.barBand);
                    form.changed = true;
                }
                break;
            case SettingPiece::GameHelp:
                form.Fade(false);
                form.Switch(label.c_str(), s.moveGameHelp, tip);
                break;
            case SettingPiece::WindowDefaults: WindowDefaults(c); break;
            case SettingPiece::WindowList: WindowList(c); break;
            // Top Panel and Widgets
            case SettingPiece::Theme:
                form.Fade(false);
                ThemeDropdown(c, label, tip);
                break;
            case SettingPiece::DeleteTheme:
                form.Fade(false);
                if (s.barTheme == BarTheme::Custom && form.Button(label.c_str(), label.c_str(), kResetPill, tip))
                {
                    c.state.themeDelete = s.customThemeName;
                    s.barTheme          = s.customPalette.parent;
                    form.changed        = true;
                }
                break;
            case SettingPiece::WidgetBackground:
                form.Fade(!BarOn(s));
                form.Switch(label.c_str(), s.widgetBackground, tip);
                break;
            case SettingPiece::ShowSeparator:
            {
                // Never is the first check, on alone when nothing else is; picking it clears the rest.
                form.Fade(!BarOn(s));
                const uint32_t shown = s.panelSeparators == 0 ? 1u : static_cast<uint32_t>(s.panelSeparators) << 1;
                uint32_t bits        = shown;
                form.BitsDropdown(label.c_str(), bits, c.layout.OptionsOf(e), tip);
                if (bits != shown) s.panelSeparators = (bits & ~shown & 1u) != 0 ? 0 : static_cast<int>(bits >> 1);
                break;
            }
            case SettingPiece::TitleFont:
                form.Fade(!BarOn(s));
                form.FontDropdown(label.c_str(), s.barTitleFont, c.fonts, tip, Word(c, WindowWord::Themes));
                break;
            case SettingPiece::TextFont:
                form.Fade(!BarOn(s));
                form.FontDropdown(label.c_str(), s.barTextFont, c.fonts, tip, Word(c, WindowWord::Themes));
                break;
            case SettingPiece::Height:
                form.Fade(!BarOn(s));
                form.SliderInt(label.c_str(), s.barHeight, kMinBarHeight, kMaxBarHeight, Pixels, tip);
                break;
            case SettingPiece::FontRatio:
            {
                // 1 in the slider's middle: a step a side for each of kFontRatioSteps.
                form.Fade(!BarOn(s));
                int position     = FontRatioPosition(s.barFontRatio);
                const int before = position;
                form.SliderInt(label.c_str(), position, 0, 2 * kFontRatioSteps,
                    [](int p) { return Printed("%.2fx", FontRatioAt(p)); }, tip);
                if (position != before) s.barFontRatio = FontRatioAt(position);
                break;
            }
            case SettingPiece::IconScale:
                form.Fade(!BarOn(s));
                form.SliderPercent(label.c_str(), s.barIconScale, kMinIconScale, kMaxIconScale, tip);
                break;
            case SettingPiece::WidgetSpacing:
                form.Fade(!BarOn(s));
                form.SliderInt(label.c_str(), s.barWidgetSpacing, 0, kMaxWidgetSpacing, Pixels, tip);
                break;
            case SettingPiece::WidgetPadding:
                form.Fade(!BarOn(s));
                form.SliderInt(label.c_str(), s.barWidgetPadding, 0, kMaxWidgetPadding, Pixels, tip);
                break;
            case SettingPiece::TextScale:
                form.Fade(!BarOn(s));
                form.SliderFloat(label.c_str(), s.popupTextScale, kMinPopupScale, kMaxPopupScale, "%.1fx", tip);
                break;
            case SettingPiece::PopupWidth:
                form.Fade(!BarOn(s));
                form.SliderFloat(label.c_str(), s.popupWidthScale, kMinPopupBoxScale, kMaxPopupScale, "%.1fx", tip);
                break;
            case SettingPiece::PopupHeight:
                form.Fade(!BarOn(s));
                form.SliderFloat(label.c_str(), s.popupHeightScale, kMinPopupBoxScale, kMaxPopupScale, "%.1fx", tip);
                break;
            case SettingPiece::CornerRounding:
            case SettingPiece::PopupRounding:
            case SettingPiece::SectionRounding:
            {
                int& rounding = e.piece == SettingPiece::CornerRounding ? s.widgetRounding
                                : e.piece == SettingPiece::PopupRounding ? s.popupRounding
                                                                               : s.sectionRounding;
                form.Fade(!BarOn(s));
                form.SliderInt(label.c_str(), rounding, kThemeRounding, kMaxRounding,
                    PixelsOr(Word(c, WindowWord::Themes), kThemeRounding), tip);
                break;
            }
            case SettingPiece::HideReadout:
                form.Fade(!on);
                form.Switch(label.c_str(), s.hideNetstat, tip);
                break;
            case SettingPiece::ShiftVanillaUi:
                form.Fade(!on);
                form.Switch(label.c_str(), s.shiftVanillaUi, tip);
                break;
            // The theme's looks as the panel shows them; a change to a built-in theme's makes a custom theme of it.
            case SettingPiece::ThemeName:
                form.Fade(!BarOn(s));
                if (s.barTheme == BarTheme::Custom) ThemeName(c, label, tip);
                break;
            case SettingPiece::BackgroundStyle:
            {
                form.Fade(!BarOn(s));
                BarPalette edited = ShownPalette(c);
                const std::vector<const char*> names = NamesOf(c.layout.OptionsOf(e));
                form.Segments(label.c_str(), edited.background, names.data(), kBarBackgroundCount, tip);
                EditTheme(c, edited);
                break;
            }
            case SettingPiece::ThemeBackground:
            {
                form.Fade(!BarOn(s));
                BarPalette edited = ShownPalette(c);
                if (edited.background != BarBackground::Plain) break;
                form.ArgbSwatch(label.c_str(), edited.stripTop, tip);
                EditTheme(c, edited);
                break;
            }
            case SettingPiece::ThemeTop:
            case SettingPiece::ThemeMiddle:
            case SettingPiece::ThemeBottom:
            {
                form.Fade(!BarOn(s));
                BarPalette edited = ShownPalette(c);
                if (edited.background != BarBackground::Gradient) break;
                form.ArgbSwatch(label.c_str(),
                    edited.*(e.piece == SettingPiece::ThemeTop      ? &BarPalette::stripTop
                             : e.piece == SettingPiece::ThemeMiddle ? &BarPalette::stripMiddle
                                                                    : &BarPalette::stripBottom),
                    tip);
                EditTheme(c, edited);
                break;
            }
            case SettingPiece::WindowType:
            {
                form.Fade(!BarOn(s));
                BarPalette edited = ShownPalette(c);
                if (edited.background != BarBackground::Window) break;
                form.SliderInt(label.c_str(), edited.windowType, kFollowGameWindow, kWindowTypes,
                    [&](int type) { return type == kFollowGameWindow ? std::string(Said(Word(c, WindowWord::Auto))) : std::to_string(type); },
                    tip);
                EditTheme(c, edited);
                break;
            }
            case SettingPiece::SeparatorStyle:
            {
                form.Fade(false);
                BarPalette edited  = ShownPalette(c);
                const auto options = c.layout.OptionsOf(e);
                int dash           = static_cast<int>(edited.separator);
                form.MarkupDropdown(label.c_str(), dash, {Words(options[0].name), kDotMark, kSquareMark, kDiamondMark}, tip,
                    [&](int mark) { return Words(options[static_cast<size_t>(mark)].tip); });
                edited.separator = static_cast<DashMark>(dash);
                EditTheme(c, edited);
                break;
            }
            case SettingPiece::ThemeBorderThickness:
            {
                form.Fade(!BarOn(s));
                BarPalette edited = ShownPalette(c);
                form.SliderInt(label.c_str(), edited.border, 0, kMaxStripBorder, Pixels, tip);
                EditTheme(c, edited);
                break;
            }
            case SettingPiece::ThemeRounding:
            {
                form.Fade(!BarOn(s));
                BarPalette edited = ShownPalette(c);
                form.SliderInt(label.c_str(), edited.rounding, kShapeRounding, kMaxRounding,
                    PixelsOr(Word(c, WindowWord::Auto), kShapeRounding), tip);
                EditTheme(c, edited);
                break;
            }
            case SettingPiece::BackgroundOpacity:
                form.Fade(!BarOn(s));
                ThemeOpacity(c, label, {&BarPalette::stripTop, &BarPalette::stripMiddle, &BarPalette::stripBottom}, tip);
                break;
            case SettingPiece::PopupOpacity:
                form.Fade(!BarOn(s));
                ThemeOpacity(c, label, {&BarPalette::panelTop, &BarPalette::panelBottom}, tip);
                break;
            case SettingPiece::StatusIcons:
            {
                form.Fade(!BarOn(s));
                BarPalette edited = ShownPalette(c);
                form.NumberDropdown(label.c_str(), edited.statusIcons, kVanillaStatusIcons, kStatusPackCount - 1,
                    [&](int pack) {
                        return pack == kVanillaStatusIcons ? Words(Word(c, WindowWord::StatusIconsVanilla)) : StatusIconsName(pack);
                    },
                    tip);
                EditTheme(c, edited);
                break;
            }
            case SettingPiece::StatusBackground:
            {
                form.Fade(!BarOn(s));
                BarPalette edited = ShownPalette(c);
                form.Switch(label.c_str(), edited.statusBack, tip);
                EditTheme(c, edited);
                break;
            }
            case SettingPiece::ThemePalette: ThemePalette(c, e); break;
            case SettingPiece::ProfileFiles: ProfileFiles(c); break;
            case SettingPiece::SaveAutomatically:
                form.Fade(false);
                form.Switch(label.c_str(), s.autoSave, tip);
                break;
            case SettingPiece::SaveChanges:
            {
                const bool pending = !s.autoSave && r.unsaved;
                form.Fade(!pending);
                form.Row(label.c_str(), tip);
                if (form.Button(Word(c, WindowWord::SaveChangesSave), "save changes") && pending) c.state.saveRequested = true;
                if (form.Button(Word(c, WindowWord::SaveChangesRevert), "revert changes", kRaisedPill,
                        WordTip(c, WindowWord::SaveChangesRevert)) && pending)
                    c.state.revertRequested = true;
                form.EndRow();
                break;
            }
            case SettingPiece::HideGameHelp:
                form.Fade(!on);
                form.Switch(label.c_str(), s.hideHelpBar, tip);
                break;
            case SettingPiece::Position:
                form.Fade(!BarOn(s) || !s.Widget(BarWidget::GameHelp).on);
                form.Offset(label.c_str(), s.helpBarX, s.helpBarY, kMaxHelpBarOffset, tip);
                break;
            case SettingPiece::Background:
                form.Fade(!BarOn(s) || !s.Widget(BarWidget::GameHelp).on);
                form.Switch(label.c_str(), s.helpBarBackground, tip);
                break;
            case SettingPiece::HelpRounding:
                form.Fade(!BarOn(s) || !s.Widget(BarWidget::GameHelp).on || !s.helpBarBackground);
                form.SliderInt(label.c_str(), s.helpBarRounding, kThemeRounding, kMaxRounding,
                    PixelsOr(Word(c, WindowWord::Themes), kThemeRounding), tip);
                break;
            case SettingPiece::HelpTextSize:
                form.Fade(!BarOn(s) || !s.Widget(BarWidget::GameHelp).on);
                form.SliderInt(label.c_str(), s.helpBarTextSize, kAutoBarTextSize, kMaxBarTextSize,
                    PixelsOr(Word(c, WindowWord::Auto), kAutoBarTextSize), tip);
                break;
            case SettingPiece::Anchor:
            {
                form.Fade(!BarOn(s) || !s.Widget(BarWidget::GameHelp).on);
                const std::vector<const char*> names = NamesOf(c.layout.OptionsOf(e));
                form.Segments(label.c_str(), s.helpBarAnchor, names.data(), kHelpAnchorCount, tip);
                break;
            }
            case SettingPiece::FullWidth:
                form.Fade(!BarOn(s) || !s.Widget(BarWidget::GameHelp).on);
                form.Switch(label.c_str(), s.helpBarFullWidth, tip);
                break;
            case SettingPiece::ShrinkToText:
                form.Fade(!BarOn(s) || !s.Widget(BarWidget::GameHelp).on || s.helpBarFullWidth);
                form.Switch(label.c_str(), s.helpBarShrink, tip);
                break;
            case SettingPiece::HelpWidth:
                form.Fade(!BarOn(s) || !s.Widget(BarWidget::GameHelp).on || s.helpBarFullWidth);
                form.SliderInt(label.c_str(), s.helpBarWidth, kMinHelpBarWidth, kMaxHelpBarWidth, [](int percent) { return Formatted("%d%%", percent); }, tip);
                break;
            case SettingPiece::HelpHeight:
                form.Fade(!BarOn(s) || !s.Widget(BarWidget::GameHelp).on);
                form.SliderInt(label.c_str(), s.helpBarHeight, kAutoHelpBarHeight, kMaxHelpBarHeight,
                    PixelsOr(Word(c, WindowWord::Auto), kAutoHelpBarHeight), tip);
                break;
            case SettingPiece::WidgetTabs: break; // in the panel's popup, by BuildWidgetSettings
            case SettingPiece::Trays: Trays(c); break;
            // Mouse Cursor and HeadsUp Config
            case SettingPiece::PointerIcon:
            {
                form.Fade(!on);
                const PictureChoice<PointerIcon> choice = PictureDropdown(c, e, label, tip, s.pointerIcon, s.pointerFile);
                s.pointerIcon = choice.style;
                s.pointerFile = choice.file;
                break;
            }
            case SettingPiece::PointerSize:
                form.Fade(!on);
                form.SliderPercent(label.c_str(), s.pointerSize, kMinPointerSize, kMaxPointerSize, tip);
                break;
            case SettingPiece::PointerFix:
                form.Fade(!on);
                form.Switch(label.c_str(), s.fixPointer, tip);
                break;
            case SettingPiece::LockCursor:
                form.Fade(!on);
                form.Switch(label.c_str(), s.lockCursor, tip);
                break;
            case SettingPiece::KeepPointer:
                form.Fade(!on);
                Keys(c, label, tip, s.keepPointerKeys, kMaxKeptKeys, c.state.keyCapture);
                break;
            case SettingPiece::Font:
                form.Fade(false);
                form.FontDropdown(label.c_str(), s.menuFont, c.menuFonts, tip);
                break;
            case SettingPiece::Size:
                form.Fade(false);
                form.Stepper(label.c_str(), s.menuFontSize, kMinMenuFontSize, kMaxMenuFontSize, "%d px", tip);
                break;
            case SettingPiece::WindowOpacity:
                form.Fade(false);
                form.SliderPercent(label.c_str(), s.menuOpacity, kMinMenuOpacity, kMaxOpacity, tip);
                break;
            case SettingPiece::ResetAllSettings: ResetAll(c, e); break;
            case SettingPiece::EnableHeadsup:
                form.Fade(false);
                form.Switch(label.c_str(), s.enabled, tip);
                break;
            case SettingPiece::FeedbackType:
                form.Fade(false);
                OptionsDropdown(form, label, c.state.feedback.type, c.layout.OptionsOf(e), tip);
                break;
            case SettingPiece::FeedbackTopic:
                form.Fade(false);
                FeedbackTopicRow(c, e, label, tip);
                break;
            case SettingPiece::FeedbackText:
                form.Fade(false);
                form.Paragraph(label.c_str(), c.state.feedback.text, kMostFeedbackText);
                break;
            case SettingPiece::SendFeedback:
                form.Fade(false);
                SendFeedback(c, label, tip);
                break;
            case SettingPiece::DebugReport:
                form.Fade(!s.debugMode);
                form.Line(s.debugMode ? "line" : (std::string("line ") + kOffClass).c_str());
                if (form.Button(label.c_str(), label.c_str(), kWashPill, tip)) c.state.debugRequested = true;
                if (r.watching.has_value() &&
                    form.Button(Labeled(Word(c, *r.watching ? WindowWord::DebugReportStop : WindowWord::DebugReportWatch), "watch").c_str(),
                        "watch", kWashPill, WordTip(c, WindowWord::DebugReportWatch)))
                    c.state.watchRequested = true;
                if (r.exportable && form.Button(Labeled(Word(c, WindowWord::Export), "export").c_str(), "export", kWashPill,
                                        WordTip(c, WindowWord::Export)))
                    c.state.exportRequested = true;
                form.EndLine();
                if (r.lastReport != nullptr && !r.lastReport->empty())
                    form.Note(Filled(Word(c, WindowWord::DebugReportSaved), kFileMark, FileName(*r.lastReport)).c_str());
                break;
            case SettingPiece::Logging:
                form.Fade(!s.debugMode);
                form.Switch(label.c_str(), s.logging, tip);
                if (r.logFile != nullptr && !r.logFile->empty())
                    form.Note(Filled(Word(c, WindowWord::LoggingFile), kFileMark, FileName(*r.logFile)).c_str());
                break;
            case SettingPiece::MobsOutlined:
                form.Fade(!s.debugMode);
                form.Reading(label.c_str(), std::to_string(r.outlinedMobs));
                break;
            case SettingPiece::MeshesOutlined:
                form.Fade(!s.debugMode);
                form.Reading(label.c_str(), std::to_string(r.meshes));
                break;
            case SettingPiece::StencilBuffer:
                form.Fade(!s.debugMode);
                form.Reading(label.c_str(), Word(c, r.stencilAvailable ? WindowWord::StencilBufferAvailable : WindowWord::StencilBufferMissing));
                break;
            case SettingPiece::Nameplates:
                form.Fade(!s.debugMode);
                form.Reading(label.c_str(), std::to_string(r.nameplates));
                break;
            case SettingPiece::Drawn:
                form.Fade(!s.debugMode);
                form.Reading(label.c_str(), Word(c, r.drewInScene ? WindowWord::DrawnInScene : WindowWord::DrawnOnTop));
                break;
            case SettingPiece::GameSNames:
                form.Fade(!s.debugMode);
                form.Reading(label.c_str(),
                    r.hooked ? Formatted("%u, %u by HeadsUp, %u hidden", r.names, r.replaced, r.hidden)
                             : Words(Word(c, WindowWord::GameSNamesNotHooked)));
                break;
            case SettingPiece::FrameTime:
                form.Fade(!s.debugMode);
                form.Reading(label.c_str(), Formatted("%.2f ms (%.0f fps)", r.frameMs, r.frameMs > 0.0 ? 1000.0 / r.frameMs : 0.0));
                break;
            case SettingPiece::LastSave:
                form.Fade(!s.debugMode);
                form.Reading(label.c_str(),
                    Formatted("%.0f ms, %zu %s", r.lastSaveMs, r.lastSaveKeys, Word(c, r.lastSaveKeys == 1 ? WindowWord::LastSaveKey : WindowWord::LastSaveKeys)));
                break;
            case SettingPiece::YourLevel:
                form.Fade(!s.debugMode);
                form.Reading(label.c_str(),
                    r.level <= 0  ? Words(Word(c, WindowWord::YourLevelUnknown))
                    : r.sitting ? Filled(Said(Word(c, WindowWord::YourLevelSitting)), kCountMark, std::to_string(r.level))
                                : std::to_string(r.level));
                break;
            }
        }

        // What a card does of its own, wherever it is: whether it is left out, whether it is switched off, and the
        // switch in its title.
        struct CardState
        {
            bool skipped  = false;
            bool off      = false;
            bool* enabled = nullptr;
            bool bare     = false; // drawn under the cards with no card of its own
        };
        CardState CardStateOf(CardPiece piece, Context& c)
        {
            Settings& s       = c.s;
            const bool on     = s.enabled;
            const bool plates = s.enabled && s.nameplates;
            switch (piece)
            {
            case CardPiece::Plain:
            case CardPiece::ScreenshotMode:
            case CardPiece::WindowGroups:
            case CardPiece::BaseGameUi:
            case CardPiece::ThemeColors:
            case CardPiece::SettingsWindow:
            case CardPiece::Profiles:
            case CardPiece::FeedbackForm: return {};
            case CardPiece::Reset: return {.bare = true};
            case CardPiece::Nameplates: return {false, !on, &s.nameplates};
            case CardPiece::OutlineStyle: return {false, !on, &s.outlines};
            case CardPiece::TargetCursor:
            case CardPiece::ColorsAggression:
            case CardPiece::ColorsToughness:
            case CardPiece::ColorsClaimStatus:
            case CardPiece::ColorsTargeting:
            case CardPiece::ColorsEntityType:
            case CardPiece::Visibility:
            case CardPiece::PlaceholderTimers:
            case CardPiece::MousePointer: return {false, !on};
            case CardPiece::OutlineDesign: return {c.kind && !c.outlineColors, !on || !s.outlines};
            case CardPiece::Badges:
            case CardPiece::NameplateDesign:
            case CardPiece::LineStyle:
            case CardPiece::Ornament:
            case CardPiece::NameplateOpacity:
            case CardPiece::DetectionIcons:
            case CardPiece::Weaknesses:
            case CardPiece::Statuses: return {false, !plates};
            case CardPiece::TopPanel: return {false, !on, &s.showBar};
            case CardPiece::HelpBar: return {false, !BarOn(s), &s.Widget(BarWidget::GameHelp).on};
            case CardPiece::Widgets: return {.skipped = true}; // in the panel's popup, by BuildWidgetSettings
            case CardPiece::Layout:
            case CardPiece::WidgetAppearance:
            case CardPiece::PopupAppearance: return {false, !BarOn(s)};
            case CardPiece::DebugMode: return {false, false, &s.debugMode};
            case CardPiece::FishingAppearance: return {false, !on};
            }
            return {};
        }

        // A card whose settings show only through tags says which, while no line of the mobs' design holds one.
        void CardNote(CardPiece piece, Context& c)
        {
            const auto note = [&](std::initializer_list<Tag> tags) {
                if (const std::string text = TagsNote(c, c.s.Kind(PlateKind::Mobs), tags, WindowWord::UnusedTagsThese); !text.empty())
                    c.form.Note(text.c_str());
            };
            switch (piece)
            {
            case CardPiece::DetectionIcons: note({Tag::Detect}); break;
            case CardPiece::Badges: note({Tag::Weak, Tag::Resist, Tag::Status, Tag::Immune}); break;
            case CardPiece::Weaknesses: note({Tag::Weak, Tag::Resist}); break;
            case CardPiece::Statuses: note({Tag::Status}); break;
            default: break;
            }
        }

        // Setup's choice as a dropdown of its options with their tips, Your Own while the settings hold none of them.
        void SetupChoiceRow(Context& c, const SetupChoice& choice)
        {
            const std::span<const LayoutOption> options = c.layout.OptionsOf(choice.list);
            SetupRun& run                               = *c.state.setup;
            const auto [known, fresh]                   = run.picked.try_emplace(&choice);
            if (fresh) known->second = PickedOption(choice, c.s);
            const std::optional<size_t> held = known->second;
            const int was = held ? static_cast<int>(*held) : kNoOption;
            int picked    = was;
            const auto name = [&](int i) { return i < 0 ? Words(Word(c, WindowWord::SetupCustom)) : Words(options[static_cast<size_t>(i)].name); };
            const auto tip  = [&](int i) { return i < 0 ? Words(WordTip(c, WindowWord::SetupCustom)) : Words(options[static_cast<size_t>(i)].tip); };
            c.form.NumberDropdown(Labeled(Word(c, choice.word), choice.key).c_str(), picked, 0, static_cast<int>(choice.options.size()) - 1, name,
                WordTip(c, choice.word), 0.0f, tip);
            if (picked == was || picked < 0) return;
            choice.options[static_cast<size_t>(picked)].apply(c.s);
            run.picked.clear();
            c.form.changed = true;
        }

        // The Nameplates step's Advanced places: a kind's tab and a line's, then where that line stands, which a line synced to
        // another's style takes from that line.
        void SetupLines(Context& c, SetupRun& run)
        {
            Form& form                  = c.form;
            const LayoutSetting* styles = c.layout.SettingOf(SettingPiece::LineStyles);
            if (styles == nullptr) return;
            const std::vector<const char*> kinds = NamesOf(c.layout.OptionsOf(WindowList::SetupLineKinds));
            run.lineKind = form.Tabs("line kind", run.lineKind, kinds.data(), static_cast<int>(kinds.size()));
            const auto lines = c.layout.OptionsOf(WindowList::PlateLines);
            const std::vector<const char*> lineNames = NamesOf(lines);
            run.line = form.Tabs("line", run.line, lineNames.data(), kPlateLines);
            const PlateKind kind = kSetupLineKinds[run.lineKind];
            KindSettings& k      = c.s.Kind(kind);
            const int styled     = StyleLineOf(k, run.line);
            if (styled != run.line)
                form.Note(Filled(Said(Word(c, WindowWord::LineStylesUsesStyle)), kLineMark, Said(lines[static_cast<size_t>(styled)].name)).c_str());
            const std::string scope = std::string(" ") + KindKey(kind) + std::to_string(styled);
            for (const LayoutRow& row : c.layout.RowsOf(*styles))
                LinePlaceRow(form, row, kind, styled, Labeled(NameOf(row, kind), row.key, scope), k.lines[styled]);
        }

        // The Top Panel step's Advanced: a switch for each widget, in groups.
        void SetupWidgets(Context& c)
        {
            const std::span<const LayoutOption> groups  = c.layout.OptionsOf(WindowList::SetupWidgetGroups);
            const std::span<const LayoutOption> widgets = c.layout.OptionsOf(WindowList::BarWidgets);
            for (size_t g = 0; g < SetupWidgetGroups().size(); ++g)
            {
                c.form.Caption(Words(groups[g].name).c_str());
                for (BarWidget widget : SetupWidgetGroups()[g].widgets)
                {
                    const LayoutOption& named = widgets[static_cast<size_t>(widget)];
                    bool shown = AnyCopyShown(c.s, widget);
                    const bool was = shown;
                    c.form.Switch(Labeled(named.name, "setup-widget-" + std::to_string(static_cast<int>(widget))).c_str(), shown, named.tip);
                    if (shown == was) continue;
                    SetWidgetShown(c.s, widget, shown);
                    c.form.changed = true;
                }
            }
        }


        // A step's card: the warning of a run again, the preview, its choices and its settings.
        void SetupStepCard(Context& c, const SetupPage& page)
        {
            Form& form    = c.form;
            SetupRun& run = *c.state.setup;
            if (run.rerun && !run.started)
                form.Note((Words(Word(c, WindowWord::SetupWarning)) + ": " +
                           Filled(Said(WordTip(c, WindowWord::SetupWarning)), kFileMark,
                                  kSetupBackupStem + std::string(kSetupBackupWhen) + kSetupBackupExtension))
                              .c_str());
            form.Raw(std::string("<img class=\"") + kSetupPreviewClass + " " + kWheelClass + " " + kPanClass + "\" id=\"" +
                     kSetupPreviewClass + "\" src=\"" + kSetupPicture + "\"/>");
            if (const FormEvent* wheel = form.Took(kSetupPreviewClass, kWheelEvent))
                run.zoom = std::clamp(run.zoom - static_cast<int>(std::lround(std::stof(wheel->value))) * kSetupZoomNotch, kLeastSetupZoom,
                    kMostSetupZoom);
            form.SliderPercent(Labeled(Word(c, WindowWord::SetupZoom), "zoom").c_str(), run.zoom, kLeastSetupZoom, kMostSetupZoom,
                WordTip(c, WindowWord::SetupZoom));
            if (const FormEvent* pan = form.Took(kSetupPreviewClass, kPanEvent))
            {
                float across = 0.0f, down = 0.0f;
                std::sscanf(pan->value.c_str(), "%f %f", &across, &down);
                run.panX += across;
                run.panY += down;
            }
            // Zoomed z times, the picture can go (z - 1) / 2 of itself each way before the scene's edge comes in.
            const float room = (static_cast<float>(run.zoom) / static_cast<float>(kLeastSetupZoom) - 1.0f) / 2.0f;
            run.panX         = std::clamp(run.panX, -room, room);
            run.panY         = std::clamp(run.panY, -room, room);
            const auto draw = [&](std::span<const SetupChoice> choices, std::span<const SettingPiece> settings) {
                for (const SetupChoice& choice : choices)
                    SetupChoiceRow(c, choice);
                for (const SettingPiece piece : settings)
                    if (const LayoutSetting* setting = c.layout.SettingOf(piece)) DrawSetting(c, *setting);
            };
            draw(page.choices, page.settings);
            const bool lines = page.step == SetupStep::Nameplates, widgets = page.step == SetupStep::TopPanel && c.s.showBar;
            if (page.advancedChoices.empty() && page.advancedSettings.empty() && !lines && !widgets)
            {
                form.EndCard();
                return;
            }
            const WindowWord toggle = run.advanced ? WindowWord::SetupSimple : WindowWord::SetupAdvanced;
            form.Line();
            if (form.Button(Labeled(Word(c, toggle), "advanced").c_str(), "advanced", kRaisedPill, WordTip(c, toggle)))
                run.advanced = !run.advanced;
            form.EndLine();
            if (run.advanced)
            {
                draw(page.advancedChoices, page.advancedSettings);
                if (lines) SetupLines(c, run);
                if (widgets) SetupWidgets(c);
            }
            form.EndCard();
        }

        // Where Setup is and the buttons that move through it, under its card, where folding the card leaves them.
        void SetupButtons(Context& c)
        {
            Form& form      = c.form;
            SetupRun& run   = *c.state.setup;
            const int step  = static_cast<int>(run.step);
            const bool last = step + 1 == kSetupSteps;
            form.Fade(false); // a setting drawn above may have faded what follows it
            form.BeginBare(Labeled(Word(c, WindowWord::SetupTitle), kSetupButtons).c_str());
            form.Line();
            const auto button = [&](WindowWord word, const char* id, const char* classes) {
                return form.Button(Labeled(Word(c, word), id).c_str(), id, classes, WordTip(c, word));
            };
            const std::string count = Filled(Said(Word(c, WindowWord::SetupStep)), kCountMark, std::to_string(step + 1));
            form.Caption(Filled(count, kTotalMark, std::to_string(kSetupSteps)).c_str());
            bool finished = false, left = false;
            if (!run.started)
            {
                if (button(WindowWord::SetupStart, "start", kAccentPill))
                {
                    c.state.setupBackup = run.rerun;
                    run.started      = true;
                    run.step         = SetupStep::Theme;
                    form.scrollToTop = true;
                }
            }
            else
            {
                if (step > 0 && button(WindowWord::SetupBack, "back", kRaisedPill))
                {
                    run.step         = static_cast<SetupStep>(step - 1);
                    form.scrollToTop = true;
                }
                if (!last && button(WindowWord::SetupNext, "next", kAccentPill))
                {
                    run.step         = static_cast<SetupStep>(step + 1);
                    form.scrollToTop = true;
                }
                finished = last && button(WindowWord::SetupFinish, "finish", kAccentPill);
            }
            left = !finished && button(run.rerun ? WindowWord::SetupCancel : WindowWord::SetupSkip, "leave", kRaisedPill);
            form.EndLine();
            form.EndCard();
            if (!finished && !left) return;
            EndSetup(c.s, c.state, finished);
            form.changed     = true;
            form.scrollToTop = true;
        }

        // Setup in place of the pages: its steps in the sidebar, then the step open and the buttons under it.
        void BuildSetup(Context& c)
        {
            Form& form    = c.form;
            SetupRun& run = *c.state.setup;
            if (run.pickedFor != c.s)
            {
                run.pickedFor = c.s;
                run.picked.clear();
            }
            form.BeginBlock(kSidebarBlock);
            for (const SetupPage& page : SetupPages())
            {
                const std::string id = kNavStart + std::string("setup-") + page.key;
                if (run.started && form.Clicked(id) && run.step != page.step)
                {
                    run.step         = page.step;
                    form.scrollToTop = true;
                    c.state.form.popup.clear();
                }
                form.Raw("<div class=\"nav\" id=\"" + id + "\">" + EscapedRml(Words(Word(c, page.word))) + "</div>");
            }
            for (const SetupPage& page : SetupPages())
                form.Set(kNavStart + std::string("setup-") + page.key, FormValueKind::Class, kPickedClass, {}, page.step == run.step);

            form.pageKey          = kSetupPageKey;
            const SetupPage& page = SetupPageOf(run.step);
            if (form.BeginCard(Labeled(Word(c, page.word), page.key).c_str(), nullptr, WordTip(c, page.word))) SetupStepCard(c, page);
            SetupButtons(c);
        }
    }

    std::string WidgetDefaultText(const WindowLayout& layout, WidgetTabsRow row, PanelWidget widget)
    {
        static const Settings defaults = DefaultSettings();
        const headsup::WidgetOptions& o = defaults.Widget(widget);
        switch (row)
        {
        case WidgetTabsRow::Icon: return layout.OptionsOf(WindowList::IconSides)[static_cast<size_t>(o.iconSide)].name;
        case WidgetTabsRow::Alignment: return layout.OptionsOf(WindowList::WidgetAligns)[static_cast<size_t>(o.align)].name;
        case WidgetTabsRow::LinkedPadding: return Pixels(o.linkPadding);
        default: return {};
        }
    }

    void FeedbackAnswered(MenuFormState& state, bool sent)
    {
        state.feedbackStatus = sent ? FeedbackStatus::Sent : FeedbackStatus::Failed;
        if (sent) state.feedback = {};
    }

    void EndSetup(Settings& s, MenuFormState& state, bool keep)
    {
        if (!state.setup) return;
        if (!keep) s = state.setup->before;
        s.setupDone          = true;
        state.closeRequested = !keep;
        state.setup.reset();
        state.page = Page::Entities;
    }

    void BuildMenu(Form& form, Settings& s, const MenuReadings& readings, MenuFormState& state,
        const std::vector<std::string>& fonts, const std::vector<std::string>& menuFonts, const WindowLayout& layout)
    {
        form.layout = &layout;
        if (state.setup)
        {
            Context c{form, s, readings, state, fonts, menuFonts, layout, std::nullopt};
            BuildSetup(c);
            s = Clamp(std::move(s));
            return;
        }
        if (KindOfPage(state.page) && !KindPagesShown(s))
        {
            state.Leave();
            state.page       = Page::Entities;
            form.scrollToTop = true;
        }
        Context c{form, s, readings, state, fonts, menuFonts, layout, KindOfPage(state.page)};
        Sidebar(c);
        const LayoutPage& page = layout.PageOf(state.page);
        form.pageKey           = page.key;
        if (page.description != nullptr) form.PageNote(page.description);
        if (c.kind)
        {
            if (state.design.kind != *c.kind) state.design = DesignEditor{.kind = *c.kind};
            const KindSettings& k = s.Kind(*c.kind);
            for (int r = 0; r < RowCountOf(*c.kind); ++r)
            {
                c.outlineColors |= HasOutlineColor(k.rows[r].outline);
                c.plates |= DrawsPlate(k.rows[r].plate);
            }
        }
        const uint8_t kinds = c.kind ? KindBit(*c.kind) : kEveryKind;
        for (const LayoutCard& card : layout.CardsOf(page))
        {
            if ((card.kinds & kinds) == 0) continue;
            const CardState own = CardStateOf(card.piece, c);
            if (own.skipped) continue;
            form.Fade(own.off);
            // Widget Layout's arrow puts every widget back in its first place and takes off any copies.
            const bool layoutCard = card.piece == CardPiece::Layout;
            bool reset            = false;
            if (own.bare)
                form.BeginBare(Labeled(card.name, card.key).c_str());
            else
            {
                const bool open = form.BeginCard(Labeled(card.name, card.key).c_str(), own.enabled, card.about,
                    layoutCard ? &reset : nullptr, layoutCard && s.barSections != DefaultPlacement());
                if (reset)
                {
                    s.barSections = DefaultPlacement();
                    form.changed  = true;
                }
                if (!open) continue;
            }
            CardNote(card.piece, c);
            for (const LayoutSetting& setting : layout.SettingsOf(card))
            {
                if ((setting.kinds & kinds) == 0 || (setting.entity && !c.kind)) continue;
                if (card.piece == CardPiece::FishingAppearance && setting.piece != SettingPiece::FishingBar &&
                    s.fishStamina != FishBarShown::HeadsUp)
                    continue;
                form.Indent(setting.indent);
                DrawSetting(c, setting);
                form.Indent(false);
            }
            form.EndCard();
        }
        form.Fade(false);
        // A page whose every card is switched off says what switches them on.
        if (form.CardsShown() == 0 && page.cardCount > 0 && form.BeginCard(Labeled(Word(c, WindowWord::SwitchedOff), "switched-off").c_str()))
        {
            form.Note(Word(c, s.enabled ? WindowWord::SwitchedOffTopPanel : WindowWord::SwitchedOffHeadsup));
            form.EndCard();
        }
        s = Clamp(std::move(s));
    }

    void BuildWidgetSettings(Form& form, Settings& s, const MenuReadings& readings, MenuFormState& state,
        const std::vector<std::string>& fonts, const std::vector<std::string>& menuFonts, const WindowLayout& layout)
    {
        const LayoutSetting* tabs = layout.SettingOf(SettingPiece::WidgetTabs);
        if (readings.settingsOf.empty() || state.setup || tabs == nullptr) return;
        form.layout  = &layout;
        form.pageKey = layout.PageOf(Page::TopPanel).key;
        Context c{form, s, readings, state, fonts, menuFonts, layout, std::nullopt};
        form.Fade(false);
        form.BeginBare(kWidgetSettingsBlock);
        form.blocks.back().popout = true;
        PopupSettings(c, *tabs);
        form.EndCard();
        form.Fade(false);
        s = Clamp(std::move(s));
    }
}
