#include "bar_document.h"
#include "window_skin.h"
#include "test.h"

#include <algorithm>
#include <cmath>
#include <iterator>
#include <string>
#include <tuple>

using namespace headsup;

namespace
{
    BarLook LookOf(const BarPalette& p, const Settings& settings)
    {
        const BarMetrics m  = MetricsFor(45, p.shape);
        return BarLook{p, settings, m, PillRow(m), TextRow{13, 11, 10.0f, 24.0f, 13, 12.0f},
            BarTitleFont(settings, p.shape), p.shape == BarShape::Ornate, BarTextFont(settings), std::nullopt};
    }
    BarLook LookOf(BarTheme theme, const Settings& settings) { return LookOf(PaletteOf(theme), settings); }

    // Line boxes 16 and 14 px tall, the title's top at row 9 over a detail at row 23, or alone at row 14.
    BarTextPlace Place()
    {
        return BarTextPlace{BarLinePlace{16, 9.0f, 2.0f}, BarLinePlace{17, 8.0f, 1.0f}, BarLinePlace{16, 14.0f, 2.0f},
            BarLinePlace{17, 13.0f, 1.0f}, BarLinePlace{14, 23.0f, 3.0f},
            BarLinePlace{16, 0.0f, 0.0f}, BarLinePlace{17, 0.0f, 0.0f}};
    }

    bool Has(const std::string& text, std::string_view part) { return text.find(part) != std::string::npos; }
}

TEST(markup_text_is_escaped)
{
    CHECK_EQ(EscapedRml("Fish & <Chips> \"here\""), "Fish &amp; &lt;Chips&gt; &quot;here&quot;");
    CHECK_EQ(EscapedRml("plain"), "plain");
}

TEST(colors_are_written_with_their_alpha_last)
{
    CHECK_EQ(RcssColor(0xFFC9A24Au), "#c9a24aff");
    CHECK_EQ(RcssColor(0x1AC9A24Au), "#c9a24a1a");
    CHECK_EQ(RcssColor(0x00000000u), "#00000000");
}

TEST(image_names_round_trip_to_their_images)
{
    CHECK_EQ(IconSource(Icon::Link), "hu:icon/4");
    const BarPalette& umbra = PaletteOf(BarTheme::UmbraDark);
    const std::string umbraStrip = "hu:strip/ff2b2b30/ff212125/ff17171a/ff000000/00000000/80000000/1/45/13";
    CHECK_EQ(StripSource(umbra, 45, 13), umbraStrip);
    CHECK_EQ(SeparatorSource(DashMark::Diamond, 0xFFC9A24Au, 10), "hu:diamond/ffc9a24a/10");
    CHECK_EQ(SeparatorSource(DashMark::None, 0xFFC9A24Au, 10), "hu:dash/ffc9a24a/10");
    const std::optional<Image> icon = HostImage("hu:icon/4");
    CHECK(icon && icon->width == static_cast<int>(IconImage(Icon::Link).width));
    const std::optional<Image> strip = HostImage(umbraStrip);
    CHECK(strip && strip->width == 1 && strip->height == 45 + 13);
    CHECK(strip->argb == StripColumn(45, 13, umbra.stripTop, umbra.stripMiddle, umbra.stripBottom, umbra.bottomLine,
                             umbra.underLine, umbra.below, umbra.border).argb);
    BarPalette flat = umbra, one = umbra; // no gradient: its top all the way down, the other stops kept
    flat.background = BarBackground::Plain;
    one.stripMiddle = one.stripBottom = umbra.stripTop;
    CHECK(StripSource(flat, 45, 13) == StripSource(one, 45, 13) && flat.stripBottom == umbra.stripBottom);
    BarPalette thick = umbra;
    thick.border     = 4;
    const std::optional<Image> bordered = HostImage(StripSource(thick, 45, 13));
    CHECK(bordered && bordered->argb[41] == umbra.bottomLine && bordered->argb[40] != umbra.bottomLine);
    const std::optional<Image> diamond = HostImage("hu:diamond/ffc9a24a/10");
    CHECK(diamond && diamond->width == 10 && diamond->height == 10);
    CHECK(diamond->argb[5 * 10 + 5] == 0xFFC9A24Au); // its middle pixel is solid in its color
    // Each Separator Style draws its own: solid in its middle, and only the dash across the whole width.
    for (const char* kind : {"dot", "square", "dash"})
    {
        const std::optional<Image> mark = HostImage(std::string("hu:") + kind + "/ffc9a24a/10");
        CHECK(mark && mark->width == 10 && mark->argb[5 * 10 + 5] == 0xFFC9A24Au);
        CHECK_EQ(mark->argb[5 * 10] == 0xFFC9A24Au, std::string(kind) == "dash"); // the left edge's middle
        CHECK_EQ(mark->argb[0], 0u);                                             // a corner is clear
    }
    // None draws no mark in the separator's room.
    const std::optional<Image> blank = HostImage(SeparatorSource(DashMark::Blank, 0xFFC9A24Au, 10));
    CHECK(blank && blank->width == 10 && blank->height == 10);
    CHECK(std::ranges::all_of(blank->argb, [](uint32_t pixel) { return pixel == 0u; }));
    CHECK(!HostImage("hu:stars/ffc9a24a/10"));
    CHECK(!HostImage("hu:coin/17")); // gil is XIUI's picture, no longer a painted coin
    CHECK(!HostImage("hu:icon/999") && !HostImage("hu:nothing") && !HostImage("icon.png") && !HostImage("hu:strip/9/45/13"));
    CHECK(!HostImage("hu:strip/ff2b2b30/ff212125/zz17171a/ff000000/00000000/80000000/1/45/13"));
    // The reset arrow, in the color asked for: the bottom of its ring is that color.
    const std::optional<Image> reset = HostImage(ResetSource(0xFF8C7270u, 32));
    CHECK(reset && reset->width == 32 && reset->height == 32);
    CHECK_EQ(reset->argb[(16 + 10) * 32 + 16], 0xFF8C7270u);
    CHECK(!HostImage("hu:reset/32"));
    // The cog the same way: its ring, under its top tooth, is that color and its middle is clear.
    const std::optional<Image> cog = HostImage(CogSource(0xFF8C7270u, 32));
    CHECK(cog && cog->width == 32 && cog->height == 32);
    CHECK_EQ(cog->argb[(16 - 9) * 32 + 16], 0xFF8C7270u);
    CHECK_EQ(cog->argb[16 * 32 + 16], 0u);
}

