#pragma once

#include "form.h"
#include "menu_state.h"
#include "settings.h"
#include "setup.h"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace headsup
{
    // What the pages read from the frame that just ended: the This Frame card's figures, and what other pages ask.
    struct MenuReadings
    {
        uint32_t outlinedMobs = 0, meshes = 0, nameplates = 0;
        double frameMs = 0.0;
        double lastSaveMs = 0.0;   // the last settings write
        size_t lastSaveKeys = 0;   // and how many keys it wrote
        bool stencilAvailable = true;
        bool drewInScene = false;  // nameplates went into the game's scene image, behind walls
        bool hooked = false;       // the game's name routine
        uint32_t names = 0, replaced = 0, hidden = 0; // the game's names, those HeadsUp drew, those no one drew
        int level = 0;             // yours; 0 while unknown
        bool sitting = false;
        int barTitlePixels = 0, barDetailPixels = 0; // the line sizes the bar drew last frame, 0 when it is off
        // In the game's menu pixels: the line the game's windows went under last frame, and the panel's own lowest
        // reach without its help line; 0 while the panel is off.
        int windowsBand = 0, barBand = 0;
        const std::vector<std::string>* seenWindows = nullptr; // the game's windows seen under Game help's bar
        const std::vector<WindowPlace>* windowPlaces = nullptr; // where each window seen last stood
        const std::vector<std::string>* cursorFiles = nullptr; // the cursors folder's files, without their extensions
        const std::vector<std::string>* themeFiles  = nullptr; // the themes folder's, the same way
        const std::vector<std::string>* profiles    = nullptr; // the profiles folder's, the same way
        const std::string* profile = nullptr; // the profile in use, empty while the old settings.ini is
        bool unsaved = false; // changes not yet in the file
        std::optional<bool> watching;            // whether the debug watch runs; none in a build without one
        const std::string* lastReport = nullptr; // the file the last report went to, empty before one
        const std::string* logFile    = nullptr; // the file notes go to, empty before one
        bool exportable = false; // a developer's build, which can write its built-in layout out as a file
        // The widgets whose settings the panel's open popup shows, in its pill's order; none while it shows none.
        std::vector<PanelWidget> settingsOf;
        // The picture each part's chip in the Widget Designer shows, as the game is now.
        std::array<PartMark, kWidgetPartCount> partMarks{};
    };

    // The Game Windows card's own state: its filter and whether it lists every window.
    struct WindowFilter
    {
        std::string text;
        bool all = false;
    };

    // How near the preview is, in percent: the whole scene, and each notch of the wheel over it.
    constexpr int kLeastSetupZoom = 100, kMostSetupZoom = 300, kSetupZoomNotch = 25;
    // Setup while the window shows it: the step open, whether it is a run after the first (which warns, then starts
    // from the defaults), whether it has started, and the settings as they were when it opened, which Skip and
    // Cancel put back.
    struct SetupRun
    {
        SetupStep step = SetupStep::Welcome;
        bool rerun     = false;
        bool started   = false;
        Settings before;
        int zoom = kLeastSetupZoom;
        float panX = 0.0f, panY = 0.0f; // shares of the picture it is dragged across and down, as far as the zoom leaves room
        int lineKind = 0, line = 0; // the Nameplates step's Advanced tabs: into kSetupLineKinds, and the line
        bool advanced = false;      // each step shows the parts of its presets, until hidden again
        // Which option each choice drawn holds, while the settings are still pickedFor.
        Settings pickedFor = {};
        std::unordered_map<const SetupChoice*, std::optional<size_t>> picked = {};
    };

    // What Setup's warning names its backup by in place of when it is made (SetupBackupName), and the picture Setup
    // shows its preview in.
    constexpr const char* kSetupBackupWhen = "<date>-<time>";
    constexpr const char* kSetupPicture    = "hu:setup/scene";

    // What the settings window keeps between passes that is no setting.
    struct MenuFormState
    {
        FormState form;
        Page page          = Page::Entities;
        // A widget whose settings the panel's popup is to show, or stop showing when it shows them: a chip clicked in
        // the Layout, a clone made, a copy removed. Once, for the plugin.
        std::optional<PanelWidget> settingsAsked;
        // A widget whose designer was asked to switch off its last part: it says why under its lines.
        std::optional<PanelWidget> partRefused;
        bool confirmReset  = false; // the reset button was pressed once and asks for a second press
        bool debugRequested = false;
        bool watchRequested = false; // the debug watch is to start, or stop
        bool exportRequested = false;
        bool cursorsRequested = false; // a list of pictures was opened: the cursors folder is to be read again
        bool themesRequested  = false; // the list of saved themes was opened: the themes folder is to be read again
        std::string themeName;   // typed to save the Custom theme under
        std::string themePicked; // the saved theme picked to load
        std::optional<std::string> themeSave, themeLoad; // what the plugin is to write or read in, once
        bool saveRequested = false, revertRequested = false; // by hand: write the changes, or drop them
        bool profilesRequested = false; // the list of profiles was opened: the profiles folder is to be read again
        std::string profileName;   // typed for a new profile, or the one in use's new name
        std::string profilePicked; // the profile picked in the list
        // What the plugin is to do with a profile, once: use it, make it from the settings now, give the one in use
        // its name, or delete it.
        std::optional<std::string> profileUse, profileNew, profileRename, profileDelete;
        KeyCapture keyCapture;      // the Mouse Cursor page's
        KeyCapture shotKeyCapture;  // screenshot mode's
        WindowFilter windows;
        std::optional<HelpMode> shiftsFor; // the Window Groups card's mode shown; none for the one the help is in
        DesignEditor design;
        LineDraft fishLine; // the fishing bar's line, as its box is being written
        std::optional<SetupRun> setup; // Setup is shown in place of the pages
        // The nameplates' font choices, made from the installed fonts in plateFontsFrom.
        std::vector<std::string> plateFonts, plateFontsFrom;
        bool setupBackup    = false;   // a run again started: the settings file is to be copied first
        bool closeRequested = false;   // Setup was skipped or cancelled, and the window closes

        // What a page was in the middle of is dropped when it is left, by another page or by the window closing: a
        // row waiting for a key would take one pressed for the game, and a reset asked once would wait armed.
        void Leave()
        {
            form.popup.clear();
            confirmReset   = false;
            keyCapture     = {};
            shotKeyCapture = {};
        }
    };

    // Setup ended, never to open by itself again: Finish keeps the picks, and Skip, Cancel or the window's close
    // button put back the settings it opened with and close the window.
    void EndSetup(Settings& s, MenuFormState& state, bool keep);

    // One pass of the window: the sidebar, then the open page's cards, as the layout orders and words them. fonts:
    // FontChoices, for the nameplates' and the bar's fonts. menuFonts: the fonts the window itself can be set in.
    void BuildMenu(Form& form, Settings& settings, const MenuReadings& readings, MenuFormState& state,
        const std::vector<std::string>& fonts, const std::vector<std::string>& menuFonts,
        const WindowLayout& layout = kWindowLayout);

    // The block of the settings the panel's popup shows, whether the window is open or not: each widget's under its
    // name when there are more than one. Nothing while it shows none.
    constexpr const char* kWidgetSettingsBlock = "widgets"; // its controls' ids are the Widgets card's
    void BuildWidgetSettings(Form& form, Settings& settings, const MenuReadings& readings, MenuFormState& state,
        const std::vector<std::string>& fonts, const std::vector<std::string>& menuFonts,
        const WindowLayout& layout = kWindowLayout);
}
