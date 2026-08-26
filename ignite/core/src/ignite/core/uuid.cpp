// Copyright (c) 2026 Evangelion Manuhutu

#include "uuid.hpp"

#include <random>

static std::random_device s_RandomDevice;
static std::mt19937_64 s_RandomEngine(s_RandomDevice());
static std::uniform_int_distribution<uint64_t> s_UniformDist;

namespace ignite
{
    UUID::UUID()
        : m_UUID(s_UniformDist(s_RandomEngine))
    {
    }

    UUID::UUID(const uint64_t uuid)
        : m_UUID(uuid)
    {
    }
}
