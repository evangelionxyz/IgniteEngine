local script = {}

local base_pos = Ignite.Vec3(0.0, 0.0, 0.0)
local previous_target = nil
local platform_time = 0.0
local mode = "horizontal_x"
local amplitude = 4.0
local speed = 1.4
local phase = 0.0

function script.on_create(entity)
    local tr = entity:get_component("Transform")
    if tr then
        base_pos = tr.position
    end
    previous_target = nil
    platform_time = 0.0

    local name = entity:get_name()
    if name == "Platform_Elevator" then
        mode = "vertical"
        amplitude = 3.2
        speed = 1.3
    elseif name == "Platform_Shuttle_A" then
        mode = "horizontal_x"
        amplitude = 4.2
        speed = 1.5
    elseif name == "Platform_Shuttle_B" then
        mode = "horizontal_z"
        amplitude = 4.0
        speed = 1.35
        phase = 1.57
    end
end

function script.on_fixed_update(entity, dt)
    local tr = entity:get_component("Transform")
    local rb = entity:get_component("Rigidbody")
    if not tr or not rb or dt <= 0.0 then return end

    local net = entity:get_component("NetworkIdentity")
    if Ignite.Net.IsConnected() and net and not net.is_local_player then return end

    platform_time = platform_time + dt
    local t = platform_time * speed + phase
    local offset = Ignite.Vec3(0.0, 0.0, 0.0)

    if mode == "vertical" then
        offset.y = (math.sin(t) - math.sin(phase)) * amplitude
    elseif mode == "horizontal_x" then
        offset.x = (math.sin(t) - math.sin(phase)) * amplitude
    elseif mode == "horizontal_z" then
        offset.z = (math.sin(t) - math.sin(phase)) * amplitude
    end

    local desired = base_pos + offset
    local current = tr.position
    local vel = previous_target and (desired - previous_target) / dt or Ignite.Vec3(0.0, 0.0, 0.0)
    previous_target = desired

    rb:activate()
    rb.linear_velocity = vel

    local st = _G.DemoState
    if st and st.showDebugOverlay then
        Ignite.Debug.DrawTextWorld(
            current + Ignite.Vec3(0.0, 1.0, 0.0),
            string.format("%s (%.1f, %.1f, %.1f)", entity:get_name(), current.x, current.y, current.z),
            Ignite.Vec4(1.0, 0.7, 0.25, 0.95),
            0.26,
            true,
            true
        )
    end
end

return script
