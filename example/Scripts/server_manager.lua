local script = {}

local PLAYER_TEMPLATE = "PlayerRobot"
local SPAWN_POINTS = {
	{ position = Ignite.Vec3(2.0, 1.35, -3.0), yaw = 0.0 },
	{ position = Ignite.Vec3(-2.0, 1.35, -3.0), yaw = 0.0 },
	{ position = Ignite.Vec3(0.0, 1.35, -6.0), yaw = 180.0 },
	{ position = Ignite.Vec3(0.0, 1.35, 0.0), yaw = 180.0 },
}

function script.on_create(entity)
end

function script.on_network_client_joined(entity, client_id, client_name)
	local spawn = SPAWN_POINTS[((client_id - 1) % #SPAWN_POINTS) + 1]
	local player = Ignite.Net.SpawnEntity(PLAYER_TEMPLATE, spawn.position, spawn.yaw, nil, client_id)
	if not player then
		Ignite.LogError("Failed to spawn player for client " .. tostring(client_id))
		return
	end

	Ignite.Log("Spawned " .. client_name .. " at " .. tostring(spawn.position))
end

return script
