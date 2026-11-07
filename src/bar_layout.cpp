#include "bar_layout.h"

#include "mouse_messages.h"
#include "rml_pure.h"
#include "utf16.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <utility>

namespace headsup
{
    namespace
    {
        constexpr const char* kEllipsis = "...";
        constexpr float kBottomLine  = 1.0f;  // the strip's last row is a line
        constexpr float kInkMargin   = 0.06f; // the ink's clear space inside a pill, of its height
        constexpr float kMinInkMargin = 2.0f; // pixels
        constexpr float kLineGap     = 0.08f; // H from the title's descenders to the detail's top, when there is room
        constexpr float kMinLineGap  = 3.0f;  // pixels: two clear rows between them still read as touching
        // The detail's share of a title, as a float, can fall either side of a half: this rounds every size alike.
        constexpr float kShareSlack  = 0.001f;

        float Width(const std::array<float, kPanelSlots>& natural, PanelWidget w)
        {
            return natural[SlotOf(w)];
        }

        constexpr float kFitSlack = 0.01f; // pixels of float rounding: a widget that fits exactly is not dropped

        constexpr float kDiamondGap = 2.0f; // pixels clear between a frame or hover tint and a divider's diamond

        // Between neighbours in a section: `between`, and in ornate, where a diamond on the plain neighbour's edge
        // divides them, the framed target's clear space from it.
        struct Spacing
        {
            float between = 0.0f, frameClear = 0.0f;
            float room = 0.0f; // every widget's room more on each side
            const std::array<PanelWidget, kPanelSlots>* groups = nullptr;
            bool Together(PanelWidget a, PanelWidget b) const { return (*groups)[SlotOf(a)] == (*groups)[SlotOf(b)]; }
            float Frame(PanelWidget w) const { return w.kind == BarWidget::Target ? frameClear : 0.0f; }
            // left is the one placed further left.
            float Apart(PanelWidget left, PanelWidget right) const
            {
                return Together(left, right) ? 0.0f : between + Frame(left) + Frame(right) + 2.0f * room;
            }
            // From a's edge: the middle of what is between them, but for a's frame's clear space.
            float Middle(PanelWidget a) const { return Frame(a) + between / 2.0f + room; }
        };

        struct Group
        {
            std::vector<PlacedWidget> widgets; // in placing order
            std::vector<float> dividers;
            float targetWidth = 0.0f;
        };

        // Places order one after another from `from` towards `limit` (leftward when limit is less): each widget at its
        // width, spaced from the last; the first that does not fit goes with every one after it, and with the widgets
        // of its pill placed before it.
        Group PlaceGroup(const std::vector<PanelWidget>& order, float from, float limit, const Spacing& spacing,
                         bool diamonds, const std::array<float, kPanelSlots>& widths, const BarRow& pill,
                         float rowY, float rowH)
        {
            Group g;
            const float dir = limit >= from ? 1.0f : -1.0f;
            float at        = from;
            for (const PanelWidget w : order)
            {
                const float width = Width(widths, w);
                if (width <= 0.0f) continue;
                const bool first = g.widgets.empty();
                const float gap  = first ? 0.0f
                                         : dir > 0.0f ? spacing.Apart(g.widgets.back().widget, w)
                                                      : spacing.Apart(w, g.widgets.back().widget);
                if (width > (limit - at) * dir - gap + kFitSlack)
                {
                    for (PanelWidget next = w; !g.widgets.empty() && spacing.Together(g.widgets.back().widget, next);)
                    {
                        next = g.widgets.back().widget;
                        g.widgets.pop_back();
                        if (!g.widgets.empty() && diamonds && !spacing.Together(g.widgets.back().widget, next))
                            g.dividers.pop_back();
                    }
                    break;
                }
                if (!first && diamonds && !spacing.Together(g.widgets.back().widget, w))
                    g.dividers.push_back(at + dir * spacing.Middle(g.widgets.back().widget));
                const float x       = dir > 0.0f ? at + gap : at - gap - width;
                const bool isTarget = w.kind == BarWidget::Target; // framed on the pill row in every shape
                const PanelWidget head = (*spacing.groups)[SlotOf(w)];
                g.widgets.push_back({w, BarRect{x, isTarget ? pill.y : rowY, width, isTarget ? pill.height : rowH},
                    head == w ? std::nullopt : std::optional<PanelWidget>(head)});
                if (isTarget) g.targetWidth = width;
                at = dir > 0.0f ? x + width : x;
            }
            return g;
        }

