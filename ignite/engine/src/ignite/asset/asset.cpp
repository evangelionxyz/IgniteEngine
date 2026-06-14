// Copyright (c) 2026 Evangelion Manuhutu
#include "pch.hpp"

#include "asset.hpp"

#include "ignite/core/base.hpp"
#include "ignite/serializer/serializer.hpp"

namespace ignite
{
    bool Asset::SerializeMetaFile(const ignite::Path &filepath, const MetaSerializer &customSerializer) const
    {
        if (filepath.empty())
        {
            return false;
        }

        Serializer sr(filepath);
        sr.BeginMap();
        sr.AddKeyValue("ENGINE_VERSION", ENGINE_VERSION);
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
}
