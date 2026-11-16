local script = {}

local base_pos = Ignite.Vec3(0.0, 0.0, 0.0)
local collected = false
local is_goal = false
local rot_y = 0.0

function script.on_create(entity)
    local tr = entity:get_component("Transform")
    if tr then
        base_pos = tr.position
    end
    is_goal = (entity:get_name() == "Goal_Trophy")

    _G.DemoState = _G.DemoState or {}
    if not is_goal then
        _G.DemoState.totalCollectibles = (_G.DemoState.totalCollectibles or 0) + 1
    end
end

function script.on_update(entity, dt)
    if collected then return end

    local tr = entity:get_component("Transform")
    if not tr then return end

    local t = Ignite.Scene.GetTime()
    rot_y = (rot_y + (is_goal and 65.0 or 110.0) * dt) % 360.0
    local bob = math.sin(t * 3.0 + base_pos.x) * 0.25
    tr.position = base_pos + Ignite.Vec3(0.0, bob, 0.0)
    tr.rotation = Ignite.Vec3(0.0, rot_y, 0.0)

    local st = _G.DemoState
    if st and st.playerPos then
        local d = (st.playerPos + Ignite.Vec3(0.0, 0.9, 0.0) - tr.position):length()
        local pickupRadius = is_goal and 2.2 or 1.55
        if d < pickupRadius then
            if is_goal then
                st.goalReached = true
            else
                collected = true
                st.score = (st.score or 0) + 1
                local rc = entity:get_component("Rendering")
                if rc then rc.visible = false end
                Ignite.Log("Collected " .. entity:get_name() .. "! Score: " .. tostring(st.score))
            end
        end
    end
end

return script
