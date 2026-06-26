// Copyright (c) 2026 Evangelion Manuhutu

#ifndef IGN_IMPORT_FILE_EVENT_HPP
#define IGN_IMPORT_FILE_EVENT_HPP

#include "event.hpp"
#include "ignite/asset/asset.hpp"
#include "ignite/core/path.hpp"
#include <vector>

namespace ignite
{
	class AssetImportEvent final : public Event
	{
	public:
		AssetImportEvent(const std::vector<ignite::Path> &filepaths, AssetType assetType, const ignite::Path &targetDirectory = {})
			: m_Filepaths(filepaths), m_AssetType(assetType), m_TargetDirectory(targetDirectory)
		{
		}

		const AssetType &GetAssetType() const { return m_AssetType; }

		std::vector<ignite::Path> &GetFilepaths() { return m_Filepaths; }
		const ignite::Path &GetTargetDirectory() const { return m_TargetDirectory; }

		EVENT_CLASS_TYPE(AssetImport)
		EVENT_CLASS_CATEGORY(EventCategoryAsset)
	private:

		AssetType m_AssetType;
		std::vector<ignite::Path> m_Filepaths;
		ignite::Path m_TargetDirectory;
	};
}

#endif