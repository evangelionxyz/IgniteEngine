local script = {}

script.bounce_speed = Ignite.SerializeField(16.5):min(1.0):max(50.0):slider(5.0, 35.0):tooltip("Vertical launch speed when stepping on the bounce pad")
script.cooldown_duration = Ignite.SerializeField(0.35):range(0.05, 2.0):speed(0.05)

local cooldown = 0.0

function script.on_update(entity, dt)
    if cooldown > 0.0 then
        cooldown = math.max(0.0, cooldown - dt)
    end

    local st = _G.DemoState
    local tr = entity:get_component("Transform")
    if not st or not tr or not st.playerPos then return end

    local p = tr.position
    local dp = st.playerPos - p
    local horizDist = math.sqrt(dp.x * dp.x + dp.z * dp.z)

    if horizDist < 1.65 and dp.y >= -0.3 and dp.y < 1.45 and cooldown <= 0.0 then
        st.bounceRequested = true
        st.bounceVel = script.bounce_speed
        cooldown = script.cooldown_duration
        if _G.PlayParticleBurst then
            _G.PlayParticleBurst(p + Ignite.Vec3(0.0, 0.35, 0.0), "Fire", 750, 0.6)
        end
    end

    if st.showDebugOverlay then
        Ignite.Debug.DrawTextWorld(
            p + Ignite.Vec3(0.0, 1.1, 0.0),
            string.format("BOUNCE PAD (+%.1f m/s)", script.bounce_speed),
            Ignite.Vec4(0.2, 1.0, 0.55, 1.0),
            0.28,
            true,
            true
        )
    end
end

return script
