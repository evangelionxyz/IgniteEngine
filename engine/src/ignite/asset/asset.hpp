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

#include "ignite/core/uuid.hpp"

#include <string>
#include <filesystem>
#include <map>
#include <nvrhi/nvrhi.h>

namespace ignite {

    using AssetHandle = UUID;

    enum class AssetType
    {
        Invalid,
        Audio,
        Model,
        Project,
        Texture,
        Material,
        Font,
        TextureCube,
        SkeletalAnimation,
        Environment,
        Anim2D,
        Skeleton,
        SkeletalMesh,
        StaticMesh,
        Scene
    };

    static std::string AssetTypeToString(AssetType type)
    {
        switch (type)
        {
            case ignite::AssetType::Texture: return "Texture";
            case ignite::AssetType::Material: return "Material";
            case ignite::AssetType::Audio: return "Audio";
            case ignite::AssetType::Model: return "Model";
            case ignite::AssetType::Font: return "Font";
            case ignite::AssetType::Project: return "Project";
            case ignite::AssetType::TextureCube: return "TextureCube";
            case ignite::AssetType::Scene: return "Scene";
            case ignite::AssetType::SkeletalAnimation: return "SkeletalAnimation";
            case ignite::AssetType::Anim2D: return "Anim2D";
			case ignite::AssetType::SkeletalMesh: return "SkeletalMesh";
            case ignite::AssetType::StaticMesh: return "StaticMesh";
            case ignite::AssetType::Skeleton: return "Skeleton";
            case ignite::AssetType::Environment: return "Environment";
            case ignite::AssetType::Invalid:
            default: return "Invalid";
        }
    }

    static std::map<std::string, AssetType> s_AssetExtensionMap =
    {
        { ".ixproj", AssetType::Project },
        { ".ixscene", AssetType::Scene },
        { ".jpg", AssetType::Texture },
        { ".png", AssetType::Texture },
        { ".jpeg", AssetType::Texture },
        { ".hdr", AssetType::Texture },
        { ".otf", AssetType::Font },
        { ".ttf", AssetType::Font },
        { ".mp3", AssetType::Audio },
        { ".flac", AssetType::Audio },
        { ".wav", AssetType::Audio },

        { ".gltf", AssetType::StaticMesh },
        { ".bin", AssetType::StaticMesh },
        { ".ixsm", AssetType::StaticMesh },

        { ".ixskel", AssetType::Skeleton},
        { ".ixmat", AssetType::Material},
        { ".ixenv", AssetType::Environment},
    };

    static AssetType AssetTypeFromString(const std::string &typeStr)
    {
        if (typeStr == "Scene") return AssetType::Scene;
        if (typeStr == "Texture") return AssetType::Texture;
        if (typeStr == "TextureCube") return AssetType::TextureCube;
        if (typeStr == "Audio") return AssetType::Audio;
        if (typeStr == "Project") return AssetType::Project;
        if (typeStr == "Model") return AssetType::Model;
        if (typeStr == "SkeletalAnimation") return AssetType::SkeletalAnimation;
        if (typeStr == "Anim2D")  return AssetType::Anim2D;
        if (typeStr == "StaticMesh")  return AssetType::StaticMesh;
        if (typeStr == "SkeletalMesh")  return AssetType::SkeletalMesh;
        if (typeStr == "Skeleton")  return AssetType::Skeleton;
        if (typeStr == "Material")  return AssetType::Material;
        if (typeStr == "Environment")  return AssetType::Environment;
        if (typeStr == "Font")  return AssetType::Font;
        return AssetType::Invalid;
    }

    // Binary Extensions
    static std::string GetAssetExtensionFromType(const AssetType type)
    {
        switch (type)
        {
        case AssetType::StaticMesh: return ".ixsm";
        case AssetType::Scene: return ".ixscene";
        case AssetType::Project: return ".ixproj";
        case AssetType::Material: return ".ixmat";
        case AssetType::Environment: return ".ixenv";
        default: return ".invalid";
        }
    }

    static AssetType GetAssetTypeFromExtension(const std::string &ext)
    {
        if (s_AssetExtensionMap.contains(ext))
        {
            return s_AssetExtensionMap.at(ext);
        }

        return AssetType::Invalid;
    }

    struct AssetMetaData
    {
        AssetMetaData() = default;
        AssetMetaData(const std::filesystem::path &filepath, const AssetType type)
            : filepath(filepath), type(type)
        {}

        std::filesystem::path filepath;
        AssetType type = AssetType::Invalid;
    };

    class Asset : public std::enable_shared_from_this<Asset>
    {
    public:
        AssetHandle handle;

        virtual ~Asset() { };

        template<typename T>
        Ref<T> As()
        {
            return std::dynamic_pointer_cast<T>(shared_from_this());
        }

        virtual AssetType GetAssetType() { return AssetType::Invalid; }

        void SetDirtyFlag(bool dirty)  { m_Dirty = dirty; }
        bool IsDirty() const  { return m_Dirty; }

        void SetReadyFlag(bool ready) { m_Ready = ready; }
        bool IsReady() const { return m_Ready; }

    protected:
        bool m_Ready = false;
        bool m_Dirty = true;
    };
}
