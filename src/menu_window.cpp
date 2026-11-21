#include "menu_window.h"

#include "bar_document.h"
#include "form.h"
#include "markup.h"
#include "menu_sizes.h"

#include <RmlUi/Core/Factory.h>
#include <RmlUi/Core/StyleSheetContainer.h>

#include <algorithm>
#include <array>
#include <cmath>

namespace headsup
{
    namespace
    {
        constexpr const char* kSource       = "game window";
        constexpr const char* kPaneId       = "pane-";
        constexpr const char* kHeadId       = "game-head";
        constexpr const char* kFootId       = "game-foot";
        constexpr const char* kCogPicture   = "cog-picture";
        constexpr const char* kTabAttribute = "tab";
        constexpr const char* kTabClass     = "tab";
        constexpr const char* kListClass    = "list";
        constexpr int kRightButton          = 1; // RmlUi's
        // In ems of the window's text.
        constexpr float kWidth       = 34.0f;
        constexpr float kHeadHeight  = 2.4f; // the title's row, with the cog and the close button in its corner
        constexpr float kListWidth   = 14.0f;
        constexpr int kListRows      = 6; // entries a list shows before it scrolls
        constexpr float kEntryHeight = 1.8f;
        constexpr float kEntryGap    = 0.25f; // around a list's entries
        constexpr float kEntryPad    = 0.6f;  // an entry's text from its sides
        constexpr float kCogShare    = 0.7f;  // the cog's picture, of its button
        constexpr float kEdge        = 1.0f;  // pixels: every selectable element's edge, which the selection lights

        std::string CogFor(const MenuLook& look)
        {
            return CogSource(look.colors.muted, static_cast<int>(std::lround(kCogShare * kFrame * static_cast<float>(look.fontPixels))));
        }

        std::string GameWindowStyle(const MenuLook& look)
        {
            const MenuColors& c    = look.colors;
            const auto pixels      = static_cast<float>(look.fontPixels);
            const std::string edge = Px(kEdge) + " transparent;";
            const std::string close = std::string("#") + kMenuCloseId, cog = std::string("#") + kGameCogId;
            std::string css = MenuStyle(look);
            css += "body{width:" + Em(kWidth) + ";height:auto;min-width:0px;min-height:0px;padding:" + Em(kGutter) + ";font-size:" +
                   Px(pixels) + ";z-index:" + std::to_string(kGameWindowZ) + ";}";
            css += std::string("#") + kHeadId + "{height:" + Em(kHeadHeight) + ";}";
            css += close + "," + cog + "{box-sizing:border-box;border:" + edge + "}";
            css += cog + "{position:absolute;right:" + Em(kGutter + kFrame + kControlGap) + ";top:" + Em(kGutter) + ";width:" +
                   Em(kFrame) + ";height:" + Em(kFrame) + ";border-radius:" + Em(kFrame / 2.0f) + ";background-color:" +
                   RcssColor(c.control) + ";}";
            css += cog + ":hover{background-color:" + RcssColor(c.hover) + ";}";
            const float picture = kCogShare * kFrame;
            css += cog + " img{position:absolute;left:50%;top:50%;width:" + Em(picture) + ";height:" + Em(picture) + ";margin-left:" +
                   Em(-picture / 2.0f) + ";margin-top:" + Em(-picture / 2.0f) + ";}";
            css += ".tab.nav{box-sizing:border-box;border:" + edge + "}";
            css += ".row{display:flex;flex-direction:row;align-items:flex-start;}.row > *{margin-right:" + Em(kControlGap) + ";}";
            css += ".column{display:flex;flex-direction:column;}.column > *{margin-bottom:" + Em(kControlGap) + ";}";
            // As tall as its rows and the gap above each, the last entry's own gap under it.
            const float listTall = (kEntryGap + static_cast<float>(kListRows) * (kEntryHeight + kEntryGap)) * pixels + 2.0f * kEdge;
            css += std::string(".") + kListClass + "{box-sizing:border-box;width:" + Em(kListWidth) + ";height:" + Px(listTall) +
                   ";overflow-y:auto;padding:" + Em(kEntryGap) + " " + Em(kEntryGap) + " 0px " + Em(kEntryGap) + ";background-color:" +
                   RcssColor(c.inset) + ";border:" + Px(kEdge) + " " + RcssColor(c.border) + ";border-radius:" + Em(kRadius) + ";}";
            css += ".entry{box-sizing:border-box;height:" + Em(kEntryHeight) + ";margin-bottom:" + Em(kEntryGap) + ";padding:0px " +
                   Em(kEntryPad) + ";line-height:" + Px(kEntryHeight * pixels - 2.0f * kEdge) + ";white-space:nowrap;border:" + edge +
                   "border-radius:" + Em(kRadius) + ";background-color:" + RcssColor(c.control) + ";}";
            css += ".entry:hover{background-color:" + RcssColor(c.hover) + ";}";
            css += ".text{color:" + RcssColor(c.label) + ";}";
            css += std::string("#") + kFootId + "{margin-top:" + Em(kGutter) + ";color:" + RcssColor(c.muted) + ";}";
            // Last, over every control's own edge.
            css += std::string(".") + kGameNavClass + "." + kGameSelClass + "{border-color:" + RcssColor(c.text) + ";}";
            css += std::string(".entry.") + kGameSelClass + "{background-color:" + RcssColor(c.accentSoft) + ";}";
            return css;
        }

