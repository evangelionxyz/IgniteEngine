// Copyright(c) 2026 Evangelion Manuhutu

#ifndef IGN_MATERIAL_2D_HPP
#define IGN_MATERIAL_2D_HPP

#include "ignite/asset/asset.hpp"
#include <glm/glm.hpp>

namespace ignite
{
	enum Material2DType
	{
		MATERIAL_2D_TYPE_UNLIT,
		MATERIAL_2D_TYPE_LIT,
	};

	struct Material2DData
	{
        glm::vec4 baseColor = { 1.0f, 1.0f, 1.0f, 1.0f };
		glm::vec4 additiveColor = { 0.0f, 0.0f, 0.0f, 0.0f };
		glm::vec2 tilingFactor = { 1.0f, 1.0f };
		
		bool flipX = false;
		bool flipY = false;

		Material2DType type = MATERIAL_2D_TYPE_UNLIT;
	};

	class IGN_API Material2D : public Asset
	{
	public:
		std::string name = "NewMaterial2D";
		AssetHandle textureHandle = AssetHandle(0);
		Material2DData data;

		virtual bool Serialize(const ignite::Path &filepath) override;
		static Ref<Material2D> Deserialize(const ignite::Path &filepath);

		static AssetType GetStaticAssetType() { return AssetType::Material2D; }
		virtual AssetType GetAssetType() override { return GetStaticAssetType(); }
	};
}

#endif