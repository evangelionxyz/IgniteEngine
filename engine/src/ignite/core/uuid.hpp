// Copyright (c) 2026 Evangelion Manuhutu

#pragma once
#ifndef UUID_HPP
#define UUID_HPP

#include <functional>
#include "types.hpp"

namespace ignite
{
    class UUID
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

template<>
struct std::hash<ignite::UUID>
{
    std::size_t operator() (const ignite::UUID& uuid) const noexcept
    {
        return hash<uint64_t>()(uuid);
    }
};

#endif