TEST(the_document_holds_the_strip_the_caps_the_dividers_and_every_placed_widget)
{
    Settings settings;
    const std::string rml = BarDocument(LookOf(BarTheme::PhoenixGold, settings), Place());
    CHECK(Has(rml, "<rml>") && Has(rml, "</rml>") && Has(rml, "<body class=\"ornate\">"));
    CHECK(Has(rml, StripSource(PaletteOf(BarTheme::PhoenixGold), 45, 13).c_str())); // 45 rows and 0.28 H below
    CHECK(Has(rml, "height:59px")); // those, and the gold theme's line under the strip
    CHECK(Has(rml, "id=\"w-zone\"") && Has(rml, "id=\"w-target\"") && Has(rml, "id=\"w-earthTime\""));
    CHECK(!Has(rml, "id=\"w-gameHelp\"")); // never on the bar
    // A divider between each two of the most widgets there can be: the target, every created widget, and four copies of
    // each of the rest.
    CHECK(Has(rml, "id=\"d-0\"") && Has(rml, ("id=\"d-" + std::to_string(kDividerCount - 1) + "\"").c_str()) &&
          !Has(rml, ("id=\"d-" + std::to_string(kDividerCount) + "\"").c_str()));
    CHECK_EQ(kDividerCount, 1 + kMostCreatedWidgets + (kBarWidgetCount - 3) * kMostCopies - 1); // game help is not on the bar
    CHECK(Has(rml, "id=\"w-gil-2\"") && Has(rml, "id=\"w-gil-4\"") && !Has(rml, "id=\"w-gil-5\""));
    CHECK(!Has(rml, "id=\"w-target-2\"")); // one target only
    CHECK(Has(rml, "class=\"widget hidden target\" id=\"w-target\""));
    CHECK(Has(rml, "font-size:13px") && Has(rml, "font-size:11px"));
    CHECK(Has(rml, "\"Marcellus SC\"") && Has(rml, "\"Barlow Condensed\""));
    CHECK(Has(rml, "hu:diamond/ffc9a24a/14")); // the end caps in the accent: 1.4 diamonds of 0.22 H, whole pixels
    CHECK(Has(rml, "hu:diamond/ff8a6d2c/10")); // the dividers in the divider color
    // The pill row is 36 px from row 4: content starts at row 5, so a title at row 9 sits 4 px under it, alone 9,
    // and the detail at row 23 comes 23 - (9 + 16) = -2 px after the title's line.
    CHECK(Has(rml, ".widget{position:absolute;top:4px;height:36px;"));
    CHECK(Has(rml, "line-height:16px;margin-top:4px;") && Has(rml, "font-size:13px;line-height:16px;margin-top:9px;"));
    CHECK(Has(rml, "line-height:14px;margin-top:-2px;"));
    // After a title of figures, whose line is 17 px from row 8: 23 - 25 again.
    CHECK(Has(rml, ".detail.fig{margin-top:-2px;}"));
    // The target's frame: the accent around the hover tint, 3 px corners.
    CHECK(Has(rml, ".ornate .widget.target{border-width:1px;margin:0px;padding:0px 13.85px;"
                   "background-color:#c9a24a1a;border-color:#c9a24aff;border-radius:3px;}"));
    CHECK(Has(rml, ".ornate .widget.target.lit{background-color:#c9a24a34;")); // twice the tint
}

TEST(a_strip_with_nothing_fading_below_it_is_as_tall_as_its_image)
{
    // A theme copied from Vanilla has the art's palette, with nothing below the strip: on a gradient, its strip
    // element was the glow's rows taller than its image, which stretched the strip down over them.
    Settings settings;
    BarPalette p = PaletteOf(BarTheme::UmbraDark);
    p.below      = 0;
    const std::string rml = BarDocument(LookOf(p, settings), Place());
    const std::string source = StripSource(p, 45, 0);
    CHECK(Has(rml, "height:45px;decorator:image(\"" + source + "\");"));
    const std::optional<Image> strip = HostImage(source);
    CHECK(strip && strip->height == 45);
}

TEST(the_caps_and_dividers_take_the_themes_separator_style)
{
    Settings settings;
    BarPalette dots = PaletteOf(BarTheme::PhoenixGold);
    dots.separator  = DashMark::Dot;
    const std::string rml = BarDocument(LookOf(dots, settings), Place());
    CHECK(Has(rml, "hu:dot/ffc9a24a/14") && Has(rml, "hu:dot/ff8a6d2c/10") && !Has(rml, "hu:diamond/"));
}

TEST(a_pill_theme_frames_each_widget_and_its_caps_follow_the_settings)
{
    Settings settings;
    CHECK(Has(BarDocument(LookOf(BarTheme::UmbraDark, settings), Place()), "cap-right\" src"));
    settings.panelSeparators = SeparatorBit(Separator::Between);
    const std::string rml    = BarDocument(LookOf(BarTheme::UmbraDark, settings), Place());
    CHECK(Has(rml, "<body class=\"pills\">") && !Has(rml, "cap-right\" src"));
    // Without its Show Background a widget has no pill until lit; with it, 0.17 H corners, the pill's fill and edge.
    CHECK(Has(rml, ".pills .widget{background-color:transparent;border-color:transparent;}"));
    CHECK(Has(rml, ".pills .widget.backed{background-color:#222226ff;border-color:#3a3a42ff;border-radius:7.65px;}"));
    CHECK(Has(rml, ".pills .widget.lit{background-color:#2a2a30ff;border-color:#5b8defff;"));
    CHECK(Has(rml, "class=\"widget hidden\" id=\"w-zone\""));
    CHECK(Has(rml, "hu:diamond/ff5b8def/10")); // a pill theme's dividers take the accent
    CHECK(Has(rml, "height:58px")); // no line under this strip
}

TEST(a_widgets_icon_on_the_right_comes_after_its_lines_and_a_picture_is_drawn_as_an_icon)
{
    Settings settings;
    const BarLook look = LookOf(BarTheme::PhoenixGold, settings);
    WidgetContent c;
    c.shown   = true;
    c.picture = GamePicture::BastokCrest;
    c.title   = {Text("Valkurm Dunes")};
    std::string rml = WidgetRml(c, look, {});
    const size_t picture = rml.find("class=\"icon pic\""), text = rml.find("class=\"text\"");
    CHECK(picture != std::string::npos && picture < text);
    c.iconsRight = true;
    rml = WidgetRml(c, look, {});
    CHECK(rml.find("class=\"icon pic right\"") > rml.find("Valkurm Dunes"));
    // Its room counts toward the widget's width either side.
    CHECK(look.Leading(c) > 0.0f);
}

