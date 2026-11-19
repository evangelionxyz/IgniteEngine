// Copyright (c) 2026 Evangelion Manuhutu

#include <gtest/gtest.h>

#include "ignite/asset/asset.hpp"
#include "ignite/asset/asset_importer.hpp"
#include "ignite/asset/asset_manager.hpp"
#include "ignite/core/application.hpp"
#include "ignite/core/command.hpp"
#include "ignite/core/vfs/vfs.hpp"
#include "ignite/graphics/objects/material.hpp"
#include "ignite/graphics/objects/mesh.hpp"
#include "ignite/graphics/renderer.hpp"
#include "ignite/graphics/texture.hpp"
#include "ignite/graphics/ui/widget_button.hpp"
#include "ignite/graphics/ui/widget_canvas.hpp"
#include "ignite/graphics/ui/widget_container.hpp"
#include "ignite/graphics/ui/widget_label.hpp"
#include "ignite/project/project.hpp"
#include "ignite/scene/component.hpp"
#include "ignite/scene/entity.hpp"
#include "ignite/scene/entity_command_manager.hpp"
#include "ignite/scene/entity_destroy_command.hpp"
#include "ignite/scene/entity_rename_command.hpp"
#include "ignite/scene/entity_reparent_command.hpp"
#include "ignite/scene/scene.hpp"
#include "ignite/scene/scene_manager.hpp"
#include "ignite/serializer/scene_serializer.hpp"

#include <filesystem>
#include <fstream>
#include <string>
#include <unordered_set>
#include <vector>

using namespace ignite;

namespace
{
    Ref<Project> CreateTempTestProject(const std::string &name, std::filesystem::path &outDir)
    {
        const std::filesystem::path testResourcesRoot = vfs::GetExecutableDirectory() / "test-resources";
        outDir = testResourcesRoot / "temp" / name;

        std::error_code ec;
        if (std::filesystem::exists(outDir, ec))
        {
            std::filesystem::remove_all(outDir, ec);
        }
        std::filesystem::create_directories(outDir, ec);

        ProjectInfo info{};
        info.name = name;
        info.filepath = outDir / (name + ".ixproj");
        info.rootDirectory = outDir;
        info.assetDirectory = "Assets";
        info.scriptsDirectory = "Scripts";
        info.assetRegistryFilepath = "AssetRegistry.ixreg";
        info.configuration = ProjectConfiguration::Debug;

        Ref<Project> project = Project::Create(info);
        if (project)
        {
            AssetManager::GetInstance()->SetActiveProject(project);
        }
        return project;
    }

    void PumpMainThreadAndTextures()
    {
        if (auto *app = Application::GetInstance())
        {
            app->ProcessMainThreadSubmissions();
        }
        Texture::ProcessAsyncUploads(8, 32 * 1024 * 1024);
    }

    void WriteEmbeddedTwoTextureGltf(const std::filesystem::path &gltfPath)
    {
        std::filesystem::create_directories(gltfPath.parent_path());
        std::ofstream out(gltfPath, std::ios::trunc);
        ASSERT_TRUE(out.is_open());

        out << R"JSON({
  "asset": { "version": "2.0" },
  "scene": 0,
  "scenes": [{ "nodes": [0] }],
  "nodes": [{ "mesh": 0, "name": "TriNode" }],
  "meshes": [{
    "name": "TriMesh",
    "primitives": [{
      "attributes": { "POSITION": 0 },
      "indices": 1,
      "material": 0
    }]
  }],
  "materials": [{
    "name": "MatEmbedded",
    "pbrMetallicRoughness": {
      "baseColorTexture": { "index": 0 },
      "metallicFactor": 0.0,
      "roughnessFactor": 0.5
    },
    "normalTexture": { "index": 1 }
  }],
  "textures": [
    { "source": 0 },
    { "source": 1 }
  ],
  "images": [
    { "uri": "data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAAAEAAAABCAYAAAAfFcSJAAAADUlEQVR4nGP43+DwHwAHAAK/K9fH4gAAAABJRU5ErkJggg==" },
    { "uri": "data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAAAEAAAABCAYAAAAfFcSJAAAADUlEQVR4nGP43+DwHwAHAAK/K9fH4gAAAABJRU5ErkJggg==" }
  ],
  "buffers": [{
    "uri": "data:application/octet-stream;base64,AAAAAAAAAAAAAAAAAACAPwAAAAAAAAAAAAAAAAAAgD8AAAAAAAABAAIA",
    "byteLength": 42
  }],
  "bufferViews": [
    { "buffer": 0, "byteOffset": 0, "byteLength": 36, "target": 34962 },
    { "buffer": 0, "byteOffset": 36, "byteLength": 6, "target": 34963 }
  ],
  "accessors": [
    { "bufferView": 0, "componentType": 5126, "count": 3, "type": "VEC3", "min": [0, 0, 0], "max": [1, 1, 0] },
    { "bufferView": 1, "componentType": 5123, "count": 3, "type": "SCALAR" }
  ]
})JSON";
        out.close();
    }
}

