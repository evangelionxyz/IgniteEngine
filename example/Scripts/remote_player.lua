-- Copyright (c) 2026 Evangelion Manuhutu
local script = {}

local patrol_center = Ignite.Vec3(4.0, 1.35, -4.0)
local patrol_target = Ignite.Vec3(12.0, 1.35, -4.0)
local moving_to_target = true
local current_yaw = 90.0

function script.on_create(entity)
    local tr = entity:get_component("Transform")
    if tr then
        patrol_center = tr.position
        patrol_target = patrol_center + Ignite.Vec3(8.0, 0.0, 0.0)
    end
    Ignite.Log("Remote player avatar spawned: " .. entity:get_name())
end

function script.on_update(entity, dt)
    local tr = entity:get_component("Transform")
    local skel = entity:get_component("SkeletalMesh")
    local net = entity:get_component("NetworkIdentity")
    if not tr then return end

    local pos = tr.position
    local speed = 0.0
    local status = "Offline Patrol"

    -- Check if active network connection is driving this entity
    if Ignite.Net.IsConnected() and net and not net.is_local_player then
        status = "Net Client #2 Replicated"
        -- Speed is derived from transform delta
        speed = 0.0
    else
        -- Autonomous demo patrol when offline or showcasing
        local dest = moving_to_target and patrol_target or patrol_center
        local dir = dest - pos
        dir.y = 0.0
        local dist = dir:length()
        if dist > 0.25 then
            dir = dir:normalized()
            speed = 3.6
            pos = pos + dir * speed * dt
            tr.position = pos
            current_yaw = Ignite.Math.Rad2Deg(math.atan(dir.x, dir.z))
            tr.rotation = Ignite.Vec3(0.0, current_yaw, 0.0)
        else
            moving_to_target = not moving_to_target
            speed = 0.0
        end
    end

    -- Drive skeletal animation controller
    if skel then
        skel:set_float("Speed", speed)
        skel:set_bool("IsGrounded", true)
        skel:set_bool("IsDancing", false)
        skel:set_bool("IsWaving", speed < 0.1)
    end

    -- Floating world-space nametag
    local st = _G.DemoState
    if st and st.showDebugOverlay then
        local label = string.format("[Player 2 (Remote)] %s (%.1f m/s)", status, speed)
        Ignite.Debug.DrawTextWorld(
            pos + Ignite.Vec3(0.0, 2.25, 0.0),
            label,
            Ignite.Vec4(0.3, 0.85, 1.0, 1.0),
            0.28,
            true,
            true
        )
    end
end

return script
