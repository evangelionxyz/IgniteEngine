// Copyright (c) 2026 Evangelion Manuhutu

#pragma once
#ifndef IGN_CORE_UUID_HPP
#define IGN_CORE_UUID_HPP

#include "ignite/core/base.hpp"

#include <functional>

namespace ignite
{
    class IGN_CORE_API UUID
    {
    public:
        UUID();
        explicit UUID(uint64_t uuid);
        UUID(const UUID &uuid) = default;
        operator uint64_t() const { return m_UUID; }

        UUID &operator =(uint64_t uuid) { m_UUID = uuid; return *this; }
        UUID &operator =(int uuid) { m_UUID = static_cast<uint64_t>(uuid); return *this; }

    private:
        uint64_t m_UUID;
    };
}

namespace std
{
    template<>
    struct hash<ignite::UUID>
    {
        std::size_t operator() (const ignite::UUID& uuid) const noexcept
        {
            return std::hash<uint64_t>()(uuid);
        }
    };
}

#endif
