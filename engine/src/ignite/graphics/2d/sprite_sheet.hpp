// Copyright(c) 2026 Evangelion Manuhutu

#ifndef SPRITE_SHEET_HPP
#define SPRITE_SHEET_HPP

#include "ignite/asset/asset.hpp"

namespace ignite
{
	class SpriteSheet : public Asset
	{
	public:

		virtual AssetType GetAssetType() override { return GetStaticAssetType(); }
		static AssetType GetStaticAssetType() { return AssetType::SpriteSheet; }
	};
}

#endif