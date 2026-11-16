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
    }

    void CombatPopupRender::Fail(std::string what)
    {
        m_Failed  = true;
        m_Failure = std::move(what);
    }

    bool CombatPopupRender::Prepare(IDirect3DDevice8* device, Texture& t, const std::string& words, uint32_t color,
        const std::string& font, int raster)
    {
        const int edge = CombatPopupEdge(raster);
        Coverage coverage;
        if (!m_Text.Rasterize(words.c_str(), font.c_str(), raster, 0, 0, edge, coverage))
        {
            Fail("GDI could not draw text in " + font + " at " + std::to_string(raster) + " px");
            return false;
        }
        const Image image = Outlined(coverage, edge, color, kBlack);
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
        float toX, float toY)
    {
        ++m_Frame;
        for (Texture& idle : m_Textures.TakeIdle(m_Frame, kIdleFrames))
            ReleaseTexture(idle.texture);
        m_Text.Sweep();
        if (m_Failed || popups.empty() || toY <= 0.0f) return;
        SetQuadStates(device, true);
        for (const ShownCombatPopup& popup : popups)
        {
            // Drawn at the height it shows on the screen, which the scene image is toY times.
            const int raster = RasterHeight(popup.lineHeight / toY, 0);
            Texture& t       = m_Textures.Use(std::tuple{font, popup.words, popup.color, raster}, m_Frame);
            if (t.texture == nullptr && !Prepare(device, t, popup.words, popup.color, font, raster)) break;
            const SquareQuad q = PlaceCombatPopupText(popup.center, popup.lineHeight, t.size, toX / toY);
            DrawQuad(device, GpuQuad{t.texture, q.x, q.y, q.width, q.height, 0.0f, t.u, t.v, popup.depth, WithAlpha(kWhite, popup.alpha)},
                true);
        }
        device->SetTexture(0, nullptr); // the textures may be released before the game binds its own
    }

    std::string CombatPopupRender::TakeFailure() { return std::exchange(m_Failure, std::string()); }

    void CombatPopupRender::Release()
    {
        for (Texture& t : m_Textures.TakeAll())
            ReleaseTexture(t.texture);
        m_Text.Release();
    }
}
