---
name: ignite-editor-systems
description: Work with Ignite Editor and Engine asset importing (unique naming, embedded textures, hot-reload), GPU thumbnail caching, scene hierarchy folders/reordering/duplication, and the unified scene/selection/asset-editor undo-redo architecture.
---

# Ignite Editor & Asset Pipeline Systems

Use this skill when modifying asset import naming, FileWatcher hot-reload, GPU thumbnail caching, Scene Hierarchy ordering/folders/duplication, or Undo/Redo across Scene, Entity selection, and Asset Editor tabs.

## 1. Asset Importing, Unique Naming & Hot-Reload
- **Collision-Safe Paths** (`ignite/engine/src/ignite/asset/asset_importer.hpp`):
  - Always allocate non-overwrite output paths through `AssetImporter::BuildUniqueAssetPath(directory, baseName, extension, assetManager, &reservedLowerPaths, checkExisting)` and sanitize names with `AssetImporter::SanitizeAssetName`.
  - Collisions produce `_1`, `_2`, ... suffixes rather than overwriting existing files; `reservedLowerPaths` deduplicates within a single multi-asset import batch.
- **Embedded & Mesh Texture Naming** (`ignite/engine/src/ignite/graphics/objects/mesh.cpp`):
  - Embedded/unnamed glTF textures are named `<MeshStem>_<SlotType>_texture` across `BaseColor`, `Emissive`, `MetallicRoughness`, `Normal`, and `Occlusion`. Non-embedded textures keep their URI/image stem and deduplicate on collision.
- **Interactive Import vs. Hot-Reload Overwrite**:
  - Interactive imports (`AssetImporterPanel`) keep `StaticMeshImportPayload::overwriteExisting = false` and mutex-guard `PrepareAssetForImport`.
  - In-place source reimports (`AssetManager::OnMeshSourceFileChanged`) set `overwriteExisting = true` so `BuildUniqueAssetPath(..., checkExisting=false)` updates existing assets in place while `reservedLowerPaths` still prevents intra-import collisions.
  - Hot-reload and lifetime APIs on `AssetManager`: `ReloadAssetAsync`, `ReloadAssetImmediate`, `TrackMeshSourceAsset`, `UntrackMeshSourceAsset`, `SuppressWatchPath`, `DeferReleaseResource`.

## 2. GPU Thumbnail Generation & Binary Cache
- `ThumbnailGenerator` (`ignite/engine/src/ignite/graphics/thumbnail_generator.hpp`):
  - Caches RGBA thumbnails per asset on disk under `<project>/.cache/thumbnails/{key}.bin` and validates timestamps.
  - Renders cache misses in `ProcessFrame(frameContext)` with frame pacing (`SetRenderIntervalFrames`, default `2`) and async readback slots (`kReadbackSlotCount = 3`).
  - Call `InvalidateThumbnail` when assets change on disk, `FlushPendingReadbacks(timeoutMs)` in deterministic tests, and `Shutdown()` before graphics device destruction.

## 3. Scene Hierarchy Folders, Reordering & Unique Duplication
- **Folders & Sibling Ordering** (`ignite/engine/src/ignite/scene/component.hpp`, `scene.hpp`, `scene_manager.hpp`):
  - Folders use `EntityType_Folder = BIT(7)` (skipped by viewport picking; rendered in hierarchy with `resources/ui/editor/ic_editor_folder.png`).
  - Top-level sibling order is stored in `Scene::rootEntityOrder` (child order in `IDComponent::children`).
  - Mutate hierarchy exclusively via `SceneManager::CreateFolder`, `MoveEntity`, `ReorderRelativeTo(..., HierarchyDropPosition::Above|Inside|Below)`, `SyncHierarchyOrder`, and `GetSiblingIndex`.
