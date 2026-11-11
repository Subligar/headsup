#include "case_render.h"

#include "case_pictures.h"

namespace headsup
{
    bool CaseRender::Make(IDirect3DDevice8* device, const Image& image, Picture& picture)
    {
        ReleaseTexture(picture.texture);
        picture = Picture{};
        if (image.argb.empty()) return true;
        picture.texture = CreateTexture(device, image.argb.data(), image.width, image.height, picture.u, picture.v);
        picture.aspect  = static_cast<float>(image.width) / static_cast<float>(image.height);
        return picture.texture != nullptr;
    }

    bool CaseRender::MakeText(IDirect3DDevice8* device, const std::string& text, float height, Picture& picture)
    {
        if (text.empty()) return Make(device, Image{}, picture);
        const int pixels = CaseFontPixels(height), margin = pixels / 2; // room for what Wine draws past the measure
        Coverage coverage;
        if (!m_Text.Rasterize(text.c_str(), kCaseFont, pixels, 0, 0, margin, coverage)) return Make(device, Image{}, picture);
        return Make(device, CaseTextImage(coverage, margin), picture);
    }

    bool CaseRender::Prepare(IDirect3DDevice8* device, const CaseRun& run, const std::string& name, const std::string& skillLine,
        float unit, FunctionRef<std::optional<Image>(uint16_t)> icon)
    {
        for (int i = 0; i < kCaseFixedPictures; ++i)
            if (m_Fixed[i].texture == nullptr && !Make(device, CaseImage(static_cast<CasePicture>(i)), m_Fixed[i])) return false;
        for (const uint16_t item : run.tiles)
            if (!m_Icons.contains(item))
                if (const std::optional<Image> picture = icon(item); !Make(device, picture.value_or(Image{}), m_Icons[item])) return false;
        return MakeText(device, kCaseTitle, kCaseTitleHeight * unit, m_Title) && MakeText(device, name, kCaseNameHeight * unit, m_Name) &&
               MakeText(device, skillLine, kCaseSkillHeight * unit, m_Skill);
    }

    CaseWords CaseRender::Words() const { return CaseWords{m_Title.aspect, m_Name.aspect, m_Skill.aspect}; }

    const CaseRender::Picture* CaseRender::PictureOf(const CaseQuad& quad) const
    {
        switch (quad.picture)
        {
        case CasePicture::Title: return &m_Title;
        case CasePicture::Name: return &m_Name;
        case CasePicture::Skill: return &m_Skill;
        case CasePicture::Item:
        case CasePicture::ItemCrisp:
        {
            const auto found = m_Icons.find(quad.item);
            return found == m_Icons.end() ? nullptr : &found->second;
        }
        default: return &m_Fixed[static_cast<size_t>(quad.picture)];
        }
    }

    void CaseRender::Draw(IDirect3DDevice8* device, const std::vector<CaseQuad>& quads)
    {
        m_Text.Sweep();
        if (quads.empty()) return;
        SetQuadStates(device, false);
        for (const CaseQuad& quad : quads)
        {
            const Picture* picture = PictureOf(quad);
            if (picture == nullptr || picture->texture == nullptr) continue;
            const D3DTEXTUREFILTERTYPE filter = quad.picture == CasePicture::ItemCrisp ? D3DTEXF_POINT : D3DTEXF_LINEAR;
            device->SetRenderState(D3DRS_DESTBLEND, quad.add ? D3DBLEND_ONE : D3DBLEND_INVSRCALPHA);
            device->SetTextureStageState(0, D3DTSS_MINFILTER, filter);
            device->SetTextureStageState(0, D3DTSS_MAGFILTER, filter);
            DrawQuad(device, GpuQuad{picture->texture, quad.x, quad.y, quad.width, quad.height, 0.0f, picture->u, picture->v, 0.0f, quad.argb}, false);
        }
        // What draws next in the same states expects SetQuadStates' own.
        SetQuadStates(device, false);
        device->SetTexture(0, nullptr);
    }

    void CaseRender::Release()
    {
        for (Picture& picture : m_Fixed) ReleaseTexture(picture.texture);
        for (auto& [item, picture] : m_Icons) ReleaseTexture(picture.texture);
        m_Icons.clear();
        for (Picture* picture : {&m_Title, &m_Name, &m_Skill}) ReleaseTexture(picture->texture);
        m_Text.Release();
    }
}
