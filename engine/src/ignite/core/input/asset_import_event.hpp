// Copyright (c) 2026 Evangelion Manuhutu

#ifndef IMPORT_FILE_EVENT_HPP
#define IMPORT_FILE_EVENT_HPP

#include "event.hpp"

#include <vector>
#include <filesystem>

namespace ignite
{
	class AssetImportEvent : public Event
	{
	public:
		AssetImportEvent();

		EVENT_CLASS_TYPE(AssetImporter)
		EVENT_CLASS_CATEGORY(EventCategoryApplication)
	private:
		std::vector<std::filesystem::path> m_Filepaths;
	};
}

#endif