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
    struct GameWindowSpec
    {
        std::string id, title;
        std::vector<GameTab> tabs;
        std::string footer;
        std::string settingsPage, settingsCard;
    };

    // A window's content. A list's entries are "<id>-<n>" from 0; a row's parts stand side by side, a column's one
    // under another.
    std::string GameButtonRml(const std::string& id, const std::string& label);
    std::string GameListRml(const std::string& id, const std::vector<std::string>& entries);
    std::string GameRowRml(const std::string& rml);
    std::string GameColumnRml(const std::string& rml);
    std::string GameTextRml(const std::string& text, const std::string& id = {});

    // Every game window's own: its cog, and its tabs by number ("tab-0"). Its close button is the settings window's,
    // kMenuCloseId.
    constexpr const char* kGameCogId    = "cog";
    constexpr const char* kGameTabId    = "tab-";
    constexpr const char* kGameNavClass = "selectable"; // what the selection can be on
    constexpr const char* kGameSelClass = "selected";   // where it is
    constexpr const char* kGameGroundClass = "ground";  // inside each: what lights it, drawn larger behind its text
    // Under the settings window, which a window's cog opens over it.
    constexpr int kGameWindowZ = kMenuWindowZ - 1;

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
        // Puts text in one of a window's elements.
        void SetText(const std::string& window, const std::string& id, const std::string& text);
        // Once each: the elements pressed by a click or Confirm, by id, but for the tabs, close buttons and cogs; and
        // the settings page and card a cog asked for.
        std::vector<std::string> TakePressed() { return std::exchange(m_Pressed, {}); }
        std::optional<std::pair<std::string, std::string>> TakeSettingsAsked();
        // Once each: the windows dragged by their titles and let go, by id, at their top-left corners in pixels.
        std::vector<WindowPlace> TakeMoved() { return std::exchange(m_Moved, {}); }
        // The newest window's selected element, by id; empty with none open.
        std::string Selected() const;
        size_t TabShown(const std::string& id) const;
        Rml::ElementDocument* DocumentOf(const std::string& id) const;

    private:
        struct Window
        {
            std::string id, parent, settingsPage, settingsCard;
            Rml::ElementDocument* document = nullptr;
            size_t tabs = 0, tab = 0;
            Rml::Element* selected = nullptr;
            bool byKeys            = false; // the selection was made by the keys or the pad, not the pointer
        };
        void ProcessEvent(Rml::Event& event) override;
        const Window* Find(const std::string& id) const;
        void Restyle(const Window& window) const;
        void Select(Window& window, Rml::Element* element, bool byKeys);
        void Activate(Window& window, Rml::Element* element);
        void ShowTab(Window& window, size_t tab, bool selectContent);
        void Move(Window& window, MenuAction move);

        std::vector<Window> m_Open; // oldest first
        Rml::Context* m_Context = nullptr;
        std::optional<MenuLook> m_Look;
        std::vector<std::string> m_Pressed;
        std::vector<WindowPlace> m_Places, m_Moved;
        std::optional<std::pair<std::string, std::string>> m_SettingsAsked;
    };
}
