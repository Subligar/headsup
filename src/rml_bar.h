#pragma once

#include "bar_document.h"
#include "bar_game_help.h"
#include "bar_layout.h"
#include "bar_look.h"
#include "bar_widgets.h"
#include "gpu.h"
#include "memo.h"
#include "rml_host.h"
#include "settings.h"

#include <array>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace headsup
{
    // The top bar as an RmlUi document: the widgets' content and the bar's layout are the bar's own (bar_widgets,
    // bar_layout), measured with RmlUi's fonts, and each widget is an element placed where the layout puts it.
    class RmlBar
    {
    public:
        // The room the settings window laid out the open popup's settings in last, while it shows them.
        std::optional<SettingsRoom> settingsRoom;

        // Each frame: lays out the bar, shows it in the host's bar context and tells input where everything is.
        // While the bar is off it closes its document. hidden: laid out, but not drawn. skin: the game's window art in
        // use, once it is read.
        void Update(RmlHost& host, const BarState& state, const Settings& settings, const GameSkinLook* skin,
            float screenWidth, float screenHeight, BarInput& input, bool hidden);
        // Closes the bar's document; before the host stops, and while another renderer draws the bar.
        void Close();
        void Release();
        // The most Game help's bar can cover while it is on, in back buffer pixels.
        std::optional<BarRect> HelpBarReach() const { return m_HelpReach; }
        // The top bar's bottom, its under line counted, while it draws; 0 while it does not.
        float BarBottomNow() const { return m_BarBottom; }
        // Where the open popup's settings go on the screen while it shows them, in back buffer pixels.
        std::optional<BarRect> SettingsBody() const { return m_SettingsBody; }

    private:
        // The bar's text sizes and places follow its height, its text size setting and its shape and fonts.
        struct LinesKey
        {
            float height   = 0.0f;
            BarShape shape = BarShape::Ornate;
            float fontRatio = kDefaultFontRatio;
            const char* titleFont  = nullptr; // interned by BarTitleFont and BarTextFont: one pointer per name
            const char* detailFont = nullptr;
            int helpText = kAutoBarTextSize;
            bool operator==(const LinesKey&) const = default;
        };
        struct Rows
        {
            TextRow lines;
            BarTextPlace place;
            float helpBarHeight = 0.0f; // fitted to its text, the same whatever it shows
        };
        // A widget's element and what it shows now.
        struct Shown
        {
            Rml::Element* element = nullptr;
            WidgetContent content; // as the widget gave it, which its natural width was measured from
            float natural = 0.0f; // what it shows, without its padding
            WidgetPads pads;      // the room from its edges to what it shows
            std::vector<float> columns; // a widget of cells', as measured
            bool measured = false;
            WidgetContent drawn; // as its markup was made from: the target's is cut to its room
            bool marked = false; // its markup is drawn's
            // The values RmlUi was last given, so each is given only when it changes.
            std::optional<float> left, width, marginLeft, marginRight, paddingLeft, paddingRight;
        };
        struct Divider
        {
            Rml::Element* element = nullptr;
            std::optional<float> left;
        };
        struct FitKey
        {
            std::string text;
            std::string_view font;
            int pixels = 0;
            long room  = 0; // in tenths of a pixel
            bool operator==(const FitKey&) const = default;
        };
        struct FaceDigit
        {
            std::string_view family;
            int pixels = 0;
            char digit = '0';
        };
        struct Widgets
        {
            std::array<WidgetContent, kPanelSlots> contents;
            std::array<bool, kPanelSlots> hasPopup{}; // a combined pill's first widget's when any of its widgets has one
            PanelGroups panel;                        // the panel as it is drawn, its widgets combined
        };
        // The help bar's text follows the game's, its fonts and their sizes, and the room its lines and its caption
        // have beside the bag's count.
        struct HelpBarKey
        {
            StripText strip;
            std::string_view captionFont, lineFont;
            int captionPixels = 0, linePixels = 0;
            long room = 0, gap = 0; // in tenths of a pixel
            bool operator==(const HelpBarKey&) const = default;
        };
        // A popup's layout follows its content, its pin, the bar's height and its text size, which pick its sizes,
        // and the room below the bar, which cuts it.
        struct HelpBarFit
        {
            HelpBarText text;
            float captionWidth = 0.0f, helpWidth = 0.0f;
        };
        struct PopupKey
        {
            PopupContent content;
            bool pinned     = false;
            float height    = 0.0f;
            float popupText = 0.0f;
            float room      = 0.0f;
            float least     = 0.0f;
            std::optional<SettingsRoom> settings;
            bool operator==(const PopupKey&) const = default;
        };
        // A font's line as GDI draws it, which the popups are laid out by (its height, and its ink's top and bottom
        // in it), and as RmlUi sets it: its line box's height, and how far under GDI's line top that box starts for
        // the two baselines to meet.
        struct FaceLine
        {
            std::string_view family;
            int pixels    = 0;
            float height  = 0.0f, inkTop = 0.0f, inkBottom = 0.0f;
            int rmlHeight = 0;
            float drop    = 0.0f;
        };

        const Rows& RowsFor(RmlHost& host, const BarMetrics& metrics, BarShape shape, float fontRatio, int helpText,
            const char* titleFont, const char* detailFont);
        Rows MakeRows(RmlHost& host, const BarMetrics& metrics, const char* titleFont, const char* detailFont,
            int helpText = kAutoBarTextSize);
        BarLinePlace PlaceLine(RmlHost& host, const char* family, int pixels, const char* glyphs, float top);
        // Builds the document again when what it is made from changed. False when RmlUi refused it.
        bool Document(RmlHost& host, const BarLook& look, const BarTextPlace& place);
        BarLayout PlaceWidgets(RmlHost& host, const BarLook& look, const BarState& state, float screenWidth,
            Widgets& widgets);
        void ShowWidgets(const BarLook& look, const BarLayout& layout, const Widgets& widgets, const BarInput& input,
            float screenWidth);
        // Game help's own bar under the top bar, from top and moved offsetX: where its box went (empty for none).
        // cut: its text was, so the bar has a popup with all of it.
        BarRect ShowHelpBar(RmlHost& host, const BarLook& look, const Rows& rows, const BarState& state, float top,
            float offsetX, float screenWidth, bool& cut);
        // Places each popup the input shows in the host's overlay, the last on top, and hides the others: where each
        // went, with its cards' tabs and the parts that scroll at their places on the screen.
        std::vector<PopupPlace> ShowPopups(RmlHost& host, const BarLook& look, const BarState& state,
            const BarLayout& layout, float screenWidth, float screenHeight, const BarInput& input, bool hidden);
        // One pill's popup, as ShowPopups places it; nothing when its pill is not on the bar. over: drawn over the
        // settings another popup shows.
        std::optional<PopupPlace> ShowPopup(RmlHost& host, const BarLook& look, const BarState& state,
            const BarLayout& layout, float screenWidth, float screenHeight, const BarInput& input, bool hidden,
            PanelWidget open, bool over);
        bool PopupDocumentFor(RmlHost& host, const BarLook& look);
        void ClosePopup();
        const FaceLine& FaceLineOf(RmlHost& host, const char* family, int pixels);

        float Width(RmlHost& host, const std::string& text, const TextFace& face) const;
        float SpansWidth(RmlHost& host, const std::vector<Span>& spans, const TextFace& face) const;
        // The room a widget's line takes: its width with every figure as wide as the font's widest, so a clock or a
        // counter ticking does not move the widgets beside it.
        float SteadyWidth(RmlHost& host, std::vector<Span> spans, const TextFace& face);
        char WidestDigitOf(RmlHost& host, const TextFace& face);
        const std::string& Fitted(RmlHost& host, Memo<FitKey, std::string>& memo, std::string text, const TextFace& face,
            float room);

        GdiText m_Gdi; // the bar's text rows are sized by GDI's ink, as the bar drawn with it is
        Memo<LinesKey, Rows> m_Rows;
        Rml::ElementDocument* m_Document = nullptr;
        std::string m_DocumentText, m_StripSource; // what the document was built from, and its strip image
        bool m_Visible = false;
        std::array<Shown, kPanelSlots> m_Shown;
        std::array<Divider, kDividerCount> m_Dividers;
        Rml::Element* m_RightCap = nullptr;
        std::optional<float> m_RightCapLeft;
        Rml::Element* m_Help = nullptr;
        // What the help bar's markup was made from, with Background.
        std::optional<std::pair<HelpBarParts, bool>> m_HelpDrawn;
        Memo<HelpBarKey, HelpBarFit> m_HelpBar;
        // The popups' documents: under the settings a popup shows, and over them for the popups shown after that one.
        std::array<Rml::ElementDocument*, 2> m_PopupDocuments{};
        std::string m_PopupDocumentText;
        // Each pill's popup, by its first widget's SlotOf.
        struct Popup
        {
            Rml::Element* element = nullptr;                   // the one of elements in use
            std::array<Rml::Element*, 2> elements{};           // in each of m_PopupDocuments, made as needed
            bool marked = false; // its markup is the layout's
            std::optional<float> left, top, width, height;
            Memo<PopupKey, PopupLayout> layout;
            // The widest it has been since it opened, which it keeps as its text changes with the time; its content's
            // or its settings' (settings), apart.
            bool shown = false, settings = false;
            float widest = 0.0f;
            std::optional<float> keptX; // where across it opened under its widget, which it keeps while open
            // Each popup font's widths of the texts it measured, its figures at the font's widest digit.
            std::array<std::unordered_map<std::string, float>, kPopupFontCount> widths;
        };
        std::array<Popup, kPanelSlots> m_Popups;
        PopupFaces m_PopupFaces;
        std::vector<FaceLine> m_FaceLines;
        Memo<FitKey, std::string> m_TitleFit, m_SubtitleFit;
        std::vector<FaceDigit> m_WidestDigits;
        std::optional<BarRect> m_HelpReach;
        float m_BarBottom = 0.0f;
        std::optional<BarRect> m_SettingsBody;
    };
}
