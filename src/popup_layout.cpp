#include "popup_layout.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <optional>

namespace headsup
{
    namespace
    {
        constexpr float kTitleIcon   = 1.6f;  // the title icon, in popupText
        constexpr float kCellIcon    = 1.1f;
        constexpr float kMark        = 0.7f;  // the box an immune mark's ink is centered in
        constexpr float kIconGap     = 0.3f;
        constexpr float kStar        = 0.85f; // a starred cell's star, about as tall as a capital
        constexpr float kCellGap     = 0.6f;  // between cells of a line
        constexpr float kColumnGap   = 0.8f;
        constexpr float kRoomyColumnGap = 1.4f; // a roomy grid's
        constexpr float kMiddleRight = 1.0f;  // a spread grid's centered column, right of the card's middle
        constexpr float kPairGap     = 1.2f;  // the least between a pair's label and its value
        constexpr float kTagPadX     = 0.6f, kTagPadY = 0.4f, kTagGap = 0.3f; // from a tag's ink to its frame
        constexpr float kCardPad     = 0.55f; // a card's contents from its edges
        constexpr float kCardGap     = 0.45f; // between cards, and between the columns of them
        constexpr float kMinWidth    = 12.0f; // popupText, so a short popup is not a sliver
        constexpr float kPinChip     = 1.4f;  // the round pin chip's side, in popup text sizes
        constexpr float kMarkShare   = 0.7f;  // the pin or cog, of its chip's height
        constexpr float kSocketInset = 0.9f;  // a socket's rows from its left and right edges
        constexpr float kSocketPad   = 0.35f; // above and below a socket's rows
        constexpr size_t kMostSideBySide  = 4; // groups of a tall grid's lines set side by side before a card goes on
        constexpr size_t kSideBySideAbove = 6; // the lines a grid has before it is set in groups
        constexpr size_t kSocketLines     = 10; // the lines a socket shows, the rest scrolling
        constexpr float kScrollbar        = 0.25f; // a scrollbar's width
        constexpr size_t kMostTabsInRow   = 6;     // a card's tabs share rows of no more than this, evenly

        std::string Upper(std::string text)
        {
            for (char& c : text)
                c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
            return text;
        }

        Span HeadingSpan(const std::string& heading, bool ornate)
        {
            return Span{ornate ? heading : Upper(heading), Tone::Heading, 0};
        }

        // Its ink from the cell's top: the icon's, and its text's font's.
        TextSize CellSize(const PopupCell& cell, float unit, SpanMeasure measure)
        {
            TextSize size;
            const bool pictured = cell.icon || cell.item != 0 || cell.picture != GamePicture::None;
            if (pictured) size = TextSize{unit * kCellIcon, unit * kCellIcon, 0.0f, unit * kCellIcon};
            bool inked = pictured;
            for (const Span& span : cell.text)
            {
                const TextSize t = measure(span, PopupFont::Text);
                size.width += t.width + (pictured && &span == &cell.text.front() ? unit * kIconGap : 0.0f);
                size.height    = std::max(size.height, t.height);
                size.inkTop    = inked ? std::min(size.inkTop, t.inkTop) : t.inkTop;
                size.inkBottom = inked ? std::max(size.inkBottom, t.inkBottom) : t.inkBottom;
                inked          = true;
            }
            if (cell.star) size.width += unit * (kIconGap + kStar);
            return size;
        }

        // From a tag's ink to its frame, in whole pixels: the frame and its text, rounded to pixels alike when drawn,
        // keep all of it.
        float TagPadX(float unit) { return std::round(unit * kTagPadX); }
        float TagPadY(float unit) { return std::round(unit * kTagPadY); }

        // A tag's frame around its cell's ink, with where that ink is in it.
        TextSize Framed(const TextSize& cell, float unit)
        {
            const float ink = cell.inkBottom - cell.inkTop, padY = TagPadY(unit);
            return TextSize{cell.width + 2.0f * TagPadX(unit), ink + 2.0f * padY, padY, ink + padY};
        }

        // A section laid out on its own, from its card's top left: its pieces, the width its rows need and its
        // height, and which pieces are pair values to move to the card's right edge once its width is known.
        struct Block
        {
            std::vector<PopupPiece> pieces;
            float width = 0.0f, height = 0.0f; // the card's, its padding included
            float pad   = 0.0f;                // 0 for the rows under the title, which have no card
            struct Value
            {
                size_t first, last; // pieces [first, last)
                float right;        // where the value ends now, or its middle
                float inset;        // how far in from the card's padding it ends: a socket's room on its right
                bool middle = false; // it goes to the card's middle, not its right edge
            };
            std::vector<Value> values;
            struct Chip
            {
                std::string key, tab;
                size_t piece;          // its frame, its name after it
                size_t column, across; // its place in its row, and how many a row holds
                size_t inRow;          // how many its own row has, fewer in a short last row
                float y, height, name; // from the card's top; its name's width
                float gap;             // between it and the next in its row
                float left = 0.0f, width = 0.0f; // from the card's left, once spread
                size_t parts = 1;                // the pieces after its frame: its icon, if it has one, and its name
                bool fleeting = false;           // its card's tab is picked anew each time the popup opens
            };
            std::vector<Chip> chips;
            struct Press
            {
                std::string tells;
                float x, y, width, height; // from the card's top-left
                float fromRight = 0.0f;    // above 0: its left edge from the card's right, once its width is known
            };
            std::vector<Press> presses; // pressable cells
            std::vector<size_t> sockets; // pieces to widen to the card's inside once its width is known
            struct Anchor
            {
                size_t piece;
                float fromRight; // its left edge, from the card's right
            };
            std::vector<Anchor> anchors; // pieces placed from the card's right once its width is known
            struct Scroll
            {
                std::string key;
                float y, height;
                size_t most;              // lines it scrolls
                float trackY, track, thumb; // its scrollbar
                float grip;               // the right padding's width, the scrollbar in it
            };
            std::vector<Scroll> scrolls;
            bool card     = false;
            float reserve = 0.0f; // its height with its tallest tab picked, which sets its column
            bool beside   = false; // in one row with the card before it
            bool under    = false; // under the card before it, in its column
            bool collapsed = false; // as tall as its heading, wherever it is
        };

