// Copyright (c) 2026 Evangelion Manuhutu

#pragma once
#ifndef IGN_SIGNALS_HPP
#define IGN_SIGNALS_HPP

namespace ignite
{
	enum class SignalType
	{
		NONE = 0,
		Asset,
		Project,
		ScriptEngine,
	};

	struct SuccessResultSignal
	{
		bool isSuccess = false;
		SignalType type = SignalType::NONE;
	};

}

#endif