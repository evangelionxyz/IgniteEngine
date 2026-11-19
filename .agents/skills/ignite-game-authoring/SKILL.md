---
name: ignite-game-authoring
description: Author Ignite scenes, Lua 5.4 gameplay scripts with serialized inspector fields, networked multiplayer entities, and package standalone game executables.
---

# Ignite Game Authoring, Lua Scripting & Packaging

## 1. Lua 5.4 Gameplay API
Scripts attached via `LuaScriptComponent` return a module table with lifecycle callbacks:
- `script.on_create(entity)` — called once on scene start or runtime script spawn
- `script.on_update(entity, dt)` — called every frame
- `script.on_fixed_update(entity, fixedDt)` — called at deterministic 60 Hz (`1.0 / 60.0`)
- `script.on_collision_enter(entity, other, contactPoint, contactNormal)` / `on_collision_stay` / `on_collision_exit`
- `script.on_destroy(entity)` — called on cleanup

### Serialized Inspector Fields (Decorators)
Declare editor-exposed, serialized Lua properties on the returned `script` table using fluent `Ignite.SerializeField` builders or decorator helpers (automatically shown in the Inspector and persisted per entity in `.ixscene`):
```lua
local script = {}

script.bounce_speed = Ignite.SerializeField(16.5):min(1.0):max(50.0):slider(5.0, 35.0):tooltip("Launch speed")
script.cooldown = Ignite.SerializeField(0.35):range(0.05, 2.0):speed(0.05)
script.target = Ignite.EntityField()
script.fx_material = Ignite.AssetField("Material")

return script
```
Top-level helpers available alongside fluent chaining: `Ignite.Min`, `Ignite.Max`, `Ignite.Range`, `Ignite.Slider`, `Ignite.Speed`, `Ignite.Color`, `Ignite.Entity`, `Ignite.Asset`, `Ignite.Enum`, `Ignite.Int`, `Ignite.Float`, `Ignite.Tooltip`.

### Core Globals & Math
- `Ignite.Vec2(x, y)`, `Ignite.Vec3(x, y, z)`, `Ignite.Vec4(x, y, z, w)` support `+`, `-`, `*`, `/`, unary `-`, `:length()`, `:normalized()`, `:dot(other)`, `:cross(other)`, `:distance(other)`, `:lerp(other, t)`.
- `Ignite.Math.Clamp`, `Lerp`, `Smoothstep`, `Deg2Rad`, `Rad2Deg`, `Dot`, `Cross`, `Distance`.
- `Ignite.CreateEntity(name)`, `Ignite.CreatePrimitive(primitiveType, name)`, `Ignite.FindEntity(uuid)` / `FindEntityByName(name)`, `Ignite.DestroyEntity(entity)`, `Ignite.DuplicateEntity(entity)` (generates unique numbered names such as `Cube` → `Cube (1)` → `Cube (2)`), `Ignite.GetEntitiesWith(componentName)`, `Ignite.InstantiatePrefab(assetPath)`.
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
Supported component types in Lua (`PrimitiveName`: `Cube`, `Sphere`, `Plane`, `Cylinder`, `Cone`, `Torus`):
`Rendering`, `Transform`, `Rigidbody2D`, `Rigidbody` / `Rigidbody3D`, `Sprite2D`, `Circle2D`, `StaticMesh`, `SkeletalMesh`, `AudioSource`, `Camera`, `DirectionalLight`, `PointLight`, `SpotLight`, `CharacterController`, `BoxCollider`, `SphereCollider`, `CapsuleCollider`, `BoxCollider2D`, `CircleCollider2D`, `Text`, `Widget`, `NetworkIdentity`, `PhysicsJoint` / `Joint`, `ParticleSystem` / `Particle`.

### IDE Autocomplete & Stub Verification
API stubs live in `ignite/editor/resources/lua/ignite.d.lua` (and project `.luarc.json`). After modifying `lua.cpp` bindings, update `ignite.d.lua` and run `python scripts/gen_lua_stubs.py --check`.

## 2. Networking Model
- Mark synchronized entities with `NetworkIdentityComponent` (`netId`, `ownerClientId`, `isLocalPlayer`, `syncComponentsMask` bitflags).
- Run authoritative server with `Ignite.Network.exe --port 8192 --project <proj.ixproj> --scene <scene.ixscene> --tickrate 60`.
- Connect runtime client via `Ignite.Runtime.exe --project <proj.ixproj> --connect 127.0.0.1:8192`.
- Local predicted player smooths reconciliation errors via `NetClientPrediction` visual offset; remote replicated entities skip conflicting local client physics overrides.

## 3. Standalone Packaging
Export from Editor (`File -> Package Game...`), via `AgentController` (`package_game`), or MCP `ignite_package_game`:
- Outputs `<GameName>.exe` (renamed `Ignite.Runtime.exe`), optional `<GameName>_Server.exe`, non-editor DLLs, `resources/shaders/`, `runtimes/`, `<Project>.ixproj`, `<Project>.ixreg`, `Assets/`, `Scripts/`, `Bin/`.
- `<GameName>.exe` auto-discovers `<Project>.ixproj` in its executable directory and runs standalone.
