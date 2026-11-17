---@meta
-- Ignite Engine Lua API (LuaCATS definitions for lua-language-server).
-- Keep in sync with ignite/engine/src/ignite/scripting/lua/lua.cpp
-- (verified by scripts/gen_lua_stubs.py --check).

---@alias PrimitiveName "Cube"|"Sphere"|"Plane"|"Cylinder"|"Cone"|"Torus"
---@alias ComponentName "Rendering"|"Transform"|"Rigidbody2D"|"Rigidbody"|"Rigidbody3D"|"Sprite2D"|"Circle2D"|"StaticMesh"|"SkeletalMesh"|"AudioSource"|"Camera"|"DirectionalLight"|"PointLight"|"SpotLight"|"CharacterController"|"BoxCollider"|"SphereCollider"|"CapsuleCollider"|"BoxCollider2D"|"CircleCollider2D"|"Text"|"Widget"|"NetworkIdentity"|"PhysicsJoint"|"Joint"|"ParticleSystem"|"Particle"

---@class Vec2
---@field x number
---@field y number
---@operator add(Vec2): Vec2
---@operator sub(Vec2): Vec2
---@operator mul(number|Vec2): Vec2
---@operator div(number|Vec2): Vec2
---@operator unm: Vec2
local Vec2 = {}
---@return number
function Vec2:length() end
---@return Vec2
function Vec2:normalized() end
---@param other Vec2
---@return number
function Vec2:dot(other) end
---@param other Vec2
---@return number
function Vec2:distance(other) end
---@param other Vec2
---@param t number
---@return Vec2
function Vec2:lerp(other, t) end

---@class Vec3
---@field x number
---@field y number
---@field z number
---@operator add(Vec3): Vec3
---@operator sub(Vec3): Vec3
---@operator mul(number|Vec3): Vec3
---@operator div(number|Vec3): Vec3
---@operator unm: Vec3
local Vec3 = {}
---@return number
function Vec3:length() end
---@return Vec3
function Vec3:normalized() end
---@param other Vec3
---@return number
function Vec3:dot(other) end
---@param other Vec3
---@return Vec3
function Vec3:cross(other) end
---@param other Vec3
---@return number
function Vec3:distance(other) end
---@param other Vec3
---@param t number
---@return Vec3
function Vec3:lerp(other, t) end

---@class Vec4
---@field x number
---@field y number
---@field z number
---@field w number
---@operator add(Vec4): Vec4
---@operator sub(Vec4): Vec4
---@operator mul(number|Vec4): Vec4
---@operator div(number|Vec4): Vec4
---@operator unm: Vec4
local Vec4 = {}
---@return number
function Vec4:length() end
---@return Vec4
function Vec4:normalized() end
---@param other Vec4
---@return number
function Vec4:dot(other) end
---@param other Vec4
---@return number
function Vec4:distance(other) end
---@param other Vec4
---@param t number
---@return Vec4
function Vec4:lerp(other, t) end

---@class Component
---@field [string] any  Component properties are dynamic; see the specific component classes.
local Component = {}

---@class TransformComponent : Component
---@field position Vec3 Local position
---@field rotation Vec3 Local Euler rotation in degrees
---@field scale Vec3
---@field world_position Vec3
---@field world_scale Vec3
---@field forward Vec3
---@field right Vec3
---@field up Vec3
local TransformComponent = {}
---@param delta Vec3
function TransformComponent:translate(delta) end
---@param eulerDegrees Vec3
function TransformComponent:rotate(eulerDegrees) end
---@param target Vec3
function TransformComponent:look_at(target) end

---@class RenderingComponent : Component
---@field visible boolean
local RenderingComponent = {}
---@param visible boolean
function RenderingComponent:set_visible(visible) end

---@class Rigidbody2DComponent : Component
---@field linear_velocity Vec2
---@field angular_velocity number
---@field gravity_scale number
---@field linear_damping number
---@field angular_damping number
---@field fixed_rotation boolean
---@field is_awake boolean
local Rigidbody2DComponent = {}
function Rigidbody2DComponent:apply_force(...) end
function Rigidbody2DComponent:apply_linear_impulse(...) end
function Rigidbody2DComponent:apply_torque(...) end

---@class RigidbodyComponent : Component
---@field linear_velocity Vec3
---@field angular_velocity Vec3
---@field use_gravity boolean
---@field gravity_factor number
---@field friction number
---@field restitution number
---@field mass number
---@field linear_damping number
---@field angular_damping number

