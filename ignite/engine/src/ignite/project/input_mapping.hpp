// Copyright (c) 2026 Evangelion Manuhutu
#pragma once
#ifndef IGN_INPUT_MAPPING_HPP
#define IGN_INPUT_MAPPING_HPP

#include "ignite/asset/asset.hpp"

namespace ignite
{
	class IGN_API InputMapping : public Asset
	{
	public:
		void AddInputMapping(const std::string &action, const std::string &input);
		void RemoveInputMapping(const std::string &action);

		std::unordered_map<std::string, std::string> m_InputMappings;

		virtual bool Serialize(const ignite::Path &filepath) override;
		static Ref<InputMapping> Deserialize(const ignite::Path &filepath);

		static AssetType GetStaticAssetType() { return AssetType::InputMapping; }
		virtual AssetType GetAssetType() override { return GetStaticAssetType(); }
	};
}

#endif
