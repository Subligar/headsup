#pragma once

#include "fish_case.h"
#include "function_ref.h"
#include "gpu.h"
#include "plate_image.h"


#include <array>
#include <map>
#include <optional>
#include <string>
#include <vector>

namespace headsup
{
    // The Case's pictures as textures, and its quads drawn with them.
    class CaseRender
    {
    public:
        // Before a Case plays, and again when its size changes: its tiles' icons (kept once made) and its three texts at
        // the size they show, in place of the last Case's. False when Direct3D refused a picture; nothing draws then.
        bool Prepare(IDirect3DDevice8* device, const CaseRun& run, const std::string& name, const std::string& skillLine,
            float unit, FunctionRef<std::optional<Image>(uint16_t)> icon);
        CaseWords Words() const;
        // Inside the caller's saved states.
        void Draw(IDirect3DDevice8* device, const std::vector<CaseQuad>& quads);
        void Release();

    private:
        struct Picture
        {
            IDirect3DTexture8* texture = nullptr;
            float u = 0.0f, v = 0.0f, aspect = 0.0f;
        };
        static bool Make(IDirect3DDevice8* device, const Image& image, Picture& picture);
        bool MakeText(IDirect3DDevice8* device, const std::string& text, float height, Picture& picture);
        const Picture* PictureOf(const CaseQuad& quad) const;

        std::array<Picture, kCaseFixedPictures> m_Fixed{};
        std::map<uint16_t, Picture> m_Icons; // an item the game has no icon for keeps an empty one
        Picture m_Title, m_Name, m_Skill;
        GdiText m_Text;
    };
}