        float GroupWidth(const std::vector<PanelWidget>& order, const std::array<float, kPanelSlots>& widths,
                         const Spacing& spacing)
        {
            float total = 0.0f;
            std::optional<PanelWidget> last;
            for (const PanelWidget w : order)
                if (Width(widths, w) > 0.0f)
                {
                    total += Width(widths, w) + (last ? spacing.Apart(*last, w) : 0.0f);
                    last = w;
                }
            return total;
        }

        // The ink's clear space above and below inside a pill row, in whole pixels, so the drawn text, rounded to
        // them, keeps all of it.
        float InkMargin(float rowHeight) { return std::ceil(std::max(kMinInkMargin, kInkMargin * rowHeight)); }

        float InkRoom(float rowHeight) { return rowHeight - 2.0f * kPillBorder - 2.0f * InkMargin(rowHeight); }
    }

    BarLayout LayoutBar(float screenWidth, const BarMetrics& m, BarShape shape,
                        const std::array<float, kPanelSlots>& natural, float targetMinimum,
                        const BarPlacement& placement, int separators, float spacing,
                        const std::array<PanelWidget, kPanelSlots>& groups)
    {
        const bool ornate   = shape == BarShape::Ornate;
        const bool diamonds = (separators & SeparatorBit(Separator::Between)) != 0;
        // Ornate neighbours abut, a diamond on their boundary, the tints keeping clear of it. Pills sit a gap apart; a
        // diamond between them takes its width and a gap on either side.
        const float pillGap = diamonds ? 2.0f * m.gap + static_cast<float>(WholePixels(m.diamond)) : m.gap;
        const Spacing apart{.between = ornate ? 0.0f : pillGap, .frameClear = ornate ? DiamondClearance(m) : 0.0f, .room = spacing,
            .groups = &groups};
        const bool caps     = (separators & SeparatorBit(Separator::EndCaps)) != 0;
        const float edge    = caps ? m.cap : m.gap; // between a section and the screen's edge
        const BarRow pill   = PillRow(m);
        const float rowY    = ornate ? 0.0f : pill.y;
        const float rowH    = ornate ? m.height : pill.height;
        const auto& left    = placement[static_cast<int>(BarSection::Left)];
        const auto& center  = placement[static_cast<int>(BarSection::Center)];
        const auto& right   = placement[static_cast<int>(BarSection::Right)];

        // The target gives way first, wherever it sits: it gives up what the bar is short of, in whole pixels, and goes
        // when that leaves it less than min(its width, its minimum). Only then do other widgets go. Sections with
        // something shown are a gap apart and the outer ones an edge from the screen's sides.
        const auto giveWay = [&](std::array<float, kPanelSlots> widths)
        {
            const auto shortOf = [&]
            {
                float demand = 2.0f * edge;
                int filled   = 0;
                for (const std::vector<PanelWidget>* section : {&left, &center, &right})
                    if (const float w = GroupWidth(*section, widths, apart); w > 0.0f)
                        demand += w + (filled++ > 0 ? m.gap : 0.0f);
                return demand - screenWidth;
            };
            constexpr int kTarget = SlotOf(BarWidget::Target);
            if (const float want = widths[kTarget], lack = shortOf(); want > 0.0f && lack > kFitSlack)
            {
                const float got = std::floor(want - lack);
                widths[kTarget] = got > 0.0f && got >= std::min(want, targetMinimum) ? got : 0.0f;
            }
            return widths;
        };

        const auto place = [&](const std::array<float, kPanelSlots>& widths)
        {
            BarLayout l;
            l.height = m.height;
            Group lg = PlaceGroup(left, edge, screenWidth - edge, apart, diamonds, widths, pill, rowY, rowH);
            const float leftEnd =
                lg.widgets.empty() ? edge - m.gap : lg.widgets.back().rect.x + lg.widgets.back().rect.width;
            const std::vector<PanelWidget> rightInward(right.rbegin(), right.rend());
            Group rg = PlaceGroup(rightInward, screenWidth - edge, leftEnd + m.gap, apart, diamonds, widths, pill,
                rowY, rowH);
            std::reverse(rg.widgets.begin(), rg.widgets.end());
            const float rightStart = rg.widgets.empty() ? screenWidth - edge + m.gap : rg.widgets.front().rect.x;

            // The center: what fits between the sides, a gap from each, its widgets going from its end.
            const float lowest = leftEnd + m.gap, highest = rightStart - m.gap;
            std::vector<PanelWidget> shown;
            for (const PanelWidget w : center)
                if (Width(widths, w) > 0.0f) shown.push_back(w);
            while (!shown.empty() && GroupWidth(shown, widths, apart) > highest - lowest + kFitSlack)
            {
                const PanelWidget last = shown.back();
                while (!shown.empty() && apart.Together(shown.back(), last))
                    shown.pop_back();
            }
            Group cg;
            if (!shown.empty())
            {
                const float total = GroupWidth(shown, widths, apart);
                // The two bounds are equal when the center got all the room, and float rounding can put the first an
                // ulp above the second, which std::clamp does not allow.
                const float x = std::max(lowest, std::min(screenWidth / 2.0f - total / 2.0f, highest - total));
                // Its widths fit already: no limit, so float rounding cannot drop its last widget.
                cg = PlaceGroup(shown, x, std::numeric_limits<float>::infinity(), apart, diamonds, widths, pill,
                    rowY, rowH);
            }

            for (const Group* g : {&lg, &cg, &rg})
            {
                l.widgets.insert(l.widgets.end(), g->widgets.begin(), g->widgets.end());
                l.dividers.insert(l.dividers.end(), g->dividers.begin(), g->dividers.end());
                l.targetWidth = std::max(l.targetWidth, g->targetWidth);
            }
            std::sort(l.dividers.begin(), l.dividers.end());
            return l;
        };

        constexpr int kTarget = SlotOf(BarWidget::Target);
        const std::array<float, kPanelSlots> widths = giveWay(natural);
        BarLayout l = place(widths);
        // The target gave way against every widget; those that went anyway leave room it takes back.
        std::array<float, kPanelSlots> kept = natural;
        for (int w = 0; w < kPanelSlots; ++w)
            if (w != kTarget && RectOf(l, WidgetInSlot(w)) == nullptr) kept[w] = 0.0f;
        if (const std::array<float, kPanelSlots> refit = giveWay(kept); refit[kTarget] != widths[kTarget])
            l = place(refit);
        return l;
    }

