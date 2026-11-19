// Copyright (c) 2026 Evangelion Manuhutu

#include "ignite_pch.hpp"

#include "asset.hpp"
#include "ignite/core/signal_bus.hpp"
#include "ignite/core/signals/asset_signal.hpp"

#include "ignite/core/application.hpp"
#include "ignite/serializer/serializer.hpp"

namespace ignite
{
    bool Asset::SerializeMetaFile(const std::filesystem::path &filepath, const MetaSerializer &customSerializer) const
    {
        if (filepath.empty())
        {
            return false;
        }

        Serializer sr(filepath);
        sr.BeginMap();
        sr.AddKeyValue("Version", Application::GetVersion());
        sr.AddKeyValue("ASSET_HANDLE", static_cast<uint64_t>(handle));
        sr.AddKeyValue("ASSET_TYPE", AssetTypeToString(const_cast<Asset *>(this)->GetAssetType()));

        sr.BeginMap("DATA");
        if (!m_SourcePath.empty())
        {
            sr.AddKeyValue("SourcePath", m_SourcePath.generic_string());
        }
        if (customSerializer)
        {
            customSerializer(sr);
        }
        sr.EndMap();

        sr.EndMap();
        sr.Serialize();
        return true;
    }

    std::filesystem::path Asset::GetMetaFilePath(const std::filesystem::path &assetFilepath)
    {
        if (assetFilepath.empty())
        {
            return {};
        }

        std::filesystem::path metaPath = assetFilepath;
        metaPath += ".meta";
        return metaPath;
    }

    std::filesystem::path Asset::ReadSourcePathFromMetaFile(const std::filesystem::path &assetFilepath)
    {
        const std::filesystem::path metaPath = GetMetaFilePath(assetFilepath);
        if (metaPath.empty())
        {
            return {};
        }

        std::error_code ec;
        if (!std::filesystem::exists(metaPath, ec) || ec)
        {
            return {};
        }

        const JsonNode root = Serializer::Deserialize(metaPath);
        if (!root)
        {
            return {};
        }

        const JsonNode dataNode = root["DATA"];
        if (dataNode && dataNode["SourcePath"])
        {
            const std::string sourceStr = dataNode["SourcePath"].as<std::string>();
            if (!sourceStr.empty())
            {
                return std::filesystem::path(sourceStr).lexically_normal();
            }
        }

        return {};
    }

	void Asset::NotifyChange()
	{
		const AssetHandle assetHandle = handle;
		const AssetType assetType = GetAssetType();
		Application::SubmitToMainThread([assetHandle, assetType]()
		{
			SignalBus::Emit<AssetChangeSignal>(AssetChangeSignal{ assetHandle, assetType });
		});
	}

}
