// Copyright(c) 2026 Evangelion Manuhutu

#ifndef IGN_INPUT_LAYER_HPP
#define IGN_INPUT_LAYER_HPP

#include "ignite/core/base.hpp"

namespace ignite
{
	enum class EMainInputLayer : uint8_t
	{
		Editor = 0,
		Game = 1
	};

	enum class ESceneInputLayer : uint8_t
	{
		GameWorld = 0,
		GameUI = 1
	};
}

#endif