local script = {}

local CAM_DIST = 6.8
local MIN_DIST = 4.0
local MAX_DIST = 18.0
local ORBIT_SPEED = 95.0
local MOUSE_SENS = 0.25

local yaw = 0.0
local pitch = 20.0
local dist = CAM_DIST
local smooth_target = Ignite.Vec3(0.0, 1.35, -3.0)

function script.on_create(entity)
    _G.DemoState = _G.DemoState or {}
    _G.DemoState.camYaw = yaw
    _G.DemoState.camPitch = pitch
end

function script.on_update(entity, dt)
    local st = _G.DemoState
    if not st then return end

    -- Keyboard orbit (J/L yaw, I/K pitch, U/O zoom)
    if Ignite.Input.IsKeyPressed(Ignite.Key.J) then yaw = yaw + ORBIT_SPEED * dt end
    if Ignite.Input.IsKeyPressed(Ignite.Key.L) then yaw = yaw - ORBIT_SPEED * dt end
    if Ignite.Input.IsKeyPressed(Ignite.Key.I) then pitch = math.min(68.0, pitch + 60.0 * dt) end
    if Ignite.Input.IsKeyPressed(Ignite.Key.K) then pitch = math.max(-5.0, pitch - 60.0 * dt) end
    if Ignite.Input.IsKeyPressed(Ignite.Key.U) then dist = math.max(MIN_DIST, dist - 8.0 * dt) end
    if Ignite.Input.IsKeyPressed(Ignite.Key.O) then dist = math.min(MAX_DIST, dist + 8.0 * dt) end

    -- Mouse look when cursor is locked (Disabled == 2) or while right-mouse is held
    if Ignite.Input.GetCursorMode() == 2 or Ignite.Input.IsMouseButtonPressed(Ignite.Mouse.Right) then
        local delta = Ignite.Input.GetMouseDelta()
        yaw = yaw - delta.x * MOUSE_SENS
        pitch = Ignite.Math.Clamp(pitch + delta.y * MOUSE_SENS, -5.0, 68.0)
    end

    st.camYaw = yaw
    st.camPitch = pitch

    local player = Ignite.FindEntityByName("PlayerRobot")
    local target = st.playerPos or Ignite.Vec3(0.0, 1.5, 0.0)
    if player and player:is_valid() then
        target = player:get_component("Transform").position + Ignite.Vec3(0.0, 1.35, 0.0)
    end

    smooth_target = smooth_target:lerp(target, math.min(1.0, 10.0 * dt))

    local yawRad = Ignite.Math.Deg2Rad(yaw)
    local pitchRad = Ignite.Math.Deg2Rad(pitch)

    -- Camera sits behind player (-forward) and above
    local offset = Ignite.Vec3(
        -math.sin(yawRad) * math.cos(pitchRad),
        math.sin(pitchRad),
        -math.cos(yawRad) * math.cos(pitchRad)
    ) * dist

    local camPos = smooth_target + offset

    -- Simple raycast occlusion avoidance
    local dir = (camPos - smooth_target)
    local len = dir:length()
    if len > 0.1 then
        local hit = Ignite.Physics.Raycast(smooth_target, dir / len, len)
        if hit and hit.hit and hit.entity and hit.entity:get_name() ~= "PlayerRobot" then
            camPos = smooth_target + (dir / len) * math.max(1.8, hit.distance - 0.35)
        end
    end

    local tr = entity:get_component("Transform")
    if tr then
        tr.position = camPos
        tr:look_at(smooth_target)
    end
end

return script
