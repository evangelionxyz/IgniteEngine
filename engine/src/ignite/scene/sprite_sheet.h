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
	struct SpriteSheetData
	{
		glm::vec2 uv0 = glm::vec2(0.0f);
		glm::vec2 uv1 = glm::vec2(1.0f);
	};

	class SpriteSheet : public Asset
	{
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

		std::vector<SpriteSheetData> &GetSprites() { return m_Sprites; }
		const std::vector<SpriteSheetData> &GetSprites() const { return m_Sprites; }

        bool Serialize(const std::filesystem::path &filepath) const
		{
			YAML::Emitter out;
			out << YAML::BeginMap;
			out << YAML::Key << "SpriteSheet" << YAML::Value << YAML::BeginMap;
			out << YAML::Key << "TextureHandle" << YAML::Value << static_cast<uint64_t>(m_TextureHandle);
			out << YAML::Key << "AtlasSize" << YAML::Value << YAML::Flow << YAML::BeginSeq << m_AtlasSize.x << m_AtlasSize.y << YAML::EndSeq;

			out << YAML::Key << "Sprites" << YAML::Value << YAML::BeginSeq;
			for (const auto &sprite : m_Sprites)
			{
				out << YAML::BeginMap;
				out << YAML::Key << "UV0" << YAML::Value << YAML::Flow << YAML::BeginSeq << sprite.uv0.x << sprite.uv0.y << YAML::EndSeq;
				out << YAML::Key << "UV1" << YAML::Value << YAML::Flow << YAML::BeginSeq << sprite.uv1.x << sprite.uv1.y << YAML::EndSeq;
				out << YAML::EndMap;
			}
			out << YAML::EndSeq;

			out << YAML::EndMap;
			out << YAML::EndMap;

			std::ofstream file(filepath);
			if (!file.is_open())
			{
				return false;
			}

			file << out.c_str();
			return true;
		}

		static Ref<SpriteSheet> Deserialize(const std::filesystem::path &filepath)
		{
			if (!std::filesystem::exists(filepath))
			{
				return nullptr;
			}

			YAML::Node root = YAML::LoadFile(filepath.string());
			YAML::Node node = root["SpriteSheet"];
			if (!node)
			{
				return nullptr;
			}

			Ref<SpriteSheet> spriteSheet = CreateRef<SpriteSheet>();

			if (YAML::Node textureNode = node["TextureHandle"])
			{
				spriteSheet->SetTextureHandle(AssetHandle(textureNode.as<uint64_t>()));
			}

			if (YAML::Node atlasNode = node["AtlasSize"]; atlasNode && atlasNode.IsSequence() && atlasNode.size() == 2)
			{
				spriteSheet->SetAtlasSize({ atlasNode[0].as<float>(), atlasNode[1].as<float>() });
			}

			auto &sprites = spriteSheet->GetSprites();
			sprites.clear();
			if (YAML::Node spritesNode = node["Sprites"])
			{
				for (const YAML::Node &spriteNode : spritesNode)
				{
					SpriteSheetData data;

					if (YAML::Node uv0Node = spriteNode["UV0"]; uv0Node && uv0Node.IsSequence() && uv0Node.size() == 2)
					{
						data.uv0 = { uv0Node[0].as<float>(), uv0Node[1].as<float>() };
					}

					if (YAML::Node uv1Node = spriteNode["UV1"]; uv1Node && uv1Node.IsSequence() && uv1Node.size() == 2)
					{
						data.uv1 = { uv1Node[0].as<float>(), uv1Node[1].as<float>() };
					}

					sprites.push_back(data);
				}
			}

			spriteSheet->SetReadyFlag(true);
			spriteSheet->SetDirtyFlag(false);
			return spriteSheet;
		}

		static AssetType GetStaticType() { return AssetType::SpriteSheet; }
		virtual AssetType GetAssetType() override { return GetStaticType(); }

	private:
		glm::vec2 m_AtlasSize = { 32.0f, 32.0f };
		std::vector<SpriteSheetData> m_Sprites;
		AssetHandle m_TextureHandle;

	};
}

#endif