TEST(a_widgets_lines_line_up_with_the_side_of_the_panel_its_section_is_on)
{
    const Settings settings;
    const BarLook look    = LookOf(BarTheme::UmbraDark, settings);
    const std::string css = BarDocument(look, Place());
    const auto rule = [&](BarSection section) {
        return std::string(".widget.") + SectionClass(section) + " .text{text-align:";
    };
    CHECK(Has(css, (rule(BarSection::Left) + "left;").c_str()));
    CHECK(Has(css, (rule(BarSection::Center) + "center;").c_str()));
    CHECK(Has(css, (rule(BarSection::Right) + "right;").c_str()));
    CHECK(std::string(SectionClass(BarSection::Left)) != SectionClass(BarSection::Right));
}

TEST(a_widgets_markup_has_its_leading_icons_lines_and_marks_inside_its_lines_gil_as_xiuis_picture)
{
    Settings settings;
    const BarLook look = LookOf(BarTheme::PhoenixGold, settings);
    const std::string gil = IconSource(Icon::Gil);
    WidgetContent c;
    c.shown    = true;
    c.icons    = {Icon::Sight, Icon::Sound, Icon::Gil};
    Span link  = Text("");
    link.icon  = Icon::Link;
    Span coin  = Text("12");
    coin.coin  = true;
    Span bar   = Text("");
    bar.bar    = InlineBar{0.4f, 90, 6, ""};
    c.title    = {Text("Rolanberry <Fields>"), Colored(" [S]", 0xFF6FE36Fu), link};
    c.subtitle = {Text("Sandy", Tone::Dim), coin, bar};
    const std::string rml = WidgetRml(c, look, {});
    CHECK(Has(rml, "<img class=\"icon\" src=\"hu:icon/5\"/>") && Has(rml, "hu:icon/7"));
    CHECK(Has(rml, "<img class=\"icon\" src=\"" + gil + "\"/>"));
    CHECK(Has(rml, "Rolanberry &lt;Fields&gt;") && Has(rml, "#6fe36fff") && Has(rml, "#efe3c6ff"));
    // The link's icon after the title's text, inside the title's line.
    const size_t title = rml.find("<div class=\"title\">"), detail = rml.find("class=\"detail\"");
    CHECK(title != std::string::npos && detail != std::string::npos);
    const size_t linkAt = rml.find("<img class=\"line-icon\" src=\"hu:icon/4\"/>");
    CHECK(linkAt > rml.find("[S]") && linkAt < detail);
    // Gil's picture before its amount and the bar after it, filled 40%, on the second line.
    const size_t coinAt = rml.find("<img class=\"line-icon\" src=\"" + gil + "\"/>", detail);
    CHECK(coinAt != std::string::npos && coinAt < rml.find(">12<", detail));
    CHECK(rml.find("class=\"ibar\"", detail) > rml.find(">12<", detail) && Has(rml, "width: 40%"));
    CHECK(Has(rml, "#a8936dff") && Has(rml, "Sandy"));
    c.subtitle.clear();
    const std::string bare = WidgetRml(c, look, {});
    CHECK(!Has(bare, "ibar") && !Has(bare, "detail"));
}

TEST(a_widget_of_figures_takes_the_detail_font_for_its_title)
{
    Settings settings;
    const BarLook look = LookOf(BarTheme::PhoenixGold, settings);
    WidgetContent c;
    c.shown   = true;
    c.title   = {Text("21:07")};
    c.figures = true;
    // No second line: the title alone.
    CHECK(Has(WidgetRml(c, look, {}), "class=\"title figures alone\""));
    c.figures = false;
    CHECK(Has(WidgetRml(c, look, {}), "class=\"title alone\""));
    c.figures  = true;
    c.subtitle = {Text("Earth", Tone::Dim)};
    CHECK(Has(WidgetRml(c, look, {}), "class=\"title figures\"") && Has(WidgetRml(c, look, {}), "class=\"detail fig\""));
}

TEST(the_bars_text_carries_its_shadow)
{
    Settings settings;
    const std::string rml = BarDocument(LookOf(BarTheme::PhoenixGold, settings), Place());
    // A 13 px title's shadow blurs 0.12 of its height: a Gaussian of 0.78 px, which RmlUi makes from a width of 2.
    // Once in place and once a pixel down, at 60%.
    CHECK(Has(rml, "font-effect:glow(0px 2px 0px 1px #00000099),glow(0px 2px 0px 0px #00000099);"));
}

TEST(the_help_bar_is_its_box_and_its_three_texts_centered_on_the_row)
{
    Settings settings;
    const BarLook look = LookOf(BarTheme::PhoenixGold, settings);
    HelpBarParts parts;
    parts.row     = HelpBarRow{BarRect{8.0f, 54.0f, 400.0f, 24.0f}, 23.0f, 120.0f, 180.0f};
    parts.caption = "Items";
    parts.bag     = Colored("6/30", 0xFF6FE36Fu);
    parts.help    = "Use <this>";
    const std::string rml = HelpBarRml(parts, look, Place());
    // The panel's gradient and gold edge, 0.11 H corners.
    CHECK(Has(rml, "<div class=\"help-box\" style=\"left:8px;top:54px;width:400px;height:24px;\"></div>"));
    // A 16 px line in a 24 px row starts 4 px down; the help's 17 px line, 4 (3.5 rounded).
    CHECK(Has(rml, "<div class=\"p hc\" style=\"left:23px;top:58px;color:#efe3c6ff;\">Items</div>"));
    CHECK(Has(rml, "<div class=\"p hc\" style=\"left:120px;top:58px;color:#6fe36fff;\">6/30</div>"));
    CHECK(Has(rml, "<div class=\"p hh\" style=\"left:180px;top:58px;color:#e4d8bdff;\">Use &lt;this&gt;</div>"));
    settings.helpBarBackground = false;
    CHECK(!Has(HelpBarRml(parts, look, Place()), "help-box"));
    parts.row = HelpBarRow{};
    CHECK(HelpBarRml(parts, look, Place()).empty());
    const std::string document = BarDocument(look, Place());
    CHECK(Has(document, "id=\"help\"") && Has(document, ".help-box{"));
    CHECK(Has(document, "decorator:vertical-gradient(#1c160ff7 #0c0a08f7)"));
}

