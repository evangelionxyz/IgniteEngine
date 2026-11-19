// Copyright (c) 2026 Evangelion Manuhutu

#ifndef IGN_FONT_HPP
#define IGN_FONT_HPP

#include "ignite/asset/asset.hpp"
#include "ignite/core/types.hpp"
#include "ignite/core/buffer.hpp"

#include "texture.hpp"

#include <atomic>
#include <mutex>
#include <vector>

#include <msdf-atlas-gen.h>
#include <glm/glm.hpp>

namespace ignite
{
    class IGN_API Font : public Asset
    {
    public:
        Font();
        Font(const std::filesystem::path &filepath);
        virtual ~Font() override;

        static Ref<Font> Create(const std::filesystem::path &filepath, bool async = true);

        const Ref<Texture> GetAtlasTexture() const
        {
            std::lock_guard<std::mutex> lock(m_AtlasMutex);
            return m_AtlasTexture;
        }

        const std::vector<msdf_atlas::GlyphGeometry> &GetGlyphs() { return m_Glyphs; }
        const msdf_atlas::FontGeometry &GetGeometry() { return m_FontGeometry; }

        bool IsLoading() const { return m_Loading.load(std::memory_order_acquire); }
        void SetLoadingFlag(bool loading) { m_Loading.store(loading, std::memory_order_release); }
        void SetAtlasTexture(const Ref<Texture> &atlas)
        {
            std::lock_guard<std::mutex> lock(m_AtlasMutex);
            m_AtlasTexture = atlas;
        }

        glm::vec2 MeasureString(const std::string &str, float kerning = 0.0f, float linespacing = 0.0f) const;

        static AssetType GetStaticType() { return AssetType::Font; }
        AssetType GetAssetType() override { return GetStaticType(); }

    private:
        void LoadGlyphs(const std::filesystem::path &filepath);

		std::vector<msdf_atlas::GlyphGeometry> m_Glyphs;
		msdf_atlas::FontGeometry m_FontGeometry;

        Ref<Texture> m_AtlasTexture;
        mutable std::mutex m_AtlasMutex;
        std::atomic<bool> m_Loading{ false };
    };

}

#endif