        Block LayoutSection(const PopupSection& section, const BarMetrics& metrics, bool ornate, SpanMeasure measure)
        {
            const float unit = metrics.popupText;
            Block b;
            // Every section is a card; one without a heading is its rows alone.
            b.card = true;
            b.pad  = std::round(unit * kCardPad);
            float y = b.pad, contentWidth = 0.0f;
            if (!section.heading.empty())
            {
                Span heading     = HeadingSpan(section.heading, ornate);
                const TextSize t = measure(heading, PopupFont::Heading);
                contentWidth     = t.width + t.overhang;
                b.pieces.push_back(PopupPiece{PopupPiece::Kind::Text, b.pad, y, t.width, t.height, std::move(heading),
                    PopupFont::Heading});
                // Its collapse chip, round and as tall as the heading, against the card's right padding: a dash while
                // open, a plus while collapsed.
                if (!section.collapsePress.empty())
                {
                    const float side = std::round(t.height);
                    contentWidth += unit * kCellGap + side;
                    b.anchors.push_back(Block::Anchor{b.pieces.size(), b.pad + side});
                    PopupPiece chip{PopupPiece::Kind::ActionChip, 0.0f, y, side, side, {}};
                    chip.font = PopupFont::Pin;
                    b.pieces.push_back(std::move(chip));
                    b.anchors.push_back(Block::Anchor{b.pieces.size(), b.pad + side});
                    b.pieces.push_back(PopupPiece{PopupPiece::Kind::Sign, 0.0f, y, side, side,
                        Span{section.collapsed ? kCollapsedSign : kOpenSign, Tone::Heading, 0}});
                    b.presses.push_back(Block::Press{section.collapsePress, 0.0f, y, side, side, b.pad + side});
                }
                y += t.height + metrics.rowGap;
                if (section.collapsed)
                {
                    b.width  = contentWidth + 2.0f * b.pad;
                    b.height = y - metrics.rowGap + b.pad;
                    return b;
                }
            }
            // The section's rows, then its picked tab's.
            std::vector<PopupRow> rows = section.rows;
            if (!section.tabs.empty())
            {
                const size_t picked = std::min(section.tab, section.tabs.size() - 1);
                rows.insert(rows.end(), section.tabs[picked].rows.begin(), section.tabs[picked].rows.end());
                // One width, the widest name's, in as few rows as hold them, shared evenly; spread across the card
                // once its width is known.
                const float gap = unit * kTagGap;
                float widest = 0.0f, tall = 0.0f;
                for (const PopupTab& tab : section.tabs)
                {
                    const TextSize framed = Framed(CellSize(Cell(tab.name, tab.icon), unit, measure), unit);
                    widest = std::max(widest, framed.width);
                    tall   = std::max(tall, framed.height);
                }
                const size_t count = section.tabs.size();
                const size_t lines = section.tabsAcross > 0 ? (count + section.tabsAcross - 1) / section.tabsAcross
                                                            : (count + kMostTabsInRow - 1) / kMostTabsInRow;
                const size_t across = (count + lines - 1) / lines;
                contentWidth = std::max(contentWidth, static_cast<float>(across) * (widest + gap) - gap);
                for (size_t i = 0; i < count; ++i)
                {
                    const PopupTab& tab   = section.tabs[i];
                    const TextSize size   = CellSize(Cell(tab.name, tab.icon), unit, measure);
                    const TextSize framed = Framed(size, unit);
                    const float top       = y + static_cast<float>(i / across) * (tall + gap);
                    const float inside    = top + framed.inkTop - size.inkTop; // where its icon and name start
                    b.chips.push_back(Block::Chip{std::string(CardKey(section.heading)), tab.name.text, b.pieces.size(), i % across, across,
                        std::min(across, count - i / across * across), top, tall, size.width, gap});
                    b.chips.back().fleeting = section.tabEachOpen;
                    PopupPiece chip{PopupPiece::Kind::Tab, 0.0f, top, widest, tall, {}};
                    chip.open = i == picked;
                    b.pieces.push_back(std::move(chip));
                    // Each after the frame placed from where the chip's content starts; spread, the chip adds where.
                    float x = 0.0f;
                    if (tab.icon)
                    {
                        const float s = unit * kCellIcon;
                        b.pieces.push_back(PopupPiece{PopupPiece::Kind::Icon, x, inside, s, s, {}, PopupFont::Text, *tab.icon});
                        x += s + unit * kIconGap;
                        b.chips.back().parts = 2;
                    }
                    const TextSize name = measure(tab.name, PopupFont::Text);
                    b.pieces.push_back(PopupPiece{PopupPiece::Kind::Text, x, inside, name.width, name.height, tab.name});
                }
                y += static_cast<float>(lines) * (tall + gap) - gap + metrics.rowGap;
            }
            // Grid rows one after another with as many columns line them up: a grid carried on across rows, or cards.
            std::vector<std::vector<float>> runColumns(rows.size());
            for (size_t first = 0, last = 0; first < rows.size(); first = std::max(last, first + 1))
            {
                const PopupRow& head = rows[first];
                last = first;
                if (head.kind != RowKind::Grid || head.columns <= 0) continue;
                std::vector<float> widths(static_cast<size_t>(head.columns), 0.0f);
                for (; last < rows.size() && rows[last].kind == RowKind::Grid && rows[last].columns == head.columns; ++last)
                    for (size_t i = 0; i < rows[last].cells.size(); ++i)
                        widths[i % widths.size()] =
                            std::max(widths[i % widths.size()], CellSize(rows[last].cells[i], unit, measure).width);
                for (size_t r = first; r < last; ++r)
                    runColumns[r] = widths;
            }
            std::vector<TextSize> cells;
            std::vector<float> columns;
            const float socketPad = std::round(unit * kSocketPad), inset = std::round(unit * kSocketInset);
            // A row's lines that scroll: a grid's but for its heads, which stay; any other row is one.
            const auto linesOf = [](const PopupRow& row) -> size_t {
                if (row.kind != RowKind::Grid || row.columns <= 0) return 1;
                const auto columns = static_cast<size_t>(row.columns);
                const size_t head  = row.header ? columns : 0;
                return row.cells.size() > head ? (row.cells.size() - head + columns - 1) / columns : 0;
            };
            // The open socket, for the nested rows now being laid out: which of its lines show, and where it scrolls.
            struct Socket
            {
                size_t piece;
                std::string key;
                size_t lines = 0, shown = 0, from = 0, at = 0; // its lines, how many show, the first shown, the next
                float top  = 0.0f;                             // its first row's
                size_t bar = 0;                                // its scrollbar's track, then its thumb, in the pieces
            };
            std::optional<Socket> socket;
            const auto closeSocket = [&] {
                if (!socket) return;
                if (socket->shown < socket->lines)
                {
                    // A scrollbar down the right padding beside its rows, its thumb the share of them shown.
                    const float bar   = std::max(1.0f, std::round(unit * kScrollbar));
                    const float track = y - metrics.rowGap - socket->top;
                    const float thumb = track * static_cast<float>(socket->shown) / static_cast<float>(socket->lines);
                    const float down  = static_cast<float>(socket->from) / static_cast<float>(socket->lines - socket->shown);
                    const float right = b.pad + (inset + bar) / 2.0f;
                    socket->bar       = b.pieces.size();
                    b.anchors.push_back(Block::Anchor{b.pieces.size(), right});
                    b.pieces.push_back(PopupPiece{PopupPiece::Kind::ScrollTrack, 0.0f, socket->top, bar, track, {}});
                    b.anchors.push_back(Block::Anchor{b.pieces.size(), right});
                    b.pieces.push_back(
                        PopupPiece{PopupPiece::Kind::ScrollThumb, 0.0f, socket->top + (track - thumb) * down, bar, thumb, {}});
                }
                PopupPiece& ground = b.pieces[socket->piece];
                ground.height      = y - metrics.rowGap + socketPad - ground.y;
                y                  = ground.y + ground.height + metrics.rowGap;
                b.sockets.push_back(socket->piece);
                if (socket->shown < socket->lines)
                {
                    const PopupPiece& track = b.pieces[socket->bar], &thumb = b.pieces[socket->bar + 1];
                    b.scrolls.push_back(Block::Scroll{socket->key, ground.y, ground.height, socket->lines - socket->shown,
                        track.y, track.height, thumb.height, inset});
                }
                socket.reset();
            };
            // Whether a socket's line shows, taking the next of its lines; heads always show.
            const auto showLine = [&](const PopupRow& row, bool heads) {
                if (!socket || !row.nested || heads) return true;
                const size_t line = socket->at++;
                return line >= socket->from && line < socket->from + socket->shown;
            };
            for (size_t r = 0; r < rows.size(); ++r)
            {
                const PopupRow& row = rows[r];
                if (!row.nested) closeSocket();
                if (row.cells.empty()) continue;
                const bool grid = row.kind == RowKind::Grid && row.columns > 0;
                const bool tags = row.kind == RowKind::Tags;
                const bool pair = row.kind == RowKind::Pair && row.cells.size() == 2;
                const float gap = unit * (tags ? kTagGap : pair ? kPairGap : kCellGap);
                // A socket's rows as far in from its right as from its left.
                const float indent = row.nested || row.inset ? inset : 0.0f;
                cells.clear();
                columns = grid ? runColumns[r] : std::vector<float>{};
                float rowWidth = 2.0f * indent;
                for (size_t i = 0; i < row.cells.size(); ++i)
                {
                    const TextSize s = cells.emplace_back(CellSize(row.cells[i], unit, measure));
                    if (!grid) rowWidth += (tags ? Framed(s, unit) : s).width + (i > 0 ? gap : 0.0f);
                }
                const float columnGap = unit * (row.roomy ? kRoomyColumnGap : kColumnGap);
                // Two columns of nothing but icons sit as close as an icon to its text.
                const auto iconsOnly = [&](size_t column) {
                    for (size_t i = column; i < row.cells.size(); i += columns.size())
                        if (!row.cells[i].icon || !row.cells[i].text.empty() || row.cells[i].star) return false;
                    return true;
                };
                std::vector<float> gaps(columns.size(), columnGap);
                for (size_t c = 0; c + 1 < columns.size(); ++c)
                    if (iconsOnly(c) && iconsOnly(c + 1)) gaps[c] = unit * kIconGap;
                if (grid && row.spread && columns.size() % 2 == 1 && row.cells.size() > columns.size() / 2 &&
                    row.cells[columns.size() / 2].align == Align::Center)
                {
                    // The middle column just right of the card's middle: room for the wider side on either half.
                    const size_t middle = columns.size() / 2;
                    float sides[2]      = {0.0f, 0.0f};
                    for (size_t c = 0; c < columns.size(); ++c)
                        if (c != middle) sides[c > middle] += columns[c] + columnGap;
                    const float right = unit * kMiddleRight;
                    rowWidth += 2.0f * std::max(sides[0] - right, sides[1] + right) + columns[middle];
                }
                else
                {
                    for (size_t c = 0; c < columns.size(); ++c)
                        rowWidth += columns[c] + (c + 1 < columns.size() ? gaps[c] : 0.0f);
                }
                contentWidth = std::max(contentWidth, rowWidth);
                if (row.nested && !socket)
                {
                    socket = Socket{b.pieces.size(), row.scroll};
                    for (size_t n = r; n < rows.size() && rows[n].nested; ++n)
                        socket->lines += linesOf(rows[n]);
                    socket->shown = std::min(socket->lines, kSocketLines);
                    socket->from  = std::min(row.scrolled, socket->lines - socket->shown);
                    b.pieces.push_back(PopupPiece{PopupPiece::Kind::Socket, b.pad, y, 0.0f, 0.0f, {}});
                    y += socketPad;
                    socket->top = y;
                }

                const size_t perLine = grid ? columns.size() : row.cells.size();
                // A line holding a game picture (a nation's crest) has its texts' ink and its stars centered on it.
                const auto centered = [&](size_t from) -> std::optional<float> {
                    for (size_t c = from; c < std::min(from + perLine, row.cells.size()); ++c)
                        if (row.cells[c].picture != GamePicture::None && !row.cells[c].text.empty())
                        {
                            const TextSize t = measure(row.cells[c].text.front(), PopupFont::Text);
                            return unit * kCellIcon / 2.0f - (t.inkTop + t.inkBottom) / 2.0f;
                        }
                    return std::nullopt;
                };
                float lineHeight = 0.0f, x = b.pad + indent;
                bool shown       = showLine(row, grid && row.header);
                std::optional<float> drop = centered(0);
                for (size_t i = 0; i < row.cells.size(); ++i)
                {
                    if (i > 0 && i % perLine == 0)
                    {
                        if (shown) y += lineHeight + metrics.rowGap;
                        lineHeight = 0.0f;
                        x          = b.pad + indent;
                        shown      = showLine(row, false);
                        drop       = centered(i);
                    }
                    if (!shown) continue;
                    const PopupCell& cell = row.cells[i];
                    TextSize size         = cells[i];
                    float cx = x, cy = y;
                    if (grid && cell.align != Align::Left)
                        cx += (columns[i % columns.size()] - size.width) * (cell.align == Align::Center ? 0.5f : 1.0f);
                    const size_t first = b.pieces.size();
                    const float start  = cx;
                    if (tags)
                    {
                        // The cell's ink centered in its frame: the text's line box can reach past it.
                        const TextSize framed = Framed(size, unit);
                        b.pieces.push_back(PopupPiece{PopupPiece::Kind::Tag, x, y, framed.width, framed.height, {}});
                        cx += TagPadX(unit);
                        cy += framed.inkTop - size.inkTop;
                        size = framed;
                    }
                    if (cell.item != 0)
                    {
                        const float s = unit * kCellIcon;
                        PopupPiece picture{PopupPiece::Kind::Item, cx, cy, s, s, {}};
                        picture.item = cell.item;
                        b.pieces.push_back(std::move(picture));
                        cx += s + (cell.text.empty() ? 0.0f : unit * kIconGap);
                    }
                    else if (cell.picture != GamePicture::None)
                    {
                        const float s = unit * kCellIcon;
                        PopupPiece picture{PopupPiece::Kind::Picture, cx, cy, s, s, {}};
                        picture.picture = cell.picture;
                        b.pieces.push_back(std::move(picture));
                        cx += s + (cell.text.empty() ? 0.0f : unit * kIconGap);
                    }
                    else if (cell.icon)
                    {
                        const float s = unit * kCellIcon, mark = unit * kMark;
                        b.pieces.push_back(PopupPiece{PopupPiece::Kind::Icon, cx, cy, s, s, {}, PopupFont::Text, *cell.icon});
                        if (cell.tint)
                        {
                            b.pieces.back().tinted = true;
                            b.pieces.back().span   = *cell.tint;
                        }
                        if (cell.immune)
                            b.pieces.push_back(PopupPiece{PopupPiece::Kind::ImmuneMark, cx + s - mark, cy + s - mark, mark,
                                mark, {kTimesSign, Tone::Immune, 0}});
                        cx += s + (cell.text.empty() ? 0.0f : unit * kIconGap);
                    }
                    for (const Span& text : cell.text)
                    {
                        const TextSize t = measure(text, PopupFont::Text);
                        const float ty   = cy + drop.value_or(0.0f);
                        b.pieces.push_back(PopupPiece{PopupPiece::Kind::Text, cx, ty, t.width, t.height, text});
                        lineHeight = std::max(lineHeight, ty - cy + t.height);
                        cx += t.width;
                    }
                    if (cell.star)
                    {
                        const float s = unit * kStar;
                        cx += unit * kIconGap;
                        const float sy = drop ? cy + (unit * kCellIcon - s) / 2.0f : cy + cells[i].inkTop;
                        b.pieces.push_back(PopupPiece{PopupPiece::Kind::Star, cx, sy, s, s, {}});
                        cx += s;
                    }
                    if (!cell.press.empty()) b.presses.push_back(
                            Block::Press{cell.press, x, y, grid ? columns[i % columns.size()] : cells[i].width, cells[i].height});
                    const bool spread = grid && row.spread && cell.align != Align::Left;
                    if ((pair && i == 1) || (spread && cell.align == Align::Right))
                    {
                        b.values.push_back(Block::Value{first, b.pieces.size(), cx, indent});
                        for (size_t v = first; v < b.pieces.size(); ++v)
                            b.pieces[v].right = b.pieces[v].kind == PopupPiece::Kind::Text;
                    }
                    else if (spread)
                        b.values.push_back(Block::Value{first, b.pieces.size(), (start + cx) / 2.0f - unit * kMiddleRight, 0.0f, true});
                    lineHeight = std::max(lineHeight, size.height);
                    x += grid ? columns[i % columns.size()] + gaps[i % columns.size()] : size.width + gap;
                }
                if (shown) y += lineHeight + metrics.rowGap;
            }
            closeSocket();
            b.width  = contentWidth + 2.0f * b.pad;
            b.height = std::max(y - metrics.rowGap, b.pad) + b.pad;
            return b;
        }

