local script = {}

local elapsed = 0.0
local base_y = nil

function script.on_create(entity)
    if entity:has_component("Transform") then
        base_y = entity:get_component("Transform").position.y
    end
end

function script.on_update(entity, dt)
    if not entity:is_valid() or not entity:has_component("Transform") then
        return
    end

    elapsed = elapsed + dt
    local tr = entity:get_component("Transform")
    local name = entity:get_name() or ""

    local speed_deg = 28.0
    local bob_amp = 0.06
    if string.find(name, "Helmet") then
        speed_deg = 32.0
        bob_amp = 0.08
    elseif string.find(name, "Lantern") then
        speed_deg = -18.0
        bob_amp = 0.03
    elseif string.find(name, "JumpPad") then
        speed_deg = 65.0
        bob_amp = 0.0
    end

    local rot = tr.rotation
    tr.rotation = Ignite.Vec3(rot.x, (rot.y + speed_deg * dt) % 360.0, rot.z)

    if base_y and bob_amp > 0.0 then
        local p = tr.position
        tr.position = Ignite.Vec3(p.x, base_y + math.sin(elapsed * 2.0) * bob_amp, p.z)
    end
end

return script
