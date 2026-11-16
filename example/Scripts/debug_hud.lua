local script = {}

local fps_smooth = 60.0

function script.on_update(entity, dt)
    if dt > 0.0001 then
        local inst_fps = 1.0 / dt
        fps_smooth = fps_smooth * 0.9 + inst_fps * 0.1
    end

    local st = _G.DemoState
    if not st then return end

    -- Update the Screen-Space TextComponent banner if present in scene
    local banner = Ignite.FindEntityByName("HUD_Banner")
    if banner and banner:is_valid() and banner:has_component("Text") then
        local tx = banner:get_component("Text")
        if st.goalReached then
            tx.text = string.format("IGNITE LUA DEMO  |  SUMMIT REACHED!  |  Gems: %d/%d", st.score or 0, st.totalCollectibles or 0)
            tx.color = Ignite.Vec4(0.35, 1.0, 0.55, 1.0)
        else
            tx.text = string.format("IGNITE LUA DEMO  |  Collectibles: %d/%d  |  Anim: %s", st.score or 0, st.totalCollectibles or 0, st.animState or "Idle")
        end
    end

    -- Screen-Space Debug Font Rendering via Ignite.Debug.DrawTextScreen
    if st.showDebugOverlay then
        local pos = st.playerPos or Ignite.Vec3(0.0, 0.0, 0.0)
        local vel = st.playerVel or Ignite.Vec3(0.0, 0.0, 0.0)

        Ignite.Debug.DrawTextScreen(24, 56,
            string.format("FPS: %.0f  |  Pos: (%.2f, %.2f, %.2f)  |  Vel: (%.1f, %.1f, %.1f)",
                fps_smooth, pos.x, pos.y, pos.z, vel.x, vel.y, vel.z),
            Ignite.Vec4(0.82, 0.94, 1.0, 1.0), 18.0, false)

        Ignite.Debug.DrawTextScreen(24, 80,
            string.format("Grounded: %s  |  Anim State: %s  |  Raycast Target: %s (%.1fm)",
                tostring(st.isGrounded), tostring(st.animState), tostring(st.rayHitName), st.rayHitDist or 0.0),
            Ignite.Vec4(1.0, 0.90, 0.45, 1.0), 18.0, false)

        Ignite.Debug.DrawTextScreen(24, 108,
            "Controls: WASD Move | Shift Sprint | Space Jump | J/L/I/K or RMB Camera | F Dance | G Wave | E Blast/Spawn | R Reset Stack | Tab Debug Labels",
            Ignite.Vec4(0.85, 0.88, 0.92, 0.9), 15.0, false)
    end
end

return script