        // Moves a block's pieces to x, y and its pair values to its right edge at width, or its middle; a card's ground goes
        // first.
        void Place(Block& b, float x, float y, float width, size_t line, PopupLayout& l)
        {
            for (const Block::Value& v : b.values)
                for (size_t i = v.first; i < v.last; ++i)
                    b.pieces[i].x += (v.middle ? width / 2.0f : width - b.pad - v.inset) - v.right;
            for (const size_t socket : b.sockets)
                b.pieces[socket].width = width - 2.0f * b.pad;
            for (const Block::Anchor& anchor : b.anchors)
                b.pieces[anchor.piece].x = width - anchor.fromRight;
            // Each tab as wide as its share of the card's inside, its name in its middle; a short row in the middle.
            for (Block::Chip& chip : b.chips)
            {
                const float across = static_cast<float>(chip.across);
                chip.width = (width - 2.0f * b.pad - (across - 1.0f) * chip.gap) / across;
                const float step = chip.width + chip.gap;
                chip.left  = b.pad + static_cast<float>(chip.across - chip.inRow) * step / 2.0f +
                            static_cast<float>(chip.column) * step;
                b.pieces[chip.piece].x     = chip.left;
                b.pieces[chip.piece].width = chip.width;
                for (size_t part = 1; part <= chip.parts; ++part)
                    b.pieces[chip.piece + part].x += chip.left + (chip.width - chip.name) / 2.0f;
            }
            if (b.card) l.pieces.push_back(PopupPiece{PopupPiece::Kind::Card, x, y, width, b.height, {}, PopupFont::Text,
                            Icon::AggroNQ, line});
            for (PopupPiece& piece : b.pieces)
            {
                piece.x += x;
                piece.y += y;
                piece.line = line;
                l.pieces.push_back(std::move(piece));
            }
            for (Block::Chip& chip : b.chips)
                l.chips.push_back(PopupTabChip{std::move(chip.key), x + chip.left, y + chip.y, chip.width, chip.height, line,
                    std::move(chip.tab), false, chip.fleeting});
            for (Block::Press& press : b.presses)
                if (press.fromRight > 0.0f) press.x = width - press.fromRight;
            for (Block::Press& press : b.presses)
                l.chips.push_back(PopupTabChip{{}, x + press.x, y + press.y, press.width, press.height, line,
                    std::move(press.tells), true});
            for (Block::Scroll& scroll : b.scrolls)
                l.scrolls.push_back(PopupScroll{std::move(scroll.key), x + b.pad, y + scroll.y, width - 2.0f * b.pad,
                    scroll.height, line, scroll.most,
                    BarRect{x + width - b.pad - scroll.grip, y + scroll.trackY, scroll.grip, scroll.track}, scroll.thumb});
            l.lines.push_back(PopupLine{y + b.height});
        }
        // A card as LayoutSection lays it out, as wide as with its widest tab picked, so its chips wrap the same
        // whichever is, and with its reserve: its height with its tallest tab picked. Collapsed, it keeps its open width.
        Block LayoutCard(const PopupSection& section, const BarMetrics& metrics, bool ornate, SpanMeasure measure)
        {
            Block b     = LayoutSection(section, metrics, ornate, measure);
            b.beside    = section.beside;
            b.under     = section.under;
            b.collapsed = section.collapsed && !section.heading.empty();
            PopupSection each = section;
            each.collapsed    = false;
            for (size_t tab = 0; tab < std::max<size_t>(section.tabs.size(), 1); ++tab)
            {
                each.tab           = tab;
                const Block picked = LayoutSection(each, metrics, ornate, measure);
                b.reserve          = b.collapsed ? b.height : std::max(b.reserve, picked.height);
                b.width            = std::max(b.width, picked.width);
            }
            return b;
        }