TEST(AssetImporterUniqueNaming, BuildUniqueAssetPathDeduplicatesDiskAndReserved)
{
    const std::filesystem::path dir = vfs::GetExecutableDirectory() / "test-resources" / "temp" / "UniquePathTest";
    std::error_code ec;
    std::filesystem::remove_all(dir, ec);
    std::filesystem::create_directories(dir, ec);

    const std::string matExt = GetAssetExtensionFromType(AssetType::Material);

    const std::filesystem::path p0 = AssetImporter::BuildUniqueAssetPath(dir, "Wood", matExt);
    EXPECT_EQ(p0.filename().string(), "Wood" + matExt);
    std::ofstream(p0) << "{}";

    const std::filesystem::path p1 = AssetImporter::BuildUniqueAssetPath(dir, "Wood", matExt);
    EXPECT_EQ(p1.filename().string(), "Wood_1" + matExt);
    std::ofstream(p1) << "{}";

    const std::filesystem::path p2 = AssetImporter::BuildUniqueAssetPath(dir, "Wood", matExt);
    EXPECT_EQ(p2.filename().string(), "Wood_2" + matExt);

    // When checkExisting == false (hot-reload mode), disk files are allowed to be overwritten in place,
    // while reservedLowerPaths still prevents intra-import collisions.
    std::unordered_set<std::string> reserved;
    const std::filesystem::path hot0 = AssetImporter::BuildUniqueAssetPath(dir, "Wood", matExt, nullptr, &reserved, false);
    const std::filesystem::path hot1 = AssetImporter::BuildUniqueAssetPath(dir, "Wood", matExt, nullptr, &reserved, false);
    EXPECT_EQ(hot0.filename().string(), "Wood" + matExt);
    EXPECT_EQ(hot1.filename().string(), "Wood_1" + matExt);
}