        std::string WindowRml(const GameWindowSpec& spec, const MenuLook& look)
        {
            const std::string nav = std::string(" class=\"") + kGameNavClass + "\"";
            std::string rml = "<rml><head><style>" + GameWindowStyle(look) + "</style></head><body><div id=\"" + kMenuArtId + "\">" +
                              kArtRml + "</div><div id=\"" + kHeadId + "\"><div id=\"title\">" + EscapedRml(spec.title) + "</div></div>";
            rml += std::string("<div id=\"") + kGameCogId + "\"" + nav + "><img id=\"" + kCogPicture + "\" src=\"" + CogFor(look) + "\"/></div>";
            rml += std::string("<div id=\"") + kMenuCloseId + "\"" + nav + "><div class=\"cross\"></div><div class=\"cross back\"></div></div>";
            if (spec.tabs.size() > 1)
            {
                rml += "<div class=\"tab-row\">";
                for (size_t i = 0; i < spec.tabs.size(); ++i)
                    rml += std::string("<div class=\"") + kTabClass + " " + kGameNavClass + "\" id=\"" + kGameTabId + std::to_string(i) +
                           "\" " + kTabAttribute + "=\"" + std::to_string(i) + "\">" + EscapedRml(spec.tabs[i].name) + "</div>";
                rml += "</div><div class=\"tab-rule\"></div>";
            }
            for (size_t i = 0; i < spec.tabs.size(); ++i)
                rml += "<div id=\"" + std::string(kPaneId) + std::to_string(i) + "\">" + spec.tabs[i].rml + "</div>";
            if (!spec.footer.empty()) rml += std::string("<div id=\"") + kFootId + "\">" + spec.footer + "</div>";
            return rml + "</body></rml>";
        }

        // A move of the pointer, not a mouseover: one comes each update an element scrolls or opens under a pointer at rest.
        constexpr std::array kListened{Rml::EventId::Click, Rml::EventId::Mousemove, Rml::EventId::Mousedown};

        Rml::Element* ListOf(Rml::Element* element)
        {
            for (Rml::Element* up = element->GetParentNode(); up != nullptr; up = up->GetParentNode())
                if (up->IsClassSet(kListClass)) return up;
            return nullptr;
        }

        // Scrolls the list as little as shows the entry whole, with the gap that is around each entry: the first at the
        // top shows the list's top.
        void ScrollToShow(Rml::Element* list, Rml::Element* entry)
        {
            const float gap    = entry->GetBox().GetEdge(Rml::BoxArea::Margin, Rml::BoxEdge::Bottom);
            const float scroll = list->GetScrollTop();
            const float top    = entry->GetAbsoluteOffset(Rml::BoxArea::Border).y - list->GetAbsoluteOffset(Rml::BoxArea::Padding).y + scroll;
            const float bottom = top + entry->GetBox().GetSize(Rml::BoxArea::Border).y;
            if (top - gap < scroll)
                list->SetScrollTop(top - gap);
            else if (bottom + gap > scroll + list->GetClientHeight())
                list->SetScrollTop(bottom + gap - list->GetClientHeight());
        }

        std::vector<Rml::Element*> Selectable(Rml::Element* within)
        {
            Rml::ElementList all;
            within->QuerySelectorAll(all, std::string(".") + kGameNavClass);
            std::erase_if(all, [](Rml::Element* e) { return !e->IsVisible(true); });
            return all;
        }
    }

    std::string GameButtonRml(const std::string& id, const std::string& label)
    {
        return std::string("<div class=\"") + kRaisedPill + " " + kGameNavClass + "\" id=\"" + id + "\">" + EscapedRml(label) + "</div>";
    }

    std::string GameListRml(const std::string& id, const std::vector<std::string>& entries)
    {
        std::string rml = std::string("<div class=\"") + kListClass + "\" id=\"" + id + "\">";
        for (size_t i = 0; i < entries.size(); ++i)
            rml += std::string("<div class=\"entry ") + kGameNavClass + "\" id=\"" + id + "-" + std::to_string(i) + "\">" +
                   EscapedRml(entries[i]) + "</div>";
        return rml + "</div>";
    }

