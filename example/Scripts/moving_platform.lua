local script = {}

local base_pos = Ignite.Vec3(0.0, 0.0, 0.0)
local prev_pos = Ignite.Vec3(0.0, 0.0, 0.0)
local mode = "horizontal_x"
local amplitude = 4.0
local speed = 1.4
local phase = 0.0

function script.on_create(entity)
    local tr = entity:get_component("Transform")
    if tr then
        base_pos = tr.position
        prev_pos = tr.position
    end

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

    local t = Ignite.Scene.GetTime() * speed + phase
    local offset = Ignite.Vec3(0.0, 0.0, 0.0)

    if mode == "vertical" then
        offset.y = math.sin(t) * amplitude
    elseif mode == "horizontal_x" then
        offset.x = math.sin(t) * amplitude
    elseif mode == "horizontal_z" then
        offset.z = math.sin(t) * amplitude
    end

    local desired = base_pos + offset
    local current = tr.position
    local vel = (desired - current) / dt

    rb:activate()
    rb.linear_velocity = vel

    -- Carry player smoothly if standing above this platform
    local st = _G.DemoState
    if st and st.playerPos then
        local dp = st.playerPos - current
        if math.abs(dp.x) < 2.2 and math.abs(dp.z) < 2.2 and dp.y > 0.2 and dp.y < 1.8 then
            st.platformCarry = Ignite.Vec3(vel.x, 0.0, vel.z)
        end
    end

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

    prev_pos = current
end

return script
