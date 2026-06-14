// Copyright(c) 2026 Evangelion Manuhutu

#include "material_2d.hpp"
#include "ignite/serializer/serializer.hpp"

namespace ignite
{

	bool Material2D::Serialize(const ignite::Path &filepath)
	{
		Serializer sr(filepath);

		sr.BeginMap();
		{
			sr.BeginMap("Material2D");
			sr.AddKeyValue("Version", ENGINE_VERSION);
			sr.AddKeyValue("Name", name);
			sr.AddKeyValue("TextureHandle", static_cast<uint64_t>(textureHandle));
			sr.AddKeyValue("BaseColor", data.baseColor);
			sr.AddKeyValue("AdditiveColor", data.additiveColor);
			sr.AddKeyValue("TilingFactor", data.tilingFactor);
			sr.AddKeyValue("Type", static_cast<int>(data.type));
			sr.EndMap();
		}
		sr.EndMap();

		sr.Serialize(filepath);

		return true;
	}

	Ref<Material2D> Material2D::Deserialize(const ignite::Path &filepath)
	{
		if (!ignite::Path::exists(filepath))
		{
			return nullptr;
		}

		YAML::Node fileNode = Serializer::Deserialize(filepath);
		YAML::Node materialNode = fileNode["Material2D"];
		if (!materialNode)
		{
			return nullptr;
		}

		Ref<Material2D> material = CreateRef<Material2D>();
		if (auto n = materialNode["Name"]) material->name = n.as<std::string>();
		if (auto n = materialNode["TextureHandle"]) material->textureHandle = AssetHandle(n.as<uint64_t>());
		if (auto n = materialNode["BaseColor"]) material->data.baseColor = n.as<glm::vec4>();
		if (auto n = materialNode["AdditiveColor"]) material->data.additiveColor = n.as<glm::vec4>();
		if (auto n = materialNode["TilingFactor"]) material->data.tilingFactor = n.as<glm::vec2>();
		if (auto n = materialNode["Type"]) material->data.type = static_cast<Material2DType>(n.as<int>());

		return material;
	}

}