TEST(the_help_bars_text_is_the_bars_title_size_or_its_own)
{
    Settings settings;
    // The look's title is 13 px; the help's line boxes are 16 and 17 px.
    CHECK(Has(BarDocument(LookOf(BarTheme::PhoenixGold, settings), Place()), "font-size:13px;line-height:16px;"));
    settings.helpBarTextSize = 21;
    const std::string document = BarDocument(LookOf(BarTheme::PhoenixGold, settings), Place());
    CHECK(Has(document, "font-size:21px;line-height:16px;") && Has(document, "font-size:21px;line-height:17px;"));
}

TEST(a_popup_is_its_panel_and_each_piece_where_the_layout_put_it)
{
    Settings settings;
    const BarLook look = LookOf(BarTheme::PhoenixGold, settings);
    PopupLayout layout;
    layout.width  = 220.0f;
    layout.height = 120.0f;
    auto piece = [](PopupPiece::Kind kind, float x, float y, float width, float height) {
        PopupPiece p;
        p.kind = kind, p.x = x, p.y = y, p.width = width, p.height = height;
        return p;
    };
    PopupPiece card  = piece(PopupPiece::Kind::Card, 16.0f, 40.4f, 188.0f, 60.0f);
    PopupPiece title = piece(PopupPiece::Kind::Text, 16.4f, 16.6f, 80.0f, 24.0f);
    title.span = Text("Zone & <more>");
    title.font = PopupFont::Title;
    PopupPiece value = piece(PopupPiece::Kind::Text, 100.0f, 50.0f, 30.0f, 18.0f);
    value.span = Text("50%", Tone::Weak);
    PopupPiece time = piece(PopupPiece::Kind::Text, 140.4f, 30.0f, 40.2f, 18.0f);
    time.span       = Text("5:16");
    time.right      = true;
    PopupPiece icon = piece(PopupPiece::Kind::Icon, 20.0f, 50.0f, 19.3f, 19.3f);
    icon.icon       = Icon::Link;
    PopupPiece mark = piece(PopupPiece::Kind::ImmuneMark, 30.0f, 60.0f, 12.0f, 12.0f);
    mark.span       = Text("x", Tone::Immune);
    const PopupPiece star = piece(PopupPiece::Kind::Star, 140.0f, 52.0f, 14.9f, 14.9f);
    const PopupPiece rule = piece(PopupPiece::Kind::Rule, 60.0f, 46.0f, 100.0f, 1.0f);
    const PopupPiece tag  = piece(PopupPiece::Kind::Tag, 20.0f, 80.0f, 50.0f, 21.0f);
    const PopupPiece socket = piece(PopupPiece::Kind::Socket, 24.0f, 70.0f, 172.0f, 26.0f);
    PopupPiece tab          = piece(PopupPiece::Kind::Tab, 24.0f, 104.0f, 50.0f, 21.0f);
    tab.open                = true;
    const PopupPiece track  = piece(PopupPiece::Kind::ScrollTrack, 190.0f, 72.0f, 4.0f, 22.0f);
    const PopupPiece thumb  = piece(PopupPiece::Kind::ScrollThumb, 190.0f, 76.4f, 4.0f, 7.1f);
    PopupPiece item         = piece(PopupPiece::Kind::Item, 120.0f, 104.0f, 19.3f, 19.3f);
    item.item               = 4443;
    PopupPiece plus         = piece(PopupPiece::Kind::Sign, 100.0f, 10.0f, 17.0f, 17.0f);
    plus.span               = Text(kCollapsedSign, Tone::Heading);
    PopupPiece minus        = piece(PopupPiece::Kind::Sign, 160.0f, 10.0f, 18.0f, 18.0f);
    minus.span              = Text(kOpenSign, Tone::Heading);
    layout.pieces = {card, title, value, icon, mark, star, rule, tag, socket, time, tab, track, thumb, item, plus, minus};
    // The title's line box starts 2 px under where the layout put its line; the text's, at it.
    const PopupFaces faces = {PopupFace{"Marcellus SC", 24, 29, 2.0f}, PopupFace{"Barlow Condensed", 18, 22, 0.0f},
        PopupFace{"Marcellus SC", 15, 18, 0.0f}, PopupFace{"Marcellus SC", 15, 18, 0.0f}};
    const std::string rml = PopupRml(layout, look, faces);
    // The gold theme's panel has a second border 2 px outside it.
    CHECK(Has(rml, "<div id=\"outer\" style=\"left:-3px;top:-3px;width:226px;height:126px;\"></div>"));
    CHECK(Has(rml, "<div id=\"panel\" style=\"width:220px;height:120px;\"></div>"));
    CHECK(Has(rml, "<div class=\"p card\" style=\"left:16px;top:40px;width:188px;height:60px;\"></div>"));
    CHECK(Has(rml, "<div class=\"p f0\" style=\"left:16px;top:19px;color:#e4d8bdff;\">Zone &amp; &lt;more&gt;</div>"));
    CHECK(Has(rml, "<div class=\"p f1\" style=\"left:100px;top:50px;color:#66ff66ff;\">50%</div>")); // the weak badge color
    // A value in its whole box, against its right: from its left's pixel to its right's.
    CHECK(Has(rml, "<div class=\"p f1\" style=\"left:140px;top:30px;width:41px;text-align:right;color:#e4d8bdff;\">5:16</div>"));
    CHECK(Has(rml, "<img class=\"p\" src=\"hu:icon/4\" style=\"left:20px;top:50px;width:19.3px;height:19.3px;\"/>"));
    CHECK(Has(rml, "<div class=\"p f1 mark\" style=\"left:30px;top:60px;width:12px;height:12px;line-height:12px;color:#ff4040ff;\">x</div>"));
    // A star is HeadsUp's own white one, in the coin's gold.
    CHECK(Has(rml, "<img class=\"p\" src=\"" + IconSource(Icon::Star) +
                       "\" style=\"left:140px;top:52px;width:15px;height:15px;image-color:#e8c35aff;\"/>"));
    CHECK(Has(rml, "<div class=\"p rule\" style=\"left:60px;top:46px;width:100px;\"></div>"));
    CHECK(Has(rml, "<div class=\"p tag\" style=\"left:20px;top:80px;width:50px;height:21px;border-radius:10.5px;\"></div>"));
    CHECK(Has(rml, "<div class=\"p socket\" style=\"left:24px;top:70px;width:172px;height:26px;\"></div>"));
    // A scrollbar's track and its thumb, both round at their ends.
    CHECK(Has(rml, "<div class=\"p track\" style=\"left:190px;top:72px;width:4px;height:22px;border-radius:2px;\"></div>"));
    CHECK(Has(rml, "<div class=\"p thumb\" style=\"left:190px;top:76px;width:4px;height:7px;border-radius:2px;\"></div>"));
    CHECK(Has(rml, "<img class=\"p\" src=\"hu:item/4443\" style=\"left:120px;top:104px;width:19.3px;height:19.3px;\"/>"));
    // The picked tab is lit.
    CHECK(Has(rml, "<div class=\"p tab lit\" style=\"left:24px;top:104px;width:50px;height:21px;border-radius:10.5px;\"></div>"));
    // A collapse chip's sign is bars with as many pixels on each side of them: a plus across and down, a minus across.
    CHECK(Has(rml, "<div class=\"p\" style=\"left:104px;top:17px;width:9px;height:3px;background-color:"));
    CHECK(Has(rml, "<div class=\"p\" style=\"left:107px;top:14px;width:3px;height:9px;background-color:"));
    CHECK(Has(rml, "<div class=\"p\" style=\"left:164px;top:18px;width:10px;height:2px;background-color:"));
    CHECK(!Has(rml, "left:168px;top:14px"));
    // A pill theme's panel has no outer border.
    const std::string pills = PopupRml(layout, LookOf(BarTheme::UmbraDark, settings), faces);
    CHECK(!Has(pills, "id=\"outer\""));
}

