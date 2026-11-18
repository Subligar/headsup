#pragma once

#include "bar_theme.h"
#include "notices.h"
#include "settings.h"
#include "skin_frame.h"

#include <RmlUi/Core.h>

#include <map>
#include <optional>
#include <string>
#include <vector>

namespace headsup
{
    constexpr const char* kNoticeDocument = "notices";
    constexpr const char* kNoticeStackId  = "noticestack"; // the dragged piece, holding the boxes
    // Box 0 to kNoticeBoxes - 1; its parts are found by class: .text (.title, .message), .buttons and .bar.
    std::string NoticeId(int slot);

    struct NoticeFrame
    {
        bool moving = false; // the stack takes the pointer and drags
        float barBottom = 0.0f, seconds = 0.0f;
        std::optional<Rml::Vector2f> pointer; // in the overlay's pixels; none while it is not known
        std::optional<SkinFrame> skin;        // the game's window art the boxes are drawn in, under a game theme
    };
    struct NoticePress
    {
        uint32_t id = 0;
        bool accept = false;
    };
    struct NoticeTook
    {
        bool moved = false;               // dragged somewhere new, which the settings now hold
        bool hovered = false;             // the pointer is over a notification
        std::optional<NoticePress> press; // an invite's button was clicked
    };

    // The notifications: a document of the overlay's, its boxes stacked from the spot, which drags as one.
    class NoticeBox : private Rml::EventListener
    {
    public:
        ~NoticeBox() override { Close(); }
        // Each frame. shown: in the order they stack. Let go of moving mid-drag and the drag ends where it is.
        NoticeTook Update(Rml::Context* context, const BarPalette& palette, Settings& settings,
            const std::vector<ShownNotice>& shown, const NoticeFrame& frame);
        // The shown boxes' outline on the screen as last drawn; nothing while none shows.
        std::optional<NoticeRect> Bounds() const { return m_Bounds; }
        // Before the context goes.
        void Close();

    private:
        void ProcessEvent(Rml::Event& event) override;

        Rml::ElementDocument* m_Document = nullptr;
        Rml::Element* m_Stack            = nullptr;
        // What the document was built from: a change builds it again.
        struct Look
        {
            BarPalette palette{};
            std::string titleFont, textFont;
            float titleSize = 0.0f, textSize = 0.0f;
            std::optional<SkinFrame> skin;
            int width = 0, height = 0;
            NoticeFit fit    = NoticeFit::Shrink;
            HelpAnchor align = HelpAnchor::Left;
            float radius     = 0.0f;
            bool operator==(const Look&) const = default;
        };
        Look m_Look;
        std::vector<std::optional<ShownNotice>> m_Filled; // what each box was last given
        std::map<uint32_t, float> m_At;                   // each notice's top under the spot, by id, as it slides
        Rml::Vector2f m_Grab{};                           // the pointer from the spot as the drag began
        std::optional<NoticeSpot> m_Dragged;              // where the drag left it, not yet in the settings
        std::optional<NoticeRect> m_Bounds;
        std::vector<NoticeRect> m_Boxes;     // each shown box on the screen, for the pointer
        std::optional<NoticePress> m_Press; // a button's click, for the next Update
        bool m_Dragging  = false;
        bool m_Grabbable = false; // as moving was last
        // As the last frame stacked, for a drag: the anchors, and the first box's size the spot keeps on the screen.
        HelpAnchor m_Anchor = HelpAnchor::Left;
        NoticeEdge m_Edge   = NoticeEdge::Top;
        float m_FirstWidth = 0.0f, m_FirstHeight = 0.0f;
    };
}