TEST(AssetImporterUniqueNaming, EmbeddedGltfTexturesUseMeshStemAndMaterialTypeWithoutOverwriting)
{
    Application::GetCommandManager()->ClearAll();

    std::filesystem::path projectDir;
    Ref<Project> project = CreateTempTestProject("EmbeddedGltfUnique", projectDir);
    ASSERT_NE(project, nullptr);

    AssetManager *am = AssetManager::GetInstance();
    ASSERT_NE(am, nullptr);

    const std::filesystem::path sourceGltf = projectDir / "ExternalSources" / "TestBox.gltf";
    WriteEmbeddedTwoTextureGltf(sourceGltf);

    const std::filesystem::path targetDir = project->GetAssetDirectory() / "Meshes";
    std::filesystem::create_directories(targetDir);

    const std::string meshExt = GetAssetExtensionFromType(AssetType::StaticMesh);
    const std::string matExt = GetAssetExtensionFromType(AssetType::Material);

    // Import #1
    StaticMeshImportPayload payload1{};
    payload1.targetDirectory = targetDir;
    payload1.importMaterials = true;
    payload1.forceRebuild = true;
    payload1.overwriteExisting = false;

    const AssetHandle handle1 = AssetHandle();
    AssetMetaData meta1{ sourceGltf, AssetType::StaticMesh };
    Ref<StaticMesh> mesh1 = AssetImporter::ImportStaticMesh(handle1, meta1, am, payload1);
    PumpMainThreadAndTextures();
    ASSERT_NE(mesh1, nullptr);

    const std::filesystem::path expectedMesh1 = targetDir / ("TestBox" + meshExt);
    const std::filesystem::path expectedMat1 = targetDir / ("MatEmbedded" + matExt);
    const std::filesystem::path expectedBaseTex1 = targetDir / "TestBox_BaseColor_texture.png";
    const std::filesystem::path expectedNormalTex1 = targetDir / "TestBox_Normal_texture.png";

    EXPECT_TRUE(std::filesystem::exists(expectedMesh1));
    EXPECT_TRUE(std::filesystem::exists(expectedMat1));
    EXPECT_TRUE(std::filesystem::exists(expectedBaseTex1));
    EXPECT_TRUE(std::filesystem::exists(expectedNormalTex1));

    // Import #2 into the SAME directory without overwrite: must produce unique _1 names!
    StaticMeshImportPayload payload2{};
    payload2.targetDirectory = targetDir;
    payload2.importMaterials = true;
    payload2.forceRebuild = true;
    payload2.overwriteExisting = false;

    const AssetHandle handle2 = AssetHandle();
    AssetMetaData meta2{ sourceGltf, AssetType::StaticMesh };
    Ref<StaticMesh> mesh2 = AssetImporter::ImportStaticMesh(handle2, meta2, am, payload2);
    PumpMainThreadAndTextures();
    ASSERT_NE(mesh2, nullptr);
    EXPECT_NE(mesh1->handle, mesh2->handle);

    const std::filesystem::path expectedMesh2 = targetDir / ("TestBox_1" + meshExt);
    const std::filesystem::path expectedMat2 = targetDir / ("MatEmbedded_1" + matExt);
    const std::filesystem::path expectedBaseTex2 = targetDir / "TestBox_BaseColor_texture_1.png";
    const std::filesystem::path expectedNormalTex2 = targetDir / "TestBox_Normal_texture_1.png";

    EXPECT_TRUE(std::filesystem::exists(expectedMesh2));
    EXPECT_TRUE(std::filesystem::exists(expectedMat2));
    EXPECT_TRUE(std::filesystem::exists(expectedBaseTex2));
    EXPECT_TRUE(std::filesystem::exists(expectedNormalTex2));

    // Import #3 (hot-reload overwrite of handle1): must update in-place without creating _2 files!
    StaticMeshImportPayload payloadOverwrite{};
    payloadOverwrite.targetDirectory = targetDir;
    payloadOverwrite.importMaterials = true;
    payloadOverwrite.forceRebuild = true;
    payloadOverwrite.overwriteExisting = true;
    payloadOverwrite.outputMeshPath = expectedMesh1;

    Ref<StaticMesh> reloaded = AssetImporter::ImportStaticMesh(handle1, meta1, am, payloadOverwrite);
    PumpMainThreadAndTextures();
    ASSERT_NE(reloaded, nullptr);
    EXPECT_FALSE(std::filesystem::exists(targetDir / ("TestBox_2" + meshExt)));
    EXPECT_FALSE(std::filesystem::exists(targetDir / "TestBox_BaseColor_texture_2.png"));

    Application::GetCommandManager()->ClearAll();
    mesh1.reset();
    mesh2.reset();
    reloaded.reset();
    Project::CloseActive();
    project.reset();
}