TEST(a_previewed_pop_outs_cards_are_drawn_in_one_box_scaled_about_the_titles_foot_and_shifted_beside_the_settings)
{
    Settings settings;
    const BarLook look = LookOf(BarTheme::PhoenixGold, settings);
    PopupLayout layout;
    layout.width = 220.0f, layout.height = 120.0f;
    PopupPiece title;
    title.kind = PopupPiece::Kind::Text, title.x = 16.0f, title.y = 16.0f, title.width = 80.0f, title.height = 24.0f;
    title.span = Text("Mule"), title.font = PopupFont::Title;
    PopupPiece card;
    card.kind = PopupPiece::Kind::Card, card.x = 16.0f, card.y = 48.0f, card.width = 100.0f, card.height = 60.0f, card.line = 1;
    PopupPiece value = title;
    value.span = Text("50%"), value.font = PopupFont::Text, value.y = 60.0f, value.line = 1;
    layout.pieces  = {title, card, value};
    layout.preview = PopupPreview{0.5f, 44.0f, 35.0f};
    const PopupFaces faces{};
    const std::string rml = PopupRml(layout, look, faces);
    const std::string box = std::string("<div class=\"p ") + kPreviewClass + "\" style=\"left:0px;top:0px;width:220px;height:120px;transform-origin:" +
                            Px(look.metrics.popupPadding) + " " + Px(44.0f) + ";transform:translateX(" + Px(35.0f) + ") scale(" +
                            RcssNumber(0.5f) + ");\">";
    const size_t at = rml.find(box);
    CHECK(at != std::string::npos && rml.find("Mule") < at && rml.find("class=\"p card\"") > at && rml.find("50%") > at);
    CHECK(rml.ends_with("</div></div>")); // the value's own close, then the box's
    // Without a preview, no box: the pieces as before.
    layout.preview.reset();
    const std::string plain = PopupRml(layout, look, faces);
    CHECK(plain.find(kPreviewClass) == std::string::npos && !plain.ends_with("</div></div>"));
}

TEST(the_popup_document_styles_its_faces_and_boxes_from_the_look)
{
    Settings settings;
    const BarLook look = LookOf(BarTheme::UmbraDark, settings);
    const PopupFaces faces = {PopupFace{"Barlow Condensed", 24, 29}, PopupFace{"Barlow Condensed", 18, 22},
        PopupFace{"Barlow Condensed", 15, 18}, PopupFace{"Barlow Condensed", 15, 18}};
    const std::string rml = PopupDocument(look, faces);
    CHECK(Has(rml, "pointer-events:none") && Has(rml, "<body></body>"));
    CHECK(Has(rml, ".f0{font-family:\"Barlow Condensed\";font-size:24px;line-height:29px;"));
    CHECK(Has(rml, ".f2{font-family:\"Barlow Condensed\";font-size:15px;line-height:18px;"));
    CHECK(!Has(rml, "letter-spacing"));
    // A tab's rows sit sunk into the card: darker, its upper edges dark and its lower ones lit.
    CHECK(Has(rml, ".socket{box-sizing:border-box;border:1px;border-color:#00000059 #ffffff14 #ffffff14 #00000059;"));
    CHECK(Has(rml, ".tab{") && Has(rml, ".tab.lit{"));
    CHECK(Has(rml, ".track{") && Has(rml, ".thumb{"));
    // The panel: one fill top to bottom, the theme's faint edge, 0.28 of the popup's unit (12 px over 0.39) rounded.
    CHECK(Has(rml, "#panel{position:absolute;left:0px;top:0px;box-sizing:border-box;border:1px #3a3a42ff;"
                   "border-radius:8.62px;decorator:vertical-gradient(#1d1d21ff #1d1d21ff);}"));
    CHECK(Has(rml, ".card{box-sizing:border-box;border:1px #ffffff1e;border-radius:8.62px;background-color:#ffffff12;}"));
    CHECK(Has(rml, ".tag{box-sizing:border-box;border:1px transparent;background-color:#2c2c33ff;}"));
    CHECK(!Has(rml, IconSource(Icon::Star)));
}

TEST(an_items_picture_is_named_by_its_id_for_the_plugin_to_serve)
{
    CHECK_EQ(ItemSource(4443), "hu:item/4443");
    CHECK(ItemOfSource("hu:item/4443") == 4443);
    CHECK(!ItemOfSource("hu:item/") && !ItemOfSource("hu:item/70000") && !ItemOfSource("hu:coin/15"));
    CHECK(!HostImage("hu:item/4443")); // the plugin's to make, from the game's resources
}

