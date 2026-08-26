// Copyright (c) 2026 Evangelion Manuhutu

#include "ignite_pch.hpp"
#include "input_mapping.hpp"
#include "ignite/core/application.hpp"

#include "nlohmann/json.hpp"

namespace ignite
{

	void InputMapping::MapKey(KeyCode key, const std::string &action)
	{
		m_KeyMappings[key] = action;
	}

	void InputMapping::MapMouseButton(MouseCode button, const std::string &action)
	{
		m_MouseMappings[button] = action;
	}

	void InputMapping::MapJoystickButton(uint8_t button, const std::string &action)
	{
		m_JoystickMappings[button] = action;
	}

	void InputMapping::UnmapKey(KeyCode key)
	{
		m_KeyMappings.erase(key);
	}

	void InputMapping::UnmapMouseButton(MouseCode button)
	{
		m_MouseMappings.erase(button);
	}

	void InputMapping::UnmapJoystickButton(uint8_t button)
	{
		m_JoystickMappings.erase(button);
	}

	bool InputMapping::Serialize(const std::filesystem::path &filepath)
	{
		nlohmann::json j;
		j["Version"] = 1;
		j["AssetType"] = static_cast<int>(GetAssetType());
		j["AssetHandle"] = static_cast<uint64_t>(handle);

		nlohmann::json keysJson = nlohmann::json::object();
		for (const auto &[key, action] : m_KeyMappings)
		{
			keysJson[std::to_string(key)] = action;
		}
		j["KeyMappings"] = keysJson;

		nlohmann::json mouseJson = nlohmann::json::object();
		for (const auto &[button, action] : m_MouseMappings)
		{
			mouseJson[std::to_string(button)] = action;
		}
		j["MouseMappings"] = mouseJson;

		nlohmann::json joyJson = nlohmann::json::object();
		for (const auto &[button, action] : m_JoystickMappings)
		{
			joyJson[std::to_string(button)] = action;
		}
		j["JoystickMappings"] = joyJson;

		std::string serialized = j.dump(4);
		std::ofstream file(filepath.generic_string(), std::ios::out | std::ios::trunc);
		file << serialized;
		file.close();

		return true;
	}

	Ref<InputMapping> InputMapping::Deserialize(const std::filesystem::path &filepath)
	{
		std::ifstream file(filepath.generic_string(), std::ios::in);
		if (!file.is_open())
		{
			return nullptr;
		}
		
		nlohmann::json j = nlohmann::json::parse(file, nullptr, false);
		if (j.is_discarded())
		{
			return nullptr;
		}

		Ref<InputMapping> mapping = CreateRef<InputMapping>();
		int engineVersion = 0;

		if (j.contains("Version")) j["Version"].get_to(engineVersion);
		if (j.contains("AssetHandle")) j["AssetHandle"].get_to((uint64_t&)mapping->handle);

		if (j.contains("KeyMappings") && j["KeyMappings"].is_object())
		{
			for (auto &[keyStr, actionVal] : j["KeyMappings"].items())
			{
				try
				{
					KeyCode key = static_cast<KeyCode>(std::stoul(keyStr));
					mapping->m_KeyMappings[key] = actionVal.get<std::string>();
				}
				catch (...) {}
			}
		}

		if (j.contains("MouseMappings") && j["MouseMappings"].is_object())
		{
			for (auto &[buttonStr, actionVal] : j["MouseMappings"].items())
			{
				try
				{
					MouseCode button = static_cast<MouseCode>(std::stoul(buttonStr));
					mapping->m_MouseMappings[button] = actionVal.get<std::string>();
				}
				catch (...) {}
			}
		}

		if (j.contains("JoystickMappings") && j["JoystickMappings"].is_object())
		{
			for (auto &[buttonStr, actionVal] : j["JoystickMappings"].items())
			{
				try
				{
					uint8_t button = static_cast<uint8_t>(std::stoul(buttonStr));
					mapping->m_JoystickMappings[button] = actionVal.get<std::string>();
				}
				catch (...) {}
			}
		}

		return mapping;
	}

}
