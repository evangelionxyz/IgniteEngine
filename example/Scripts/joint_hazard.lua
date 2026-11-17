-- Copyright (c) 2026 Evangelion Manuhutu
local script = {}

local hazard_type = "Pendulum"
local anchor_entity = nil
local bob_entity = nil
local speed_smooth = 0.0

function script.on_create(entity)
    local name = entity:get_name()
    if string.find(name, "Sweeper") then
        hazard_type = "Sweeper"
    else
        hazard_type = "Pendulum"
    end
end

function script.on_update(entity, dt)
    local st = _G.DemoState
    local tr = entity:get_component("Transform")
    if not tr then return end

    local pos = tr.position

    if hazard_type == "Pendulum" then
        if not anchor_entity or not anchor_entity:is_valid() then
            anchor_entity = Ignite.FindEntityByName("Pendulum_Anchor")
        end

        local anchorPos = anchor_entity and anchor_entity:is_valid()
            and anchor_entity:get_component("Transform").position
            or (pos + Ignite.Vec3(0.0, 5.0, 0.0))

        -- Calculate pendulum speed
        local rb = entity:get_component("Rigidbody")
        local speed = 0.0
        if rb then
            speed = rb.linear_velocity:length()
        end
        speed_smooth = speed_smooth * 0.9 + speed * 0.1

        -- Draw taut suspension steel cable
        Ignite.Debug.DrawLine(anchorPos, pos, Ignite.Vec4(0.85, 0.92, 1.0, 1.0))
        Ignite.Debug.DrawLine(anchorPos + Ignite.Vec3(0.05, 0, 0), pos, Ignite.Vec4(0.4, 0.6, 0.8, 0.8))

        if st and st.showDebugOverlay then
            local label = string.format("PENDULUM JOINT  |  Speed: %.1f m/s", speed_smooth)
            Ignite.Debug.DrawTextWorld(
                pos + Ignite.Vec3(0.0, 1.4, 0.0),
                label,
                Ignite.Vec4(1.0, 0.45, 0.25, 1.0),
                0.28,
                true,
                true
            )
        end
    elseif hazard_type == "Sweeper" then
        -- Motorized rotating sweeper obstacle
        if st and st.showDebugOverlay then
            local label = "MOTOR HINGE SWEEPER (45 deg/s)"
            Ignite.Debug.DrawTextWorld(
                pos + Ignite.Vec3(0.0, 1.2, 0.0),
                label,
                Ignite.Vec4(1.0, 0.85, 0.2, 1.0),
                0.28,
                true,
                true
            )

            -- Draw swept radius circle
            local radius = 5.0
            local segs = 16
            for i = 0, segs - 1 do
                local a0 = (i / segs) * 2.0 * math.pi
                local a1 = ((i + 1) / segs) * 2.0 * math.pi
                local p0 = pos + Ignite.Vec3(math.cos(a0) * radius, 0.05, math.sin(a0) * radius)
                local p1 = pos + Ignite.Vec3(math.cos(a1) * radius, 0.05, math.sin(a1) * radius)
                Ignite.Debug.DrawLine(p0, p1, Ignite.Vec4(1.0, 0.3, 0.1, 0.5))
            end
        end
    end
end

return script