TEST(a_status_icon_is_named_by_its_pack_and_id_for_the_plugin_to_serve)
{
    CHECK_EQ(StatusSource(StatusMark{253, 1, true}), "hu:status/1/253");
    CHECK_EQ(StatusSource(StatusMark{0, kVanillaStatusIcons, false}), "hu:status/vanilla/0");
    // Its background is an icon of its own: the name says the pack and the status alone.
    CHECK(StatusOfSource("hu:status/1/253") == (StatusMark{253, 1, false}));
    CHECK(StatusOfSource("hu:status/vanilla/0") == (StatusMark{0, kVanillaStatusIcons, false}));
    const std::string pastPacks = "hu:status/" + std::to_string(kStatusPackCount) + "/1";
    for (const std::string& bad : {pastPacks, std::string("hu:status/1/70000"), std::string("hu:status/1"),
             std::string("hu:status/x/1"), std::string("hu:item/1/1")})
        CHECK(!StatusOfSource(bad));
    CHECK(!HostImage("hu:status/1/253")); // the plugin's: a status a pack lacks is the game's
}

TEST(a_status_icon_is_its_packs_else_the_games_else_xiviews_unknown_icon)
{
    const int hd = static_cast<int>(std::ranges::find(kStatusPackNames, "HD") - std::begin(kStatusPackNames));
    const Image game{1, 1, {0xFF123456u}};
    int asked        = 0;
    const auto gives = [&](uint16_t) { ++asked; return std::optional<Image>(game); };
    const auto none  = [&](uint16_t) { ++asked; return std::optional<Image>(); };
    const auto at    = [](const Image& image, int x, int y) { return image.argb[static_cast<size_t>(y * image.width + x)]; };
    // XIView's unknown.png read with another decoder: its middle, a gold edge and a clear corner.
    const auto unknown = [&](const std::optional<Image>& image) {
        return image && image->width == 32 && at(*image, 16, 16) == 0xFFE4DFBAu && at(*image, 5, 9) == 0xFFB09020u &&
               at(*image, 0, 0) >> 24 == 0;
    };
    const std::optional<Image> packs = StatusImage(StatusSource(StatusMark{253, hd, false}), gives);
    CHECK(packs && packs->width == 32 && asked == 0);
    CHECK(StatusImage(StatusSource(StatusMark{650, hd, false}), gives)->argb == game.argb); // HD has no 650
    CHECK(StatusImage(StatusSource(StatusMark{771, kVanillaStatusIcons, false}), gives)->argb == game.argb);
    CHECK(unknown(StatusImage(StatusSource(StatusMark{771, kVanillaStatusIcons, false}), none)));
    CHECK(unknown(StatusImage(StatusSource(StatusMark{650, hd, false}), none)));
    CHECK(!StatusImage("hu:item/4443", gives));
}

TEST(every_theme_eases_breathes_and_pulses_in_its_own_colors)
{
    Settings settings;
    const PopupFaces faces = {PopupFace{"Barlow Condensed", 24, 29}, PopupFace{"Barlow Condensed", 18, 22},
        PopupFace{"Barlow Condensed", 15, 18}, PopupFace{"Barlow Condensed", 15, 18}};
    struct Lit
    {
        BarTheme theme;
        const char* breathe;
        const char* fill;
    };
    // The accent at about a third, into what it runs into at four fifths: gold into pale gold, blue into light
    // blue, the royal red into the ember, cyan into violet.
    for (const Lit& lit : {Lit{BarTheme::PhoenixGold, "from{border-color:#c9a24a59;}to{border-color:#efe3c6cc;}", "#c9a24aff #efe3c6ff"},
             Lit{BarTheme::UmbraDark, "from{border-color:#5b8def59;}to{border-color:#9cc2ffcc;}", "#5b8defff #9cc2ffff"},
             Lit{BarTheme::PhoenixRed, "from{border-color:#c5515159;}to{border-color:#e06f5ecc;}", "#c55151ff #e06f5eff"},
             Lit{BarTheme::Aurora, "from{border-color:#5ee6ff59;}to{border-color:#b48cffcc;}", "#5ee6ffff #b48cffff"}})
    {
        const BarLook look    = LookOf(lit.theme, settings);
        const std::string rml = BarDocument(look, Place());
        // Widgets ease into their lit look, the target's frame breathes until it is lit (its border eases no other
        // way), and progress is a gradient that pulses.
        CHECK(Has(rml, "transition:background-color 0.2s cubic-out,border-color 0.2s cubic-out;"));
        CHECK(Has(rml, ".widget.target{animation:2.4s sine-in-out infinite alternate breathe;"
                       "transition:background-color 0.2s cubic-out;}"));
        CHECK(Has(rml, (std::string("@keyframes breathe{") + lit.breathe + "}").c_str()));
        CHECK(Has(rml, ".widget.lit{animation:none;}"));
        CHECK(Has(rml, (std::string("decorator:horizontal-gradient(") + lit.fill +
                           ");animation:1.8s sine-in-out infinite alternate pulse;}").c_str()));
        CHECK(Has(rml, "@keyframes pulse{from{opacity:0.65;}to{opacity:1;}}"));
        // A popup drops in from its widget.
        const std::string popup = PopupDocument(look, faces);
        CHECK(Has(popup, ".popup{position:absolute;transform-origin:50% 0%;pointer-events:auto;}"));
        CHECK(Has(popup, ".popup.open{animation:0.2s cubic-out drop-in;}"));
        CHECK(Has(popup, "@keyframes drop-in{from{opacity:0;transform:translateY(-10px) scale(0.96);}to{opacity:1;"
                         "transform:translateY(0px) scale(1);}}"));
    }
}

