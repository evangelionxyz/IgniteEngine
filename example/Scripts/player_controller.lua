local script = {}

local WALK_SPEED = 5.2
local RUN_SPEED = 9.0
local JUMP_SPEED = 9.5
local GRAVITY = 22.0
local COYOTE_TIME = 0.14
local JUMP_BUFFER_TIME = 0.14
local TURN_SPEED = 12.0

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
        platformCarry = Ignite.Vec3(0.0, 0.0, 0.0),
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
local prev_r = false
local prev_tab = false
local spawn_toggle = false

function script.on_create(entity)
    local st = ensure_state()
    local tr = entity:get_component("Transform")
    if tr then
        st.playerPos = tr.position
    end
    Ignite.Debug.SetShowEntityNames(true)
    Ignite.Log("PlayerController initialized on " .. entity:get_name())
end

function script.on_update(entity, dt)
    local st = ensure_state()
    local tr = entity:get_component("Transform")
    local cc = entity:get_component("CharacterController")
    local skel = entity:get_component("SkeletalMesh")
    if not tr or not cc then return end

    local pos = tr.position
    st.playerPos = pos

    -- Fall respawn safety
    if pos.y < -12.0 then
        cc:set_position(Ignite.Vec3(0.0, 2.5, 0.0))
        vert_vel = 0.0
        cc.linear_velocity = Ignite.Vec3(0.0, 0.0, 0.0)
        st.playerPos = Ignite.Vec3(0.0, 2.5, 0.0)
    end

    -- Edge-triggered actions
    local space_now = Ignite.Input.IsKeyPressed(Ignite.Key.Space)
    if space_now and not prev_space then
        jump_buffer = JUMP_BUFFER_TIME
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
            end
        end
    end
    prev_e = e_now

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
    if grounded then
        coyote_timer = COYOTE_TIME
        if vert_vel < 0.0 then
            vert_vel = -1.5
        end
    else
        coyote_timer = math.max(0.0, coyote_timer - dt)
        vert_vel = vert_vel - GRAVITY * dt
    end

    -- Bounce pad impulse
    if st.bounceRequested then
        vert_vel = st.bounceVel
        coyote_timer = 0.0
        jump_buffer = 0.0
        st.bounceRequested = false
    end

    -- Jump with coyote time & jump buffer
    if jump_buffer > 0.0 and coyote_timer > 0.0 then
        vert_vel = JUMP_SPEED
        jump_buffer = 0.0
        coyote_timer = 0.0
    end

    local horizVel = moveDir * targetSpeed + st.platformCarry
    st.platformCarry = Ignite.Vec3(0.0, 0.0, 0.0)

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
        skel:set_bool("IsGrounded", grounded)
        skel:set_bool("IsDancing", dancing and moveLen < 0.05)
        skel:set_bool("IsWaving", waving and moveLen < 0.05)
        st.animState = skel.current_state
    end

    -- World-space live telemetry above player when debug overlay is active
    if st.showDebugOverlay then
        local label = string.format("[%s] %.1f m/s %s", st.animState, targetSpeed, grounded and "GND" or "AIR")
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