TEST(SceneHierarchy, ReorderFolderUndoAndSerializationRoundTrip)
{
    Application::GetCommandManager()->ClearAll();

    std::filesystem::path projectDir;
    Ref<Project> project = CreateTempTestProject("SceneHierarchyTest", projectDir);
    ASSERT_NE(project, nullptr);

    Ref<Scene> scene = Scene::Create(project.get());
    ASSERT_NE(scene, nullptr);

    Entity eA = SceneManager::CreateEmptyEntity(scene.get(), "Alpha");
    Entity eB = SceneManager::CreateEmptyEntity(scene.get(), "Beta");
    Entity eC = SceneManager::CreateEmptyEntity(scene.get(), "Charlie");
    Entity folder = SceneManager::CreateFolder(scene.get(), "Environment");
    ASSERT_TRUE(folder.GetComponent<IDComponent>().IsInType(EntityType_Folder));

    const ignite::UUID uA = eA.GetUUID();
    const ignite::UUID uB = eB.GetUUID();
    const ignite::UUID uC = eC.GetUUID();
    const ignite::UUID uF = folder.GetUUID();

    ASSERT_EQ(scene->rootEntityOrder, (std::vector<ignite::UUID>{ uA, uB, uC, uF }));

    // Reorder C Above A -> [C, A, B, F]
    EXPECT_TRUE(SceneManager::ReorderRelativeTo(scene.get(), uC, uA, HierarchyDropPosition::Above));
    EXPECT_EQ(scene->rootEntityOrder, (std::vector<ignite::UUID>{ uC, uA, uB, uF }));

    // Reparent A Inside Folder -> [C, B, F], F.children = [A]
    EXPECT_TRUE(SceneManager::ReorderRelativeTo(scene.get(), uA, uF, HierarchyDropPosition::Inside));
    EXPECT_EQ(scene->rootEntityOrder, (std::vector<ignite::UUID>{ uC, uB, uF }));
    EXPECT_EQ(folder.GetComponent<IDComponent>().children, (std::vector<ignite::UUID>{ uA }));
    EXPECT_EQ(eA.GetParentUUID(), uF);

    // Move B Above A (so inside Folder, before A) -> [C, F], F.children = [B, A]
    {
        const ignite::UUID oldParent = eB.GetParentUUID();
        const size_t oldIdx = SceneManager::GetSiblingIndex(scene.get(), uB);
        EXPECT_TRUE(SceneManager::ReorderRelativeTo(scene.get(), uB, uA, HierarchyDropPosition::Above));
        const ignite::UUID newParent = eB.GetParentUUID();
        const size_t newIdx = SceneManager::GetSiblingIndex(scene.get(), uB);

        EXPECT_EQ(scene->rootEntityOrder, (std::vector<ignite::UUID>{ uC, uF }));
        EXPECT_EQ(folder.GetComponent<IDComponent>().children, (std::vector<ignite::UUID>{ uB, uA }));

        // Verify EntityHierarchyMoveCommand Undo & Redo
        EntityHierarchyMoveCommand moveCmd(scene.get(), { { uB, oldParent, oldIdx, newParent, newIdx } });
        moveCmd.Undo();
        EXPECT_EQ(scene->rootEntityOrder, (std::vector<ignite::UUID>{ uC, uB, uF }));
        EXPECT_EQ(folder.GetComponent<IDComponent>().children, (std::vector<ignite::UUID>{ uA }));
        moveCmd.Execute();
        EXPECT_EQ(scene->rootEntityOrder, (std::vector<ignite::UUID>{ uC, uF }));
        EXPECT_EQ(folder.GetComponent<IDComponent>().children, (std::vector<ignite::UUID>{ uB, uA }));
    }

    // Cycle check: cannot move Folder into its own descendant A
    EXPECT_FALSE(SceneManager::MoveEntity(scene.get(), uF, uA));

    // Serialize & Deserialize round-trip preserves exact Folder flag, root order, and child order
    const std::filesystem::path scenePath = project->GetAssetDirectory() / "HierarchyTest.ixscene";
    SceneSerializer serializer(scene, project.get());
    ASSERT_TRUE(serializer.Serialize(scenePath));

    Ref<Scene> loaded = SceneSerializer::Deserialize(scenePath, project.get());
    ASSERT_NE(loaded, nullptr);
    EXPECT_EQ(loaded->rootEntityOrder, (std::vector<ignite::UUID>{ uC, uF }));

    Entity loadedFolder = SceneManager::GetEntity(loaded.get(), uF);
    ASSERT_TRUE(loadedFolder.IsValid());
    EXPECT_TRUE(loadedFolder.GetComponent<IDComponent>().IsInType(EntityType_Folder));
    EXPECT_EQ(loadedFolder.GetComponent<IDComponent>().children, (std::vector<ignite::UUID>{ uB, uA }));

    Application::GetCommandManager()->ClearAll();
    loaded.reset();
    scene.reset();
    Project::CloseActive();
    project.reset();
}