---@class SkeletalMeshComponent : Component
---@field animator_handle integer
---@field current_state string
---@field state_elapsed number
---@field state_normalized number
---@field unique_animator boolean
local SkeletalMeshComponent = {}
---@param name string
---@param value number
function SkeletalMeshComponent:set_float(name, value) end
---@param name string
---@param value boolean
function SkeletalMeshComponent:set_bool(name, value) end
---@param name string
---@param value integer
function SkeletalMeshComponent:set_int(name, value) end
---@param name string
---@param value string
function SkeletalMeshComponent:set_string(name, value) end
---@param name string
---@return number
function SkeletalMeshComponent:get_float(name) end
---@param name string
---@return boolean
function SkeletalMeshComponent:get_bool(name) end
---@param name string
---@return integer
function SkeletalMeshComponent:get_int(name) end
---@param state string
function SkeletalMeshComponent:play_state(state) end

---@class AudioSourceComponent : Component
---@field volume number
---@field pitch number
---@field pan number
---@field loop boolean
---@field play_on_start boolean
---@field is_playing boolean
---@field is_paused boolean
local AudioSourceComponent = {}
function AudioSourceComponent:play() end
function AudioSourceComponent:stop() end
function AudioSourceComponent:pause() end
function AudioSourceComponent:resume() end

---@class CameraComponent : Component
---@field primary boolean
---@field fov number
---@field near_plane number
---@field far_plane number
---@field near_clip number
---@field far_clip number
---@field ortho_size number
---@field projection_type "perspective"|"orthographic"

---@class LightComponent : Component
---@field color Vec4
---@field intensity number
---@field range number
---@field enabled boolean
---@field inner_cone_angle number
---@field outer_cone_angle number
---@field shadow_distance number
---@field cascade_shadow boolean

---@class ColliderComponent : Component
---@field center Vec3
---@field size Vec3
---@field scale Vec3
---@field radius number
---@field height number
---@field offset Vec2
---@field friction number
---@field restitution number
---@field density number
---@field is_sensor boolean

---@class Entity
---@field id integer
---@field valid boolean
---@field name string
local Entity = {}
---@param name ComponentName
---@return boolean
function Entity:has_component(name) end
---@param name "Transform"
---@return TransformComponent
---@overload fun(self: Entity, name: "Rendering"): RenderingComponent
---@overload fun(self: Entity, name: "Rigidbody2D"): Rigidbody2DComponent
---@overload fun(self: Entity, name: "Rigidbody"|"Rigidbody3D"): RigidbodyComponent
---@overload fun(self: Entity, name: "SkeletalMesh"): SkeletalMeshComponent
---@overload fun(self: Entity, name: "AudioSource"): AudioSourceComponent
---@overload fun(self: Entity, name: "Camera"): CameraComponent
---@overload fun(self: Entity, name: "DirectionalLight"|"PointLight"|"SpotLight"): LightComponent
---@overload fun(self: Entity, name: "BoxCollider"|"SphereCollider"|"CapsuleCollider"|"BoxCollider2D"|"CircleCollider2D"): ColliderComponent
---@overload fun(self: Entity, name: ComponentName): Component
function Entity:get_component(name) end
---@param name ComponentName
---@return Component
function Entity:add_component(name) end
---@param name ComponentName
function Entity:remove_component(name) end
---@return TransformComponent
function Entity:get_transform() end
function Entity:destroy() end
---@return boolean
function Entity:is_valid() end
---@return string
function Entity:get_name() end
---@return Entity?
function Entity:get_parent() end
---@param parent Entity?
function Entity:set_parent(parent) end
---@return Entity[]
function Entity:get_children() end

---@class RaycastHit
---@field hit boolean
---@field point Vec3
---@field normal Vec3
---@field fraction number
---@field distance number
---@field entity Entity?

---@class NetStats
---@field rtt integer
---@field packet_loss number
---@field in_kbps number
---@field out_kbps number
---@field client_count integer

---Ignite global API.
Ignite = {}

---@param ... any
function Ignite.Log(...) end
---@param ... any
function Ignite.LogWarn(...) end
---@param ... any
function Ignite.LogError(...) end
---@param x number
---@param y number
---@return Vec2
function Ignite.Vec2(x, y) end
---@param x number
---@param y number
---@param z number
---@return Vec3
function Ignite.Vec3(x, y, z) end
---@param x number
---@param y number
---@param z number
---@param w number
---@return Vec4
function Ignite.Vec4(x, y, z, w) end

---@param id integer
---@return Entity?
function Ignite.FindEntity(id) end
---@param name string
---@return Entity?
function Ignite.FindEntityByName(name) end
---@param name? string
---@return Entity
function Ignite.CreateEntity(name) end
---@param primitive PrimitiveName
---@return Entity
function Ignite.CreatePrimitive(primitive) end
---@param entity Entity
function Ignite.DestroyEntity(entity) end
---@param entity Entity
---@return Entity
function Ignite.DuplicateEntity(entity) end
---@param ... ComponentName
---@return Entity[]
function Ignite.GetEntitiesWith(...) end
---@param prefab string
---@return Entity?
function Ignite.InstantiatePrefab(prefab, ...) end

