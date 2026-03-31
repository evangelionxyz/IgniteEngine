// Copyright (c) 2026 Evangelion Manuhutu

#ifndef SPRITE_SHEET_HPP
#define SPRITE_SHEET_HPP

#include "ignite/asset/asset.hpp"
#include "ignite/math/math.hpp"

#include <vector>
#include <filesystem>
#include <yaml-cpp/yaml.h>
#include <fstream>

namespace ignite
{
    struct SpriteSheetSpritePayload
	{
		AssetHandle spriteSheetHandle = AssetHandle(0);
		AssetHandle textureHandle = AssetHandle(0);
		uint32_t spriteIndex = 0;
		glm::vec2 uv0 = glm::vec2(0.0f);
		glm::vec2 uv1 = glm::vec2(1.0f);
	};

	class SpriteSheet : public Asset
	{
	public:
		struct Data
		{
			glm::vec2 uv0 = glm::vec2(0.0f);
			glm::vec2 uv1 = glm::vec2(1.0f);
		};

	public:
		SpriteSheet()
			: m_TextureHandle(AssetHandle(0))
		{
		}

		SpriteSheet(AssetHandle handle)
			: m_TextureHandle(handle)
		{
		}

		void SetAtlasSize(const glm::vec2 &atlasSize) { m_AtlasSize = atlasSize; }
		glm::vec2 &GetAtlasSize() { return m_AtlasSize; }
		const glm::vec2 &GetAtlasSize() const { return m_AtlasSize; }

		void SetTextureHandle(AssetHandle handle) { m_TextureHandle = handle; }
		AssetHandle GetTextureHandle() const { return m_TextureHandle; }

		std::vector<Data> &GetSprites() { return m_Sprites; }
		const std::vector<Data> &GetSprites() const { return m_Sprites; }

		virtual bool Serialize(const std::filesystem::path &filepath) override;
		static Ref<SpriteSheet> Deserialize(const std::filesystem::path &filepath);

		static AssetType GetStaticType() { return AssetType::SpriteSheet; }
		virtual AssetType GetAssetType() override { return GetStaticType(); }

	private:
		glm::vec2 m_AtlasSize = { 32.0f, 32.0f };
		std::vector<Data> m_Sprites;
		AssetHandle m_TextureHandle;

	};
}

#endif