TEST(SceneDuplication, UniqueNamesAndSingleUndoForSubtree)
{
    Application::GetCommandManager()->ClearAll();

    std::filesystem::path projectDir;
    Ref<Project> project = CreateTempTestProject("SceneDuplicationTest", projectDir);
    ASSERT_NE(project, nullptr);

    Ref<Scene> scene = Scene::Create(project.get());
    ASSERT_NE(scene, nullptr);

    Entity cube = SceneManager::CreatePrimitive(scene.get(), EMeshType::CUBE, "Cube");
    Entity child = SceneManager::CreatePrimitive(scene.get(), EMeshType::SPHERE, "ChildMesh");
    SceneManager::MoveEntity(scene.get(), child.GetUUID(), cube.GetUUID());

    // Clear initial creation commands to isolate Duplicate undo test
    Application::GetCommandManager()->ClearSceneHistory();

    Entity dup1 = SceneManager::DuplicateEntity(scene.get(), cube, true, true);
    ASSERT_TRUE(dup1.IsValid());
    EXPECT_EQ(dup1.GetName(), "Cube (1)");
    ASSERT_EQ(dup1.GetComponent<IDComponent>().children.size(), 1u);
    Entity dup1Child = SceneManager::GetEntity(scene.get(), dup1.GetComponent<IDComponent>().children[0]);
    ASSERT_TRUE(dup1Child.IsValid());
    EXPECT_EQ(dup1Child.GetName(), "ChildMesh");
    EXPECT_NE(dup1Child.GetUUID(), child.GetUUID());

    // Duplicating "Cube (1)" must yield "Cube (2)", not "Cube (1) (1)"
    Entity dup2 = SceneManager::DuplicateEntity(scene.get(), dup1, true, true);
    ASSERT_TRUE(dup2.IsValid());
    EXPECT_EQ(dup2.GetName(), "Cube (2)");

    // Expected root order: Cube, Cube (1), Cube (2)
    EXPECT_EQ(scene->rootEntityOrder, (std::vector<ignite::UUID>{ cube.GetUUID(), dup1.GetUUID(), dup2.GetUUID() }));

    // One Undo must destroy dup2 AND its child cleanly
    const ignite::UUID dup2UUID = dup2.GetUUID();
    const ignite::UUID dup2ChildUUID = dup2.GetComponent<IDComponent>().children[0];
    EXPECT_TRUE(Application::GetCommandManager()->Undo());
    EXPECT_FALSE(SceneManager::GetEntity(scene.get(), dup2UUID).IsValid());
    EXPECT_FALSE(SceneManager::GetEntity(scene.get(), dup2ChildUUID).IsValid());
    EXPECT_EQ(scene->rootEntityOrder, (std::vector<ignite::UUID>{ cube.GetUUID(), dup1.GetUUID() }));

    // One Redo restores dup2 AND its child + StaticMeshComponent
    EXPECT_TRUE(Application::GetCommandManager()->Redo());
    Entity restoredDup2 = SceneManager::GetEntity(scene.get(), dup2UUID);
    ASSERT_TRUE(restoredDup2.IsValid());
    EXPECT_TRUE(restoredDup2.HasComponent<StaticMeshComponent>());
    Entity restoredDup2Child = SceneManager::GetEntity(scene.get(), dup2ChildUUID);
    ASSERT_TRUE(restoredDup2Child.IsValid());
    EXPECT_TRUE(restoredDup2Child.HasComponent<StaticMeshComponent>());

    Application::GetCommandManager()->ClearAll();
    scene.reset();
    Project::CloseActive();
    project.reset();
}

