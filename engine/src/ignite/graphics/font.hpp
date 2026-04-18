// Copyright (c) 2026 Evangelion Manuhutu

#ifndef FONT_HPP
#define FONT_HPP

#include "ignite/asset/asset.hpp"
#include "ignite/core/types.hpp"
#include "ignite/core/buffer.hpp"

#include "texture.hpp"

#include <vector>
#include <filesystem>

#include <msdf-atlas-gen.h>
#include <glm/glm.hpp>

namespace ignite
{
    class Font : public Asset
    {
    public:

        Font(const std::filesystem::path &filepath);
        ~Font();

        static Ref<Font> Create(const std::filesystem::path &filepath);

        const Ref<Texture> GetAtlasTexture() const { return m_AtlasTexture; }
        
        const std::vector<msdf_atlas::GlyphGeometry> &GetGlyphs() { return m_Glyphs; }
        const msdf_atlas::FontGeometry &GetGeometry() { return m_FontGeometry; }

        glm::vec2 MeasureString(const std::string &str, float kerning = 0.0f, float linespacing = 0.0f) const;

        static AssetType GetStaticType() { return AssetType::Font; }
        AssetType GetAssetType() override { return GetStaticType(); }

    private:
        void LoadGlyphs(const std::filesystem::path &filepath);

		std::vector<msdf_atlas::GlyphGeometry> m_Glyphs;
		msdf_atlas::FontGeometry m_FontGeometry;

        Ref<Texture> m_AtlasTexture;
    };

}

#endif