    std::string GameRowRml(const std::string& rml) { return "<div class=\"row\">" + rml + "</div>"; }
    std::string GameColumnRml(const std::string& rml) { return "<div class=\"column\">" + rml + "</div>"; }
    std::string GameTextRml(const std::string& text, const std::string& id)
    {
        return "<div class=\"text\"" + (id.empty() ? std::string() : " id=\"" + id + "\"") + ">" + EscapedRml(text) + "</div>";
    }

    void GameWindows::Update(Rml::Context* context, const MenuLook& look)
    {
        // Another context: the one the windows were in is gone, and they with it.
        if (context != m_Context) m_Open.clear();
        m_Context = context;
        if (m_Look == look) return;
        m_Look = look;
        for (const Window& window : m_Open)
            Restyle(window);
    }

    void GameWindows::Restyle(const Window& window) const
    {
        window.document->SetStyleSheetContainer(Rml::Factory::InstanceStyleSheetString(GameWindowStyle(*m_Look)));
        if (Rml::Element* picture = window.document->GetElementById(kCogPicture)) picture->SetAttribute("src", CogFor(*m_Look));
    }

    bool GameWindows::Open(const GameWindowSpec& spec, const std::string& parent)
    {
        if (m_Context == nullptr || !m_Look || IsOpen(spec.id) || (!parent.empty() && !IsOpen(parent))) return false;
        Rml::ElementDocument* document = m_Context->LoadDocumentFromMemory(WindowRml(spec, *m_Look), kSource);
        if (document == nullptr) return false;
        for (const Rml::EventId id : kListened)
            document->AddEventListener(id, this);
        document->Show(Rml::ModalFlag::None, Rml::FocusFlag::None);
        document->PullToFront();
        document->UpdateDocument();
        const Rml::Vector2f size   = document->GetBox().GetSize(Rml::BoxArea::Border);
        const Rml::Vector2i screen = m_Context->GetDimensions();
        document->SetProperty(Rml::PropertyId::Left, Rml::Property(std::round((static_cast<float>(screen.x) - size.x) / 2.0f), Rml::Unit::PX));
        document->SetProperty(Rml::PropertyId::Top, Rml::Property(std::round((static_cast<float>(screen.y) - size.y) / 2.0f), Rml::Unit::PX));
        m_Open.push_back({spec.id, parent, spec.settingsPage, spec.settingsCard, document, spec.tabs.size()});
        ShowTab(m_Open.back(), 0, true);
        return true;
    }

    void GameWindows::Close(const std::string& id)
    {
        // A sub-window opens after its parent, so one pass finds every window under this one.
        std::vector<std::string> closing{id};
        for (auto at = m_Open.begin(); at != m_Open.end();)
        {
            if (at->id != id && std::find(closing.begin(), closing.end(), at->parent) == closing.end())
            {
                ++at;
                continue;
            }
            closing.push_back(at->id);
            for (const Rml::EventId event : kListened)
                at->document->RemoveEventListener(event, this);
            at->document->Close();
            at = m_Open.erase(at);
        }
    }

    void GameWindows::Close()
    {
        while (!m_Open.empty())
            Close(std::string(m_Open.front().id));
    }

    bool GameWindows::Act(MenuAction action)
    {
        if (m_Open.empty()) return false;
        Window& window = m_Open.back();
        switch (action)
        {
        case MenuAction::Up:
        case MenuAction::Down:
        case MenuAction::Left:
        case MenuAction::Right: Move(window, action); break;
        case MenuAction::Confirm:
            if (window.selected != nullptr) Activate(window, window.selected);
            break;
        case MenuAction::Cancel: Close(std::string(window.id)); break;
        case MenuAction::TabLeft:
            if (window.tab > 0) ShowTab(window, window.tab - 1, true);
            break;
        case MenuAction::TabRight:
            if (window.tab + 1 < window.tabs) ShowTab(window, window.tab + 1, true);
            break;
        case MenuAction::None: break;
        }
        return true;
    }

    bool GameWindows::IsOpen(const std::string& id) const { return Find(id) != nullptr; }

    void GameWindows::SetText(const std::string& window, const std::string& id, const std::string& text)
    {
        const Window* open = Find(window);
        if (Rml::Element* element = open != nullptr ? open->document->GetElementById(id) : nullptr) element->SetInnerRML(EscapedRml(text));
    }

    std::string GameWindows::Selected() const
    {
        return m_Open.empty() || m_Open.back().selected == nullptr ? std::string() : m_Open.back().selected->GetId();
    }

    size_t GameWindows::TabShown(const std::string& id) const
    {
        const Window* window = Find(id);
        return window != nullptr ? window->tab : 0;
    }

    Rml::ElementDocument* GameWindows::DocumentOf(const std::string& id) const
    {
        const Window* window = Find(id);
        return window != nullptr ? window->document : nullptr;
    }

