#pragma once

#include "form.h"
#include "menu_document.h"
#include "menu_form.h"
#include "popup_layout.h"
#include "settings.h"
#include "window_skin.h"

#include <RmlUi/Core.h>

#include <array>
#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace headsup
{
    // What the settings window is given by where it runs.
    struct MenuHost
    {
        Rml::Context* context = nullptr;      // the one it is a document of; none while RmlUi is not running
        std::string textFont;                 // the family its text is set in, one RmlUi has
        std::vector<std::string> menuFonts;   // the fonts it can be set in, by their kMenuFonts names
        std::function<bool(uint8_t)> keyDown; // whether a key is down now, by its virtual-key code
        const WindowLayout* layout = &kWindowLayout; // its pages, cards and settings, in order, with their words
        bool developer = false; // a developer's build, whose window can read its layout's file anew and write it out
        std::optional<GameSkinLook> skin = std::nullopt; // the game's window art of the Window Type in use, once it is read
    };

    // The /headsup settings window as an RmlUi document in the host's overlay. While it is open, each frame runs
    // the pages against the kit (menu_form, form) with what the player did since the last frame, replaces the
    // cards whose markup changed and sets the values that did.
    class RmlMenu : public Rml::EventListener
    {
    public:
        bool open = false;
        // Where the panel's open popup leaves room for its settings on the screen, while it shows them
        // (MenuReadings::settingsOf).
        std::optional<BarRect> settingsAt;

        // True when a setting changed and should be saved. fonts: FontChoices for the nameplates' and the bar's fonts.
        bool Update(const MenuHost& host, Settings& settings, const MenuReadings& readings,
            const std::vector<std::string>& fonts, double version);
        // Closes the window's document; before the host stops.
        void Close();
        // The room these widgets' settings take as they were last laid out over the panel's popup: 0 tall while they
        // were not. None for no widgets.
        std::optional<SettingsRoom> RoomFor(const std::vector<PanelWidget>& widgets, const Settings& settings) const;
        // Once: the widget whose popup is to show its settings, or stop showing them (MenuFormState::settingsAsked).
        std::optional<PanelWidget> TakeSettingsAsked();
        // True once after the Debug Report button was pressed: write the /hu debug report.
        bool TakeDebugRequest();
        // Opens the window on Setup, from the settings as they are: the first time, or again (rerun), which warns.
        void StartSetup(const Settings& settings, bool rerun);
        bool InSetup() const { return m_State.setup.has_value(); }
        const SetupRun* SetupShown() const { return m_State.setup ? &*m_State.setup : nullptr; }
        // True once after Setup was run again: the settings file is to be copied first.
        bool TakeSetupBackup();
        bool TakeWatchRequest();
        bool TakeReloadRequest(); // the layout's file is to be read again and the window built anew
        bool TakeExportRequest(); // the built-in layout is to be written out as a file
        // True once after a list of pictures (Target Cursor, Cursor Icon) was opened: read the cursors folder again.
        bool TakeCursorsRequest();
        // Once each: the themes folder is to be read again, a custom theme read in and picked, or the one in use renamed.
        bool TakeThemesRequest();
        std::optional<std::string> TakeThemeLoad();
        std::optional<std::string> TakeThemeRename();
        std::optional<std::string> TakeThemeDelete();
        bool TakeProfilesRequest();
        bool TakeSaveRequest();
        bool TakeRevertRequest();
        std::optional<std::string> TakeProfileUse();
        std::optional<std::string> TakeProfileNew();
        std::optional<std::string> TakeProfileRename();
        std::optional<std::string> TakeProfileDelete();
        // Once after Send Feedback was pressed with the form filled: the feedback to send. Answered says how it went.
        std::optional<FeedbackDraft> TakeFeedback();
        void FeedbackAnswered(bool sent) { headsup::FeedbackAnswered(m_State, sent); }
        // Opens the window at the page named, as its sidebar spells it in lower case without spaces ("toppanel"),
        // scrolled to the top of its card of this key when one is given; false for a name no page has.
        bool OpenPage(const std::string& name, const std::string& card = {});
        // Scrolls the page this far down when it is next drawn.
        void ScrollTo(float pixels) { m_ScrollTo = pixels; }

        void ProcessEvent(Rml::Event& event) override;
        // The middle of the element of this id, in the screen's pixels, scrolled into view first; nothing while the
        // window lacks it.
        std::optional<Rml::Vector2f> CenterOf(const std::string& id, bool scroll = true);

    private:
        const WindowLayout* m_Layout = &kWindowLayout; // the host's, from its last update
        struct Applied
        {
            std::string text;
            bool on = false;
        };

        // Builds the document again when its look changed. False when RmlUi refused it.
        bool Document(const MenuHost& host, const Settings& settings);
        // Puts the window where the settings have it, or in the screen's middle; then keeps the settings to where the
        // player moves and sizes it. True when they changed.
        bool Place(const MenuHost& host, Settings& settings, float pixels);
        void ApplyOpacity();
        bool RunKey(Rml::ElementFormControlInput* input, const std::string& id, Rml::Input::KeyIdentifier key);
        // A drop, or a drag held (type), of dragged on the control under the pointer, as the form is told of it.
        FormEvent DropOn(Rml::Element* hovered, Rml::Element* dragged, const char* type, Rml::Event& event) const;
        // One pass of the pages, while the window is open, and the panel popup's settings over these events, told
        // which control has the keyboard.
        Form Pass(std::vector<FormEvent> events, const MenuHost& host, Settings& settings, const MenuReadings& readings,
            const std::vector<std::string>& fonts, bool window = true);
        void FitRuns();
        // The window's pages when window, and the settings over the panel's popup, for these widgets at this size.
        void ApplyBlocks(const std::vector<FormBlock>& blocks, const std::vector<PanelWidget>& of, int pixels, bool window = true);
        // Shows the settings' block over the panel's popup at settingsAt, or hides them without one. True when its
        // markup was replaced.
        bool ApplyView(const FormBlock* block, const std::vector<PanelWidget>& of, int pixels);
        // No tip stays once the pointer has left the document, and a slider of it gives up the keyboard.
        void LetGo(const MenuHost& host, Rml::ElementDocument* document, Rml::Element* tip);
        void ApplyValues(const std::vector<FormValue>& values);
        void ApplyPopup(const std::optional<FormPopup>& popup);
        // Every element with an id, found again after markup was replaced.
        void Index();
        void ShowTip(Rml::Element* under, float x, float y);
        Rml::Element* Find(const std::string& id) const;

        Rml::ElementDocument* m_Document = nullptr;
        MenuLook m_Look;              // what the document was built with
        float m_FontPixels = 0.0f;
        int m_Opacity      = 0; // the percent the document was last given; 0 for none yet
        bool m_Visible     = false;
        bool m_Revealing   = false; // shown this frame, hidden until the next
        bool m_Placed      = false;   // it has its first place
        std::optional<std::array<int, 4>> m_OpenedAt; // where it was first laid out: left, top, width, height
        Rml::Element* m_Sidebar   = nullptr;
        Rml::Element* m_Page      = nullptr;
        Rml::Element* m_PageTitle = nullptr;
        Rml::Element* m_Popup     = nullptr;
        Rml::Element* m_Tip       = nullptr;
        Rml::Element* m_PopupIn   = nullptr; // the popup holding m_PopupRml: the window's or the settings'
        Rml::Element* m_TipIn     = nullptr; // the tip showing m_TipShown
        // The panel popup's settings, in a document of their own with a popup and tip.
        Rml::ElementDocument* m_View = nullptr;
        Rml::Element* m_ViewBody  = nullptr;
        Rml::Element* m_ViewPopup = nullptr;
        Rml::Element* m_ViewTip   = nullptr;
        bool m_ViewShown          = false;
        int m_ViewPixels          = 0;
        std::optional<BarRect> m_ViewAt;
        std::vector<PanelWidget> m_ViewFor; // whose settings it holds
        std::string m_SidebarRml, m_PopupRml, m_ViewRml, m_TitleShown, m_TipShown;
        std::vector<FormBlock> m_Cards;
        std::unordered_map<std::string, Rml::Element*> m_ById;
        std::vector<Rml::Element*> m_Runs; // the stretches of the line boxes
        std::unordered_map<std::string, Applied> m_Applied; // what each value was last set to, by its id, kind and name
        std::unordered_map<std::string, std::string> m_FocusValue; // a text box's text as it took the keyboard
        std::vector<FormEvent> m_Events; // since the last pass
        FormEvent m_HeldOver;            // the drag held, as last told
        Rml::Vector2f m_PanAt;           // where the pointer was at the last move of a pan
        MenuFormState m_State;
        std::optional<float> m_ScrollTo;
        std::string m_ScrollToCard; // the id of the card the page scrolls to the top of when next drawn
        bool m_Closed = false; // its close button was clicked
        bool m_Reload = false;
    };
}