    float DiamondClearance(const BarMetrics& m)
    {
        return static_cast<float>(WholePixels(m.diamond)) / 2.0f + kDiamondGap;
    }

    BarRow PillRow(const BarMetrics& m)
    {
        const float room   = std::max(0.0f, m.height - kBottomLine);
        const float height = std::max(0.0f, std::min(std::round(m.pillHeight), room));
        return BarRow{std::floor((room - height) / 2.0f), height};
    }

    TextRow FitTextRow(const BarMetrics& m, FunctionRef<LineInk(bool title, int pixels)> ink)
    {
        const BarRow row    = PillRow(m);
        const float inside  = row.height - 2.0f * kPillBorder;
        const float margin  = InkMargin(row.height);
        // The margin grows a pixel a side at a time, so a row a pixel taller can have a pixel less room for the ink: a
        // row takes no more than the next one's, and a taller bar never gets smaller text.
        const float room    = std::min(InkRoom(row.height), InkRoom(row.height + 1.0f));
        const float maxGap  = std::max(kMinLineGap, std::round(kLineGap * m.height));
        TextRow t;
        LineInk title, detail;
        float spare = 0.0f; // for the gap
        // A pixel of the title at a time, the detail line keeping its share of it, so a larger size or a taller bar
        // never draws either line smaller.
        const float share = m.subtitle / m.title;
        for (t.title = std::max(1, WholePixels(m.title));; --t.title)
        {
            t.detail = std::max(1, WholePixels(share * static_cast<float>(t.title) + kShareSlack));
            title    = ink(true, t.title);
            detail   = ink(false, t.detail);
            spare    = room - (title.bottom - title.top) - (detail.bottom - detail.top);
            if (spare >= kMinLineGap || t.title == 1) break;
        }
        const float gap   = std::clamp(spare, 0.0f, maxGap);
        const float block = (title.bottom - title.top) + gap + (detail.bottom - detail.top);
        // The title's ink top to the detail's baseline centered, as the eye reads two lines, but the detail's descenders
        // no nearer the bottom than the margin.
        const float read     = block - (detail.bottom - detail.baseline);
        const float even     = row.y + kPillBorder + (inside - read) / 2.0f;
        const float top      = std::min(even, row.y + kPillBorder + inside - margin - block); // of the title's ink
        t.titleY          = top - title.top;
        t.detailY         = top + (title.bottom - title.top) + gap - detail.top;
        // A title alone: as large as its ink fits, its capitals centered, raised if its descenders would come too near
        // the bottom.
        LineInk alone;
        for (t.alone = WholePixels(m.title); t.alone > 1; --t.alone)
            if (alone = ink(true, t.alone); alone.bottom - alone.top <= room) break;
        alone = ink(true, t.alone);
        const float centered = row.y + (row.height - (alone.baseline - alone.top)) / 2.0f;
        const float lowest   = row.y + row.height - kPillBorder - margin - (alone.bottom - alone.top);
        t.aloneY             = std::min(centered, lowest) - alone.top;
        t.side               = t.detail;
        t.sideY              = row.y + (row.height - (detail.baseline - detail.top)) / 2.0f - detail.top;
        return t;
    }