        // A grid's lines in groups side by side, down each group before the next, with an empty column between groups;
        // a header line heads each group.
        PopupRow SideBySide(const PopupRow& row, size_t groups)
        {
            const auto columns  = static_cast<size_t>(row.columns);
            const size_t head   = row.header ? columns : 0;
            const size_t lines  = (row.cells.size() - head + columns - 1) / columns;
            const size_t down   = (lines + groups - 1) / groups;
            const size_t across = (lines + down - 1) / down;
            const auto cellOf   = [&](size_t index) { return index < row.cells.size() ? row.cells[index] : PopupCell{}; };
            PopupRow out = row;
            out.cells.clear();
            out.columns = static_cast<int>(columns * across + across - 1);
            // One line across the groups: each group's cells from first, or empty past the end, an empty one between.
            const auto addLine = [&](auto first) {
                for (size_t group = 0; group < across; ++group)
                {
                    const std::optional<size_t> from = first(group);
                    for (size_t at = 0; at < columns; ++at)
                        out.cells.push_back(from ? cellOf(*from + at) : PopupCell{});
                    if (group + 1 < across) out.cells.emplace_back();
                }
            };
            if (row.header) addLine([](size_t) { return std::optional<size_t>(0); });
            for (size_t step = 0; step < down; ++step)
                addLine([&](size_t group) {
                    const size_t line = group * down + step;
                    return line < lines ? std::optional<size_t>(head + line * columns) : std::nullopt;
                });
            return out;
        }

