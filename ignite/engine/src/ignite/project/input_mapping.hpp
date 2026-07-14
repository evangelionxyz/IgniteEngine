// Copyright (c) 2026 Evangelion Manuhutu
#pragma once
#ifndef IGN_INPUT_MAPPING_HPP
#define IGN_INPUT_MAPPING_HPP

#include "ignite/asset/asset.hpp"
#include "ignite/core/input/key_codes.hpp"
#include "ignite/core/input/mouse_codes.hpp"
#include <unordered_map>
#include <string>

namespace ignite
{
	class IGN_API InputMapping : public Asset
	{
	public:
		void MapKey(KeyCode key, const std::string &action);
		void MapMouseButton(MouseCode button, const std::string &action);
		void MapJoystickButton(uint8_t button, const std::string &action);

		void UnmapKey(KeyCode key);
		void UnmapMouseButton(MouseCode button);
		void UnmapJoystickButton(uint8_t button);

		std::unordered_map<KeyCode, std::string> m_KeyMappings;
		std::unordered_map<MouseCode, std::string> m_MouseMappings;
		std::unordered_map<uint8_t, std::string> m_JoystickMappings;

		virtual bool Serialize(const ignite::Path &filepath) override;
		static Ref<InputMapping> Deserialize(const ignite::Path &filepath);

		static AssetType GetStaticAssetType() { return AssetType::InputMapping; }
		virtual AssetType GetAssetType() override { return GetStaticAssetType(); }
	};
}

#endif
