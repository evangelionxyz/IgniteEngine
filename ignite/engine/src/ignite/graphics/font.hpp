// Copyright (c) 2026 Evangelion Manuhutu

#ifndef IGN_FONT_HPP
#define IGN_FONT_HPP

#include "ignite/asset/asset.hpp"
#include "ignite/core/types.hpp"
#include "ignite/core/buffer.hpp"

#include "texture.hpp"

#include <vector>
#include "ignite/core/path.hpp"

#include <msdf-atlas-gen.h>
#include <glm/glm.hpp>

namespace ignite
{
    class IGN_API Font : public Asset
    {
    public:

        Font(const ignite::Path &filepath);
        virtual ~Font() override;

        static Ref<Font> Create(const ignite::Path &filepath);

        const Ref<Texture> GetAtlasTexture() const { return m_AtlasTexture; }
        
        const std::vector<msdf_atlas::GlyphGeometry> &GetGlyphs() { return m_Glyphs; }
        const msdf_atlas::FontGeometry &GetGeometry() { return m_FontGeometry; }

        glm::vec2 MeasureString(const std::string &str, float kerning = 0.0f, float linespacing = 0.0f) const;

        static AssetType GetStaticType() { return AssetType::Font; }
        AssetType GetAssetType() override { return GetStaticType(); }

    private:
        void LoadGlyphs(const ignite::Path &filepath);

		std::vector<msdf_atlas::GlyphGeometry> m_Glyphs;
		msdf_atlas::FontGeometry m_FontGeometry;

        Ref<Texture> m_AtlasTexture;
    };

}

#endif