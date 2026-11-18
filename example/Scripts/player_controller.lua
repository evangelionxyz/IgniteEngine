local script = {}

local WALK_SPEED = 5.2
local RUN_SPEED = 9.0
local JUMP_SPEED = 9.5
local COYOTE_TIME = 0.14
local JUMP_BUFFER_TIME = 0.14
local TURN_SPEED = 12.0
local INPUT_BUTTON_RESPAWN = 16
local INPUT_BUTTON_BOUNCE = 32
local BOUNCE_SPEED = 16.5
local RESPAWN_POSITION = Ignite.Vec3(0.0, 0.0, 0.0)

local function ensure_state()
    _G.DemoState = _G.DemoState or {
        camYaw = 0.0,
        camPitch = 20.0,
        playerPos = Ignite.Vec3(0.0, 2.0, 0.0),
        playerVel = Ignite.Vec3(0.0, 0.0, 0.0),
        playerYaw = 0.0,
        isGrounded = false,
        animState = "Idle",
        score = 0,
        totalCollectibles = 0,
        goalReached = false,
        showDebugOverlay = true,
        rayHitName = "None",
        rayHitDist = 0.0,
        spawnedCount = 0,
        resetPhysicsRequested = false,
        bounceRequested = false,
        bounceVel = 0.0,
    }
    return _G.DemoState
end

local function wrap_angle_deg(a)
    while a > 180.0 do a = a - 360.0 end
    while a < -180.0 do a = a + 360.0 end
    return a
end

local function lerp_angle_deg(current, target, t)
    local diff = wrap_angle_deg(target - current)
    return current + diff * math.min(1.0, t)
end

local coyote_timer = 0.0
local jump_buffer = 0.0
local vert_vel = 0.0
local current_yaw = 0.0
local prev_space = false
local prev_e = false
local prev_q = false
local prev_r = false
local prev_tab = false
local prev_f2 = false
local prev_f3 = false
local prev_f4 = false
local prev_t = false
local spawn_toggle = false
local net_sync_timer = 0.0

local server_jump_held = false
local server_respawn_jump_grace = false
local respawn_request_pending = false
local bounce_request_pending = false
local jump_send_latch = 0.0   -- keeps the jump bit set until it has been sent (and redundantly resent)
local server_coyote = 0.0
local server_jump_buffer = 0.0
local SERVER_COYOTE_TIME = COYOTE_TIME + 0.08
local SERVER_JUMP_BUFFER_TIME = JUMP_BUFFER_TIME + 0.08
local NET_SEND_INTERVAL = 1.0 / 60.0

function script.on_create(entity)
    local st = ensure_state()
    local tr = entity:get_component("Transform")
    if tr then
        st.playerPos = tr.position
        current_yaw = tr.rotation.y
        st.playerYaw = current_yaw
    end
    Ignite.Input.SetCursorMode("Disabled")
    Ignite.Debug.SetShowEntityNames(true)
    Ignite.Log("PlayerController initialized on " .. entity:get_name())
end

function script.on_destroy(entity)
    Ignite.Input.SetCursorMode("Normal")
end

function script.on_network_input(entity, client_id, move_x, move_y, buttons, yaw, dt)
    local cc = entity:get_component("CharacterController")
    if not cc then return end

    if (buttons & INPUT_BUTTON_RESPAWN) ~= 0 then
        cc.linear_velocity = Ignite.Vec3(0.0, 0.0, 0.0)
        cc:set_position(RESPAWN_POSITION)
        cc:set_rotation(Ignite.Vec3(0.0, yaw, 0.0))
        server_jump_held = false
        server_respawn_jump_grace = true
        server_coyote = 0.0
        server_jump_buffer = 0.0
        return
    end

    local grounded = cc.is_grounded
    if grounded then
        server_respawn_jump_grace = false
    end
    local ground_velocity = cc.ground_velocity
    local vertical_velocity = cc.linear_velocity.y - (grounded and ground_velocity.y or 0.0)
    if grounded and vertical_velocity < 0.0 then
        vertical_velocity = 0.0
    end

    local jump_held = (buttons & 1) ~= 0
    if (buttons & INPUT_BUTTON_BOUNCE) ~= 0 then
        vertical_velocity = BOUNCE_SPEED
        server_respawn_jump_grace = false
    else
        if grounded then
            server_coyote = SERVER_COYOTE_TIME
        else
            server_coyote = math.max(0.0, server_coyote - dt)
        end
        if jump_held and not server_jump_held then
            server_jump_buffer = SERVER_JUMP_BUFFER_TIME
        else
            server_jump_buffer = math.max(0.0, server_jump_buffer - dt)
        end
        if server_jump_buffer > 0.0 and (server_coyote > 0.0 or server_respawn_jump_grace) then
            vertical_velocity = JUMP_SPEED
            server_jump_buffer = 0.0
            server_coyote = 0.0
            server_respawn_jump_grace = false
        end
    end
    server_jump_held = jump_held

    local speed = (buttons & 2) ~= 0 and RUN_SPEED or WALK_SPEED
    cc.linear_velocity = Ignite.Vec3(move_x * speed, vertical_velocity, move_y * speed)
    cc:set_rotation(Ignite.Vec3(0.0, yaw, 0.0))
