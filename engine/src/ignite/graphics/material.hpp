/* MIT License
* 
* Copyright (c) 2025 Evangelion Manuhutu | IGNITE STUDIO
* 
* Permission is hereby granted, free of charge, to any person obtaining a copy
* of this software and associated documentation files (the "Software"), to deal
* in the Software without restriction, including without limitation the rights
* to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
* copies of the Software, and to permit persons to whom the Software is
* furnished to do so, subject to the following conditions:
* 
* The above copyright notice and this permission notice shall be included in all
* copies or substantial portions of the Software.
* 
* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
* IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
* FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
* AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
* LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
* OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
* SOFTWARE.
*/

#pragma once

#include "ignite/core/buffer.hpp"
#include "ignite/core/application.hpp"
#include "constant_buffer.hpp"

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
        nvrhi::TextureHandle handle;

        ~MaterialTextureResource()
        {
            if (data)
                delete data;
        }
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

    class Material : public Asset
    {
    public:
        Material();
        Material(const aiScene *aiScene, aiMaterial *aiMat, const std::filesystem::path &baseFilepath);

        ~Material();

        std::string name;

        MaterialConstants params;
        MaterialType type = MaterialType::PBR;
        MaterialBlendMode blendMode = MaterialBlendMode::Opaque;

        uint32_t mipLevels = 4;
        nvrhi::TextureHandle cubeMapTexture;
        std::unordered_map<MaterialTextureType, Ref<MaterialTextureResource>> textures;

        nvrhi::BindingSetHandle bindingSet;

        void LoadTexture(const aiScene *aiScene, const aiMaterial *aiMat, const std::filesystem::path &filepath, MaterialTextureType textureType);
        void CreateDefaultTextures();
        void CreateTextures();

        void UpdateBindingSet();
        void UpdateTexture(const Ref<Texture> &texture, MaterialTextureType textureType);
        void WriteTexture(nvrhi::ICommandList *commandList);
        void WriteBuffer(nvrhi::ICommandList *commandList);

        static void UploadTextureWithMips(nvrhi::ICommandList *commandList, const nvrhi::TextureHandle &handle,
            const void *baseData, uint32_t baseWidth, uint32_t baseHeight, uint32_t baseRowPitch, nvrhi::Format format, uint32_t mipLevels);

        static AssetType GetStaticType() { return AssetType::Material; }
        virtual AssetType GetType() override { return GetStaticType(); }

    private:
        Ref<ConstantBuffer> m_ConstantBuffer;
    };
}