---@class LuaFieldDesc
local LuaFieldDesc = {}
---@param minValue number
---@return LuaFieldDesc
function LuaFieldDesc:min(minValue) end
---@param maxValue number
---@return LuaFieldDesc
function LuaFieldDesc:max(maxValue) end
---@param minValue number
---@param maxValue number
---@return LuaFieldDesc
function LuaFieldDesc:range(minValue, maxValue) end
---@param minValue? number
---@param maxValue? number
---@return LuaFieldDesc
function LuaFieldDesc:slider(minValue, maxValue) end
---@param step number
---@return LuaFieldDesc
function LuaFieldDesc:speed(step) end
---@return LuaFieldDesc
function LuaFieldDesc:color() end
---@return LuaFieldDesc
function LuaFieldDesc:entity() end
---@param assetType? string
---@return LuaFieldDesc
function LuaFieldDesc:asset(assetType) end
---@param options string[]
---@return LuaFieldDesc
function LuaFieldDesc:enum(options) end
---@return LuaFieldDesc
function LuaFieldDesc:int() end
---@return LuaFieldDesc
function LuaFieldDesc:float() end
---@param text string
---@return LuaFieldDesc
function LuaFieldDesc:tooltip(text) end

---@param defaultValue? any
---@return LuaFieldDesc
function Ignite.SerializeField(defaultValue) end
---@return LuaFieldDesc
function Ignite.EntityField() end
---@param assetType? string
---@return LuaFieldDesc
function Ignite.AssetField(assetType) end
---@param minValue number
---@param field? any
---@return LuaFieldDesc | fun(field: any): LuaFieldDesc
function Ignite.Min(minValue, field) end
---@param maxValue number
---@param field? any
---@return LuaFieldDesc | fun(field: any): LuaFieldDesc
function Ignite.Max(maxValue, field) end
---@param minValue number
---@param maxValue number
---@param field? any
---@return LuaFieldDesc | fun(field: any): LuaFieldDesc
function Ignite.Range(minValue, maxValue, field) end
---@param minValue? number|any
---@param maxValue? number
---@param field? any
---@return LuaFieldDesc | fun(field: any): LuaFieldDesc
function Ignite.Slider(minValue, maxValue, field) end
---@param step number
---@param field? any
---@return LuaFieldDesc | fun(field: any): LuaFieldDesc
function Ignite.Speed(step, field) end
---@param field? any
---@return LuaFieldDesc | fun(field: any): LuaFieldDesc
function Ignite.Color(field) end
---@param field? any
---@return LuaFieldDesc | fun(field: any): LuaFieldDesc
function Ignite.Entity(field) end
---@param assetType? string|any
---@param field? any
---@return LuaFieldDesc | fun(field: any): LuaFieldDesc
function Ignite.Asset(assetType, field) end
---@param options string[]
---@param field? any
---@return LuaFieldDesc | fun(field: any): LuaFieldDesc
function Ignite.Enum(options, field) end
---@param field? any
---@return LuaFieldDesc | fun(field: any): LuaFieldDesc
function Ignite.Int(field) end
---@param field? any
---@return LuaFieldDesc | fun(field: any): LuaFieldDesc
function Ignite.Float(field) end
---@param text string
---@param field? any
---@return LuaFieldDesc | fun(field: any): LuaFieldDesc
function Ignite.Tooltip(text, field) end

---@class IgniteMath
Ignite.Math = {}
---@param a number
---@param b number
---@param t number
---@return number
function Ignite.Math.Lerp(a, b, t) end
---@param v number
---@param min number
---@param max number
---@return number
function Ignite.Math.Clamp(v, min, max) end
---@param edge0 number
---@param edge1 number
---@param x number
---@return number
function Ignite.Math.Smoothstep(edge0, edge1, x) end
---@param deg number
---@return number
function Ignite.Math.Deg2Rad(deg) end
---@param rad number
---@return number
function Ignite.Math.Rad2Deg(rad) end
---@generic V
---@param a V
---@param b V
---@return number
function Ignite.Math.Dot(a, b) end
---@param a Vec3
---@param b Vec3
---@return Vec3
function Ignite.Math.Cross(a, b) end
---@generic V
---@param a V
---@param b V
---@return number
function Ignite.Math.Distance(a, b) end
---@param x number
---@param y number
---@return Vec2
function Ignite.Math.Vec2(x, y) end
---@param x number
---@param y number
---@param z number
---@return Vec3
function Ignite.Math.Vec3(x, y, z) end
---@param x number
---@param y number
---@param z number
---@param w number
---@return Vec4
function Ignite.Math.Vec4(x, y, z, w) end

