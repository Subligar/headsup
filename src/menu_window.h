#pragma once

#include "menu_document.h"
#include "menu_nav.h"
#include "native_locate.h"

#include <RmlUi/Core.h>

#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace headsup
{
    // A tab of a game window: its name and what it shows (GameButtonRml, GameListRml and the like).
    struct GameTab
    {
        std::string name, rml;
    };

    // One of HeadsUp's game windows: its title, its tabs from left to right (a window of one shows no tabs), the markup
    // under them, and the settings window's page and card its cog opens. Its ids are its own: no two windows share one.
    // Its style is RCSS after the window's own, to size its parts by id. A window that owns Cancel is told of it as a
    // press of kGameCancelId, and Cancel closes nothing.
    struct GameWindowSpec
    {
        std::string id, title;
        std::vector<GameTab> tabs;
        std::string footer;
        std::string settingsPage, settingsCard;
        std::string style{};
        bool ownsCancel = false;
    };

    // A list's entry: its first cell fills the entry, the rest stand at its right. A dim one is muted; a picked one is
    // lit as the selection is, wherever the selection is. Its picture, an image's source, stands before the first cell
    // and is loaded as its row comes near the view, then kept (kGamePicturesKept); none when empty. Its details are
    // markup, as GameDetailsAttribute's.
    struct GameEntry
    {
        std::vector<std::string> cells;
        bool dim = false, picked = false;
        std::string picture{};
        std::string details{};
    };

    // A window's content. A list's entries are "<id>-<n>" from 0; a row's parts stand side by side, a column's one
    // under another. A picked button shows as in effect. A text box shows its hint dimmed while it is empty; selected,
    // it has the keyboard, and Enter in it is a press of its id. One that takes the arrows keeps the selection on them:
    // each is a press of ArrowPress (the keyboard's Left and Right still move its caret).
    std::string GameButtonRml(const std::string& id, const std::string& label, bool picked = false);
    std::string GameListRml(const std::string& id, const std::vector<GameEntry>& entries);
    std::string GameEntriesRml(const std::string& listId, const std::vector<GameEntry>& entries); // a list's inside, for SetRml
    std::string GameTextBoxRml(const std::string& id, const std::string& hint, bool takesArrows = false);
    std::string ArrowPress(const std::string& id, MenuAction arrow);
    std::string GameRowRml(const std::string& rml);
    std::string GameColumnRml(const std::string& rml);
    std::string GameTextRml(const std::string& text, const std::string& id = {});
    // An attribute for any element of a window's markup, leading space and all: its details, markup shown in the
    // settings window's tip while the pointer is over it, and beside its window, level with it, while the keys or the
    // pad have it selected.
    std::string GameDetailsAttribute(const std::string& details);

    // Every game window's own: its cog, and its tabs by number ("tab-0"). Its close button is the settings window's,
    // kMenuCloseId.
    constexpr const char* kGameCogId    = "cog";
    constexpr const char* kGameCancelId = "cancel"; // pressed by Cancel on a window that owns it
    constexpr const char* kGameTabId    = "tab-";
    constexpr const char* kGameNavClass = "selectable"; // what the selection can be on
    constexpr const char* kGameSelClass = "selected";   // where it is
    constexpr const char* kGameGroundClass = "ground";  // inside each: what lights it, drawn larger behind its text
    constexpr const char* kGamePictureClass = "pic";    // a list entry's picture
    // Entries' pictures kept loaded across the windows, the least recently shown let go past it: 2,048 of 32 by 32 at
    // 4 bytes a pixel is 8 MB.
    constexpr size_t kGamePicturesKept = 2048;

    // HeadsUp's game windows in the overlay, in the settings window's look, the newest over the rest. The keys and the
    // pad act on the newest; the mouse on any but one that has a sub-window open. Each has one element selected, which
    // the mouse's pointer selects too.
    class GameWindows : private Rml::EventListener
    {
    public:
        ~GameWindows() override { Close(); }
        // Each frame, before anything opens or acts: the context they are in, the look they take, and where the player
        // left each by its id, which it opens at.
        void Update(Rml::Context* context, const MenuLook& look, const std::vector<WindowPlace>& places);
        // Opens a window on its left-most tab, where the player left it or in the screen's middle; with a parent, as its
        // sub-window, which closes with it and keeps the mouse from it. False before the first Update, for an id open
        // already or a parent that is not.
        bool Open(const GameWindowSpec& spec, const std::string& parent = {});
        // A window and its sub-windows.
        void Close(const std::string& id);
        // Every window; before the context goes.
        void Close();
        // Does what the player asked of the newest window: Cancel closes it. True while any was open to take it.
        bool Act(MenuAction action);
        bool AnyOpen() const { return !m_Open.empty(); }
        bool IsOpen(const std::string& id) const;
        // Puts text, or markup, in one of a window's elements in place of what it held. A selection inside it is dropped,
        // and the next arrow selects the shown tab's first part.
        void SetText(const std::string& window, const std::string& id, const std::string& text);
        void SetRml(const std::string& window, const std::string& id, const std::string& rml);
        // One cell of a list's entry, from 0, in place: the list keeps its selection and scroll.
        void SetCell(const std::string& window, const std::string& entry, size_t cell, const std::string& text);
        // A text box's text; empty for anything else.
        std::string TextOf(const std::string& window, const std::string& id) const;
        void SetValue(const std::string& window, const std::string& id, const std::string& text);
        // Shows a tab, or selects an element, as the keys would.
        void ShowTab(const std::string& window, size_t tab);
        void Select(const std::string& window, const std::string& id);
        // Once each: the elements pressed by a click or Confirm, by id, but for the tabs, close buttons and cogs; and
        // the settings page and card a cog asked for.
        std::vector<std::string> TakePressed() { return std::exchange(m_Pressed, {}); }
        std::optional<std::pair<std::string, std::string>> TakeSettingsAsked();
        // Once each: the windows dragged by their titles and let go, by id, at their top-left corners in pixels.
        std::vector<WindowPlace> TakeMoved() { return std::exchange(m_Moved, {}); }
        // The newest window's selected element, by id; empty with none open.
        std::string Selected() const;
        bool SelectedByKeys() const; // or by the pad, not the pointer
        bool Typing() const; // a text box of theirs has the keyboard
        size_t TabShown(const std::string& id) const;
        Rml::ElementDocument* DocumentOf(const std::string& id) const;

    private:
        struct Window
        {
            std::string id, parent, settingsPage, settingsCard, style;
            Rml::ElementDocument* document = nullptr;
            size_t tabs = 0, tab = 0;
            Rml::Element* selected = nullptr;
            bool byKeys            = false; // the selection was made by the keys or the pad, not the pointer
            bool ownsCancel        = false;
            bool picturesDue       = true; // what its lists show may have changed, or rows near the view wait for pictures
        };
        void ProcessEvent(Rml::Event& event) override;
        const Window* Find(const std::string& id) const;
        Window* Find(const std::string& id) { return const_cast<Window*>(std::as_const(*this).Find(id)); }
        void Restyle(Window& window) const;
        // The pictures of its lists' rows in view, then of rows nearer them, as many as are ahead.
        void ShowPictures(Window& window, int& ahead);
        void Draw(Rml::Element* picture);
        // A picture's texture, and wherever it is drawn.
        void LetGo(const std::string& source);
        void LetGoOfPictures();
        // The details of what the pointer is over, else of the newest window's selection while the keys chose it.
        void ShowTip();
        void HideTip();
        void Select(Window& window, Rml::Element* element, bool byKeys);
        void Activate(Window& window, Rml::Element* element);
        void ShowTab(Window& window, size_t tab, bool selectContent);
        // The shown tab's first part, else its name, else the close button.
        void SelectFirst(Window& window);
        void Move(Window& window, MenuAction move);
        // Over the settings window, as one so a sub-window stays over its parent.
        void PullToFront() const;

        std::vector<Window> m_Open; // oldest first
        Rml::Context* m_Context = nullptr;
        std::optional<MenuLook> m_Look;
        std::vector<std::string> m_Pressed;
        std::vector<WindowPlace> m_Places, m_Moved;
        std::optional<std::pair<std::string, std::string>> m_SettingsAsked;
        std::vector<std::string> m_Pictures; // the pictures' sources kept loaded, the least recently drawn first
        Rml::ElementDocument* m_Tip = nullptr;
        std::string m_TipShown;
        std::optional<Rml::Vector2f> m_TipAt;
        bool m_TipUp = false;
        Rml::Vector2f m_Pointer; // where it last moved over a window
    };
}
