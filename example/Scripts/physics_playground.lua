local script = {}

local crate_starts = {}

function script.on_create(entity)
    -- Record initial transforms of all physics crates & spheres so R resets them cleanly
    local all = Ignite.GetEntitiesWith("Rigidbody")
    for _, e in ipairs(all) do
        local name = e:get_name()
        if string.find(name, "PhysCrate_") or string.find(name, "PhysSphere_") then
            local tr = e:get_component("Transform")
            table.insert(crate_starts, {
                entity = e,
                pos = tr.position,
                rot = tr.rotation,
            })
        end
    end
end

function script.on_update(entity, dt)
    local st = _G.DemoState
    if not st then return end

    if st.resetPhysicsRequested then
        st.resetPhysicsRequested = false
        for _, item in ipairs(crate_starts) do
            if item.entity:is_valid() then
                local tr = item.entity:get_component("Transform")
                local rb = item.entity:get_component("Rigidbody")
                tr.position = item.pos
                tr.rotation = item.rot
                if rb then
                    rb.linear_velocity = Ignite.Vec3(0.0, 0.0, 0.0)
                    rb.angular_velocity = Ignite.Vec3(0.0, 0.0, 0.0)
                    rb:activate()
                end
            end
        end
    end

    if st.showDebugOverlay then
        for _, item in ipairs(crate_starts) do
            if item.entity:is_valid() then
                local tr = item.entity:get_component("Transform")
                local rb = item.entity:get_component("Rigidbody")
                local spd = rb and rb.linear_velocity:length() or 0.0
                if spd > 0.25 then
                    Ignite.Debug.DrawTextWorld(
                        tr.position + Ignite.Vec3(0.0, 0.9, 0.0),
                        string.format("%.1f m/s", spd),
                        Ignite.Vec4(0.55, 1.0, 0.45, 1.0),
                        0.24,
                        true,
                        true
                    )
                end
            end
        end
    end
end

return script
