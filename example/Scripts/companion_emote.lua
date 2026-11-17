local script = {}

function script.on_create(entity)
    local skel = entity:get_component("SkeletalMesh")
    if skel then
        skel:set_bool("IsWaving", true)
    end
end

function script.on_update(entity, dt)
    local skel = entity:get_component("SkeletalMesh")
    if skel then
        skel:set_bool("IsWaving", true)
    end
end

return script
