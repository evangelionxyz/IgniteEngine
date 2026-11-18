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
    public static void Load(AssetHandle handle)
    {
        CoreInternalCalls.Scene_Load(handle.Value);
    }

    /// <summary>
    /// Transitions from the current active scene to the new scene specified by its project-relative filepath.
    /// </summary>
    public static void Load(string scenePath)
    {
        CoreInternalCalls.Scene_LoadByPath(scenePath);
    }

    /// <summary>
    /// Stops the active scene (exits Play/Simulate mode in the editor, or closes the standalone runtime).
    /// Note: Deferred to the end of the current frame.
    /// </summary>
    public static void Stop()
    {
        CoreInternalCalls.Scene_Stop();
    }
}
