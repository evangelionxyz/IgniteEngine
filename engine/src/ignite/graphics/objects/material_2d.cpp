// Copyright(c) 2026 Evangelion Manuhutu

#include "material_2d.hpp"
#include "ignite/serializer/serializer.hpp"

namespace ignite
{

	bool Material2D::Serialize(const std::filesystem::path &filepath)
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

	Ref<Material2D> Material2D::Deserialize(const std::filesystem::path &filepath)
	{
		if (!std::filesystem::exists(filepath))
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
		if (materialNode["Name"]) material->name = materialNode["Name"].as<std::string>();
		if (materialNode["TextureHandle"]) material->textureHandle = AssetHandle(materialNode["TextureHandle"].as<uint64_t>());
		if (materialNode["BaseColor"]) material->data.baseColor = materialNode["BaseColor"].as<glm::vec4>();
		if (materialNode["AdditiveColor"]) material->data.additiveColor = materialNode["AdditiveColor"].as<glm::vec4>();
		if (materialNode["TilingFactor"]) material->data.tilingFactor = materialNode["TilingFactor"].as<glm::vec2>();
		if (materialNode["Type"]) material->data.type = static_cast<Material2DType>(materialNode["Type"].as<int>());

		return material;
	}

}