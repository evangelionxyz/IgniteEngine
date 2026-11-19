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
#include <atomic>

namespace ignite
{
    class Serializer;
    class Project;

    class IGN_API Asset : public std::enable_shared_from_this<Asset>
    {
    public:
        using MetaSerializer = std::function<void(Serializer &)>;

        AssetHandle handle = AssetHandle(0);

        Asset() = default;
        Asset(const Asset &other)
            : std::enable_shared_from_this<Asset>(other),
              handle(other.handle),
              m_Ready(other.m_Ready.load(std::memory_order_acquire)),
              m_Dirty(other.m_Dirty),
              m_SourcePath(other.m_SourcePath)
        {
        }
        Asset &operator=(const Asset &other)
        {
            if (this != &other)
            {
                handle = other.handle;
                m_Ready.store(other.m_Ready.load(std::memory_order_acquire), std::memory_order_release);
                m_Dirty = other.m_Dirty;
                m_SourcePath = other.m_SourcePath;
            }
            return *this;
        }
        Asset(Asset &&other) noexcept
            : std::enable_shared_from_this<Asset>(std::move(other)),
              handle(other.handle),
              m_Ready(other.m_Ready.load(std::memory_order_acquire)),
              m_Dirty(other.m_Dirty),
              m_SourcePath(std::move(other.m_SourcePath))
        {
        }
        Asset &operator=(Asset &&other) noexcept
        {
            if (this != &other)
            {
                handle = other.handle;
                m_Ready.store(other.m_Ready.load(std::memory_order_acquire), std::memory_order_release);
                m_Dirty = other.m_Dirty;
                m_SourcePath = std::move(other.m_SourcePath);
            }
            return *this;
        }

        virtual ~Asset() = default;

        template<typename T>
        Ref<T> As()
        {
            return std::dynamic_pointer_cast<T>(shared_from_this());
        }

        virtual bool Serialize(const std::filesystem::path &filepath) { return true; }
        virtual bool SerializeMetaFile(const std::filesystem::path &filepath, const MetaSerializer &customSerializer = nullptr) const;

        static std::filesystem::path GetMetaFilePath(const std::filesystem::path &assetFilepath);
        static std::filesystem::path ReadSourcePathFromMetaFile(const std::filesystem::path &assetFilepath);

        virtual AssetType GetAssetType() { return AssetType::Invalid; }

        void NotifyChange();

        void SetSourcePath(const std::filesystem::path &sourcePath) { m_SourcePath = sourcePath; }
        const std::filesystem::path &GetSourcePath() const { return m_SourcePath; }
        bool HasSourcePath() const { return !m_SourcePath.empty(); }

        void SetDirtyFlag(bool dirty) { m_Dirty = dirty; }
        bool IsDirty() const { return m_Dirty; }

        void SetReadyFlag(bool ready) { m_Ready.store(ready, std::memory_order_release); }
        bool IsReady() const { return m_Ready.load(std::memory_order_acquire); }

    protected:
        std::atomic<bool> m_Ready{ true };
        bool m_Dirty = false;
        std::filesystem::path m_SourcePath;
    };
}

#endif // IGN_ASSET_HPP
