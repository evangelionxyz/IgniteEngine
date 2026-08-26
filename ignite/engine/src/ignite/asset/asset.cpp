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
        if (customSerializer)
        {
            customSerializer(sr);
        }
        sr.EndMap();

        sr.EndMap();
        sr.Serialize();
        return true;
    }

	void Asset::NotifyChange()
	{
		Application::SubmitToMainThread([this]()
		{
			SignalBus::Emit<AssetChangeSignal>(AssetChangeSignal{ handle, GetAssetType() });
		});
	}

}
