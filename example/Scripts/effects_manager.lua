-- Copyright (c) 2026 Evangelion Manuhutu
local script = {}

local active_effects = {}
local effect_counter = 0

function _G.PlayParticleBurst(pos, preset, maxParticles, duration)
    preset = preset or "Fire"
    maxParticles = maxParticles or 1000
    duration = duration or 1.0

    effect_counter = effect_counter + 1
    local name = string.format("BurstFX_%s_%d", preset, effect_counter)
    local ent = Ignite.CreateEntity(name)
    if not ent then return nil end

    local tr = ent:get_component("Transform")
    if tr then
        tr.position = pos
    end

    local ps = ent:add_component("ParticleSystem")
    if ps then
        ps:set_preset(preset)
        ps.max_particles = maxParticles
        ps.lifetime_min = 0.2
        ps.lifetime_max = duration * 0.85
        ps.emission_rate = maxParticles / math.max(0.2, duration * 0.5)
        ps.is_playing = true
    end

    table.insert(active_effects, {
        entity = ent,
        timer = duration
    })
    return ent
end

local prev_grounded = true
local footstep_timer = 0.0

function script.on_update(entity, dt)
    -- Clean up finished burst effects
    for i = #active_effects, 1, -1 do
        local fx = active_effects[i]
        fx.timer = fx.timer - dt
        if fx.timer <= 0.0 then
            if fx.entity and fx.entity:is_valid() then
                local ps = fx.entity:get_component("ParticleSystem")
                if ps then ps:stop() end
                fx.entity:destroy()
            end
            table.remove(active_effects, i)
        end
    end

    local st = _G.DemoState
    if not st or not st.playerPos or not st.playerVel then return end

    -- Trigger landing puff when player touches ground
    if st.isGrounded and not prev_grounded and math.abs(st.playerVel.y) > 2.0 then
        _G.PlayParticleBurst(st.playerPos + Ignite.Vec3(0.0, 0.1, 0.0), "Smoke", 350, 0.7)
    end
    prev_grounded = st.isGrounded

    -- Trigger sprinting footstep spark trail
    local speed = st.playerVel:length()
    if st.isGrounded and speed > 6.0 then
        footstep_timer = footstep_timer + dt
        if footstep_timer > 0.18 then
            footstep_timer = 0.0
            _G.PlayParticleBurst(st.playerPos + Ignite.Vec3(0.0, 0.08, 0.0), "Spark", 120, 0.4)
        end
    else
        footstep_timer = 0.0
    end
end

return script