---@class IgniteScene
Ignite.Scene = {}
---@param path string
function Ignite.Scene.Load(path) end
---@return number
function Ignite.Scene.GetTime() end
---@return boolean
function Ignite.Scene.IsPlaying() end
---@return boolean
function Ignite.Scene.IsSimulating() end
---@return boolean
function Ignite.Scene.IsPaused() end
---@return Entity?
function Ignite.Scene.GetPrimaryCamera() end

---@class ignitePhysics
Ignite.Physics = {}
---@param origin Vec3
---@param direction Vec3
---@param maxDistance? number
---@return RaycastHit
function Ignite.Physics.Raycast(origin, direction, maxDistance) end

---@class IgniteDebug
Ignite.Debug = {}
function Ignite.Debug.DrawTextWorld(...) end
function Ignite.Debug.DrawTextScreen(...) end
function Ignite.Debug.DrawLine(...) end
---@param show boolean
function Ignite.Debug.SetShowEntityNames(show) end
---@return boolean
function Ignite.Debug.GetShowEntityNames() end

---@class IgniteNet
Ignite.Net = {}
---@return boolean
function Ignite.Net.IsConnected() end
---@return boolean
function Ignite.Net.IsServerRunning() end
---@return boolean
function Ignite.Net.IsConnecting() end
---@return integer
function Ignite.Net.GetClientId() end
function Ignite.Net.StartServer(...) end
function Ignite.Net.StopServer() end
function Ignite.Net.Connect(...) end
function Ignite.Net.Disconnect() end
function Ignite.Net.SpawnEntity(...) end
function Ignite.Net.SendInput(...) end
---@param message string
function Ignite.Net.SendChat(message) end
---@return NetStats
function Ignite.Net.GetStats() end
---@return integer
function Ignite.Net.GetServerTick() end
---@return integer
function Ignite.Net.GetClientServerTick() end
---@return integer
function Ignite.Net.GetClientCount() end

---@class IgniteInput
Ignite.Input = {}
---@param key integer Ignite.Key.*
---@return boolean
function Ignite.Input.IsKeyPressed(key) end
---@param button integer Ignite.Mouse.*
---@return boolean
function Ignite.Input.IsMouseButtonPressed(button) end
---@param action string
---@return boolean
function Ignite.Input.IsActionPressed(action) end
---@return Vec2
function Ignite.Input.GetMousePosition() end
---@return Vec2
function Ignite.Input.GetMouseDelta() end
---@param x number
---@param y number
function Ignite.Input.SetMousePosition(x, y) end

---@class IgniteKey
---@field Space integer
---@field Enter integer
---@field Escape integer
---@field Tab integer
---@field Backspace integer
---@field Left integer
---@field Right integer
---@field Up integer
---@field Down integer
---@field LeftShift integer
---@field RightShift integer
---@field LeftControl integer
---@field RightControl integer
---@field LeftAlt integer
---@field RightAlt integer
---@field A integer
---@field B integer
---@field C integer
---@field D integer
---@field E integer
---@field F integer
---@field G integer
---@field H integer
---@field I integer
---@field J integer
---@field K integer
---@field L integer
---@field M integer
---@field N integer
---@field O integer
---@field P integer
---@field Q integer
---@field R integer
---@field S integer
---@field T integer
---@field U integer
---@field V integer
---@field W integer
---@field X integer
---@field Y integer
---@field Z integer
---@field D0 integer
---@field D1 integer
---@field D2 integer
---@field D3 integer
---@field D4 integer
---@field D5 integer
---@field D6 integer
---@field D7 integer
---@field D8 integer
---@field D9 integer
---@field F1 integer
---@field F2 integer
---@field F3 integer
---@field F4 integer
---@field F5 integer
---@field F6 integer
---@field F7 integer
---@field F8 integer
---@field F9 integer
---@field F10 integer
---@field F11 integer
---@field F12 integer
Ignite.Key = {}

---@class IgniteMouse
---@field Left integer
---@field Right integer
---@field Middle integer
Ignite.Mouse = {}

---Callback table a gameplay script returns.
---@class ScriptCallbacks
---@field on_create? fun(entity: Entity)
---@field on_update? fun(entity: Entity, dt: number)
---@field on_fixed_update? fun(entity: Entity, fixedDt: number) Called at 60 Hz
---@field on_collision_enter? fun(entity: Entity, other: Entity, contactPoint: Vec3, contactNormal: Vec3)
---@field on_collision_stay? fun(entity: Entity, other: Entity, contactPoint: Vec3, contactNormal: Vec3)
---@field on_collision_exit? fun(entity: Entity, other: Entity, contactPoint: Vec3, contactNormal: Vec3)
---@field on_destroy? fun(entity: Entity)