TEST(UndoRedoSystem, HistoryLimitMemoryEstimateTransactionsAndFullEntityRestore)
{
    // 1. History default 200, trimming & memory estimation
    CommandHistory history;
    EXPECT_EQ(history.GetMaxHistory(), 200u);

    int counter = 0;
    for (int i = 0; i < 215; ++i)
    {
        history.ExecuteCommand(CreateScope<LambdaCommand>(
            "Step",
            [&counter]() { ++counter; },
            [&counter]() { --counter; },
            128));
    }
    EXPECT_EQ(counter, 215);
    EXPECT_EQ(history.GetUndoCount(), 200u);
    EXPECT_GT(history.GetEstimatedMemoryUsage(), 200u * 128u);

    history.SetMaxHistory(50);
    EXPECT_EQ(history.GetUndoCount(), 50u);

    // 2. ScopedCommandTransaction groups commands into 1 undo step and reverts in reverse
    std::filesystem::path projectDir;
    Ref<Project> project = CreateTempTestProject("UndoSystemTest", projectDir);
    ASSERT_NE(project, nullptr);

    Application::GetCommandManager()->ClearAll();
    std::vector<std::string> ops;
    {
        ScopedCommandTransaction tx("Batch Op");
        CommandManager::ExecuteCommand(CreateScope<LambdaCommand>(
            "A", [&ops]() { ops.push_back("doA"); }, [&ops]() { ops.push_back("undoA"); }));
        CommandManager::ExecuteCommand(CreateScope<LambdaCommand>(
            "B", [&ops]() { ops.push_back("doB"); }, [&ops]() { ops.push_back("undoB"); }));
    }
    EXPECT_EQ(ops, (std::vector<std::string>{ "doA", "doB" }));
    EXPECT_EQ(Application::GetCommandManager()->GetSceneHistory().GetUndoCount(), 1u);
    EXPECT_TRUE(Application::GetCommandManager()->Undo());
    EXPECT_EQ(ops, (std::vector<std::string>{ "doA", "doB", "undoB", "undoA" }));
    EXPECT_TRUE(Application::GetCommandManager()->Redo());
    EXPECT_EQ(ops, (std::vector<std::string>{ "doA", "doB", "undoB", "undoA", "doA", "doB" }));

    // 3. EntityDestroyCommand full-component + subtree + sibling order restore
    Application::GetCommandManager()->ClearSceneHistory();
    Ref<Scene> scene = Scene::Create(project.get());
    Entity anchor = SceneManager::CreateEmptyEntity(scene.get(), "Anchor");
    Entity target = SceneManager::CreatePrimitive(scene.get(), EMeshType::CUBE, "RichEntity");
    auto &ptLight = target.AddComponent<PointLightComponent>();
    ptLight.intensity = 7.5f;
    ptLight.range = 22.0f;
    Entity camChild = SceneManager::CreateCamera(scene.get(), "CamChild");
    SceneManager::MoveEntity(scene.get(), camChild.GetUUID(), target.GetUUID());
    Entity tail = SceneManager::CreateEmptyEntity(scene.get(), "Tail");

    const ignite::UUID targetUUID = target.GetUUID();
    const ignite::UUID camChildUUID = camChild.GetUUID();

    Application::GetCommandManager()->ClearSceneHistory();
    CommandManager::ExecuteCommand(CreateScope<EntityDestroyCommand>(scene.get(), target));

    EXPECT_FALSE(SceneManager::GetEntity(scene.get(), targetUUID).IsValid());
    EXPECT_FALSE(SceneManager::GetEntity(scene.get(), camChildUUID).IsValid());
    EXPECT_EQ(scene->rootEntityOrder, (std::vector<ignite::UUID>{ anchor.GetUUID(), tail.GetUUID() }));

    EXPECT_TRUE(Application::GetCommandManager()->Undo());
    EXPECT_EQ(scene->rootEntityOrder, (std::vector<ignite::UUID>{ anchor.GetUUID(), targetUUID, tail.GetUUID() }));

    Entity restoredTarget = SceneManager::GetEntity(scene.get(), targetUUID);
    ASSERT_TRUE(restoredTarget.IsValid());
    EXPECT_TRUE(restoredTarget.HasComponent<StaticMeshComponent>());
    ASSERT_TRUE(restoredTarget.HasComponent<PointLightComponent>());
    EXPECT_FLOAT_EQ(restoredTarget.GetComponent<PointLightComponent>().intensity, 7.5f);
    EXPECT_FLOAT_EQ(restoredTarget.GetComponent<PointLightComponent>().range, 22.0f);

    Entity restoredChild = SceneManager::GetEntity(scene.get(), camChildUUID);
    ASSERT_TRUE(restoredChild.IsValid());
    EXPECT_TRUE(restoredChild.HasComponent<CameraComponent>());
    EXPECT_EQ(restoredChild.GetParentUUID(), targetUUID);

    // 4. Project EditorSettings serialization round-trip
    project->GetInfo().editorSettings.undoHistoryLimit = 350;
    project->GetInfo().editorSettings.recordSelectionUndo = false;
    ASSERT_TRUE(project->Serialize(project->GetFilepath()));

    Ref<Project> reopened = Project::Open(project->GetFilepath());
    ASSERT_NE(reopened, nullptr);
    EXPECT_EQ(reopened->GetInfo().editorSettings.undoHistoryLimit, 350u);
    EXPECT_FALSE(reopened->GetInfo().editorSettings.recordSelectionUndo);

    Application::GetCommandManager()->ClearAll();
    scene.reset();
    Project::CloseActive();
    reopened.reset();
    project.reset();
}