    std::string Shortest(const std::string& text, int characters)
    {
        size_t end = 0;
        for (int n = 0; n < characters && end < text.size(); ++n)
            do
                ++end;
            while (end < text.size() && ContinuesCharacter(text[end]));
        if (end >= text.size()) return text;
        std::string cut = text.substr(0, end);
        while (!cut.empty() && cut.back() == ' ')
            cut.pop_back();
        return cut.empty() ? std::string() : cut + kEllipsis;
    }

    std::optional<PanelWidget> WidgetAt(const BarLayout& layout, float x, float y)
    {
        for (const PlacedWidget& p : layout.widgets)
            if (p.rect.Contains(x, y)) return p.group.value_or(p.widget);
        return std::nullopt;
    }

    std::optional<BarRect> GroupRect(const BarLayout& layout, PanelWidget head)
    {
        std::optional<BarRect> around;
        for (const PlacedWidget& p : layout.widgets)
        {
            if (p.group.value_or(p.widget) != head) continue;
            if (!around)
            {
                around = p.rect;
                continue;
            }
            const float left = std::min(around->x, p.rect.x), top = std::min(around->y, p.rect.y);
            const float right  = std::max(around->x + around->width, p.rect.x + p.rect.width);
            const float bottom = std::max(around->y + around->height, p.rect.y + p.rect.height);
            around             = BarRect{left, top, right - left, bottom - top};
        }
        return around;
    }

    PillJoins JoinsOf(const BarLayout& layout, PanelWidget widget)
    {
        const auto shares = [](const PlacedWidget& a, const PlacedWidget& b) {
            return a.group.value_or(a.widget) == b.group.value_or(b.widget);
        };
        const std::vector<PlacedWidget>& w = layout.widgets;
        for (size_t i = 0; i < w.size(); ++i)
            if (w[i].widget == widget)
                return PillJoins{i > 0 && shares(w[i - 1], w[i]), i + 1 < w.size() && shares(w[i], w[i + 1])};
        return {};
    }

    std::array<WidgetPads, kPanelSlots> PillPads(const BarPlacement& placement, const std::array<PanelWidget, kPanelSlots>& groups,
        const std::array<float, kPanelSlots>& shown, const std::array<float, kPanelSlots>& linked, float padding)
    {
        std::array<WidgetPads, kPanelSlots> pads;
        pads.fill(WidgetPads{padding, padding});
        for (const std::vector<PanelWidget>& section : placement)
        {
            std::optional<PanelWidget> last;
            for (const PanelWidget w : section)
            {
                if (shown[SlotOf(w)] <= 0.0f) continue;
                if (last && groups[SlotOf(*last)] == groups[SlotOf(w)])
                {
                    pads[SlotOf(*last)].right = linked[SlotOf(*last)];
                    pads[SlotOf(w)].left      = 0.0f;
                }
                last = w;
            }
        }
        return pads;
    }

