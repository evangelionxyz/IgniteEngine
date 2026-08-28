// Copyright (c) 2026 Evangelion Manuhutu

#pragma once
#ifndef IGN_ASSET_STATE_HPP
#define IGN_ASSET_STATE_HPP

#include <cstdint>

namespace ignite
{
    enum class AssetState : std::uint8_t
    {
        Unloaded = 0,
        Queued,
        Loading,
        Finalizing,
        Ready,
        Failed,
        Unloading,
    };
}

#endif
