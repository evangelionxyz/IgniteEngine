// Copyright (c) 2026 Evangelion Manuhutu

#include "ignite_pch.hpp"

#include "animator.hpp"

namespace ignite
{    
    // -----------------------------------------------------------------------
    // Param setters
    // -----------------------------------------------------------------------
    void Animator::SetParamFloat(const std::string &name, float v)
    {
        if (auto *p = GetParam(name)) p->floatVal = v;
    }

    void Animator::SetParamInt(const std::string &name, int v)
    {
        if (auto *p = GetParam(name)) p->intVal = v;
    }

    void Animator::SetParamBool(const std::string &name, bool v)
    {
        if (auto *p = GetParam(name)) p->boolVal = v;
    }

    void Animator::SetParamString(const std::string &name, const std::string &v)
    {
        if (auto *p = GetParam(name)) p->strVal = v;
    }

    AnimParam *Animator::GetParam(const std::string &name)
    {
        auto it = params.find(name);
        if (it != params.end())
            return &it->second;
        return nullptr;
    }

    const AnimParam *Animator::GetParam(const std::string &name) const
    {
		auto it = params.find(name);
		if (it != params.end())
			return &it->second;
		return nullptr;
    }

	AnimParam *Animator::FindAnimParam(std::unordered_map<std::string, AnimParam> &params, const std::string &name)
	{
		auto it = params.find(name);
		if (it != params.end())
			return &it->second;
		return nullptr;
	}

	const AnimParam *Animator::FindAnimParam(const std::unordered_map<std::string, AnimParam> &params, const std::string &name)
	{
		auto it = params.find(name);
		if (it != params.end())
			return &it->second;
		return nullptr;
	}
}