    void SharePopups(std::array<bool, kPanelSlots>& hasPopup, const std::array<PanelWidget, kPanelSlots>& groups)
    {
        for (int slot = 0; slot < kPanelSlots; ++slot)
            if (hasPopup[slot]) hasPopup[SlotOf(groups[slot])] = true;
    }

    const BarRect* RectOf(const BarLayout& layout, PanelWidget widget)
    {
        for (const PlacedWidget& p : layout.widgets)
            if (p.widget == widget) return &p.rect;
        return nullptr;
    }

    BarRect PlacePopup(const BarRect& widget, float barHeight, float width, float height, float screenWidth,
                       float screenHeight, float gap, float ring)
    {
        const float clear    = gap + ring;
        const float centered = widget.x + widget.width / 2.0f - width / 2.0f;
        const float x        = std::max(0.0f, std::max(clear, std::min(centered, screenWidth - clear - width)));
        const float y        = barHeight + clear;
        return BarRect{x, y, width, std::max(0.0f, std::min(height, PopupRoom(barHeight, screenHeight, gap, ring)))};
    }

    PopupSpot SpotOf(const BarRect& popup, float screenWidth, float screenHeight)
    {
        const bool right = popup.x + popup.width / 2.0f > screenWidth / 2.0f;
        const bool lower = popup.y + popup.height / 2.0f > screenHeight / 2.0f;
        return PopupSpot{static_cast<int>(std::lround(right ? screenWidth - popup.x - popup.width : popup.x)),
            static_cast<int>(std::lround(lower ? screenHeight - popup.y - popup.height : popup.y)), right, lower};
    }

    BarRect PlaceAtSpot(const PopupSpot& spot, float width, float height, float screenWidth, float screenHeight)
    {
        width  = std::min(width, screenWidth);
        height = std::min(height, screenHeight);
        const auto x = static_cast<float>(spot.x), y = static_cast<float>(spot.y);
        return BarRect{std::clamp(spot.fromRight ? screenWidth - x - width : x, 0.0f, screenWidth - width),
            std::clamp(spot.fromBottom ? screenHeight - y - height : y, 0.0f, screenHeight - height), width, height};
    }

    float PopupRoom(float barHeight, float screenHeight, float gap, float ring)
    {
        return screenHeight - barHeight - 2.0f * (gap + ring);
    }

    std::string FitText(const std::string& text, float width, FunctionRef<float(const std::string&)> measure)
    {
        if (text.empty() || measure(text) <= width) return text;
        // Each shorter start once, without the spaces it ends in.
        std::vector<size_t> cuts;
        for (size_t end = 1; end < text.size(); ++end)
        {
            if (ContinuesCharacter(text[end])) continue;
            size_t cut = end;
            while (cut > 0 && text[cut - 1] == ' ')
                --cut;
            if (cut > 0 && (cuts.empty() || cuts.back() != cut)) cuts.push_back(cut);
        }
        // A longer start is never narrower, so the longest that fits is found by halving.
        const auto past = std::ranges::partition_point(
            cuts, [&](size_t cut) { return measure(text.substr(0, cut) + kEllipsis) <= width; });
        return past == cuts.begin() ? std::string() : text.substr(0, *(past - 1)) + kEllipsis;
    }

    void BarInput::Frame(const BarLayout& layout, const std::array<bool, kPanelSlots>& hasPopup)
    {
        m_Layout   = layout;
        m_HasPopup = hasPopup;
        std::erase_if(m_Pinned, [&](PanelWidget pill) { return !GroupRect(m_Layout, pill); });
        if (m_Settings && !IsPinned(*m_Settings)) m_Settings.reset();
        Reevaluate();
    }

    void BarInput::SetPopups(std::vector<PopupPlace> popups)
    {
        m_Popups = std::move(popups);
        Reevaluate();
    }

    std::vector<PanelWidget> BarInput::Shown() const
    {
        std::vector<PanelWidget> shown = m_Pinned;
        if (m_Hover && !IsPinned(*m_Hover)) shown.push_back(*m_Hover);
        return shown;
    }

