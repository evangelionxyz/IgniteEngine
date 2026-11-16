---
name: ignite-game-authoring
description: Author Ignite scenes, Lua 5.4 gameplay scripts, networked multiplayer entities, and package standalone game executables.
---

# Ignite Game Authoring, Lua Scripting & Packaging

## 1. Lua 5.4 Gameplay API
Scripts attached via `LuaScriptComponent` return a module table with lifecycle callbacks:
- `script.on_create(entity)` — called once on scene start or runtime script spawn
- `script.on_update(entity, dt)` — called every frame
- `script.on_fixed_update(entity, fixedDt)` — called at deterministic 60 Hz (`1.0 / 60.0`)
- `script.on_collision_enter(entity, other, contactPoint, contactNormal)` / `on_collision_stay` / `on_collision_exit`
- `script.on_destroy(entity)` — called on cleanup

### Core Globals & Math
- `Ignite.Vec2(x, y)`, `Ignite.Vec3(x, y, z)`, `Ignite.Vec4(x, y, z, w)` support `+`, `-`, `*`, `/`, unary `-`, `:length()`, `:normalized()`, `:dot(other)`, `:cross(other)`, `:distance(other)`, `:lerp(other, t)`.
- `Ignite.Math.Clamp`, `Lerp`, `Smoothstep`, `Deg2Rad`, `Rad2Deg`, `Dot`, `Cross`, `Distance`.
- `Ignite.CreateEntity(name)`, `Ignite.CreatePrimitive(primitiveType, name)`, `Ignite.FindEntity(uuid)` / `FindEntityByName(name)`, `Ignite.DestroyEntity(entity)`, `Ignite.DuplicateEntity(entity)`, `Ignite.GetEntitiesWith(componentName)`, `Ignite.InstantiatePrefab(assetPath)`.
- `Ignite.Input.IsKeyPressed(Ignite.Key.W)`, `IsMouseButtonPressed(Ignite.Mouse.Left)`, `IsActionPressed(name)`, `GetMousePosition()`, `GetMouseDelta()`, `SetMousePosition(x, y)`.
- `Ignite.Physics.Raycast(originVec3, dirVec3, maxDistance)` returns `nil` or `{ hit=true, entity=..., distance=..., fraction=..., point=Vec3, normal=Vec3 }`.
- `Ignite.Scene.Load(handleOrPath)`, `GetTime()`, `IsPlaying()`, `IsSimulating()`, `IsPaused()`, `GetPrimaryCamera()`.
- `Ignite.Net.IsConnected()`, `IsServerRunning()`, `GetClientId()`, `StartServer(port)`, `StopServer()`, `Connect(host, port, name)`, `Disconnect()`, `SendInput(dx, dy, jump, action, yaw)`, `SendChat(msg)`.

### Entity & Component Operations
```lua
local script = {}
function script.on_fixed_update(entity, dt)
    local tr = entity:get_component("Transform")
    local rb = entity:get_component("Rigidbody")
    if Ignite.Input.IsKeyPressed(Ignite.Key.W) then
        local fwd = tr.forward
        rb:apply_force(fwd * 25.0)
    end
end
return script
```
Supported component types in Lua: `Rendering`, `Transform`, `Rigidbody2D`, `Rigidbody3D`, `Sprite2D`, `Circle2D`, `StaticMesh`, `SkeletalMesh`, `AudioSource`, `Camera`, `DirectionalLight`, `PointLight`, `SpotLight`, `CharacterController`, `BoxCollider`, `SphereCollider`, `CapsuleCollider`, `BoxCollider2D`, `CircleCollider2D`, `Text`, `Widget`, `NetworkIdentity`.

## 2. Networking Model
- Mark synchronized entities with `NetworkIdentityComponent` (`netId`, `ownerClientId`, `isLocalPlayer`, `syncComponentsMask` bitflags).
- Run authoritative server with `Ignite.Network.exe --port 8192 --project <proj.ixproj> --scene <scene.ixscene> --tickrate 60`.
- Connect runtime client via `Ignite.Runtime.exe --project <proj.ixproj> --connect 127.0.0.1:8192`.
- Local predicted player smooths reconciliation errors via `NetClientPrediction` visual offset; remote replicated entities skip conflicting local client physics overrides.

## 3. Standalone Packaging
Export from Editor (`File -> Package Game...`), via `AgentController` (`package_game`), or MCP `ignite_package_game`:
- Outputs `<GameName>.exe` (renamed `Ignite.Runtime.exe`), optional `<GameName>_Server.exe`, non-editor DLLs, `resources/shaders/`, `runtimes/`, `<Project>.ixproj`, `<Project>.ixreg`, `Assets/`, `Scripts/`, `Bin/`.
- `<GameName>.exe` auto-discovers `<Project>.ixproj` in its executable directory and runs standalone.
