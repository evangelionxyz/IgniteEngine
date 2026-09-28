// Copyright (c) 2026 Evangelion Manuhutu

#include "ignite_pch.hpp"

#include "sprite_sheet.hpp"
#include "ignite/serializer/serializer.hpp"

namespace ignite
{
	bool SpriteSheet::Serialize(const std::filesystem::path &filepath)
	{
		Serializer sr(filepath);
		sr.BeginMap();
		sr.BeginMap("SpriteSheet");
		sr.AddKeyValue("TextureHandle", static_cast<uint64_t>(m_TextureHandle));
		sr.BeginSequence("AtlasSize");
		sr.AddValue(m_AtlasSize.x);
		sr.AddValue(m_AtlasSize.y);
		sr.EndSequence();

		sr.BeginSequence("Sprites");
		for (const auto &sprite : m_Sprites)
		{
			sr.BeginMap();
			sr.BeginSequence("UV0");
			sr.AddValue(sprite.uv0.x);
			sr.AddValue(sprite.uv0.y);
			sr.EndSequence();
			sr.BeginSequence("UV1");
			sr.AddValue(sprite.uv1.x);
			sr.AddValue(sprite.uv1.y);
			sr.EndSequence();
			sr.EndMap();
		}
		sr.EndSequence();

		sr.EndMap();
		sr.EndMap();

		sr.Serialize();
		SetDirtyFlag(false);
		return true;
	}

	Ref<SpriteSheet> SpriteSheet::Deserialize(const std::filesystem::path &filepath)
	{
		if (!std::filesystem::exists(filepath))
		{
			return nullptr;
		}

		JsonNode root = Serializer::Deserialize(filepath);
		JsonNode node = root["SpriteSheet"];
		if (!node)
		{
			return nullptr;
		}

		Ref<SpriteSheet> spriteSheet = CreateRef<SpriteSheet>();
		if (JsonNode textureNode = node["TextureHandle"])
		{
			spriteSheet->SetTextureHandle(AssetHandle(textureNode.as<uint64_t>()));
		}

		if (JsonNode atlasNode = node["AtlasSize"]; atlasNode && atlasNode.IsSequence() && atlasNode.size() == 2)
		{
			spriteSheet->SetAtlasSize({ atlasNode[0].as<float>(), atlasNode[1].as<float>() });
		}

		auto &sprites = spriteSheet->GetSprites();
		sprites.clear();
		if (JsonNode spritesNode = node["Sprites"])
		{
			for (const JsonNode &spriteNode : spritesNode)
			{
				SpriteSheet::Data data;
				if (JsonNode uv0Node = spriteNode["UV0"]; uv0Node && uv0Node.IsSequence() && uv0Node.size() == 2)
				{
					data.uv0 = { uv0Node[0].as<float>(), uv0Node[1].as<float>() };
				}

				if (JsonNode uv1Node = spriteNode["UV1"]; uv1Node && uv1Node.IsSequence() && uv1Node.size() == 2)
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
