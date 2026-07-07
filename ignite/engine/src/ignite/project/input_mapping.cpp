// Copyright (c) 2026 Evangelion Manuhutu

#include "ignite_pch.hpp"
#include "input_mapping.hpp"
#include "ignite/core/application.hpp"

#include "nlohmann/json.hpp"

namespace ignite
{

	void InputMapping::AddInputMapping(const std::string &action, const std::string &input)
	{
		m_InputMappings[action] = input;
	}

	void InputMapping::RemoveInputMapping(const std::string &action)
	{
		if (m_InputMappings.contains(action))
		{
			m_InputMappings.erase(action);
		}
	}

	bool InputMapping::Serialize(const ignite::Path &filepath)
	{
		nlohmann::json j;
		j["Version"] = 1;
		j["AssetType"] = static_cast<int>(GetAssetType());
		j["AssetHandle"] = static_cast<uint64_t>(handle);
		j["InputMappings"] = m_InputMappings;

		std::string serailized = j.dump(4);
		std::ofstream file(filepath.generic_string(), std::ios::out | std::ios::trunc);
		file << serailized;
		file.close();

		return true;
	}

	Ref<InputMapping> InputMapping::Deserialize(const ignite::Path &filepath)
	{
		std::ifstream file(filepath.generic_string(), std::ios::in);
		nlohmann::json::error_handler_t errorHandler = nlohmann::json::error_handler_t::strict;
		nlohmann::json j = nlohmann::json::parse(file, nullptr, false);

		Ref<InputMapping> inputSystem = CreateRef<InputMapping>();
		int engineVersion;

		j["Version"].get_to(engineVersion);
		j["AssetHandle"].get_to((uint64_t&)inputSystem->handle);
		j["InputMappings"].get_to(inputSystem->m_InputMappings);

		return inputSystem;
	}

}