using System.Collections.Generic;
using Ignite;
using static Ignite.Mathf;

namespace PlatformerDemo.FPS
{
    public class FPSServerManager : Entity
    {
        private static readonly Vector3[] SpawnPoints =
        {
            new Vector3(0.0f,  2.1f,  15.0f),
            new Vector3(-9.0f, 2.1f,  12.0f),
            new Vector3(9.0f,  2.1f,  12.0f),
            new Vector3(0.0f,  2.1f, -15.0f),
        };

        private readonly Dictionary<uint, Entity> _clientAvatars = new();

        public override void OnCreate()
        {
            if (FPSGameSession.Intent == FpsConnectionIntent.HostListen && !Net.IsServerRunning)
            {
                Net.StartServer(FPSGameSession.ServerPort, 60, 20);
                FPSGameSession.LastEventBanner = $"LISTEN SERVER HOSTING ON :{FPSGameSession.ServerPort}";
            }
        }

        public override void OnNetworkClientJoined(uint clientId, string clientName)
        {
            base.OnNetworkClientJoined(clientId, clientName);

            int idx = (int)((clientId - 1) % (uint)SpawnPoints.Length);
            Vector3 spawnPos = SpawnPoints[idx];
            float yawDeg = idx == 3 ? 180.0f : 0.0f;

            Entity avatar = Net.SpawnEntity("PlayerAvatar", spawnPos, yawDeg, 0.0f, clientId);
            if (avatar != null)
            {
                _clientAvatars[clientId] = avatar;
            }

            Debug.Log($"[FPSServerManager] Spawned FPS operative for client {clientId} ({clientName}) at {spawnPos}", Debug.LogLevel.Info);
            Net.SendChat($"[SERVER] {clientName} deployed to Vanguard Arena");
        }

        public override void OnNetworkClientLeft(uint clientId, string clientName)
        {
            base.OnNetworkClientLeft(clientId, clientName);

            if (_clientAvatars.TryGetValue(clientId, out Entity avatar) && avatar != null)
            {
                if (avatar.HasComponent<NetworkIdentityComponent>())
                {
                    ulong netId = avatar.GetComponent<NetworkIdentityComponent>().NetId;
                    if (netId != 0)
                        Net.DespawnEntity(netId);
                }
                _clientAvatars.Remove(clientId);
            }

            Debug.Log($"[FPSServerManager] Client {clientId} ({clientName}) disconnected", Debug.LogLevel.Info);
        }
    }
}
