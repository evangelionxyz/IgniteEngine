// Copyright (c) 2026 Evangelion Manuhutu

using System;
using Ignite.Core;

namespace Ignite;

public static class Scene
{
    /// <summary>
    /// Transitions from the current active scene to the new scene specified by the asset handle.
    /// Note: The transition is deferred and will be executed at the end of the current frame.
    /// </summary>
    /// <param name="handle">The asset handle of the next scene to transition to.</param>
    public static void TransitionTo(AssetHandle handle)
    {
        CoreInternalCalls.Scene_TransitionTo(handle.Value);
    }
}
