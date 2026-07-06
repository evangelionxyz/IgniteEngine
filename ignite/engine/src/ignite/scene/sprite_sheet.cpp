// Copyright (c) 2026 Evangelion Manuhutu

#include "ignite_pch.hpp"

#include "sprite_sheet.hpp"
#include "ignite/serializer/serializer.hpp"

namespace ignite
{
	bool SpriteSheet::Serialize(const ignite::Path &filepath)
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
		SetDirtyFlag(false);
		return true;
	}

	Ref<SpriteSheet> SpriteSheet::Deserialize(const ignite::Path &filepath)
	{
		if (!ignite::Path::exists(filepath))
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
				SpriteSheet::Data data;
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
}