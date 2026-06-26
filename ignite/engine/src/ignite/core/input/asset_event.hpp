// Copyright(c) 2026 Evangelion Manuhutu
#pragma once
#ifndef IGN_ASSET_EVENT_HPP
#define IGN_ASSET_EVENT_HPP

#include "ignite/core/base.hpp"
#include "event.hpp"

#include "ignite/asset/asset.hpp"

namespace ignite
{
	class AssetChangeEvent final : public Event
	{
	public:
		AssetChangeEvent(AssetHandle handle, AssetType type)
			: m_Handle(handle), m_Type(type)
		{
		}

		const AssetHandle &GetAssetHandle() const { return m_Handle; }
		const AssetType &GetAssetType() const { return m_Type; }

		EVENT_CLASS_TYPE(AssetChange)
		EVENT_CLASS_CATEGORY(EventCategoryAsset)

	private:
		AssetHandle m_Handle;
		AssetType m_Type;
	};
}

#endif