    void BarInput::Pin(std::optional<PanelWidget> widget)
    {
        m_Settings.reset();
        m_PinnedForSettings = false;
        m_Hover.reset();
        if (!widget)
        {
            m_Pinned.clear();
            return;
        }
        std::erase(m_Pinned, *widget);
        m_Pinned.push_back(*widget);
    }

    void BarInput::ShowSettings(std::optional<PanelWidget> pill)
    {
        if (!pill)
        {
            if (m_Settings && m_PinnedForSettings) std::erase(m_Pinned, *m_Settings);
            m_Settings.reset();
            m_PinnedForSettings = false;
            return;
        }
        const bool held = IsPinned(*pill);
        Pin(pill);
        m_Settings          = pill;
        m_PinnedForSettings = !held;
    }

    void BarInput::TogglePin(PanelWidget pill)
    {
        if (m_Settings == pill)
        {
            m_Settings.reset();
            m_PinnedForSettings = false;
        }
        if (IsPinned(pill))
            std::erase(m_Pinned, pill);
        else
            m_Pinned.push_back(pill);
    }

    const PopupPlace* BarInput::PlaceAt(float x, float y) const
    {
        for (auto place = m_Popups.rbegin(); place != m_Popups.rend(); ++place)
            if ((IsPinned(place->pill) || m_Hover == place->pill) && place->rect.Contains(x, y)) return &*place;
        return nullptr;
    }

    void BarInput::DragTo(float x, float y)
    {
        if (m_PopupDrag)
        {
            m_PopupDrag->at.x = x - m_PopupDrag->grabX;
            m_PopupDrag->at.y = y - m_PopupDrag->grabY;
            return;
        }
        if (!m_Drag) return;
        for (const PopupPlace& place : m_Popups)
            for (const BarScroll& scroll : place.scrolls)
                if (scroll.key == m_Drag->key && scroll.track.height > scroll.thumb)
                {
                    const float along = (y - m_Drag->grab - scroll.track.y) / (scroll.track.height - scroll.thumb);
                    m_Scrolled[scroll.key] =
                        static_cast<size_t>(std::lround(std::clamp(along, 0.0f, 1.0f) * static_cast<float>(scroll.most)));
                }
    }

    void BarInput::EndDrag()
    {
        if (m_PopupDrag) m_Moved = std::pair{m_PopupDrag->pill, m_PopupDrag->at};
        m_Drag.reset();
        m_PopupDrag.reset();
    }

    std::optional<BarRect> BarInput::DraggedPopup(PanelWidget pill) const
    {
        return m_PopupDrag && m_PopupDrag->pill == pill ? std::optional(m_PopupDrag->at) : std::nullopt;
    }

    bool BarInput::Wheel(int notches)
    {
        const PopupPlace* place = PlaceAt(m_X, m_Y);
        if (place == nullptr) return false;
        for (const BarScroll& scroll : place->scrolls)
            if (scroll.rect.Contains(m_X, m_Y))
            {
                const long now = static_cast<long>(std::min(m_Scrolled[scroll.key], scroll.most)) + kLinesPerNotch * notches;
                m_Scrolled[scroll.key] = static_cast<size_t>(std::clamp(now, 0L, static_cast<long>(scroll.most)));
                return true;
            }
        return false;
    }

    void BarInput::Move(float x, float y)
    {
        m_X = x;
        m_Y = y;
        Reevaluate();
    }

    void BarInput::Leave() { Move(kAway, kAway); }

    void BarInput::Reevaluate()
    {
        m_Under = WidgetAt(m_Layout, m_X, m_Y);
        if (m_Under && m_HasPopup[SlotOf(*m_Under)])
            m_Hover = m_Under;
        else if (!(m_Hover && m_HasPopup[SlotOf(*m_Hover)] && InHoverColumn(m_X, m_Y)))
            m_Hover.reset();
        if (Shown().empty())
        {
            m_Scrolled.clear();
            m_Refused = false;
            for (const std::string& card : std::exchange(m_Fleeting, {}))
                m_Tabs.erase(card);
        }
    }

