// Copyright (c) 2026 Evangelion Manuhutu

#include "pch.hpp"

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
        auto it = std::find_if(params.begin(), params.end(), [&name](const AnimParam &p) { return p.name == name; });
        return it != params.end() ? &(*it) : nullptr;
    }

    const AnimParam *Animator::GetParam(const std::string &name) const
    {
        auto it = std::find_if(params.begin(), params.end(), [&name](const AnimParam &p) { return p.name == name; });
        return it != params.end() ? &(*it) : nullptr;
    }
}