TEST(WidgetCanvasUndo, DeterministicStringRoundTrip)
{
    Ref<WidgetCanvas> canvas = CreateRef<WidgetCanvas>(nullptr);
    canvas->SetName("MainHUD");
    WidgetContainer *root = canvas->CreateRoot(1280, 720);
    const WidgetID btnId = canvas->AddButton(root, "Start");
    const WidgetID lblId = canvas->AddLabel(root, "Title");
    ASSERT_GT(btnId, 0);
    ASSERT_GT(lblId, 0);

    const std::string snapBefore = canvas->SerializeToString();
    ASSERT_FALSE(snapBefore.empty());

    // Mutate canvas
    Ref<WidgetButton> btn = canvas->GetItems().at(btnId)->As<WidgetButton>();
    ASSERT_NE(btn, nullptr);
    btn->label->text = "Resume";
    const WidgetID imgId = canvas->AddImage(root);
    const std::string snapAfter = canvas->SerializeToString();
    EXPECT_NE(snapBefore, snapAfter);

    // Restore snapBefore (Undo)
    ASSERT_TRUE(canvas->DeserializeFromString(snapBefore));
    EXPECT_EQ(canvas->GetItems().count(imgId), 0u);
    EXPECT_EQ(canvas->GetItems().at(btnId)->As<WidgetButton>()->label->text, "Start");

    // Restore snapAfter (Redo)
    ASSERT_TRUE(canvas->DeserializeFromString(snapAfter));
    EXPECT_EQ(canvas->GetItems().count(imgId), 1u);
    EXPECT_EQ(canvas->GetItems().at(btnId)->As<WidgetButton>()->label->text, "Resume");
}