        // A card that would run taller than tall: its long grids' lines set side by side in it, as few groups as fit;
        // failing that, cut into parts that fit between its rows, a grid's lines counting as rows, each part keeping
        // the heading. A row taller than tall alone is a part of its own.
        std::vector<PopupSection> SplitToFit(const PopupSection& section, float tall, const BarMetrics& metrics, bool ornate,
                                         SpanMeasure measure)
        {
            // As tall as it can be: with its tallest tab picked, so picking another never sets it differently.
            const auto tallest = [&](PopupSection variant) {
                float most = 0.0f;
                for (size_t tab = 0; tab < std::max<size_t>(variant.tabs.size(), 1); ++tab)
                {
                    variant.tab = tab;
                    most        = std::max(most, LayoutSection(variant, metrics, ornate, measure).height);
                }
                return most;
            };
            if (tallest(section) <= tall) return ListOf(section);
            PopupSection wide = section;
            for (size_t groups = 2; groups <= kMostSideBySide; ++groups)
            {
                wide     = section;
                bool set = false;
                const auto widen = [&](std::vector<PopupRow>& rows) {
                    for (PopupRow& row : rows)
                    {
                        const auto columns = static_cast<size_t>(std::max(row.columns, 0));
                        if (row.kind != RowKind::Grid || columns == 0 ||
                            row.cells.size() <= (kSideBySideAbove + (row.header ? 1 : 0)) * columns)
                            continue;
                        row = SideBySide(row, groups);
                        set = true;
                    }
                };
                widen(wide.rows);
                for (PopupTab& tab : wide.tabs)
                    widen(tab.rows);
                if (!set) break;
                if (tallest(wide) <= tall) return ListOf(wide);
            }
            // A card of tabs stays whole, as wide as it can be set, for the cut to take what is past the room.
            if (!section.tabs.empty()) return ListOf(wide);
            std::vector<PopupRow> rows;
            for (const PopupRow& row : section.rows)
            {
                const auto columns = static_cast<size_t>(std::max(row.columns, 0));
                if (row.kind != RowKind::Grid || columns == 0 || row.cells.size() <= columns)
                {
                    rows.push_back(row);
                    continue;
                }
                for (size_t i = 0; i < row.cells.size(); i += columns)
                {
                    PopupRow line = row;
                    line.cells.assign(row.cells.begin() + static_cast<ptrdiff_t>(i),
                        row.cells.begin() + static_cast<ptrdiff_t>(std::min(i + columns, row.cells.size())));
                    rows.push_back(std::move(line));
                }
            }
            // A card's height is its empty card's and each row's, a gap before each.
            const float empty = LayoutSection(PopupSection{section.heading, {}}, metrics, ornate, measure).height;
            std::vector<PopupSection> parts{PopupSection{section.heading, {}}};
            float height = empty;
            for (PopupRow& row : rows)
            {
                const float more = metrics.rowGap + LayoutSection(PopupSection{"", ListOf(row)}, metrics, ornate, measure).height;
                if (!parts.back().rows.empty() && height + more > tall)
                {
                    parts.push_back(PopupSection{section.heading, {}});
                    height = empty;
                }
                parts.back().rows.push_back(std::move(row));
                height += more;
            }
            return parts;
        }
    }

