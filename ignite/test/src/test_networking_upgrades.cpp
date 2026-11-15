// Copyright (c) 2026 Evangelion Manuhutu

#include <gtest/gtest.h>

#include <ignite/net/network_manager.hpp>
#include <ignite/net/net_prediction.hpp>
#include <ignite/project/project.hpp>
#include <ignite/scene/scene.hpp>
#include <ignite/scene/scene_manager.hpp>
#include <ignite/scene/entity.hpp>
#include <ignite/scene/component.hpp>
#include <ignite/serializer/scene_serializer.hpp>

#include <filesystem>

using namespace ignite;

TEST(NetworkingUpgrades, ProjectServerSettingsRoundtrip)
{
    NetworkManager *net = NetworkManager::GetInstance();
    ASSERT_NE(net, nullptr);

    const std::filesystem::path tempDir = std::filesystem::temp_directory_path() / "IgniteNetSettingsTest";
    std::error_code ec;
    std::filesystem::remove_all(tempDir, ec);
    std::filesystem::create_directories(tempDir, ec);

    ProjectInfo info;
    info.name = "NetSyncProj";
    info.rootDirectory = tempDir;
    info.filepath = tempDir / "NetSyncProj.ixproj";
    info.serverSettings.port = 9150;
    info.serverSettings.tickRate = 75;
    info.serverSettings.snapshotRate = 25;
    info.serverSettings.maxClients = 32;

    Ref<Project> proj = Project::Create(info);
    net->SyncFromProject(proj.get());

    const ServerSettings fetched = net->GetServerSettings();
    EXPECT_EQ(fetched.port, 9150);
    EXPECT_EQ(fetched.tickRate, 75);
    EXPECT_EQ(fetched.snapshotRate, 25);
    EXPECT_EQ(fetched.maxClients, 32);

    net->SetServerPort(9200);
    net->SyncToProject(proj.get());
    EXPECT_EQ(proj->GetInfo().serverSettings.port, 9200);

    std::filesystem::remove_all(tempDir, ec);
}

TEST(NetworkingUpgrades, NetworkIdentitySerializationPreservesMaskAndLocalFlag)
{
    const std::filesystem::path tempDir = std::filesystem::temp_directory_path() / "IgniteNetSceneSerializeTest";
    std::error_code ec;
    std::filesystem::remove_all(tempDir, ec);
    std::filesystem::create_directories(tempDir, ec);

    ProjectInfo info;
    info.name = "NetSerializeProj";
    info.rootDirectory = tempDir;
    info.filepath = tempDir / "NetSerializeProj.ixproj";
    Ref<Project> proj = Project::Create(info);

    const std::filesystem::path scenePath = tempDir / "net_test.ixscene";
    uint64_t expectedEntityId = 0;

    {
        Ref<Scene> scene = CreateRef<Scene>(proj.get());
        Entity player = SceneManager::CreateEntity(scene.get(), "PlayerOne", EntityType_Node);
        expectedEntityId = static_cast<uint64_t>(player.GetUUID());

        auto &nid = player.AddComponent<NetworkIdentityComponent>();
        nid.netId = 424242ULL;
        nid.ownerClient = 7u;
        nid.isLocalPlayer = true;
        nid.syncComponentsMask = 0x5Au;

        SceneSerializer serializer(scene, proj.get());
        ASSERT_TRUE(serializer.Serialize(scenePath));
    }

    {
        Ref<Scene> loaded = SceneSerializer::Deserialize(scenePath, proj.get());
        ASSERT_NE(loaded, nullptr);
        Entity found;
        auto view = loaded->registry->view<IDComponent>();
        for (entt::entity e : view)
        {
            if (view.get<IDComponent>(e).name == "PlayerOne")
            {
                found = Entity{ e, loaded.get() };
                break;
            }
        }
        ASSERT_TRUE(static_cast<bool>(found));
        EXPECT_EQ(static_cast<uint64_t>(found.GetUUID()), expectedEntityId);
        ASSERT_TRUE(found.HasComponent<NetworkIdentityComponent>());
        const auto &nid = found.GetComponent<NetworkIdentityComponent>();
        EXPECT_EQ(nid.netId, 424242ULL);
        EXPECT_EQ(nid.ownerClient, 7u);
        EXPECT_TRUE(nid.isLocalPlayer);
        EXPECT_EQ(nid.syncComponentsMask, 0x5Au);
    }

    std::filesystem::remove_all(tempDir, ec);
}

TEST(NetworkingUpgrades, ClientPredictionVisualOffsetDecaysMonotonically)
{
    ClientPrediction pred(0.05f);

    netproto::InputCmd cmd;
    cmd.seq = 1;
    pred.Record(1, 1, glm::vec3(5.0f, 0.0f, 0.0f), glm::quat(1.0f, 0.0f, 0.0f, 0.0f), cmd);

    glm::vec3 corrPos;
    glm::quat corrRot;
    ASSERT_TRUE(pred.Reconcile(1, glm::vec3(3.0f, 0.0f, 0.0f), glm::quat(1.0f, 0.0f, 0.0f, 0.0f), 1.0f / 60.0f, nullptr, corrPos, corrRot));
    EXPECT_NEAR(pred.GetVisualOffset().x, 2.0f, 0.001f);

    float prevLen = glm::length(pred.GetVisualOffset());
    for (int i = 0; i < 30; ++i)
    {
        pred.UpdateVisualOffset(1.0f / 60.0f, 12.0f);
        const float currLen = glm::length(pred.GetVisualOffset());
        EXPECT_LE(currLen, prevLen);
        prevLen = currLen;
    }
    EXPECT_LT(glm::length(pred.GetVisualOffset()), 0.2f);
}
