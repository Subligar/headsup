#pragma once

#include "bar_theme.h"
#include "friend_notices.h"
#include "settings.h"

#include <RmlUi/Core.h>

#include <map>
#include <optional>
#include <string>
#include <vector>

namespace headsup
{
    constexpr const char* kNoticeDocument = "notices";
    constexpr const char* kNoticeStackId  = "noticestack"; // the dragged piece, holding the boxes
    // Box 0 to kMaxNotices - 1; its children are .title, .message and .bar.
    std::string NoticeId(int slot);

    // The friend notices: a document of the overlay's, its boxes stacked from the spot, which drags while Move is on.
    class NoticeBox : private Rml::EventListener
    {
    public:
        ~NoticeBox() override { Close(); }
        // Each frame. shown: oldest first. moving: the boxes take the pointer and drag as one. seconds: the frame's,
        // for the slide. True when it was dragged somewhere new, which the settings then hold.
        bool Update(Rml::Context* context, const BarPalette& palette, Settings& settings,
            const std::vector<ShownNotice>& shown, bool moving, float barBottom, float seconds);
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
            bool operator==(const Look&) const = default;
        };
        Look m_Look;
        std::vector<std::optional<ShownNotice>> m_Filled; // what each box was last given
        std::map<uint32_t, float> m_At;                   // each notice's top under the spot, by id, as it slides
        Rml::Vector2f m_Grab{};                           // the pointer from the spot as the drag began
        std::optional<NoticeSpot> m_Dragged;              // where the drag left it, not yet in the settings
        bool m_Dragging = false;
    };
}
