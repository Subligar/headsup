#include "menu_window.h"

#include "argb.h"
#include "bar_document.h"
#include "form.h"
#include "markup.h"
#include "menu_sizes.h"
#include "rml_pure.h"

#include <RmlUi/Core/Elements/ElementFormControlInput.h>
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
        constexpr const char* kBodyId       = "game-window"; // whose rules outweigh the settings window's own on an id
        constexpr const char* kPaneId       = "pane-";
        // Classes of their own: the settings window's row and column spread their parts apart.
        constexpr const char* kPaneClass    = "game-pane";
        constexpr const char* kRowClass     = "game-row";
        constexpr const char* kColumnClass  = "game-column";
        constexpr const char* kHeadId       = "game-head";
        constexpr const char* kFootId       = "game-foot";
        constexpr const char* kTitleId      = "title"; // the settings window's, whose look it takes
        constexpr const char* kCogPicture   = "cog-picture";
        constexpr const char* kTabAttribute = "tab";
        constexpr const char* kTabClass     = "tab";
        constexpr const char* kListClass    = "list";
        constexpr const char* kEntryClass   = "entry";
        constexpr const char* kTextClass    = "text";
        constexpr const char* kKeptClass    = "kept"; // a list's entry the keys chose, lit while the selection is elsewhere
        constexpr const char* kDimClass     = "dim";
        constexpr const char* kFillClass    = "fill"; // an entry's first cell
        constexpr const char* kCellClass    = "cell"; // and the rest
        constexpr const char* kArrowsClass  = "arrows"; // a text box that takes the arrows
        constexpr const char* kPictureAttribute = "picture"; // its source, drawn by a decorator once its row is near the view
        constexpr const char* kSourceAttribute  = "src";
        constexpr const char* kDetailsAttribute = "details";
        // In ems of the window's text.
        constexpr float kWidth       = 34.0f;
        constexpr float kHeadHeight  = 2.4f; // the title's row, with the cog and the close button in its corner
        constexpr float kTitleSize   = 1.3f; // smaller than the settings window's
        constexpr float kListWidth   = 14.0f;
        constexpr int kListRows      = 6; // entries a list shows before it scrolls
        constexpr float kEntryHeight = 1.8f;
        constexpr float kEntryGap    = 0.25f; // around a list's entries
        constexpr float kEntryPad    = 0.6f;  // an entry's text from its sides
        constexpr float kCogShare    = 0.7f;  // the cog's picture, of its button
        constexpr float kListEdge    = 1.0f;  // pixels
        constexpr float kPictureSide = 1.5f;  // a square a little under the entry's height
        // The rows in view, part-shown ones too, and one more get their pictures at once; each frame gives as many more,
        // nearest them first, so textures never load in a burst. Drawn by a decorator: setting an <img>'s source lays the
        // whole window out again.
        constexpr int kRowsPictured  = kListRows + 2;
        constexpr int kPicturesAhead = kRowsPictured;
        // The selection: its ground drawn this much larger, its text made bold by a copy of itself a pixel to the right,
        // as each font is loaded in one weight. Text drawn larger or edged by half a pixel is blurred.
        constexpr float kSelectedGrowth = 1.04f;
        constexpr int kBoldShift        = 1; // pixels
        // Either side of a list's entries, the room a grown one takes.
        constexpr float kEntryRoom = kListWidth * (kSelectedGrowth - 1.0f) / 2.0f;

        // The selection's ground: the soft accent laid over a control's own, which the soft accent alone would darken.
        uint32_t LitGround(const MenuColors& c)
        {
            const float share = static_cast<float>(Channel(c.accentSoft, kAlphaShift)) / static_cast<float>(kOpaqueAlpha);
            const auto mixed  = [&](int shift) {
                const auto under = static_cast<float>(Channel(c.control, shift)), over = static_cast<float>(Channel(c.accentSoft, shift));
                return static_cast<uint8_t>(std::lround(under + (over - under) * share));
            };
            return Opaque(mixed(kRedShift), mixed(kGreenShift), mixed(kBlueShift));
        }

        std::string GroundRml() { return std::string("<div class=\"") + kGameGroundClass + "\"></div>"; }

        // An entry's picture, just after its ground; none without one.
        Rml::Element* PictureIn(Rml::Element* entry)
        {
            Rml::Element* picture = entry->GetNumChildren() > 1 ? entry->GetChild(1) : nullptr;
            return picture != nullptr && picture->IsClassSet(kGamePictureClass) ? picture : nullptr;
        }
        bool Drawn(Rml::Element* picture) { return picture->GetLocalProperty(Rml::PropertyId::Decorator) != nullptr; }

        std::string CogFor(const MenuLook& look)
        {
            return CogSource(look.colors.muted, static_cast<int>(std::lround(kCogShare * kFrame * static_cast<float>(look.fontPixels))));
        }

        std::string GameWindowStyle(const MenuLook& look)
        {
            const MenuColors& c    = look.colors;
            const auto pixels      = static_cast<float>(look.fontPixels);
            const std::string close = std::string("#") + kMenuCloseId, cog = std::string("#") + kGameCogId;
            std::string css = MenuStyle(look);
            css += "body{width:" + Em(kWidth) + ";height:auto;min-width:0px;min-height:0px;padding:" + Em(kGutter) + ";font-size:" +
                   Px(pixels) + ";}";
            css += std::string("#") + kHeadId + "{height:" + Em(kHeadHeight) + ";}";
            css += std::string("#") + kTitleId + "{font-size:" + Em(kTitleSize) + ";}";
            css += cog + "{position:absolute;right:" + Em(kGutter + kFrame + kControlGap) + ";top:" + Em(kGutter) + ";width:" +
                   Em(kFrame) + ";height:" + Em(kFrame) + ";border-radius:" + Em(kFrame / 2.0f) + ";background-color:" +
                   RcssColor(c.control) + ";}";
            css += cog + ":hover{background-color:" + RcssColor(c.hover) + ";}";
            const float picture = kCogShare * kFrame;
            css += cog + " img{position:absolute;left:50%;top:50%;width:" + Em(picture) + ";height:" + Em(picture) + ";margin-left:" +
                   Em(-picture / 2.0f) + ";margin-top:" + Em(-picture / 2.0f) + ";}";
            // A tab's parts stand together in the middle.
            css += std::string(".") + kPaneClass + "{display:flex;flex-direction:column;align-items:center;}";
            css += std::string(".") + kPaneClass + "." + kHiddenClass + "{display:none;}";
            css += std::string(".") + kRowClass + "{display:flex;flex-direction:row;align-items:flex-start;gap:" + Em(kControlGap) + ";}";
            css += std::string(".") + kColumnClass + "{display:flex;flex-direction:column;gap:" + Em(kControlGap) + ";}";
            // As tall as its rows and the gap above each, the last entry's own gap under it.
            const float listTall = (kEntryGap + static_cast<float>(kListRows) * (kEntryHeight + kEntryGap)) * pixels + 2.0f * kListEdge;
            const std::string side = Em(kEntryGap + kEntryRoom);
            css += std::string(".") + kListClass + "{box-sizing:border-box;width:" + Em(kListWidth + 2.0f * kEntryRoom) + ";height:" +
                   Px(listTall) + ";overflow-y:auto;padding:" + Em(kEntryGap) + " " + side + " 0px " + side + ";background-color:" +
                   RcssColor(c.inset) + ";border:" + Px(kListEdge) + " " + RcssColor(c.border) + ";border-radius:" + Em(kRadius) + ";}";
            css += std::string(".") + kEntryClass + "{display:flex;box-sizing:border-box;height:" + Em(kEntryHeight) + ";margin-bottom:" + Em(kEntryGap) + ";padding:0px " +
                   Em(kEntryPad) + ";line-height:" + Em(kEntryHeight) + ";white-space:nowrap;border-radius:" + Em(kRadius) +
                   ";background-color:" + RcssColor(c.control) + ";}";
            css += std::string(".") + kEntryClass + ":hover{background-color:" + RcssColor(c.hover) + ";}";
            css += std::string(".") + kFillClass + "{flex:1 1 auto;overflow:hidden;text-overflow:ellipsis;}";
            css += std::string(".") + kCellClass + "{flex:0 0 auto;margin-left:" + Em(kControlGap) + ";}";
            css += std::string(".") + kGamePictureClass + "{flex:0 0 auto;align-self:center;width:" + Em(kPictureSide) + ";height:" + Em(kPictureSide) +
                   ";margin-right:" + Em(kControlGap) + ";}";
            css += std::string(".") + kTextClass + "{color:" + RcssColor(c.label) + ";}";
            css += std::string("#") + kFootId + "{margin-top:" + Em(kGutter) + ";color:" + RcssColor(c.muted) + ";}";
            // Each selectable element stands over its ground, which takes its shape.
            const std::string ground = std::string(" > .") + kGameGroundClass;
            css += std::string(".") + kGameNavClass + "{position:relative;z-index:0;}";
            css += std::string(".") + kGameGroundClass + "{position:absolute;left:0px;top:0px;width:100%;height:100%;z-index:-1;pointer-events:none;}";
            css += ".btn" + ground + "," + cog + ground + "," + close + ground + "{border-radius:" + Em(kFrame / 2.0f) + ";}";
            css += std::string(".") + kEntryClass + ground + "{border-radius:" + Em(kRadius) + ";}";
            css += std::string(".") + kTabClass + ground + "{border-radius:" + Em(kRadius) + " " + Em(kRadius) + " 0px 0px;}";
            // A text box is lit by its own look of having the keyboard, which it has while selected.
            const std::string lit = RcssColor(LitGround(c)), selected = std::string("#") + kBodyId + " div." + kGameNavClass + "." + kGameSelClass;
            const auto bold = [](uint32_t color) { return "font-effect:shadow(" + Px(static_cast<float>(kBoldShift)) + " 0px " + RcssColor(color) + ");"; };
            css += selected + "{background-color:transparent;border-color:transparent;color:" + RcssColor(c.text) + ";" + bold(c.text) + "}";
            css += std::string(".") + kEntryClass + "." + kDimClass + "{color:" + RcssColor(c.muted) + ";}";
            css += selected + "." + kDimClass + "{color:" + RcssColor(c.muted) + ";" + bold(c.muted) + "}";
            css += selected + ground + "{background-color:" + lit + ";transform:scale(" + RcssNumber(kSelectedGrowth) + ");}";
            css += std::string("#") + kBodyId + " ." + kKeptClass + ",#" + kBodyId + " ." + kEntryClass + "." + kPickedClass + "{background-color:" + lit + ";}";
            return css;
        }

        // The settings window's tip in a document of its own: what it shows is laid out apart from the windows' lists.
        std::string TipStyle(const MenuLook& look)
        {
            return MenuViewStyle(look) + "body{pointer-events:none;}#" + kMenuTipId + "{position:static;}";
        }
        std::string TipRml(const MenuLook& look)
        {
            return "<rml><head><style>" + TipStyle(look) + "</style></head><body><div id=\"" + kMenuTipId + "\"></div></body></rml>";
        }

        // Where a tip goes beside what it is about: right of it with a gap, else left; from down, else ending at up.
        struct TipSides
        {
            float left, right, down, up;
        };
        Rml::Vector2f TipPlace(const TipSides& about, Rml::Vector2f size, Rml::Vector2i screen)
        {
            const float gap = kMenuTipDrop * 0.5f;
            const auto width = static_cast<float>(screen.x), height = static_cast<float>(screen.y);
            const float x = about.right + gap + size.x <= width ? about.right + gap : about.left - gap - size.x;
            const float y = about.down + size.y <= height ? about.down : about.up - size.y;
            return {std::round(std::clamp(x, 0.0f, std::max(0.0f, width - size.x))), std::round(std::clamp(y, 0.0f, std::max(0.0f, height - size.y)))};
        }

        std::string WindowRml(const GameWindowSpec& spec, const MenuLook& look)
        {
            const std::string nav = std::string(" class=\"") + kGameNavClass + "\"";
            std::string rml = "<rml><head><style>" + GameWindowStyle(look) + spec.style + "</style></head><body id=\"" + kBodyId + "\"><div id=\"" +
                              kMenuArtId + "\">" + kArtRml + "</div><handle id=\"" + kHeadId + "\" " + MoveHandleAttributes() +
                              "><div id=\"" + kTitleId + "\">" + EscapedRml(spec.title) + "</div></handle>";
            rml += std::string("<div id=\"") + kGameCogId + "\"" + nav + ">" + GroundRml() + "<img id=\"" + kCogPicture + "\" src=\"" +
                   CogFor(look) + "\"/></div>";
            rml += std::string("<div id=\"") + kMenuCloseId + "\"" + nav + ">" + GroundRml() +
                   "<div class=\"cross\"></div><div class=\"cross back\"></div></div>";
            if (spec.tabs.size() > 1)
            {
                rml += "<div class=\"tab-row\">";
                for (size_t i = 0; i < spec.tabs.size(); ++i)
                    rml += std::string("<div class=\"") + kTabClass + " " + kGameNavClass + "\" id=\"" + kGameTabId + std::to_string(i) +
                           "\" " + kTabAttribute + "=\"" + std::to_string(i) + "\">" + GroundRml() + EscapedRml(spec.tabs[i].name) + "</div>";
                rml += "</div><div class=\"tab-rule\"></div>";
            }
            for (size_t i = 0; i < spec.tabs.size(); ++i)
                rml += std::string("<div class=\"") + kPaneClass + "\" id=\"" + kPaneId + std::to_string(i) + "\">" + spec.tabs[i].rml + "</div>";
            if (!spec.footer.empty()) rml += std::string("<div id=\"") + kFootId + "\">" + spec.footer + "</div>";
            return rml + "</body></rml>";
        }

        // A move of the pointer, not a mouseover: one comes each update an element scrolls or opens under a pointer at rest.
        constexpr std::array kListened{Rml::EventId::Click, Rml::EventId::Mousemove, Rml::EventId::Mousedown, Rml::EventId::Dragend, Rml::EventId::Scroll};
        // A text box's keys: it keeps them from the document's bubbling listeners.
        constexpr bool kInCapture = true;

        bool IsTextBox(const Rml::Element* element) { return element != nullptr && element->GetTagName() == "input"; }

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

    std::string GameButtonRml(const std::string& id, const std::string& label, bool picked)
    {
        return std::string("<div class=\"") + kRaisedPill + " " + kGameNavClass + (picked ? std::string(" ") + kPickedClass : std::string()) +
               "\" id=\"" + id + "\">" + GroundRml() + EscapedRml(label) + "</div>";
    }

    std::string GameListRml(const std::string& id, const std::vector<GameEntry>& entries)
    {
        return std::string("<div class=\"") + kListClass + "\" id=\"" + id + "\">" + GameEntriesRml(id, entries) + "</div>";
    }

    std::string GameEntriesRml(const std::string& listId, const std::vector<GameEntry>& entries)
    {
        std::string rml;
        for (size_t i = 0; i < entries.size(); ++i)
        {
            rml += std::string("<div class=\"") + kEntryClass + " " + kGameNavClass + (entries[i].dim ? std::string(" ") + kDimClass : std::string()) +
                   (entries[i].picked ? std::string(" ") + kPickedClass : std::string()) + "\" id=\"" + listId + "-" + std::to_string(i) + "\"" +
                   (entries[i].details.empty() ? std::string() : GameDetailsAttribute(entries[i].details)) + ">" + GroundRml();
            if (!entries[i].picture.empty())
                rml += std::string("<div class=\"") + kGamePictureClass + "\" " + kPictureAttribute + "=\"" + EscapedRml(entries[i].picture) + "\"></div>";
            for (size_t cell = 0; cell < entries[i].cells.size(); ++cell)
                rml += std::string("<div class=\"") + (cell == 0 ? kFillClass : kCellClass) + "\">" + EscapedRml(entries[i].cells[cell]) + "</div>";
            rml += "</div>";
        }
        return rml;
    }

    std::string GameTextBoxRml(const std::string& id, const std::string& hint, bool takesArrows)
    {
        return std::string("<input type=\"text\" class=\"") + kGameNavClass + (takesArrows ? std::string(" ") + kArrowsClass : std::string()) +
               "\" id=\"" + id + "\" placeholder=\"" + EscapedRml(hint) + "\"/>";
    }

    std::string GameDetailsAttribute(const std::string& details)
    {
        return std::string(" ") + kDetailsAttribute + "=\"" + EscapedRml(details) + "\"";
    }

    std::string ArrowPress(const std::string& id, MenuAction arrow) { return id + ":" + MenuActionName(arrow); }

    std::string GameRowRml(const std::string& rml) { return std::string("<div class=\"") + kRowClass + "\">" + rml + "</div>"; }
    std::string GameColumnRml(const std::string& rml) { return std::string("<div class=\"") + kColumnClass + "\">" + rml + "</div>"; }
    std::string GameTextRml(const std::string& text, const std::string& id)
    {
        return std::string("<div class=\"") + kTextClass + "\"" + (id.empty() ? std::string() : " id=\"" + id + "\"") + ">" + EscapedRml(text) + "</div>";
    }

    void GameWindows::Update(Rml::Context* context, const MenuLook& look, const std::vector<WindowPlace>& places)
    {
        // Another context: the one the windows were in is gone, and they with it.
        if (context != m_Context)
        {
            m_Open.clear();
            LetGoOfPictures();
            m_Tip = nullptr;
            m_TipShown.clear();
            m_TipUp = false;
        }
        m_Context = context;
        m_Places  = places;
        if (m_Look != look)
        {
            m_Look = look;
            for (Window& window : m_Open)
                Restyle(window);
            if (m_Tip != nullptr) m_Tip->SetStyleSheetContainer(Rml::Factory::InstanceStyleSheetString(TipStyle(look)));
        }
        int ahead = kPicturesAhead;
        for (Window& window : m_Open)
            ShowPictures(window, ahead);
        ShowTip();
    }

    void GameWindows::ShowTip()
    {
        // What the pointer is over first, then what the keys chose in the newest window.
        Rml::Element* over = m_Context->GetHoverElement();
        while (over != nullptr && !over->HasAttribute(kDetailsAttribute))
            over = over->GetParentNode();
        if (over != nullptr && std::none_of(m_Open.begin(), m_Open.end(), [&](const Window& w) { return w.document == over->GetOwnerDocument(); }))
            over = nullptr;
        Rml::Element* chosen = over == nullptr && !m_Open.empty() && m_Open.back().byKeys ? m_Open.back().selected : nullptr;
        Rml::Element* about  = over != nullptr ? over : chosen;
        const std::string details = about != nullptr ? about->GetAttribute<Rml::String>(kDetailsAttribute, "") : std::string();
        if (details.empty()) return HideTip();
        if (m_Tip == nullptr && (m_Tip = m_Context->LoadDocumentFromMemory(TipRml(*m_Look), kSource)) == nullptr) return;
        Rml::Element* tip = m_Tip->GetElementById(kMenuTipId);
        if (details != m_TipShown)
        {
            // Measured at the screen's corner, where it has all the room it may take.
            m_Tip->SetProperty(Rml::PropertyId::Left, Rml::Property(0.0f, Rml::Unit::PX));
            m_Tip->SetProperty(Rml::PropertyId::Top, Rml::Property(0.0f, Rml::Unit::PX));
            tip->SetInnerRML(details);
            m_Tip->UpdateDocument();
            m_TipShown = details;
            m_TipAt.reset();
        }
        TipSides sides{m_Pointer.x, m_Pointer.x, m_Pointer.y + kMenuTipDrop, m_Pointer.y - kMenuTipDrop * 0.5f};
        if (over == nullptr)
        {
            // Laid out where it stands now: a window placed this frame is laid out there only at the context's update.
            // Beside the window, level with what was chosen: beside the entry, it would cover the buttons next to its list.
            Rml::ElementDocument* window = chosen->GetOwnerDocument();
            window->UpdateDocument();
            const float y = chosen->GetAbsoluteOffset(Rml::BoxArea::Border).y, x = window->GetAbsoluteOffset(Rml::BoxArea::Border).x;
            sides = {x, x + window->GetBox().GetSize(Rml::BoxArea::Border).x, y, y + chosen->GetBox().GetSize(Rml::BoxArea::Border).y};
        }
        const Rml::Vector2f at = TipPlace(sides, tip->GetBox().GetSize(Rml::BoxArea::Border), m_Context->GetDimensions());
        if (at != m_TipAt)
        {
            m_Tip->SetProperty(Rml::PropertyId::Left, Rml::Property(at.x, Rml::Unit::PX));
            m_Tip->SetProperty(Rml::PropertyId::Top, Rml::Property(at.y, Rml::Unit::PX));
            m_TipAt = at;
        }
        if (!std::exchange(m_TipUp, true)) m_Tip->Show(Rml::ModalFlag::None, Rml::FocusFlag::None);
    }

    void GameWindows::HideTip()
    {
        if (m_Tip != nullptr && std::exchange(m_TipUp, false)) m_Tip->Hide();
    }

    void GameWindows::Restyle(Window& window) const
    {
        window.document->SetStyleSheetContainer(Rml::Factory::InstanceStyleSheetString(GameWindowStyle(*m_Look) + window.style));
        if (Rml::Element* picture = window.document->GetElementById(kCogPicture)) picture->SetAttribute(kSourceAttribute, CogFor(*m_Look));
        window.picturesDue = true;
    }

    void GameWindows::ShowPictures(Window& window, int& ahead)
    {
        if (!std::exchange(window.picturesDue, false)) return;
        // Every entry stands at the same pitch from the list's top.
        const float pitch = (kEntryHeight + kEntryGap) * static_cast<float>(m_Look->fontPixels);
        Rml::ElementList lists;
        window.document->GetElementsByClassName(lists, kListClass);
        const auto view = [&](Rml::Element* list) {
            const int first = std::min(static_cast<int>(list->GetScrollTop() / pitch), list->GetNumChildren());
            return std::pair(first, std::min(first + kRowsPictured, list->GetNumChildren()));
        };
        for (Rml::Element* list : lists)
            for (auto [row, end] = view(list); row < end; ++row)
                if (Rml::Element* picture = PictureIn(list->GetChild(row))) Draw(picture);
        for (Rml::Element* list : lists)
        {
            const auto [first, end] = view(list);
            const int rows = list->GetNumChildren();
            for (int away = 0; end + away < rows || first - away > 0; ++away)
                for (const int row : {end + away, first - 1 - away})
                {
                    Rml::Element* picture = row >= 0 && row < rows ? PictureIn(list->GetChild(row)) : nullptr;
                    if (picture == nullptr || Drawn(picture)) continue;
                    // Ahead of the view only while there is room to keep them, or they would push out what is shown.
                    if (m_Pictures.size() >= kGamePicturesKept) return;
                    if (ahead == 0)
                    {
                        window.picturesDue = true;
                        return;
                    }
                    --ahead;
                    Draw(picture);
                }
        }
    }

    void GameWindows::Draw(Rml::Element* picture)
    {
        const std::string source = picture->GetAttribute<Rml::String>(kPictureAttribute, "");
        if (!Drawn(picture)) picture->SetProperty("decorator", "image(\"" + source + "\")");
        std::erase(m_Pictures, source);
        m_Pictures.push_back(source);
        if (m_Pictures.size() > kGamePicturesKept) LetGo(std::string(m_Pictures.front()));
    }

    void GameWindows::LetGo(const std::string& source)
    {
        // RmlUi would load it again for anything still drawing it.
        for (Window& window : m_Open)
        {
            Rml::ElementList pictures;
            window.document->GetElementsByClassName(pictures, kGamePictureClass);
            for (Rml::Element* picture : pictures)
                if (Drawn(picture) && picture->GetAttribute<Rml::String>(kPictureAttribute, "") == source)
                {
                    picture->RemoveProperty(Rml::PropertyId::Decorator);
                    window.picturesDue = true;
                }
        }
        Rml::ReleaseTexture(source);
        std::erase(m_Pictures, source);
    }

    void GameWindows::LetGoOfPictures()
    {
        for (const std::string& source : m_Pictures)
            Rml::ReleaseTexture(source);
        m_Pictures.clear();
    }

    bool GameWindows::Open(const GameWindowSpec& spec, const std::string& parent)
    {
        if (m_Context == nullptr || !m_Look || IsOpen(spec.id) || (!parent.empty() && !IsOpen(parent))) return false;
        Rml::ElementDocument* document = m_Context->LoadDocumentFromMemory(WindowRml(spec, *m_Look), kSource);
        if (document == nullptr) return false;
        for (const Rml::EventId id : kListened)
            document->AddEventListener(id, this);
        document->AddEventListener(Rml::EventId::Keydown, this, kInCapture);
        document->Show(Rml::ModalFlag::None, Rml::FocusFlag::None);
        m_Open.push_back({spec.id, parent, spec.settingsPage, spec.settingsCard, spec.style, document, spec.tabs.size(), 0, nullptr, false, spec.ownsCancel});
        PullToFront();
        // Measured as it shows its first tab alone.
        ShowTab(m_Open.back(), 0, true);
        const Rml::Vector2f size   = document->GetBox().GetSize(Rml::BoxArea::Border);
        const Rml::Vector2i screen = m_Context->GetDimensions();
        Rml::Vector2f at{std::round((static_cast<float>(screen.x) - size.x) / 2.0f), std::round((static_cast<float>(screen.y) - size.y) / 2.0f)};
        const auto place = std::find_if(m_Places.begin(), m_Places.end(), [&](const WindowPlace& p) { return p.name == spec.id; });
        if (place != m_Places.end())
        {
            const auto [x, y] = KeptOnScreen(place->x, place->y, static_cast<int>(std::lround(size.x)), static_cast<int>(std::lround(size.y)),
                screen.x, screen.y);
            at = {static_cast<float>(x), static_cast<float>(y)};
        }
        document->SetProperty(Rml::PropertyId::Left, Rml::Property(at.x, Rml::Unit::PX));
        document->SetProperty(Rml::PropertyId::Top, Rml::Property(at.y, Rml::Unit::PX));
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
            at->document->RemoveEventListener(Rml::EventId::Keydown, this, kInCapture);
            at->document->Close();
            at = m_Open.erase(at);
        }
        HideTip();
    }

    void GameWindows::Close()
    {
        while (!m_Open.empty())
            Close(std::string(m_Open.front().id));
        LetGoOfPictures();
        if (m_Tip != nullptr) m_Tip->Close();
        m_Tip = nullptr;
        m_TipShown.clear();
        m_TipUp = false;
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
        case MenuAction::Cancel:
            if (window.ownsCancel)
                m_Pressed.push_back(kGameCancelId);
            else
                Close(std::string(window.id));
            break;
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

    // Out of line: GCC warns of the optional's string read uninitialized where this is inlined.
    std::optional<std::pair<std::string, std::string>> GameWindows::TakeSettingsAsked() { return std::exchange(m_SettingsAsked, std::nullopt); }

    bool GameWindows::IsOpen(const std::string& id) const { return Find(id) != nullptr; }

    void GameWindows::SetText(const std::string& window, const std::string& id, const std::string& text) { SetRml(window, id, EscapedRml(text)); }

    void GameWindows::SetRml(const std::string& window, const std::string& id, const std::string& rml)
    {
        Window* open          = Find(window);
        Rml::Element* element = open != nullptr ? open->document->GetElementById(id) : nullptr;
        if (element == nullptr) return;
        open->picturesDue = true;
        if (open->selected != nullptr && (open->selected == element || element->Contains(open->selected)))
        {
            if (IsTextBox(open->selected)) open->selected->Blur();
            open->selected = nullptr;
        }
        element->SetInnerRML(rml);
    }

    void GameWindows::SetCell(const std::string& window, const std::string& entry, size_t cell, const std::string& text)
    {
        const Window* open = Find(window);
        Rml::Element* element = open != nullptr ? open->document->GetElementById(entry) : nullptr;
        for (int i = 0; element != nullptr && i < element->GetNumChildren(); ++i)
            if (Rml::Element* child = element->GetChild(i); !child->IsClassSet(kGameGroundClass) && !child->IsClassSet(kGamePictureClass) && cell-- == 0)
                return child->SetInnerRML(EscapedRml(text));
    }

    std::string GameWindows::TextOf(const std::string& window, const std::string& id) const
    {
        const Window* open = Find(window);
        Rml::Element* element = open != nullptr ? open->document->GetElementById(id) : nullptr;
        const auto* box = IsTextBox(element) ? rmlui_dynamic_cast<Rml::ElementFormControlInput*>(element) : nullptr;
        return box != nullptr ? box->GetValue() : std::string();
    }

    void GameWindows::SetValue(const std::string& window, const std::string& id, const std::string& text)
    {
        const Window* open = Find(window);
        Rml::Element* element = open != nullptr ? open->document->GetElementById(id) : nullptr;
        if (auto* box = IsTextBox(element) ? rmlui_dynamic_cast<Rml::ElementFormControlInput*>(element) : nullptr) box->SetValue(text);
    }

    void GameWindows::ShowTab(const std::string& window, size_t tab)
    {
        if (Window* open = Find(window)) ShowTab(*open, tab, true);
    }

    void GameWindows::Select(const std::string& window, const std::string& id)
    {
        Window* open = Find(window);
        if (Rml::Element* element = open != nullptr ? open->document->GetElementById(id) : nullptr) Select(*open, element, true);
    }

    bool GameWindows::SelectedByKeys() const { return !m_Open.empty() && m_Open.back().byKeys; }

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

    void GameWindows::Select(Window& window, Rml::Element* element, bool byKeys)
    {
        Rml::Element* list = ListOf(element);
        if (window.selected != element)
        {
            if (Rml::Element* was = window.selected)
            {
                was->SetClass(kGameSelClass, false);
                if (IsTextBox(was)) was->Blur();
                if (window.byKeys && ListOf(was) != nullptr && ListOf(was) != list) was->SetClass(kKeptClass, true);
            }
            if (list != nullptr)
                for (int i = 0; i < list->GetNumChildren(); ++i)
                    list->GetChild(i)->SetClass(kKeptClass, false);
            window.selected = element;
            element->SetClass(kGameSelClass, true);
            if (IsTextBox(element)) element->Focus();
        }
        window.byKeys = byKeys;
        if (!byKeys) return;
        // The pointer lets go of what it is over, which would look selected too, until it moves.
        m_Context->ProcessMouseLeave();
        if (list == nullptr) return;
        window.document->UpdateDocument(); // an entry set since the last update has no place yet
        ScrollToShow(list, element);
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
        window.tab         = tab;
        window.picturesDue = true;
        for (size_t i = 0; i < window.tabs; ++i)
        {
            if (Rml::Element* name = window.document->GetElementById(kGameTabId + std::to_string(i))) name->SetClass(kPickedClass, i == tab);
            if (Rml::Element* pane = window.document->GetElementById(kPaneId + std::to_string(i))) pane->SetClass(kHiddenClass, i != tab);
        }
        if (selectContent) SelectFirst(window);
    }

    void GameWindows::SelectFirst(Window& window)
    {
        window.document->UpdateDocument();
        const std::vector<Rml::Element*> inside = Selectable(window.document->GetElementById(kPaneId + std::to_string(window.tab)));
        Rml::Element* first = !inside.empty() ? inside.front() : window.document->GetElementById(kGameTabId + std::to_string(window.tab));
        Select(window, first != nullptr ? first : window.document->GetElementById(kMenuCloseId), true);
    }

    void GameWindows::Move(Window& window, MenuAction move)
    {
        if (window.selected != nullptr && window.selected->IsClassSet(kArrowsClass))
        {
            m_Pressed.push_back(ArrowPress(window.selected->GetId(), move));
            return;
        }
        window.document->UpdateDocument();
        std::vector<Rml::Element*> elements = Selectable(window.document);
        // Left and right keep to the tab's own parts: past its edge is the title bar's cog, which they stand apart from.
        Rml::Element* pane = window.document->GetElementById(kPaneId + std::to_string(window.tab));
        if ((move == MenuAction::Left || move == MenuAction::Right) && pane != nullptr)
            if (std::vector<Rml::Element*> inside = Selectable(pane); std::find(inside.begin(), inside.end(), window.selected) != inside.end())
                elements = std::move(inside);
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
            SelectFirst(window);
            return;
        }
        const std::optional<size_t> next = NextFocus(boxes, current, move);
        if (!next) return;
        Rml::Element* to   = elements[*next];
        Rml::Element* list = ListOf(to);
        Rml::Element* kept = list != nullptr && list != ListOf(window.selected) ? list->QuerySelector(std::string(".") + kKeptClass) : nullptr;
        Select(window, kept != nullptr ? kept : to, true);
    }

    bool GameWindows::Typing() const
    {
        const Rml::Element* focus = m_Context != nullptr ? m_Context->GetFocusElement() : nullptr;
        return IsTextBox(focus) && std::any_of(m_Open.begin(), m_Open.end(), [&](const Window& w) { return w.document == focus->GetOwnerDocument(); });
    }

    void GameWindows::PullToFront() const
    {
        for (const Window& open : m_Open)
            open.document->PullToFront();
    }

    void GameWindows::ProcessEvent(Rml::Event& event)
    {
        Rml::Element* target                 = event.GetTargetElement();
        const Rml::ElementDocument* document = target != nullptr ? target->GetOwnerDocument() : nullptr;
        const auto window = std::find_if(m_Open.begin(), m_Open.end(), [&](const Window& w) { return w.document == document; });
        if (window == m_Open.end()) return;
        if (event.GetId() == Rml::EventId::Mousemove)
            m_Pointer = {event.GetParameter<float>("mouse_x", 0.0f), event.GetParameter<float>("mouse_y", 0.0f)};
        if (event.GetId() == Rml::EventId::Scroll)
        {
            window->picturesDue = true;
            return;
        }
        if (event.GetId() == Rml::EventId::Dragend)
        {
            // The handle has moved it, but it is laid out there only on the next update.
            window->document->UpdateDocument();
            const Rml::Vector2f at = window->document->GetAbsoluteOffset(Rml::BoxArea::Border);
            m_Moved.push_back({window->id, static_cast<int>(std::lround(at.x)), static_cast<int>(std::lround(at.y))});
            return;
        }
        // The game's own cancel, over any of them.
        if (event.GetId() == Rml::EventId::Mousedown)
        {
            PullToFront();
            if (event.GetParameter<int>("button", 0) == kRmlRightButton) Act(MenuAction::Cancel);
            return;
        }
        if (event.GetId() == Rml::EventId::Keydown)
        {
            if (!IsTextBox(target) || target != window->selected) return;
            switch (static_cast<Rml::Input::KeyIdentifier>(event.GetParameter<int>("key_identifier", 0)))
            {
            case Rml::Input::KI_UP: Move(*window, MenuAction::Up); break;
            case Rml::Input::KI_DOWN: Move(*window, MenuAction::Down); break;
            default: break;
            }
            return;
        }
        if (std::any_of(m_Open.begin(), m_Open.end(), [&](const Window& w) { return w.parent == window->id; })) return;
        Rml::Element* nav = target;
        while (nav != nullptr && nav != document && !nav->IsClassSet(kGameNavClass))
            nav = nav->GetParentNode();
        if (nav == nullptr || nav == document) return;
        Select(*window, nav, false);
        // A click into a text box gives it the keyboard; only Enter presses it.
        if (event.GetId() == Rml::EventId::Click && !IsTextBox(nav)) Activate(*window, nav);
    }
}