TEST(the_line_of_light_and_the_glass_are_the_glass_shapes_alone)
{
    Settings settings;
    const BarLook aurora  = LookOf(BarTheme::Aurora, settings);
    const std::string rml = BarDocument(aurora, Place());
    // The strip's foot: a line of light, cyan to violet, fading out to both sides.
    CHECK(Has(rml, "<div class=\"glint a\"></div><div class=\"glint b\"></div><div class=\"glint c\"></div>"));
    CHECK(Has(rml, ".glint.a{left:0%;width:30%;decorator:horizontal-gradient(#5ee6ff00 #5ee6ffff);}"));
    CHECK(Has(rml, ".glint.c{left:70%;width:30%;decorator:horizontal-gradient(#b48cffff #b48cff00);}"));
    const PopupFaces faces = {PopupFace{"Barlow Condensed", 24, 29}, PopupFace{"Barlow Condensed", 18, 22},
        PopupFace{"Barlow Condensed", 15, 18}, PopupFace{"Barlow Condensed", 15, 18}};
    // The panel is glass, lighter at its top, in a frame of the accent at a quarter.
    CHECK(Has(PopupDocument(aurora, faces),
        "#panel{background-color:transparent;decorator:vertical-gradient(#1b2740f2 #0c1220f2);border-color:#5ee6ff40;}"));
    // The others keep their strips and panels.
    const BarLook gold = LookOf(BarTheme::PhoenixGold, settings);
    CHECK(!Has(BarDocument(gold, Place()), "glint"));
    CHECK(!Has(PopupDocument(gold, faces), "background-color:transparent;decorator:vertical-gradient(#1b2740f2"));
    CHECK(Has(PopupDocument(gold, faces), "decorator:vertical-gradient(#1c160ff7 #0c0a08f7);"));
    // It is the shape's, whatever the colors: Phoenix Gold's as glass has them, Aurora's as pills has neither.
    BarPalette glassGold = PaletteOf(BarTheme::PhoenixGold), pillAurora = PaletteOf(BarTheme::Aurora);
    glassGold.shape      = BarShape::Glass;
    pillAurora.shape     = BarShape::Pills;
    CHECK(Has(BarDocument(LookOf(glassGold, settings), Place()), "glint"));
    CHECK(Has(PopupDocument(LookOf(glassGold, settings), faces), "#panel{background-color:transparent;"));
    CHECK(!Has(BarDocument(LookOf(pillAurora, settings), Place()), "glint"));
    CHECK(!Has(PopupDocument(LookOf(pillAurora, settings), faces), "#panel{background-color:transparent;"));
}

TEST(the_strip_and_the_popups_are_drawn_with_the_games_art_when_the_shape_is_window)
{
    Settings settings;
    BarPalette palette = PaletteOf(BarTheme::UmbraDark);
    palette.shape      = BarShape::Window;
    BarLook look       = LookOf(palette, settings);
    look.skin          = SkinFrame{3, 1.0f, 0.5f, 1.0f, palette.panelBorder};
    const std::string bar = BarDocument(look, Place());
    CHECK(bar.find(SkinSource(3, SkinPiece::Back, kSkinTile)) != std::string::npos);
    CHECK(bar.find("hu:strip") == std::string::npos);
    // The art is the bar's own height, so the widgets centered in the bar are centered in the frame: no room for the
    // glow other themes fade out under the strip.
    const std::string strip = bar.substr(bar.find("#strip{"));
    CHECK(strip.substr(0, strip.find('}')).find("height:" + Px(look.metrics.height) + ";") != std::string::npos);
    // The strip, the help bar and the popups are each the art's own rules: its fill in its even border.
    const auto rule = [&](const std::string& document, const char* name) {
        const std::string box = document.substr(document.find(name));
        return box.substr(0, box.find('}'));
    };
    const PopupFaces faces{};
    for (const char* name : {"#strip{", ".help-box{"})
        CHECK(rule(bar, name).find(SkinRules(*look.skin)) != std::string::npos);
    CHECK(rule(PopupDocument(look, faces), "#panel{").find(SkinRules(*look.skin)) != std::string::npos);
    // The edge is Border Thickness's pixels wide, and none at 0: the same art without its edge, everywhere.
    settings.barTheme                 = BarTheme::Custom;
    settings.customPalette.background = BarBackground::Window;
    settings.customPalette.border     = 0;
    CHECK(FrameAsSet(settings, *look.skin) == (SkinFrame{3, 1.0f, 0.5f, 1.0f, 0, 0}));
    settings.customPalette.border = 5;
    const SkinFrame thick         = FrameAsSet(settings, *look.skin);
    CHECK(thick == (SkinFrame{3, 1.0f, 0.5f, 1.0f, palette.panelBorder, 5}));
    CHECK(Has(SkinRules(thick), "border:5px " + RcssColor(palette.panelBorder) + ";"));
    // Without the art read, the shape draws its colors.
    look.skin = std::nullopt;
    CHECK(BarDocument(look, Place()).find("hu:strip") != std::string::npos);
    CHECK(BarDocument(look, Place()).find("hu:skin") == std::string::npos);
    CHECK(PopupDocument(look, faces).find("hu:skin") == std::string::npos);
}

TEST(a_title_alone_is_drawn_at_its_own_size_and_place)
{
    Settings settings;
    BarLook look     = LookOf(BarTheme::PhoenixGold, settings);
    look.lines.alone = 18; // a short row: two lines at 13 and 11 px, a line alone larger
    BarTextPlace place = Place();
    place.alone        = BarLinePlace{22, 11.0f, 2.0f};
    const std::string rml = BarDocument(look, place);
    const std::string font = BarTitleFont(settings, look.palette.shape);
    CHECK(Has(rml, ".title.alone{font-family:\"" + font + "\";font-size:18px;line-height:22px;"));
    CHECK(Has(rml, ".title{font-family:\"" + font + "\";font-size:13px;line-height:16px;"));
    CHECK(Has(rml, ".title.alone .line-icon{width:18px;height:18px;"));
}

TEST(a_widget_of_cells_lays_each_in_its_column_a_gap_apart)
{
    Settings settings;
    const BarLook look = LookOf(BarTheme::PhoenixGold, settings);
    WidgetContent c;
    c.shown    = true;
    c.cells    = true;
    c.title    = {Text("S 1"), Text("B 2")};
    c.subtitle = {Text("W 1"), Text("BM 14")};
    const std::string rml = WidgetRml(c, look, {30.0f, 44.0f});
    // The first column its width and the gap; the last its width.
    const std::string first = "class=\"cell\" style=\"width:" + Px(30.0f + look.metrics.gap) + ";";
    const std::string last  = "class=\"cell\" style=\"width:" + Px(44.0f) + ";";
    CHECK(rml.find(first) != std::string::npos && rml.find(last) != std::string::npos);
    CHECK(rml.find(">S 1<") < rml.find(">B 2<") && rml.find(">B 2<") < rml.find(">W 1<"));
    CHECK_EQ(CellsWidth({30.0f, 44.0f}, look.metrics.gap), 30.0f + look.metrics.gap + 44.0f);
    // Measured, a column is its widest cell over both lines.
    const std::vector<float> columns = CellColumns(c, [](bool title, const Span& s) {
        return static_cast<float>(s.text.size()) * (title ? 10.0f : 8.0f);
    });
    CHECK((columns == std::vector<float>{30.0f, 40.0f})); // "S 1" at 10 a letter, "BM 14" at 8
}