end

function script.on_update(entity, dt)
    local st = ensure_state()
    local tr = entity:get_component("Transform")
    local cc = entity:get_component("CharacterController")
    local skel = entity:get_component("SkeletalMesh")
    local net = entity:get_component("NetworkIdentity")
    if not tr or not cc then return end

    if net and not net.is_local_player then
        local remoteSpeed = net.replicated_movement_speed
        local remoteGrounded = net.replicated_is_grounded
        if Ignite.Net.IsServerRunning() then
            local movementVelocity = cc.linear_velocity - cc.ground_velocity
            remoteSpeed = math.sqrt(movementVelocity.x * movementVelocity.x + movementVelocity.z * movementVelocity.z)
            remoteGrounded = cc.is_supported
        end

        if skel then
            skel:set_float("Speed", remoteSpeed)
            skel:set_bool("IsGrounded", remoteGrounded)
            skel:set_bool("IsDancing", false)
            skel:set_bool("IsWaving", remoteSpeed < 0.1)
        end
        return
    end

    local pos = tr.position
    st.playerPos = pos

    -- Fall respawn safety
    if pos.y < -12.0 then
        if net and net.is_local_player and Ignite.Net.IsConnected() then
            respawn_request_pending = true
        end
        cc:set_position(RESPAWN_POSITION)
        vert_vel = 0.0
        cc.linear_velocity = Ignite.Vec3(0.0, 0.0, 0.0)
        st.playerPos = RESPAWN_POSITION
    end

    -- Edge-triggered actions
    local space_now = Ignite.Input.IsKeyPressed(Ignite.Key.Space)
    if space_now and not prev_space then
        jump_buffer = JUMP_BUFFER_TIME
        jump_send_latch = math.max(jump_send_latch, 0.12)
    else
        jump_buffer = math.max(0.0, jump_buffer - dt)
    end
    prev_space = space_now

    local tab_now = Ignite.Input.IsKeyPressed(Ignite.Key.Tab)
    if tab_now and not prev_tab then
        st.showDebugOverlay = not st.showDebugOverlay
        Ignite.Debug.SetShowEntityNames(st.showDebugOverlay)
    end
    prev_tab = tab_now

    local r_now = Ignite.Input.IsKeyPressed(Ignite.Key.R)
    if r_now and not prev_r then
        st.resetPhysicsRequested = true
    end
    prev_r = r_now

    -- Interact / Fire physics impulse or spawn dynamic object with E
    local e_now = Ignite.Input.IsKeyPressed(Ignite.Key.E)
    if e_now and not prev_e then
        local yawRad = Ignite.Math.Deg2Rad(current_yaw)
        local fwd = Ignite.Vec3(math.sin(yawRad), 0.0, math.cos(yawRad))
        local rayOrigin = pos + Ignite.Vec3(0.0, 1.1, 0.0) + fwd * 0.75
        local hit = Ignite.Physics.Raycast(rayOrigin, fwd, 12.0)

        if hit and hit.entity and hit.entity:has_component("Rigidbody") then
            local rb = hit.entity:get_component("Rigidbody")
            rb:apply_impulse(fwd * 18.0 + Ignite.Vec3(0.0, 5.0, 0.0))
            if _G.PlayParticleBurst then
                _G.PlayParticleBurst(hit.point, "Spark", 400, 0.8)
            end
        else
            spawn_toggle = not spawn_toggle
            st.spawnedCount = st.spawnedCount + 1
            local shape = spawn_toggle and "Sphere" or "Cube"
            local prop = Ignite.CreatePrimitive(shape, string.format("Dynamic_%s_%d", shape, st.spawnedCount))
            if prop then
                local ptr = prop:get_component("Transform")
                ptr.position = rayOrigin + fwd * 1.4 + Ignite.Vec3(0.0, 0.6, 0.0)
                ptr.scale = Ignite.Vec3(0.65, 0.65, 0.65)
                local rb = prop:add_component("Rigidbody")
                rb.mass = 2.0
                rb.restitution = spawn_toggle and 0.72 or 0.25
                rb.friction = 0.45
                if spawn_toggle then
                    local col = prop:add_component("SphereCollider")
                    col.radius = 0.5
                else
                    local col = prop:add_component("BoxCollider")
                    col.scale = Ignite.Vec3(0.5, 0.5, 0.5)
                end
                rb:apply_impulse(fwd * 12.0 + Ignite.Vec3(0.0, 4.0, 0.0))
                if _G.PlayParticleBurst then
                    _G.PlayParticleBurst(ptr.position, "Fire", 300, 0.6)
                end
            end
        end
    end
    prev_e = e_now

    -- Q: Nova Spark burst feature
    local q_now = Ignite.Input.IsKeyPressed(Ignite.Key.Q)
    if q_now and not prev_q then
        if _G.PlayParticleBurst then
            _G.PlayParticleBurst(pos + Ignite.Vec3(0.0, 0.9, 0.0), "Spark", 1500, 1.4)
            _G.PlayParticleBurst(pos + Ignite.Vec3(0.0, 0.2, 0.0), "Fire", 600, 0.8)
        end
        if Ignite.Net.IsConnected() then
            Ignite.Net.SendChat("Cast Nova Spark Burst!")
        end
        Ignite.Log("Triggered Nova Spark Burst at (" .. string.format("%.1f, %.1f, %.1f", pos.x, pos.y, pos.z) .. ")")
    end
    prev_q = q_now

    -- Multiplayer hotkeys: F2 (Host), F3 (Connect), F4 (Disconnect), T (Chat)
    local f2_now = Ignite.Input.IsKeyPressed(Ignite.Key.F2)
    if f2_now and not prev_f2 then
        if not Ignite.Net.IsServerRunning() then
            local ok = Ignite.Net.StartServer(8192)
            Ignite.Log("Host Server started on port 8192: " .. tostring(ok))
        else
            Ignite.Net.StopServer()
            Ignite.Log("Host Server stopped")
        end
    end
    prev_f2 = f2_now

    local f3_now = Ignite.Input.IsKeyPressed(Ignite.Key.F3)
    if f3_now and not prev_f3 then
        if not Ignite.Net.IsConnected() then
            local ok = Ignite.Net.Connect("127.0.0.1", 8192, "Player1")
            Ignite.Log("Connecting to 127.0.0.1:8192: " .. tostring(ok))
        end
    end
    prev_f3 = f3_now

    local f4_now = Ignite.Input.IsKeyPressed(Ignite.Key.F4)
    if f4_now and not prev_f4 then
        if Ignite.Net.IsConnected() then
            Ignite.Net.Disconnect()
            Ignite.Log("Disconnected from server")
        end
    end
    prev_f4 = f4_now

    local t_now = Ignite.Input.IsKeyPressed(Ignite.Key.T)
    if t_now and not prev_t then
        if Ignite.Net.IsConnected() then
            Ignite.Net.SendChat("Hello from Player1 at (" .. string.format("%.1f, %.1f", pos.x, pos.z) .. ")")
            Ignite.Log("Sent multiplayer chat message")
        end
    end
    prev_t = t_now

    -- Camera-relative movement input
    local inX = 0.0
    local inZ = 0.0
    if Ignite.Input.IsKeyPressed(Ignite.Key.W) or Ignite.Input.IsKeyPressed(Ignite.Key.Up) then inZ = inZ + 1.0 end
    if Ignite.Input.IsKeyPressed(Ignite.Key.S) or Ignite.Input.IsKeyPressed(Ignite.Key.Down) then inZ = inZ - 1.0 end
    if Ignite.Input.IsKeyPressed(Ignite.Key.D) or Ignite.Input.IsKeyPressed(Ignite.Key.Right) then inX = inX + 1.0 end
    if Ignite.Input.IsKeyPressed(Ignite.Key.A) or Ignite.Input.IsKeyPressed(Ignite.Key.Left) then inX = inX - 1.0 end

    local camYawRad = Ignite.Math.Deg2Rad(st.camYaw)
    -- Camera looks from behind player along (-sin(yaw), 0, -cos(yaw)) or forward along (sin, 0, cos)
    local camFwd = Ignite.Vec3(math.sin(camYawRad), 0.0, math.cos(camYawRad))
    local camRight = Ignite.Vec3(-math.cos(camYawRad), 0.0, math.sin(camYawRad))

    local moveDir = camFwd * inZ + camRight * inX
    local moveLen = moveDir:length()
    if moveLen > 0.01 then
        moveDir = moveDir / moveLen
    else
        moveDir = Ignite.Vec3(0.0, 0.0, 0.0)
    end

    local isSprinting = Ignite.Input.IsKeyPressed(Ignite.Key.LeftShift) or Ignite.Input.IsKeyPressed(Ignite.Key.RightShift)
    local targetSpeed = (moveLen > 0.01) and (isSprinting and RUN_SPEED or WALK_SPEED) or 0.0

    local grounded = cc.is_grounded
    st.isGrounded = grounded
    local groundVelocity = cc.ground_velocity
    vert_vel = cc.linear_velocity.y - (grounded and groundVelocity.y or 0.0)
    if grounded then
        coyote_timer = COYOTE_TIME
        if vert_vel < 0.0 then
            vert_vel = 0.0
        end
    else
        coyote_timer = math.max(0.0, coyote_timer - dt)
    end

    -- Bounce pad impulse
    if st.bounceRequested then
        vert_vel = st.bounceVel
        coyote_timer = 0.0
        jump_buffer = 0.0
        bounce_request_pending = Ignite.Net.IsConnected()
        st.bounceRequested = false
    end

    -- Jump with coyote time & jump buffer
    if jump_buffer > 0.0 and coyote_timer > 0.0 then
        vert_vel = JUMP_SPEED
        jump_buffer = 0.0
        coyote_timer = 0.0
        jump_send_latch = math.max(jump_send_latch, 0.12)
        if _G.PlayParticleBurst then
            _G.PlayParticleBurst(pos + Ignite.Vec3(0.0, 0.1, 0.0), "Smoke", 220, 0.5)
        end
    end

    local horizVel = moveDir * targetSpeed

    local finalVel = Ignite.Vec3(horizVel.x, vert_vel, horizVel.z)
    cc.linear_velocity = finalVel
    st.playerVel = finalVel

    -- Smooth rotation to face movement direction
    if moveLen > 0.01 then
        local targetYaw = Ignite.Math.Rad2Deg(math.atan(moveDir.x, moveDir.z))
        current_yaw = lerp_angle_deg(current_yaw, targetYaw, TURN_SPEED * dt)
        cc:set_rotation(Ignite.Vec3(0.0, current_yaw, 0.0))
        st.playerYaw = current_yaw
    end

    -- Send networked client input when connected
    if Ignite.Net.IsConnected() then
        net_sync_timer = net_sync_timer + dt
        jump_send_latch = math.max(0.0, jump_send_latch - dt)
        if net_sync_timer >= NET_SEND_INTERVAL then
            net_sync_timer = 0.0
            local buttons = 0
            if jump_send_latch > 0.0 or jump_buffer > 0.0 or space_now then buttons = buttons + 1 end
            if isSprinting then buttons = buttons + 2 end
            if respawn_request_pending then
                buttons = buttons + INPUT_BUTTON_RESPAWN
                respawn_request_pending = false
            end
            if bounce_request_pending then
                buttons = buttons + INPUT_BUTTON_BOUNCE
                bounce_request_pending = false
            end
            Ignite.Net.SendInput(moveDir.x, moveDir.z, buttons, current_yaw)
        end
    end

    -- Forward inspection raycast for debug HUD
    local yawRad = Ignite.Math.Deg2Rad(current_yaw)
    local lookDir = Ignite.Vec3(math.sin(yawRad), -0.1, math.cos(yawRad)):normalized()
    local inspectOrigin = pos + Ignite.Vec3(0.0, 1.0, 0.0)
    local inspectHit = Ignite.Physics.Raycast(inspectOrigin, lookDir, 18.0)
    if inspectHit and inspectHit.hit then
        st.rayHitDist = inspectHit.distance
        st.rayHitName = inspectHit.entity and inspectHit.entity:get_name() or "World"
        if st.showDebugOverlay then
            Ignite.Debug.DrawLine(inspectOrigin, inspectHit.point, Ignite.Vec4(1.0, 0.85, 0.2, 1.0))
        end
    else
        st.rayHitName = "None"
        st.rayHitDist = 0.0
    end

    -- Drive Skeletal Animation Controller parameters
    if skel then
        local dancing = Ignite.Input.IsKeyPressed(Ignite.Key.F) or st.goalReached
        local waving = Ignite.Input.IsKeyPressed(Ignite.Key.G)
        skel:set_float("Speed", targetSpeed)
        skel:set_bool("IsGrounded", cc.is_supported)
        skel:set_bool("IsDancing", dancing and moveLen < 0.05)
        skel:set_bool("IsWaving", waving and moveLen < 0.05)
        st.animState = skel.current_state
    end

    -- World-space live telemetry above player when debug overlay is active
    if st.showDebugOverlay then
        local netStatus = Ignite.Net.IsConnected() and "[ONLINE: ID " .. tostring(Ignite.Net.GetClientId()) .. "]"
            or (Ignite.Net.IsServerRunning() and "[HOST: Port 8192]" or "[OFFLINE]")
        local label = string.format("%s [Player 1: %s] %.1f m/s %s", netStatus, st.animState, targetSpeed, grounded and "GND" or "AIR")
        Ignite.Debug.DrawTextWorld(
            pos + Ignite.Vec3(0.0, 2.25, 0.0),
            label,
            Ignite.Vec4(0.35, 1.0, 0.75, 1.0),
            0.30,
            true,
            true
        )
    end
end

return script
