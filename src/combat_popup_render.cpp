#include "combat_popup_render.h"

#include "argb.h"
#include "nameplate.h"
#include "plate_image.h"

#include <utility>

namespace headsup
{
    namespace
    {
        constexpr const char* kCapital = "H"; // its ink is a font's capitals'
        // While the Damage Numbers page is open, behind each column: a faint white, its margin in screen pixels.
        constexpr uint32_t kColumnBack = 0x40FFFFFF;
        constexpr float kColumnPad     = 4.0f;
    }

    void CombatPopupRender::Fail(std::string what)
    {
        m_Failed  = true;
        m_Failure = std::move(what);
    }

    bool CombatPopupRender::Prepare(IDirect3DDevice8* device, Texture& t, const std::string& words, uint32_t color,
        const std::string& font, int raster, int edge, uint32_t outlineColor)
    {
        Coverage coverage;
        if (!m_Text.Rasterize(words.c_str(), font.c_str(), raster, 0, 0, edge, coverage))
        {
            Fail("GDI could not draw text in " + font + " at " + std::to_string(raster) + " px");
            return false;
        }
        const Image image = Outlined(coverage, edge, color, outlineColor);
        t.texture         = CreateTexture(device, image.argb.data(), image.width, image.height, t.u, t.v);
        if (t.texture == nullptr)
        {
            Fail("Direct3D could not make a " + std::to_string(image.width) + "x" + std::to_string(image.height) + " texture");
            return false;
        }
        const LineInk ink = m_Text.FontInk(font.c_str(), raster, kCapital);
        t.size = CombatTextSize{static_cast<float>(image.width), static_cast<float>(image.height), static_cast<float>(edge),
            ink.top, ink.baseline, raster};
        return true;
    }

    void CombatPopupRender::Draw(IDirect3DDevice8* device, const std::vector<ShownCombatPopup>& popups, const std::string& font,
        int edge, uint32_t outlineColor, float toX, float toY, bool columns)
    {
        ++m_Frame;
        for (Texture& idle : m_Textures.TakeIdle(m_Frame, kIdleFrames))
            ReleaseTexture(idle.texture);
        m_Text.Sweep();
        if (m_Failed || popups.empty() || toY <= 0.0f) return;
        std::vector<Texture*> textures;
        std::vector<SquareQuad> quads;
        for (const ShownCombatPopup& popup : popups)
        {
            // Drawn at the height it shows on the screen, which the scene image is toY times.
            const int raster = RasterHeight(popup.lineHeight / toY, 0);
            Texture& t       = m_Textures.Use(std::tuple{font, popup.words, popup.color, raster, edge, outlineColor}, m_Frame);
            if (t.texture == nullptr && !Prepare(device, t, popup.words, popup.color, font, raster, edge, outlineColor)) break;
            textures.push_back(&t);
            quads.push_back(PlaceCombatPopupText(popup.center, popup.lineHeight, t.size, toX / toY, popup.across));
        }
        SetQuadStates(device, true);
        if (columns && m_White == nullptr)
        {
            const uint32_t white = kWhite;
            m_White = CreateTexture(device, &white, 1, 1, m_WhiteU, m_WhiteV);
        }
        if (columns && m_White != nullptr)
            for (const SquareQuad& b : PopupColumnBacks(popups, quads, kColumnPad * toY))
                DrawQuad(device, GpuQuad{m_White, b.x, b.y, b.width, b.height, 0.0f, m_WhiteU, m_WhiteV, 0.0f, kColumnBack}, false);
        for (size_t i = 0; i < quads.size(); ++i)
        {
            const SquareQuad& q = quads[i];
            DrawQuad(device, GpuQuad{textures[i]->texture, q.x, q.y, q.width, q.height, 0.0f, textures[i]->u, textures[i]->v,
                                 popups[i].depth, WithAlpha(kWhite, popups[i].alpha)},
                true);
        }
        device->SetTexture(0, nullptr); // the textures may be released before the game binds its own
    }

    std::string CombatPopupRender::TakeFailure() { return std::exchange(m_Failure, std::string()); }

    void CombatPopupRender::Release()
    {
        for (Texture& t : m_Textures.TakeAll())
            ReleaseTexture(t.texture);
        ReleaseTexture(m_White);
        m_Text.Release();
    }
}