    bool BarInput::InHoverColumn(float x, float y) const
    {
        for (const PopupPlace& place : m_Popups)
            if (place.pill == m_Hover &&
                BarRect{place.rect.x, 0.0f, place.rect.width, place.rect.y + place.rect.height}.Contains(x, y))
                return true;
        return false;
    }

    bool BarInput::Over(float x, float y) const
    {
        return (y >= 0.0f && y < m_Layout.height) || WidgetAt(m_Layout, x, y) || InPopup(x, y);
    }

    bool BarInput::Press(float x, float y, bool left)
    {
        if (!Over(x, y)) return false;
        if (!left) return true;
        const std::optional<PanelWidget> under = WidgetAt(m_Layout, x, y);
        // The widget on the bar pins and unpins its popup, as its pin chip does.
        if (under && m_HasPopup[SlotOf(*under)])
        {
            TogglePin(*under);
            return true;
        }
        const PopupPlace* place = under ? nullptr : PlaceAt(x, y);
        if (place == nullptr) return true;
        for (const BarScroll& scroll : place->scrolls)
            if (scroll.track.Contains(x, y) && scroll.most > 0)
            {
                const size_t now = std::min(m_Scrolled[scroll.key], scroll.most);
                const float top  = scroll.track.y + (scroll.track.height - scroll.thumb) * static_cast<float>(now) /
                                                       static_cast<float>(scroll.most);
                const bool held  = y >= top && y < top + scroll.thumb;
                m_Drag           = Drag{scroll.key, held ? y - top : scroll.thumb / 2.0f};
                DragTo(x, y);
                return true;
            }
        for (const BarTabChip& chip : place->chips)
            if (chip.rect.Contains(x, y))
            {
                if (chip.press)
                    m_Presses.push_back(chip.tab);
                else
                {
                    m_Tabs[chip.key] = chip.tab;
                    if (chip.fleeting) m_Fleeting.insert(chip.key);
                }
                return true;
            }
        const PanelWidget pill = place->pill;
        if (place->pin.Contains(x, y))
            TogglePin(pill);
        else if (place->cog.Contains(x, y))
        {
            if (SettingsShown() == pill)
                ShowSettings(std::nullopt);
            else
            {
                m_PinnedForSettings = !IsPinned(pill);
                if (m_PinnedForSettings) m_Pinned.push_back(pill);
                m_Settings = pill;
            }
        }
        else if (IsPinned(pill) && place->grip.Contains(x, y))
            m_PopupDrag = PopupDrag{pill, place->rect, x - place->rect.x, y - place->rect.y};
        return true;
    }

    bool BarClicks::Take(uint32_t message, uint32_t buttons, float x, float y, bool barOn, bool overGui, bool blocked)
    {
        if (!barOn) m_Left = m_Right = false;
        const bool barsTurn = barOn && !blocked && (!overGui || m_Input.InPopup(x, y));
        switch (message)
        {
        case kMouseMove:
            // A move with a button up is the first news of a release outside the window, which nothing else reports.
            m_Left  = m_Left && (buttons & kLeftButton) != 0;
            m_Right = m_Right && (buttons & kRightButton) != 0;
            // A drag goes on wherever the pointer goes while the button is held; the game sees none of it.
            if (m_Input.Dragging())
            {
                if ((buttons & kLeftButton) != 0)
                {
                    m_Input.DragTo(x, y);
                    return true;
                }
                m_Input.EndDrag();
            }
            if (barsTurn)
                m_Input.Move(x, y);
            else
                m_Input.Leave();
            return false;
        // A wheel message's place is on the screen, not the window: the pointer's last move says where it is.
        case kMouseWheel:
            return barOn && !blocked &&
                   m_Input.Wheel(static_cast<int>(std::lround(WheelNotches(static_cast<int16_t>(buttons >> kWheelShift)))));
        case kLeftUp:
            m_Input.EndDrag();
            return std::exchange(m_Left, false);
        case kRightUp: return std::exchange(m_Right, false);
        // A new press shows the last one ended, even when its release went to another window.
        case kLeftDown:
        case kLeftDouble:
            m_Left = barsTurn && m_Input.Press(x, y, true);
            return m_Left;
        case kRightDown:
        case kRightDouble:
            m_Right = barsTurn && m_Input.Press(x, y, false);
            return m_Right;
        }
        return false;
    }
}
