// Copyright (c) 2026 Evangelion Manuhutu

#include "scene_graph.hpp"
#include "ignite/core/logger.hpp"

namespace ignite
{
	SceneGraph::SceneGraph()
	{
		LOG_WARN("[Scene Graph] Initialized");
	}

	SceneGraph::~SceneGraph()
	{
		LOG_WARN("[Scene Graph] Shutdown");
	}
}
