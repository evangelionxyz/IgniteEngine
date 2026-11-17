using Ignite;

namespace PlatformerDemo.Scripts
{
    class ServerManager : Entity
    {
        public override void OnNetworkClientJoined(uint clientId, string clientName)
        {
            base.OnNetworkClientJoined(clientId, clientName);
            Debug.Log($"Client {clientName} Joined");
        }

        public override void OnNetworkClientLeft(uint clientId, string clientName)
        {
            base.OnNetworkClientLeft(clientId, clientName);
            Debug.Log($"Client {clientName} Left");
        }
    }
}
