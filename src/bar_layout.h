#pragma once

#include "bar_placement.h"
#include "bar_theme.h"
#include "bar_types.h"
#include "function_ref.h"
#include "plate_image.h"

#include <array>
#include <map>
#include <set>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace headsup
{
    struct BarRect
    {
        float x = 0.0f, y = 0.0f, width = 0.0f, height = 0.0f;
        bool Contains(float px, float py) const { return px >= x && px < x + width && py >= y && py < y + height; }
        bool operator==(const BarRect&) const = default;
    };

    struct PlacedWidget
    {
        PanelWidget widget;
        BarRect rect;
        std::optional<PanelWidget> group{}; // the first widget of its pill, when it is not alone (PanelGroups)
    };

    struct BarLayout
    {
        float height = 0.0f;               // the strip's, without what fades below it
        std::vector<PlacedWidget> widgets; // in bar order
        std::vector<float> dividers;       // ornate: the center x of each diamond between neighbours in a group
        float targetWidth = 0.0f;          // the room the target got: at most its natural width, 0 when it did not fit
    };

    struct BarRow
    {
        float y = 0.0f, height = 0.0f;
        bool operator==(const BarRow&) const = default;
    };
    // The row the pills, the ornate target's frame and a hovered ornate widget's tint take: whole pixels, centered in
    // the strip above its bottom line.
    BarRow PillRow(const BarMetrics& metrics);
    // How far a frame or a hover tint keeps from the x of an ornate divider: half its diamond, drawn whole pixels wide,
    // and clear space.
    float DiamondClearance(const BarMetrics& metrics);

    // A widget's text: its title's and detail line's pixel sizes, and their line boxes' tops from the strip's top.
    struct TextRow
    {
        int title = 0, detail = 0;
        float titleY = 0.0f, detailY = 0.0f; // the title over the detail line
        int alone    = 0;                    // a title without one: its own size, as large as one line fits
        float aloneY = 0.0f;
        bool operator==(const TextRow&) const = default;
    };
    // Placed by their ink, which ink gives for the title's or the detail's font at a size: the title's top to the
    // detail's bottom, with a few clear pixels between the title's descenders and the detail's top, centered in the
    // pill row inside its border with clear space above and below; a title alone, its top to its baseline centered and
    // all of its ink inside. The sizes are the metrics', both lines made smaller together only as far as that needs,
    // and a title alone only as far as it needs on its own.
    TextRow FitTextRow(const BarMetrics& metrics, FunctionRef<LineInk(bool title, int pixels)> ink);

    // natural: each widget's width by SlotOf, 0 for one not shown. separators: by SeparatorBit, the diamonds between
    // neighbours and the end caps. The left section starts after the left end cap or a gap, the right one ends before
    // the right one, and the center one is centered as a group
    // in what is left. When the bar is too narrow, the target gives way first, wherever it sits: it shrinks, in whole
    // pixels, down to targetMinimum (or all of its width when that is less), then goes. Then a left widget that would
    // pass the screen's edge goes with the rest of its section, the right section loses widgets from its inner end
    // where they would reach the left, and the center loses them from its end; the target then takes back the room
    // those leave. In ornate, the target's frame keeps DiamondClearance from the diamond on a
    // neighbour's edge. Pills with diamonds sit a gap, the diamond and a gap apart.
    // spacing: every widget's room more between it and each neighbour; an ornate diamond stands in the middle of the
    // room it is in.
    // groups: by SlotOf, the first widget of each one's pill (PanelGroups). The widgets of a pill have no gap, room
    // or diamond between them, only linked's room after each (by SlotOf), and are placed or dropped together.
    BarLayout LayoutBar(float screenWidth, const BarMetrics& metrics, BarShape shape,
                        const std::array<float, kPanelSlots>& natural, float targetMinimum,
                        const BarPlacement& placement, int separators, float spacing,
                        const std::array<PanelWidget, kPanelSlots>& groups);
    // A widget's padding inside its box on each side: padding, but none where it shares its pill with the shown widget
    // before it, and its Linked Padding (linked, by SlotOf) where it shares it with the one after. shown is above 0
    // for a widget with something to show.
    struct WidgetPads
    {
        float left = 0.0f, right = 0.0f;
    };
    std::array<WidgetPads, kPanelSlots> PillPads(const BarPlacement& placement, const std::array<PanelWidget, kPanelSlots>& groups,
        const std::array<float, kPanelSlots>& shown, const std::array<float, kPanelSlots>& linked, float padding);
    // The widget under the point, or for a combined pill its first widget, which names the pill's popup.
    std::optional<PanelWidget> WidgetAt(const BarLayout& layout, float x, float y);
    const BarRect* RectOf(const BarLayout& layout, PanelWidget widget);
    // The pill a widget names, around every widget of it that was placed; none when none was.
    std::optional<BarRect> GroupRect(const BarLayout& layout, PanelWidget head);
    // Whether a placed widget shares its pill with the widget before it and after it, and so draws no edge there.
    struct PillJoins
    {
        bool before = false, after = false;
    };
    PillJoins JoinsOf(const BarLayout& layout, PanelWidget widget);
    // A combined pill has a popup when any of its widgets has one: its first widget's hasPopup says so.
    void SharePopups(std::array<bool, kPanelSlots>& hasPopup, const std::array<PanelWidget, kPanelSlots>& groups);
    // Which of the popups' documents a popup is drawn in: under the settings a popup shows, over them, or over the
    // settings window.
    enum class PopupLayer : uint8_t { UnderSettings, OverSettings, OverMenu };
    constexpr size_t kPopupLayers = static_cast<size_t>(PopupLayer::OverMenu) + 1;
    // A dragged popup's spot, by which half of the screen its middle is in across and down. One reaching the screen's
    // bottom, cut to the room below its top, is kept by its top: kept by its bottom it would grow up past where it was left.
    PopupSpot SpotOf(const BarRect& popup, float screenWidth, float screenHeight);
    // A popup of this size at the spot, its kept edges where the spot says, moved onto the screen if it would leave it.
    BarRect PlaceAtSpot(const PopupSpot& spot, float width, float height, float screenWidth, float screenHeight);
    // Where a popup is placed instead of under its widget: by its top left where it is being dragged, else at the spot
    // it was left, pinned or not; nothing for under its widget.
    std::optional<PopupSpot> PlacedSpot(const std::optional<BarRect>& dragged, const std::optional<PopupSpot>& spot);
    // The height a popup placed at the spot is cut to: the screen's below its top, or above its bottom.
    float SpotRoom(const PopupSpot& spot, float screenHeight);
    // Where a popup shows: placed, at its height, else under its widget, across at keptX, where it opened, so a widget
    // changing width while it is open does not move it.
    BarRect PopupAt(const BarRect& underWidget, float height, const std::optional<PopupSpot>& placed, float screenWidth,
        float screenHeight, std::optional<float> keptX = std::nullopt);
    // Under its widget below the bar, moved inward from the screen's sides, its height cut at the bottom: a ring
    // drawn ring pixels around it stays gap from the bar and the screen's edges.
    BarRect PlacePopup(const BarRect& widget, float barHeight, float width, float height, float screenWidth,
                       float screenHeight, float gap, float ring);
    // The height PlacePopup cuts a popup to: negative when the screen has no room under the bar.
    float PopupRoom(float barHeight, float screenHeight, float gap, float ring);
    // The longest start of text that fits width, with "..." when it was cut; empty when no start of it fits with "...".
    std::string FitText(const std::string& text, float width, FunctionRef<float(const std::string&)> measure);
    // The first characters of text and "...", the least of it worth showing; the text itself when it has no more.
    std::string Shortest(const std::string& text, int characters);

    // A tab of one of the open popup's cards, where it was drawn on the screen: its key the card's heading.
    struct BarTabChip
    {
        std::string key;
        BarRect rect;
        std::string tab;
        bool press = false; // a pressable cell: a press tells the plugin its tab
        bool fleeting = false; // its pick is forgotten once the popup closes
    };
    // A part of the open popup the wheel scrolls, where it was drawn, and how many lines it scrolls.
    struct BarScroll
    {
        std::string key;
        BarRect rect;
        size_t most = 0;
        BarRect track{};    // where a press drags its scrollbar's thumb
        float thumb = 0.0f; // the thumb's length down the track
    };

    // Where a shown popup was drawn: its pill, its rectangle, its cards' tabs and pressable cells, the parts that
    // scroll, its pin chip, its settings chip and its title row, by which a pinned popup is dragged.
    struct PopupPlace
    {
        PanelWidget pill;
        BarRect rect;
        std::vector<BarTabChip> chips{};
        std::vector<BarScroll> scrolls{};
        BarRect pin{}, cog{}, grip{};
    };

    // What a press on the Settings widget tells the plugin (BarInput::TakePresses): open the settings window.
    constexpr const char* kOpenSettingsPress = "open settings";

    // Which popups show. Hovering a widget that has one opens it; leaving both the widget and its popup, or the widget
    // losing its popup, closes it, and the pointer crossing the gap between them does not. A press on a widget or a
    // popup's pin chip pins its popup, or unpins it if it was pinned; a pinned popup stays open beside the others.
    class BarInput
    {
    public:
        // Each frame: where the widgets are now and which have popups. A pinned widget that is gone loses its pin.
        void Frame(const BarLayout& layout, const std::array<bool, kPanelSlots>& hasPopup);
        // Where each popup Shown gave was drawn, in its order.
        void SetPopups(std::vector<PopupPlace> popups);
        // The popups to draw, the last on top: each pinned one in the order it was pinned, then the hovered one.
        std::vector<PanelWidget> Shown() const;
        // The wheel turned notches toward the player: true when the pointer, where it last moved, is over a part of a
        // popup that scrolls, which then scrolls kLinesPerNotch lines a notch, no further than its ends.
        bool Wheel(int notches);
        // A left press on a scrollbar's track drags its thumb, from where it was pressed on it or, off it, from its
        // middle, which jumps there; moves drag it, held, until the button goes up.
        // A left press on a pinned popup's title row drags the popup, which ends where the button goes up.
        bool Dragging() const { return m_Drag.has_value() || m_PopupDrag.has_value(); }
        void DragTo(float x, float y);
        void EndDrag();
        // Where this pill's popup is being dragged now; nothing while it is not.
        std::optional<BarRect> DraggedPopup(PanelWidget pill) const;
        // The pill whose popup a drag left somewhere since the last call, and where.
        std::optional<std::pair<PanelWidget, BarRect>> TakeMoved() { return std::exchange(m_Moved, std::nullopt); }
        // How many lines each part of the popups has been scrolled, by its key: back to the top once none shows.
        const std::map<std::string, size_t>& Scrolled() const { return m_Scrolled; }
        // A press in a popup was refused, which it says until none shows.
        void Refuse(bool refused = true) { m_Refused = refused; }
        bool Refused() const { return m_Refused; }
        void Move(float x, float y);
        // The pointer is off the game: over an Ashita window or out of the client area, where no move reports it.
        void Leave();
        // Forget the layout, the pointer and any pin, as before the first frame.
        void Reset() { *this = BarInput{}; }
        // True when the press landed on the bar, a widget placed under it or a popup: the game must not get it. A left
        // press on a widget or a popup's pin chip pins or unpins its popup, leaving the others; on a tab it picks it,
        // and pins nothing.
        bool Press(float x, float y, bool left);
        // The tab picked last in each card since loading, by the card's heading.
        const std::map<std::string, std::string>& PickedTabs() const { return m_Tabs; }
        // What the presses on pressable cells since the last call told, oldest first.
        std::vector<std::string> TakePresses() { return std::exchange(m_Presses, {}); }
        bool Over(float x, float y) const;
        // Inside a shown popup, which is drawn over Ashita's windows.
        bool InPopup(float x, float y) const { return PlaceAt(x, y) != nullptr; }
        // Pins this widget's popup beside any pinned already, as a press on it would, or with none unpins them all; a
        // widget not on the bar loses it next frame.
        void Pin(std::optional<PanelWidget> widget);
        // Pins this pill's popup on its settings, as a press on its settings chip does, or with none closes them,
        // unpinning the popup if they pinned it.
        void ShowSettings(std::optional<PanelWidget> pill);
        // The pill whose pinned popup shows its settings now. A press on its settings chip or its pin goes back to its
        // content.
        std::optional<PanelWidget> SettingsShown() const { return m_Settings && IsPinned(*m_Settings) ? m_Settings : std::nullopt; }
        // The popup in front: the hovered one, or else the one pinned last.
        std::optional<PanelWidget> Open() const { return m_Hover ? m_Hover : Pinned(); }
        std::optional<PanelWidget> Pinned() const { return m_Pinned.empty() ? std::nullopt : std::optional(m_Pinned.back()); }
        bool IsPinned(PanelWidget pill) const { return std::ranges::find(m_Pinned, pill) != m_Pinned.end(); }
        std::optional<PanelWidget> Hovered() const { return m_Under; }
        // Whether a pill shows lit: under the pointer, or showing its popup.
        bool Lit(PanelWidget pill, bool hasPopup) const { return m_Under == pill || (hasPopup && (Open() == pill || IsPinned(pill))); }
        // Each frame, whether the settings window is open: the popups showing when it opened are drawn under it, and
        // those that came out after it over it.
        void MenuOpen(bool open);
        bool OverMenu(PanelWidget pill) const;
        // By Shown, bottom to top: each popup's layer. Those shown after the one with settings go over its settings,
        // and those come out after the window opened over the window, but for the one with settings, which stays under
        // the window with them.
        std::vector<PopupLayer> Layers() const;

    private:
        void Reevaluate();
        // Pins or unpins the pill's popup, as its widget or its pin chip do.
        void TogglePin(PanelWidget pill);
        // The topmost popup drawn over the point, if any.
        const PopupPlace* PlaceAt(float x, float y) const;
        // The hovered popup and everything above it down from the screen's top: the pointer crossing the gap keeps it
        // open.
        bool InHoverColumn(float x, float y) const;

        static constexpr float kAway = -1.0f; // left of and above everything placed: a pointer off the game
        static constexpr long kLinesPerNotch = 3;

        BarLayout m_Layout;
        std::array<bool, kPanelSlots> m_HasPopup{};
        std::vector<PopupPlace> m_Popups;
        std::map<std::string, std::string> m_Tabs;
        std::set<std::string> m_Fleeting; // the cards in m_Tabs whose picks go when no popup shows
        std::vector<std::string> m_Presses;
        struct PopupDrag
        {
            PanelWidget pill;
            BarRect at;
            float grabX, grabY; // from its top-left corner
        };
        std::optional<PopupDrag> m_PopupDrag;
        std::optional<std::pair<PanelWidget, BarRect>> m_Moved;
        struct Drag
        {
            std::string key;
            float grab; // from the thumb's top
        };
        std::optional<Drag> m_Drag;
        std::map<std::string, size_t> m_Scrolled;
        bool m_Refused = false;
        float m_X = kAway, m_Y = kAway;
        std::optional<PanelWidget> m_Under;  // the widget under the pointer
        std::optional<PanelWidget> m_Hover;  // the popup hovering keeps open
        std::optional<PanelWidget> m_Dismissed; // unpinned from its widget: no hover until the pointer leaves it
        std::vector<PanelWidget> m_Pinned;   // in the order pinned
        std::optional<std::vector<PanelWidget>> m_UnderMenu; // while the window is open: those it opened over, still shown
        std::optional<PanelWidget> m_Settings; // a pinned pill, while its popup shows its settings
        bool m_PinnedForSettings = false;      // its pin came with its settings, and goes when they close
    };

    // Which of the game's mouse messages the bar takes, by their Windows numbers: a press on the bar or its open
    // popup, and that press's release wherever it lands, as the game never saw the press. Moves go to input too, and
    // one the bar may not see tells input the pointer left.
    class BarClicks
    {
    public:
        explicit BarClicks(BarInput& input)
            : m_Input(input)
        {}
        // True when the game must not get the message. buttons: the message's MK_ flags. A press is the bar's only
        // while it is on, nothing else blocked the message and no Ashita window is under the pointer, or the pointer
        // is in the open popup, which is drawn over them.
        bool Take(uint32_t message, uint32_t buttons, float x, float y, bool barOn, bool overGui, bool blocked);
        // Each frame during a drag, Windows' pointer and whether its left button is down: the drag follows it with or
        // without moves, and ends where it shows once the button is up, even let go outside the window.
        void Held(float x, float y, bool down);

    private:
        BarInput& m_Input;
        bool m_Left = false, m_Right = false; // the bar took that button's press
    };
}
