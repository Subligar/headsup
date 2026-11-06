#pragma once

#include "bar_placement.h"
#include "bar_theme.h"
#include "bar_types.h"
#include "function_ref.h"
#include "plate_image.h"

#include <array>
#include <map>
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
        // A meter's texts, at the detail line's size; those beside its bar with their capitals on the row's middle.
        int side     = 0;
        float sideY  = 0.0f;
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
                        const std::array<PanelWidget, kPanelSlots>& groups, const std::array<float, kPanelSlots>& linked = {});
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

    // Which popup is open. Hovering a widget that has one opens it; leaving both the widget and its popup, or the
    // widget losing its popup, closes it, and the pointer crossing the gap between them does not. Only a left press
    // on the open popup's pin chip pins it, or unpins it if it was pinned. While a popup is pinned, hovering another
    // shows that one until the pointer leaves.
    class BarInput
    {
    public:
        // Each frame: where the widgets are now and which have popups. A pinned widget that is gone loses its pin.
        void Frame(const BarLayout& layout, const std::array<bool, kPanelSlots>& hasPopup);
        // Where the open popup was drawn, or an empty rectangle, its cards' tabs, the parts that scroll, its pin chip
        // and its settings chip.
        void SetPopup(const BarRect& popup, std::vector<BarTabChip> chips = {}, std::vector<BarScroll> scrolls = {},
            const BarRect& pin = {}, const BarRect& cog = {});
        // The wheel turned notches toward the player: true when the pointer, where it last moved, is over a part of the
        // popup that scrolls, which then scrolls kLinesPerNotch lines a notch, no further than its ends.
        bool Wheel(int notches);
        // A left press on a scrollbar's track drags its thumb, from where it was pressed on it or, off it, from its
        // middle, which jumps there; moves drag it, held, until the button goes up.
        bool Dragging() const { return m_Drag.has_value(); }
        void DragTo(float y);
        void EndDrag() { m_Drag.reset(); }
        // How many lines each part of the open popup has been scrolled, by its key: back to the top once it closes.
        const std::map<std::string, size_t>& Scrolled() const { return m_Scrolled; }
        // A press in the open popup was refused, which it says until it closes.
        void Refuse() { m_Refused = true; }
        bool Refused() const { return m_Refused; }
        void Move(float x, float y);
        // The pointer is off the game: over an Ashita window or out of the client area, where no move reports it.
        void Leave();
        // Forget the layout, the pointer and any pin, as before the first frame.
        void Reset() { *this = BarInput{}; }
        // True when the press landed on the bar, a widget placed under it or the open popup: the game must not get it.
        // A left press on a widget or the popup's pin chip pins or unpins its popup; on a tab it picks it, and pins
        // nothing.
        bool Press(float x, float y, bool left);
        // The tab picked last in each card since loading, by the card's heading.
        const std::map<std::string, std::string>& PickedTabs() const { return m_Tabs; }
        // What the presses on pressable cells since the last call told, oldest first.
        std::vector<std::string> TakePresses() { return std::exchange(m_Presses, {}); }
        bool Over(float x, float y) const;
        // Inside the open popup, which is drawn over Ashita's windows.
        bool InPopup(float x, float y) const { return Open() && m_Popup.Contains(x, y); }
        // Pins this widget's popup, or none, as a press on it would; a widget not on the bar loses it next frame.
        void Pin(std::optional<PanelWidget> widget)
        {
            m_Pinned = widget;
            m_Settings.reset();
            m_PinnedForSettings = false;
            m_Hover.reset();
        }
        // Pins this pill's popup on its settings, as a press on its settings chip does, or unpins and closes it.
        void ShowSettings(std::optional<PanelWidget> pill)
        {
            const bool held = pill && m_Pinned == pill;
            Pin(pill);
            m_Settings          = pill;
            m_PinnedForSettings = pill && !held;
        }
        // The pill whose popup shows its settings now: the pinned one, while it is open. A press on its settings chip
        // or its pin goes back to its content.
        std::optional<PanelWidget> SettingsShown() const
        {
            return m_Settings && m_Settings == m_Pinned && Open() == m_Pinned ? m_Settings : std::nullopt;
        }
        std::optional<PanelWidget> Open() const { return m_Hover ? m_Hover : m_Pinned; }
        std::optional<PanelWidget> Pinned() const { return m_Pinned; }
        std::optional<PanelWidget> Hovered() const { return m_Under; }

    private:
        void Reevaluate();
        // The popup and everything above it down from the screen's top: the pointer crossing the gap keeps it open.
        bool InPopupColumn(float x, float y) const;

        static constexpr float kAway = -1.0f; // left of and above everything placed: a pointer off the game
        static constexpr long kLinesPerNotch = 3;

        BarLayout m_Layout;
        std::array<bool, kPanelSlots> m_HasPopup{};
        BarRect m_Popup;
        std::vector<BarTabChip> m_Chips;
        std::map<std::string, std::string> m_Tabs;
        std::vector<std::string> m_Presses;
        std::vector<BarScroll> m_Scrolls;
        BarRect m_PinChip, m_CogChip;
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
        std::optional<PanelWidget> m_Pinned;
        std::optional<PanelWidget> m_Settings; // the pinned pill, while its popup shows its settings
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

    private:
        BarInput& m_Input;
        bool m_Left = false, m_Right = false; // the bar took that button's press
    };
}