- **Unique Duplication & Cleanup**:
  - `SceneManager::GenerateUniqueEntityName(scene, baseName)` produces `Cube` -> `Cube (1)` -> `Cube (2)` without stacking ` (1) (1)`.
  - `SceneManager::DuplicateEntity(scene, entity, addToParent, recordUndo)` places clones beside the source and records a single subtree undo command (`EntityCreateCommand`).
  - Scene/Entity/Prefab serializers traverse DFS and persist `"Order"` indices; `Scene::~Scene()` clears `NetworkManager::m_ActiveScene` if pointing to `this`.

## 4. Unified Undo/Redo Architecture
- **Core Stack & Transactions** (`ignite/engine/src/ignite/core/command.hpp`):
  - Derive commands from `ICommand` (implement `GetName()` and `GetMemoryUsage()`), or use `LambdaCommand` / `CompoundCommand` / `ScopedCommandTransaction`.
  - `CommandHistory` (default `200`, configurable) supports nested transactions, saved revision tracking (`MarkSaved()` / `IsAtSavedRevision()`), re-entrancy guards (`IsExecutingUndoRedo()`), and `GetEstimatedMemoryUsage()`.
  - `CommandManager` manages both `GetSceneHistory()` and per-asset histories (`GetOrCreateAssetHistory(assetKey)`, `FindAssetHistory`, `RemoveAssetHistory`) plus total memory queries.
- **Scene & Blender-Style Selection Undo**:
  - `EntitySubtreeSnapshot` (`entity_snapshot.hpp`) powers `EntityCreateCommand` and `EntityDestroyCommand` for full subtree, component, and sibling-order restore.
  - `EntityHierarchyMoveCommand`, `ComponentAddCommand`, and `ComponentRemoveCommand` handle structural edits.
  - `ScenePanel` records selection transitions via `SetSelectedEntities`, `ClearSelection`, and `SetSelectedEntity` when `ProjectInfo::editorSettings.recordSelectionUndo` is enabled.
- **Asset Editor Undo** (`ignite/editor/src/panels/asset_editor_undo.hpp`, `widget_canvas.hpp`):
  - Uses `IAssetSnapshot` (`CaptureAssetSnapshot`) + `AssetSnapshotCommand` (`UpdateAssetUndoTracking`, `MarkSaved()` on save) across all 16 asset editor types (using `WidgetCanvas::SerializeToString()`/`DeserializeFromString()` for UI widgets).
- **Project Settings & Routing** (`editor_layer.cpp`, `project.hpp`):
  - `Ctrl+Z` / `Ctrl+Shift+Z` / `Ctrl+Y` and `Edit -> Undo/Redo` route to the focused Asset Editor tab or active Scene.
  - Project Settings -> `"Editor / Undo"` persists `ProjectInfo::EditorSettings` (`undoHistoryLimit = 200`, `recordSelectionUndo = true`) in `.ixproj` and displays live/projected memory usage.

## 5. Global Illumination & GTAO Tooling (`editor_layer.cpp`, `gi_baker.hpp`)
- **`Lighting` Menu & Status Badge**: `Bake GI`, `Bake Reflection Probes`, `Bake All`, `Clear Baked Data`, `Cancel GI Bake`, `Show GI Probes`, and `Debug Mode` (`GI Diffuse` 9, `GI Specular` 10, `AO` 11, `Probe Weights` 12). The status bar displays a `GI Out of Date - Rebake` button whenever `IsSceneGIOutOfDate(scene)` is true (`ComputeSceneBakeHash`).
- **Scene Renderer Panel**: Shows `GPU Pass Timings (ms)` (`DepthNormalPrepass`, `GTAO`, `ColorPass`, `ShadowPass`, `CompositePass`) and `Global Illumination & AO` overrides (`giModeOverride`, `aoModeOverride`, `enableDepthPrepass`, `showGIProbes`).

## 6. Verification
Run the targeted C++ regression suite after modifying any of the subsystems above:
```powershell
Push-Location D:\Dev\Ignite\bin\Debug
.\Ignite.Test.exe --gtest_filter="AssetImporterUniqueNaming*:SceneHierarchy*:SceneDuplication*:UndoRedoSystem*:WidgetCanvasUndo*:ThumbnailsAndWatcher*"
$code = $LASTEXITCODE
Pop-Location
exit $code
```

