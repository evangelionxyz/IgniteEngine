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

        -- 1. Telemetry & Position
        Ignite.Debug.DrawTextScreen(24, 56,
            string.format("FPS: %.0f  |  Pos: (%.2f, %.2f, %.2f)  |  Vel: (%.1f, %.1f, %.1f)  |  Anim: %s",
                fps_smooth, pos.x, pos.y, pos.z, vel.x, vel.y, vel.z, tostring(st.animState)),
            Ignite.Vec4(0.82, 0.94, 1.0, 1.0), 18.0, false)

        -- 2. Multiplayer Live Status
        local netText = ""
        local netColor = Ignite.Vec4(0.7, 0.7, 0.7, 1.0)
        if Ignite.Net.IsConnected() then
            local stats = Ignite.Net.GetStats()
            local clientId = Ignite.Net.GetClientId()
            local tick = Ignite.Net.GetClientServerTick()
            netText = string.format("NET: CONNECTED  |  Client ID: %d  |  RTT: %d ms  |  Loss: %.1f%%  |  Server Tick: %d",
                clientId, stats.rtt or 0, stats.packet_loss or 0.0, tick)
            netColor = Ignite.Vec4(0.3, 1.0, 0.5, 1.0)
        elseif Ignite.Net.IsServerRunning() then
            local count = Ignite.Net.GetClientCount()
            local tick = Ignite.Net.GetServerTick()
            netText = string.format("NET: DEDICATED / LISTEN SERVER ACTIVE (Port 8192)  |  Clients: %d  |  Tick: %d",
                count, tick)
            netColor = Ignite.Vec4(0.35, 0.85, 1.0, 1.0)
        else
            netText = "NET: OFFLINE (Press F2 to Host Server, F3 to Connect, F4 to Disconnect)"
            netColor = Ignite.Vec4(1.0, 0.85, 0.35, 1.0)
        end
        Ignite.Debug.DrawTextScreen(24, 80, netText, netColor, 18.0, false)

        -- 3. Features & Environment
        Ignite.Debug.DrawTextScreen(24, 104,
            string.format("Hazards: Swinging Pendulum Joint + Motor Hinge Sweeper Active  |  Particles: Fire + Spark + Smoke  |  Ray: %s (%.1fm)",
                tostring(st.rayHitName), st.rayHitDist or 0.0),
            Ignite.Vec4(1.0, 0.65, 0.9, 1.0), 17.0, false)

        -- 4. Controls Guide
        Ignite.Debug.DrawTextScreen(24, 128,
            "Controls: WASD Move | Shift Sprint | Space Jump | Q Nova Spark | E Impulse/Spawn | F Dance | F2 Host | F3 Connect | F4 Disconnect | T Chat | Tab Labels",
            Ignite.Vec4(0.88, 0.92, 0.96, 0.95), 15.0, false)
    end
end

return script
