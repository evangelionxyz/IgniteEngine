#pragma once

#include "ignite/core/buffer.hpp"
#include "ignite/core/application.hpp"
#include "texture.hpp"

#include <glm/glm.hpp>
#include <assimp/scene.h>
#include <unordered_map>
#include <nvrhi/nvrhi.h>
#include <ranges>

namespace ignite
{
    struct MaterialConstants
    {
        glm::vec4 baseColor{ 1.0f, 1.0f, 1.0f, 1.0f };
        float specularFactor = 0.0f;
        float metallicFactor = 0.0f;
        float roughnessFactor = 1.0f;
        float emissiveFactor = 0.0f;
    };

    struct MaterialTextureResource
    {
        uint8_t *data = nullptr;
        uint32_t width = 0;
        uint32_t height = 0;
        uint32_t rowPitch = 0;
        nvrhi::Format format = nvrhi::Format::UNKNOWN;
        nvrhi::TextureHandle handle;
    };

    enum class MaterialTextureType : uint8_t
    {
        None,
        BaseColor,
        Diffuse,
        Specular,
        Ambient,
        Occlusion,
        Emissive,
        EmissionColor,
        Metalness,
        Height,
        Normals,
        Shininess,
        Roughness,
        Opacity,
        Displacement,
        Lightmap,
        Reflection,
        Sheen,
        ClearCoat,
        Transmission,
        Count
    };

    enum class MaterialType : uint8_t
    {
        PBR,
        Legacy,
        Count
    };

    enum class MaterialBlendMode : uint8_t
    {
        Opaque,
        Transparent,
        Additive,
        Count
    };


    static MaterialTextureType GetTextureTypeFromAssimp(const aiTextureType type)
    {
        switch (type)
        {
        case aiTextureType_BASE_COLOR: return MaterialTextureType::BaseColor;
        case aiTextureType_DIFFUSE: return MaterialTextureType::Diffuse;
        case aiTextureType_SPECULAR: return MaterialTextureType::Specular;
        case aiTextureType_AMBIENT: return MaterialTextureType::Ambient;
        case aiTextureType_EMISSIVE: return MaterialTextureType::Emissive;
        case aiTextureType_HEIGHT: return MaterialTextureType::Height;
        case aiTextureType_NORMALS: return MaterialTextureType::Normals;
        case aiTextureType_SHININESS: return MaterialTextureType::Shininess;
        case aiTextureType_OPACITY: return MaterialTextureType::Opacity;
        case aiTextureType_DISPLACEMENT: return MaterialTextureType::Displacement;
        case aiTextureType_LIGHTMAP: return MaterialTextureType::Lightmap;
        case aiTextureType_REFLECTION: return MaterialTextureType::Reflection;
        case aiTextureType_EMISSION_COLOR: return MaterialTextureType::EmissionColor;
        case aiTextureType_METALNESS: return MaterialTextureType::Metalness;
        case aiTextureType_DIFFUSE_ROUGHNESS: return MaterialTextureType::Roughness;
        case aiTextureType_AMBIENT_OCCLUSION: return MaterialTextureType::Occlusion;
        case aiTextureType_SHEEN: return MaterialTextureType::Sheen;
        case aiTextureType_CLEARCOAT: return MaterialTextureType::ClearCoat;
        case aiTextureType_TRANSMISSION: return MaterialTextureType::Transmission;
        case aiTextureType_NONE:
        default:return MaterialTextureType::None;
        }
    }

    static aiTextureType GetAssimpTextureType(const MaterialTextureType type)
    {
        switch (type)
        {
        case MaterialTextureType::BaseColor: return aiTextureType_BASE_COLOR;
        case MaterialTextureType::Diffuse: return aiTextureType_DIFFUSE;
        case MaterialTextureType::Specular: return aiTextureType_SPECULAR;
        case MaterialTextureType::Ambient: return aiTextureType_AMBIENT;
        case MaterialTextureType::Emissive: return aiTextureType_EMISSIVE;
        case MaterialTextureType::Height: return aiTextureType_HEIGHT;
        case MaterialTextureType::Normals: return aiTextureType_NORMALS;
        case MaterialTextureType::Shininess: return aiTextureType_SHININESS;
        case MaterialTextureType::Opacity: return aiTextureType_OPACITY;
        case MaterialTextureType::Displacement: return aiTextureType_DISPLACEMENT;
        case MaterialTextureType::Lightmap: return aiTextureType_LIGHTMAP;
        case MaterialTextureType::Reflection: return aiTextureType_REFLECTION;
        case MaterialTextureType::EmissionColor: return aiTextureType_EMISSION_COLOR;
        case MaterialTextureType::Metalness: return aiTextureType_METALNESS;
        case MaterialTextureType::Roughness: return aiTextureType_DIFFUSE_ROUGHNESS;
        case MaterialTextureType::Occlusion: return aiTextureType_AMBIENT_OCCLUSION;
        case MaterialTextureType::Sheen: return aiTextureType_SHEEN;
        case MaterialTextureType::ClearCoat: return aiTextureType_CLEARCOAT;
        case MaterialTextureType::Transmission: return  aiTextureType_TRANSMISSION;
        case MaterialTextureType::None:
        default: return aiTextureType_NONE;
        }
    }

    struct Material : public Asset
    {
        Material();

        std::string name;

        MaterialConstants params;
        MaterialType type = MaterialType::PBR;
        MaterialBlendMode blendMode = MaterialBlendMode::Opaque;

        uint32_t mipLevels = 4;
        nvrhi::TextureHandle cubeMapTexture;
        std::unordered_map<MaterialTextureType, Ref<MaterialTextureResource>> textures;

        nvrhi::BindingSetHandle bindingSet;
        nvrhi::BufferHandle paramsBuffer;

        void UpdateBindingSet();
        void UpdateTexture(const Ref<Texture> &texture, MaterialTextureType textureType);
        void WriteTexture(nvrhi::ICommandList *commandList);

        void WriteBuffer(nvrhi::ICommandList *commandList) const;

        static void UploadTextureWithMips(nvrhi::ICommandList *commandList,
            const nvrhi::TextureHandle &handle, const void *baseData,
            uint32_t baseWidth, uint32_t baseHeight, uint32_t baseRowPitch,
            nvrhi::Format format, uint32_t mipLevels);

        static AssetType GetStaticType() { return AssetType::Material; }
        virtual AssetType GetType() override { return GetStaticType(); };

    private:
        friend class MeshLoader;
    };
}
