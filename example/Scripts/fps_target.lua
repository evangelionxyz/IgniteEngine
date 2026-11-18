local script = {}

local home_pos = nil
local elapsed = 0.0
local hit_cooldown = 0.0
local respawn_timer = 0.0
local was_knocked = false

function script.on_create(entity)
    if not _G.FPSLuaState then
        _G.FPSLuaState = {
            luaTargetHits = 0,
            lastHitTarget = "None",
            resetEpoch = 0
        }
    end

    if entity:has_component("Transform") then
        local tr = entity:get_component("Transform")
        home_pos = Ignite.Vec3(tr.position.x, tr.position.y, tr.position.z)
    end
end

function script.on_update(entity, dt)
    if not entity:is_valid() or not home_pos then
        return
    end

    elapsed = elapsed + dt
    if hit_cooldown > 0.0 then
        hit_cooldown = hit_cooldown - dt
    end

    local tr = entity:get_component("Transform")
    local name = entity:get_name() or "Target"

    -- Detect dynamic impulse imparted by C# FPS raycast shot
    if entity:has_component("Rigidbody") then
        local rb = entity:get_component("Rigidbody")
        local vel = rb.linear_velocity
        local speed = math.sqrt(vel.x * vel.x + vel.y * vel.y + vel.z * vel.z)

        if speed > 1.8 and hit_cooldown <= 0.0 then
            hit_cooldown = 0.35
            was_knocked = true
            respawn_timer = 4.0

            if _G.FPSLuaState then
                _G.FPSLuaState.luaTargetHits = (_G.FPSLuaState.luaTargetHits or 0) + 1
                _G.FPSLuaState.lastHitTarget = name
            end
        end

        if was_knocked then
            respawn_timer = respawn_timer - dt
            if respawn_timer <= 0.0 or (tr.position.y < -3.0) then
                was_knocked = false
                rb.linear_velocity = Ignite.Vec3(0.0, 0.0, 0.0)
                rb.angular_velocity = Ignite.Vec3(0.0, 0.0, 0.0)
                tr.position = Ignite.Vec3(home_pos.x, home_pos.y, home_pos.z)
                tr.rotation = Ignite.Vec3(0.0, 0.0, 0.0)
            end
        elseif entity:has_component("Transform") then
            -- Gentle idle hover on zero-gravity targets when not knocked
            local phase = 0.0
            if string.find(name, "02") then phase = 1.3
            elseif string.find(name, "03") then phase = 2.6
            elseif string.find(name, "04") then phase = 3.9
            elseif string.find(name, "05") then phase = 5.1
            end

            if string.find(name, "Target_01") or string.find(name, "Target_02") or string.find(name, "Target_03") then
                local sway_x = math.sin(elapsed * 1.4 + phase) * 2.1
                local bob_y = math.cos(elapsed * 2.2 + phase) * 0.25
                tr.position = Ignite.Vec3(home_pos.x + sway_x, home_pos.y + bob_y, home_pos.z)
            end
        end
    end
end

return script
