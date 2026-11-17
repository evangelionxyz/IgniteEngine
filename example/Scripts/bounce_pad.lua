local script = {}

local BOUNCE_SPEED = 16.5
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
        st.bounceVel = BOUNCE_SPEED
        cooldown = 0.35
        if _G.PlayParticleBurst then
            _G.PlayParticleBurst(p + Ignite.Vec3(0.0, 0.35, 0.0), "Fire", 750, 0.6)
        end
    end

    if st.showDebugOverlay then
        Ignite.Debug.DrawTextWorld(
            p + Ignite.Vec3(0.0, 1.1, 0.0),
            "BOUNCE PAD (+16.5 m/s)",
            Ignite.Vec4(0.2, 1.0, 0.55, 1.0),
            0.28,
            true,
            true
        )
    end
end

return script
