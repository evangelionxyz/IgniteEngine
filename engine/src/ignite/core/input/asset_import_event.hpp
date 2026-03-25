// Copyright (c) 2026 Evangelion Manuhutu

#ifndef IMPORT_FILE_EVENT_HPP
#define IMPORT_FILE_EVENT_HPP

#include "event.hpp"

#include "ignite/asset/asset.hpp"

#include <vector>
#include <filesystem>

namespace ignite
{
	class AssetImportEvent : public Event
	{
	public:
		AssetImportEvent(const std::vector<std::filesystem::path> &filepaths, AssetType assetType)
			: m_Filepaths(filepaths), m_AssetType(assetType)
		{
		}

		AssetType GetAssetType() { return m_AssetType; }
		std::vector<std::filesystem::path> &GetFilepaths() { return m_Filepaths; }

		EVENT_CLASS_TYPE(AssetImport)
		EVENT_CLASS_CATEGORY(EventCategoryApplication)
	private:

		AssetType m_AssetType;
		std::vector<std::filesystem::path> m_Filepaths;
	};
}

#endif