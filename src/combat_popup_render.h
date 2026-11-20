#pragma once

#include "Ashita.h"
#include "combat_popup.h"
#include "frame_cache.h"
#include "gpu.h"

#include <cstdint>
#include <string>
#include <tuple>
#include <vector>

namespace headsup
{
    // The combat popups' words as GDI text inside a black edge, in textures shared by every popup that shows the same
    // words in the same font, color and height, drawn at the game's depth. It is apart from NameplateRenderer, which lets
    // go of its textures while nameplates are off, so popups draw with nameplates off.
    class CombatPopupRender
    {
    public:
        // Into the scene image, toX by toY times the screen, inside the caller's saved states.
        void Draw(IDirect3DDevice8* device, const std::vector<ShownCombatPopup>& popups, const std::string& font, float toX,
            float toY);
        // Once GDI or Direct3D refused a texture: nothing more is drawn until HeadsUp reloads.
        bool Failed() const { return m_Failed; }
        // What was refused, once.
        std::string TakeFailure();
        void Release();
        void SetGameFont(std::optional<GameFontFace> face) { m_Text.SetGameFont(std::move(face)); }

    private:
        struct Texture
        {
            IDirect3DTexture8* texture = nullptr;
            float u = 0.0f, v = 0.0f; // the image's share of its power-of-two texture
            CombatTextSize size;
        };
        bool Prepare(IDirect3DDevice8* device, Texture& t, const std::string& words, uint32_t color, const std::string& font,
            int raster);
        void Fail(std::string what);

        GdiText m_Text;
        FrameCache<std::tuple<std::string, std::string, uint32_t, int>, Texture> m_Textures; // font, words, color, raster
        uint32_t m_Frame = 0;
        bool m_Failed    = false;
        std::string m_Failure;
    };
}
