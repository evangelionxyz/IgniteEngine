// Copyright (c) 2026 Evangelion Manuhutu

#pragma once
#ifndef IGN_ASSET_HPP
#define IGN_ASSET_HPP

#include "ignite/core/base.hpp"
#include "ignite/core/uuid.hpp"
#include "ignite/core/types.hpp"
#include "ignite/asset/asset_types.hpp"
#include "ignite/asset/asset_state.hpp"

#include <string>
#include <map>
#include <functional>
#include <filesystem>

namespace ignite
{
    class Serializer;
    class Project;

    class IGN_API Asset : public std::enable_shared_from_this<Asset>
    {
    public:
        using MetaSerializer = std::function<void(Serializer &)>;

        AssetHandle handle = AssetHandle(0);

        virtual ~Asset() = default;

        template<typename T>
        Ref<T> As()
        {
            return std::dynamic_pointer_cast<T>(shared_from_this());
        }

        virtual bool Serialize(const std::filesystem::path &filepath) { return true; }
        virtual bool SerializeMetaFile(const std::filesystem::path &filepath, const MetaSerializer &customSerializer = nullptr) const;

        virtual AssetType GetAssetType() { return AssetType::Invalid; }

        void NotifyChange();

        void SetDirtyFlag(bool dirty) { m_Dirty = dirty; }
        bool IsDirty() const { return m_Dirty; }

        void SetReadyFlag(bool ready) { m_Ready = ready; }
        bool IsReady() const { return m_Ready; }

    protected:
        bool m_Ready = true;
        bool m_Dirty = false;
    };
}

#endif // IGN_ASSET_HPP
