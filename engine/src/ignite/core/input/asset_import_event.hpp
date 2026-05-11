// Copyright (c) 2026 Evangelion Manuhutu

#ifndef IMPORT_FILE_EVENT_HPP
#define IMPORT_FILE_EVENT_HPP

#include "event.hpp"

#include "ignite/asset/asset.hpp"

#include <vector>
#include "ignite/core/path.hpp"

namespace ignite
{
	class AssetImportEvent : public Event
	{
	public:
      AssetImportEvent(const std::vector<ignite::Path> &filepaths, AssetType assetType, const ignite::Path &targetDirectory = {})
			: m_Filepaths(filepaths), m_AssetType(assetType), m_TargetDirectory(targetDirectory)
		{
		}

		AssetType GetAssetType() { return m_AssetType; }
		std::vector<ignite::Path> &GetFilepaths() { return m_Filepaths; }
		const ignite::Path &GetTargetDirectory() const { return m_TargetDirectory; }

		EVENT_CLASS_TYPE(AssetImport)
		EVENT_CLASS_CATEGORY(EventCategoryApplication)
	private:

		AssetType m_AssetType;
		std::vector<ignite::Path> m_Filepaths;
      ignite::Path m_TargetDirectory;
	};
}

#endif