TEST(a_widget_of_cells_on_two_lines_draws_its_top_line_at_the_detail_size_so_its_pictures_match)
{
    Settings settings;
    const BarLook look = LookOf(BarTheme::PhoenixGold, settings);
    WidgetContent c;
    c.shown    = true;
    c.cells    = true;
    c.figures  = true;
    c.title    = {Span{" 8", Tone::Normal, 0, GamePicture::SanDoriaCrest}, Span{" 4", Tone::Normal, 0, GamePicture::BastokCrest}};
    c.subtitle = {Span{" 2", Tone::Normal, 0, GamePicture::WindurstCrest}, Span{" 5", Tone::Normal, 0, GamePicture::BeastmenCrest}};
    CHECK(Has(WidgetRml(c, look, {30.0f, 30.0f}), "<div class=\"title figures cells\">"));
    const std::string css = BarDocument(look, Place());
    CHECK(Has(css, ".title.cells{font-size:" + Px(static_cast<float>(look.lines.detail)) + ";}"));
    CHECK(Has(css, ".title.cells .line-icon{width:" + Px(static_cast<float>(look.lines.detail)) + ";height:" +
                       Px(static_cast<float>(look.lines.detail)) + ";"));
    // On one line (1 x 4) it keeps the title's size.
    c.subtitle.clear();
    CHECK(!Has(WidgetRml(c, look, {30.0f, 30.0f}), "cells\""));
}

TEST(a_span_of_a_game_picture_draws_that_picture_as_tall_as_its_letters_from_what_was_read_at_load)
{
    Settings settings;
    const BarLook look = LookOf(BarTheme::PhoenixGold, settings);
    WidgetContent c;
    c.shown = true;
    c.title = {Text("S 300"), Span{"", Tone::Normal, 0, GamePicture::PlayOnlineMessenger}, Text("2", Tone::Dim)};
    const std::string rml = WidgetRml(c, look, {});
    const std::string img = "<img class=\"line-icon pic\" src=\"" + GamePictureSource(GamePicture::PlayOnlineMessenger) + "\"/>";
    CHECK(rml.find(img) != std::string::npos && rml.find(img) < rml.find(">2<"));
    // Its pixels are the game's, B, G, R, A as an ARGB word keeps them.
    PlayOnlineIcons icons{ColorPicture{"menu    mesicon ", 1, 1, {1, 2, 3, 255}}, ColorPicture{"menu    mesicon ", 1, 1, {4, 5, 6, 255}}};
    const std::optional<Image> messenger =
        GamePictureImage(icons, std::nullopt, GamePictureSource(GamePicture::PlayOnlineMessenger));
    CHECK(messenger && messenger->width == 1 && messenger->argb == std::vector<uint32_t>{0xFF060504u});
    CHECK(!GamePictureImage(std::nullopt, std::nullopt, GamePictureSource(GamePicture::PlayOnlineMail)));
    CHECK(!GamePictureImage(icons, std::nullopt, IconSource(Icon::Gm)));
    // The nations' crests, each its own.
    const NationCrests crests{ColorPicture{"menu    scre    ", 1, 1, {7, 8, 9, 255}},
        ColorPicture{"menu    bcre    ", 1, 1, {10, 11, 12, 255}}, ColorPicture{"menu    wcre    ", 1, 1, {13, 14, 15, 255}},
        ColorPicture{"menu    mcre    ", 1, 1, {16, 17, 18, 255}}, ColorPicture{"menu    jcre    ", 1, 1, {19, 20, 21, 255}},
        ColorPicture{"menu    acre    ", 1, 1, {22, 23, 24, 255}}};
    const std::optional<Image> bastok = GamePictureImage(std::nullopt, crests, GamePictureSource(GamePicture::BastokCrest));
    CHECK(bastok && bastok->argb == std::vector<uint32_t>{0xFF0C0B0Au});
    CHECK(GamePictureImage(std::nullopt, crests, GamePictureSource(GamePicture::WindurstCrest))->argb ==
          std::vector<uint32_t>{0xFF0F0E0Du});
    CHECK(!GamePictureImage(std::nullopt, std::nullopt, GamePictureSource(GamePicture::SanDoriaCrest)));
    CHECK(GamePictureImage(std::nullopt, crests, GamePictureSource(GamePicture::BeastmenCrest))->argb ==
          std::vector<uint32_t>{0xFF121110u});
    CHECK(GamePictureImage(std::nullopt, crests, GamePictureSource(GamePicture::JeunoCrest))->argb == std::vector<uint32_t>{0xFF151413u});
    CHECK(GamePictureImage(std::nullopt, crests, GamePictureSource(GamePicture::AhtUrhganCrest))->argb ==
          std::vector<uint32_t>{0xFF181716u});
}

TEST(a_side_shared_in_a_combined_pill_keeps_its_text_where_it_was_and_draws_no_edge)
{
    constexpr float kRoom = 9.0f;
    const Settings settings;
    for (const BarTheme theme : {BarTheme::PhoenixGold, BarTheme::UmbraDark, BarTheme::Aurora})
    {
        const BarLook look = LookOf(theme, settings);
        const std::string css = BarDocument(look, Place());
        for (const BarWidget kind : {BarWidget::Zone, BarWidget::Currency})
        {
            const WidgetSide own = SideOf(look, kind, kRoom, false), shared = SideOf(look, kind, kRoom, true);
            // A plain ornate widget has no border; a pill has one, which a shared side drops.
            const float border = look.ornate ? 0.0f : 1.0f;
            CHECK(std::fabs(own.margin + border + own.padding - kRoom) < 0.01f);
            CHECK(shared.margin == 0.0f && std::fabs(shared.padding - kRoom) < 0.01f);
        }
        CHECK(Has(css, kJoinBeforeClass) && Has(css, kJoinAfterClass));
    }
}

TEST(the_settings_widgets_cog_is_an_icon_alone_in_the_titles_color_with_no_room_after_it)
{
    Settings settings;
    const BarLook look = LookOf(BarTheme::PhoenixGold, settings);
    WidgetContent c;
    c.shown = true;
    c.cog   = true;
    const std::string rml = WidgetRml(c, look, {});
    CHECK(Has(rml, ("<img class=\"icon cog\" src=\"" + CogSource(look.palette.title, WholePixels(look.metrics.icon)) + "\"").c_str()));
    // Centered in its widget: as wide as the icon, with no gap after it for text it has none of.
    CHECK_EQ(look.Leading(c), look.metrics.icon);
    CHECK(Has(BarDocument(look, Place()), ".icon.cog{margin-right:0px;}"));
}