    const GameWindows::Window* GameWindows::Find(const std::string& id) const
    {
        const auto at = std::find_if(m_Open.begin(), m_Open.end(), [&](const Window& w) { return w.id == id; });
        return at != m_Open.end() ? &*at : nullptr;
    }

    void GameWindows::Select(Window& window, Rml::Element* element, bool scroll)
    {
        if (window.selected != element)
        {
            if (window.selected != nullptr) window.selected->SetClass(kGameSelClass, false);
            window.selected = element;
            element->SetClass(kGameSelClass, true);
        }
        if (Rml::Element* list = scroll ? ListOf(element) : nullptr) ScrollToShow(list, element);
    }

    void GameWindows::Activate(Window& window, Rml::Element* element)
    {
        const std::string id = element->GetId();
        if (element->IsClassSet(kTabClass))
            ShowTab(window, static_cast<size_t>(std::max(0, element->GetAttribute<int>(kTabAttribute, 0))), false);
        else if (id == kMenuCloseId)
            Close(std::string(window.id));
        else if (id == kGameCogId)
            m_SettingsAsked = std::pair(window.settingsPage, window.settingsCard);
        else
            m_Pressed.push_back(id);
    }

    void GameWindows::ShowTab(Window& window, size_t tab, bool selectContent)
    {
        if (tab >= window.tabs) return;
        window.tab = tab;
        for (size_t i = 0; i < window.tabs; ++i)
        {
            if (Rml::Element* name = window.document->GetElementById(kGameTabId + std::to_string(i))) name->SetClass(kPickedClass, i == tab);
            if (Rml::Element* pane = window.document->GetElementById(kPaneId + std::to_string(i))) pane->SetClass(kHiddenClass, i != tab);
        }
        if (!selectContent) return;
        window.document->UpdateDocument();
        const std::vector<Rml::Element*> inside = Selectable(window.document->GetElementById(kPaneId + std::to_string(tab)));
        Rml::Element* first = !inside.empty() ? inside.front() : window.document->GetElementById(kGameTabId + std::to_string(tab));
        Select(window, first != nullptr ? first : window.document->GetElementById(kMenuCloseId), true);
    }

    void GameWindows::Move(Window& window, MenuAction move)
    {
        window.document->UpdateDocument();
        const std::vector<Rml::Element*> elements = Selectable(window.document);
        std::vector<Rml::Element*> lists;
        std::vector<NavBox> boxes;
        size_t current = elements.size();
        for (size_t i = 0; i < elements.size(); ++i)
        {
            Rml::Element* element    = elements[i];
            const Rml::Vector2f at   = element->GetAbsoluteOffset(Rml::BoxArea::Border);
            const Rml::Vector2f size = element->GetBox().GetSize(Rml::BoxArea::Border);
            NavBox box{at.x, at.y, at.x + size.x, at.y + size.y};
            if (Rml::Element* list = ListOf(element))
            {
                const auto known = std::find(lists.begin(), lists.end(), list);
                box.list         = static_cast<int>(known - lists.begin());
                if (known == lists.end()) lists.push_back(list);
                const float top = list->GetAbsoluteOffset(Rml::BoxArea::Padding).y;
                box.shown       = box.bottom > top && box.top < top + list->GetBox().GetSize(Rml::BoxArea::Padding).y;
            }
            boxes.push_back(box);
            if (element == window.selected) current = i;
        }
        if (current == elements.size())
        {
            if (!elements.empty()) Select(window, elements.front(), true);
            return;
        }
        if (const std::optional<size_t> next = NextFocus(boxes, current, move)) Select(window, elements[*next], true);
    }

    void GameWindows::ProcessEvent(Rml::Event& event)
    {
        Rml::Element* target                 = event.GetTargetElement();
        const Rml::ElementDocument* document = target != nullptr ? target->GetOwnerDocument() : nullptr;
        const auto window = std::find_if(m_Open.begin(), m_Open.end(), [&](const Window& w) { return w.document == document; });
        if (window == m_Open.end()) return;
        // The game's own cancel, over any of them.
        if (event.GetId() == Rml::EventId::Mousedown)
        {
            if (event.GetParameter<int>("button", 0) == kRightButton) Act(MenuAction::Cancel);
            return;
        }
        if (std::any_of(m_Open.begin(), m_Open.end(), [&](const Window& w) { return w.parent == window->id; })) return;
        Rml::Element* nav = target;
        while (nav != nullptr && nav != document && !nav->IsClassSet(kGameNavClass))
            nav = nav->GetParentNode();
        if (nav == nullptr || nav == document) return;
        Select(*window, nav, false);
        if (event.GetId() == Rml::EventId::Click) Activate(*window, nav);
    }
}
