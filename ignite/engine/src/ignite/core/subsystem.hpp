// Copyright (c) 2026 Evangelion Manuhutu
#pragma once
#ifndef IGN_SUBSYSTEM_HPP
#define IGN_SUBSYSTEM_HPP

#include "base.hpp"

namespace ignite
{
	class IGN_API Subsystem
	{
	public:
		virtual ~Subsystem() = default;
		virtual void Init() { };
		virtual void Shutdown() { };
	};
}

#endif