    namespace
    {
        // A round chip on the title row and how many pieces after it are on it; what it tells when pressed, if anything.
        struct TitleChip
        {
            size_t piece;
            std::string press{};
        };
        // The title row's chips, the pin's first, from the right edge once the popup's width is known, a cell's gap
        // apart, each with the picture on it.
        void PlaceChips(PopupLayout& l, const std::vector<TitleChip>& chips, float pad, float unit)
        {
            float right = l.width - pad;
            for (const TitleChip& at : chips)
            {
                PopupPiece& chip = l.pieces[at.piece];
                chip.x           = right - chip.width;
                l.pieces[at.piece + 1].x += chip.x;
                right            = chip.x - unit * kCellGap;
                const BarRect rect{chip.x, chip.y, chip.width, chip.height};
                if (chip.kind == PopupPiece::Kind::PinChip) l.pin = rect;
                else if (chip.kind == PopupPiece::Kind::CogChip) l.cog = rect;
                else l.chips.push_back(PopupTabChip{{}, rect.x, rect.y, rect.width, rect.height, 0, at.press, true});
            }
        }
    }

    PopupLayout LayoutPopup(const PopupContent& content, const BarMetrics& metrics, BarShape shape, bool pinned,
                            SpanMeasure measure, float room, float least, std::optional<SettingsRoom> settings)
    {
        const float unit = metrics.popupText, pad = metrics.popupPadding, cardGap = std::round(unit * kCardGap);
        const bool ornate = shape == BarShape::Ornate;
        PopupLayout l;
        // The title row: its icon, its text and the pin, which goes to the right edge once it is known.
        float x = pad, rowHeight = 0.0f;
        if (content.icon)
        {
            l.pieces.push_back(PopupPiece{PopupPiece::Kind::Icon, x, pad, unit * kTitleIcon, unit * kTitleIcon, {},
                PopupFont::Text, *content.icon});
            x += unit * kTitleIcon + unit * kIconGap;
            rowHeight = unit * kTitleIcon;
        }
        for (const Span& s : content.title)
        {
            const TextSize t = measure(s, PopupFont::Title);
            l.pieces.push_back(PopupPiece{PopupPiece::Kind::Text, x, pad, t.width, t.height, s, PopupFont::Title});
            x += t.width;
            rowHeight = std::max(rowHeight, t.height);
        }
        float titleWidth = x - pad;
        // The pin: a round chip at the title row's right edge with the pin alone in it, grayed until pinned. The
        // settings chip is a round one left of it, and while the settings show, the clone and remove chips left of that.
        std::vector<TitleChip> chips;
        if (!content.title.empty() || content.icon || !content.sections.empty() || settings)
        {
            const float side = std::round(unit * kPinChip), mark = std::round(side * kMarkShare);
            chips.push_back(TitleChip{l.pieces.size()});
            PopupPiece chip{PopupPiece::Kind::PinChip, 0.0f, pad, side, side, {}};
            chip.open = pinned;
            chip.font = PopupFont::Pin;
            l.pieces.push_back(std::move(chip));
            PopupPiece picture{PopupPiece::Kind::Icon, (side - mark) / 2.0f, pad + (side - mark) / 2.0f, mark, mark,
                Span{"", pinned ? Tone::Heading : Tone::Dim, 0}, PopupFont::Pin};
            picture.icon   = Icon::Pinned;
            picture.tinted = true;
            l.pieces.push_back(std::move(picture));
            titleWidth += unit * kCellGap + side;
            rowHeight = std::max(rowHeight, side);
        }
        if (!chips.empty() && content.settings)
        {
            const float side = l.pieces[chips.front().piece].height;
            const float mark = std::round(side * kMarkShare);
            chips.push_back(TitleChip{l.pieces.size()});
            PopupPiece cog{PopupPiece::Kind::CogChip, 0.0f, pad, side, side, {}};
            cog.open = settings.has_value();
            cog.font = PopupFont::Pin;
            l.pieces.push_back(std::move(cog));
            l.pieces.push_back(PopupPiece{PopupPiece::Kind::Cog, (side - mark) / 2.0f, pad + (side - mark) / 2.0f, mark, mark, {},
                PopupFont::Pin});
            titleWidth += unit * kCellGap + side;
            for (const auto& [press, icon] : {std::pair{&content.resetPress, Icon::Reset}, std::pair{&content.clonePress, Icon::Clone},
                     std::pair{&content.removePress, Icon::Delete}})
            {
                // The reset while it is pinned, where it can be dragged; the others while its settings show.
                if (press->empty() || (press == &content.resetPress ? !pinned : !settings)) continue;
                chips.push_back(TitleChip{l.pieces.size(), *press});
                PopupPiece chip{PopupPiece::Kind::ActionChip, 0.0f, pad, side, side, {}};
                chip.font = PopupFont::Pin;
                l.pieces.push_back(std::move(chip));
                PopupPiece picture{PopupPiece::Kind::Icon, (side - mark) / 2.0f, pad + (side - mark) / 2.0f, mark, mark,
                    Span{"", Tone::Heading, 0}, PopupFont::Pin};
                picture.icon   = icon;
                picture.tinted = true;
                l.pieces.push_back(std::move(picture));
                titleWidth += unit * kCellGap + side;
            }
        }
        l.lines.push_back(PopupLine{pad + rowHeight});
        l.titleRow = pad + rowHeight;
        float top = pad + rowHeight + metrics.rowGap;
        if (!content.subtitle.empty())
        {
            float sx = pad, subHeight = 0.0f;
            for (const Span& s : content.subtitle)
            {
                const TextSize t = measure(s, PopupFont::Text);
                l.pieces.push_back(PopupPiece{PopupPiece::Kind::Text, sx, top, t.width, t.height, s, PopupFont::Text});
                sx += t.width;
                subHeight = std::max(subHeight, t.height);
            }
            titleWidth = std::max(titleWidth, sx - pad);
            top += subHeight + metrics.rowGap;
        }

        float width = std::max({unit * kMinWidth, titleWidth, least - 2.0f * pad});
        if (settings)
        {
            width        = std::max(width, settings->width);
            const float tall = std::clamp(room - top - pad, 0.0f, settings->height);
            l.body       = BarRect{pad, top, width, tall};
            l.width      = width + 2.0f * pad;
            l.height     = (tall > 0.0f ? top + tall : l.lines[0].bottom) + pad;
            PlaceChips(l, chips, pad, unit);
            return l;
        }
        // The cards under the title. The most a column of them may run, with the padding under it: a card taller goes
        // on in another column.
        const float tall = room - top - pad;
        std::vector<Block> cards;
        for (const PopupSection& section : content.sections)
        {
            // A card of nothing would be an empty bubble.
            if (section.heading.empty() && section.tabs.empty() &&
                std::ranges::all_of(section.rows, [](const PopupRow& row) { return row.cells.empty(); }))
                continue;
            for (const PopupSection& part : SplitToFit(section, tall, metrics, ornate, measure))
                cards.push_back(LayoutCard(part, metrics, ornate, measure));
        }

        float lowest = top;
        if (std::ranges::any_of(cards, [](const Block& b) { return b.beside; }))
        {
            // In rows: a card beside the one before shares its row, the first at its own width and the second, with the
            // cards under it in its column, taking the rest. The row is as tall as the taller side, each card counted at
            // its tallest tab so a tab picked moves nothing, the last card of the column taking what is left of it; a card
            // alone fills the width.
            const auto tallest = [](const Block& b) { return std::max(b.reserve, b.height); };
            const auto columnEnd = [&](size_t i) { // one past the last card of the column beside card i
                size_t end = i + 2;
                while (end < cards.size() && cards[end].under) ++end;
                return end;
            };
            float whole = 0.0f;
            for (size_t i = 0; i < cards.size(); ++i)
                if (i + 1 < cards.size() && cards[i + 1].beside)
                {
                    const size_t end = columnEnd(i);
                    float column     = 0.0f;
                    for (size_t c = i + 1; c < end; ++c)
                        column = std::max(column, cards[c].width);
                    whole = std::max(whole, cards[i].width + cardGap + column);
                    i     = end - 1;
                }
                else
                    whole = std::max(whole, cards[i].width);
            width = std::max(width, whole);
            float y = top;
            for (size_t i = 0; i < cards.size(); ++i)
            {
                if (i + 1 < cards.size() && cards[i + 1].beside)
                {
                    const size_t end = columnEnd(i);
                    float column     = -cardGap;
                    for (size_t c = i + 1; c < end; ++c)
                        column += tallest(cards[c]) + cardGap;
                    const float tall  = std::max(tallest(cards[i]), column);
                    const float first = cards[i].width, rest = width - first - cardGap;
                    if (!cards[i].collapsed) cards[i].height = tall;
                    Place(cards[i], pad, y, first, l.lines.size(), l);
                    float at = y;
                    for (size_t c = i + 1; c < end; ++c)
                    {
                        cards[c].height = c + 1 == end && !cards[c].collapsed ? y + tall - at : tallest(cards[c]);
                        Place(cards[c], pad + first + cardGap, at, rest, l.lines.size(), l);
                        at += cards[c].height + cardGap;
                    }
                    y += tall + cardGap;
                    i = end - 1;
                    continue;
                }
                Place(cards[i], pad, y, width, l.lines.size(), l);
                y += cards[i].height + cardGap;
            }
            lowest = y;
        }
        else
        {
            // One column, or two when one runs tall: the split whose taller column is shortest, in their order. When that
            // runs past the room, as many columns as fill it in order. Each card counts as its reserve, so a tab picked
            // makes it grow down where it is. A popup that asks for one column keeps it.
            float stacked = 0.0f;
            for (const Block& b : cards)
                stacked += b.reserve + cardGap;
            std::vector<size_t> starts{0}; // each column's first card
            if (!content.oneColumn && cards.size() >= 2 && stacked > unit * kTwoColumnsAbove)
            {
                float best = stacked, left = 0.0f;
                size_t split = cards.size();
                for (size_t i = 1; i < cards.size(); ++i)
                {
                    left += cards[i - 1].reserve + cardGap;
                    if (const float taller = std::max(left, stacked - left); taller < best)
                    {
                        best  = taller;
                        split = i;
                    }
                }
                if (split < cards.size()) starts.push_back(split);
                if (best - cardGap > tall)
                {
                    starts = {0};
                    float column = 0.0f;
                    for (size_t i = 0; i < cards.size(); ++i)
                    {
                        if (i > starts.back() && column + cards[i].reserve > tall)
                        {
                            starts.push_back(i);
                            column = 0.0f;
                        }
                        column += cards[i].reserve + cardGap;
                    }
                }
            }
            const auto columnOf = [&](size_t card) {
                return static_cast<size_t>(std::upper_bound(starts.begin(), starts.end(), card) - starts.begin() - 1);
            };
            std::vector<float> columnWidths(starts.size(), 0.0f);
            for (size_t i = 0; i < cards.size(); ++i)
                columnWidths[columnOf(i)] = std::max(columnWidths[columnOf(i)], cards[i].width);
            float all = cardGap * static_cast<float>(columnWidths.size() - 1);
            for (const float w : columnWidths)
                all += w;
            if (all < width) columnWidths.front() += width - all; // a wide title widens the first column
            width = std::max(width, all);

            std::vector<float> columnX(starts.size(), pad), columnY(starts.size(), top);
            for (size_t c = 1; c < starts.size(); ++c)
                columnX[c] = columnX[c - 1] + columnWidths[c - 1] + cardGap;
            for (size_t i = 0; i < cards.size(); ++i)
            {
                const size_t c = columnOf(i);
                Place(cards[i], columnX[c], columnY[c], columnWidths[c], l.lines.size(), l);
                columnY[c] += cards[i].height + cardGap;
            }
            lowest = *std::max_element(columnY.begin(), columnY.end());
        }
        const float bottom = std::max(cards.empty() ? top - metrics.rowGap : lowest - cardGap, l.lines[0].bottom);
        l.width            = width + 2.0f * pad;
        l.height           = bottom + pad;
        PlaceChips(l, chips, pad, unit);
        return l;
    }

    void CutPopup(PopupLayout& layout, float height, float padding)
    {
        if (layout.height <= height) return;
        std::vector<bool> kept(layout.lines.size());
        float bottom = padding;
        for (size_t i = 0; i < layout.lines.size(); ++i)
        {
            kept[i] = i == 0 || layout.lines[i].bottom <= height - padding;
            if (kept[i]) bottom = std::max(bottom, layout.lines[i].bottom);
        }
        std::erase_if(layout.pieces, [&](const PopupPiece& piece) { return !kept[piece.line]; });
        std::erase_if(layout.chips, [&](const PopupTabChip& chip) { return !kept[chip.line]; });
        std::erase_if(layout.scrolls, [&](const PopupScroll& scroll) { return !kept[scroll.line]; });
        layout.height = bottom + padding;
    }
}
