---
name: ignite-agent-control
description: Control Ignite.Editor.exe, Ignite.Runtime.exe, packaged games, and Ignite.Network.exe via AgentExecutionContext, CLI command scripts, and scripts/ignite_mcp_server.py.
---

# Ignite Agent Control & MCP Automation

## 1. Batch Script Execution (`--command-script`)
Run reproducible Editor or Runtime automation workflows without manual clicking:
```powershell
D:\Dev\Ignite\bin\Debug\Ignite.Editor.exe `
  --project="D:\Path\To\Project.ixproj" `
  --windowed `
  --command-script "D:\Path\To\commands.json" `
  --command-report "D:\Path\To\report.json" `
  --quit-after-script
```
Supported presentation flags: `--windowed`, `--offscreen`, `--headless`, `--nogui`.
`Application::Tick` checks `AgentController::IsAutomationActive()`, so simulation and rendering proceed even when the window does not have OS focus.

## 2. Live Interactive Session via MCP Server
Start `python D:\Dev\Ignite\scripts\ignite_mcp_server.py` (stdio MCP JSON-RPC 2.0). Available MCP tools:
- **Build & Test**: `ignite_build_solution`, `ignite_sync_shaders`, `ignite_run_tests`
- **Live App Control**: `ignite_launch_app` (spawns target with `--watch-command-file` / `--response-file` and waits for `ping`), `ignite_send_command`, `ignite_stop_app`, `ignite_run_command_script`, `ignite_list_sessions`
- **Packaging & Dedicated Server**: `ignite_package_game`, `ignite_start_server` (`Ignite.Network.exe`)

## 3. Readiness Gate & Command Reference
- `get_status` returns `scene_ready` (`Scene::IsReadyToPlay()`). Both `play` and `simulate` fail with an error while scene assets are still loading (`scene_ready == false`), so poll `get_status` / `wait_frames` until `scene_ready` is `true` before entering Play or Simulate mode.
- Example command array:
```json
[
  { "command": "create_entity", "name": "HeroCube", "primitive": "Cube", "position": [0, 2, 0] },
  { "command": "add_component", "entity": "HeroCube", "type": "Rigidbody", "properties": { "body_type": "dynamic", "mass": 1.0 } },
  { "command": "add_component", "entity": "HeroCube", "type": "BoxCollider", "properties": { "size": [1, 1, 1] } },
  { "command": "create_entity", "name": "Sun", "position": [4, 8, 4] },
  { "command": "add_component", "entity": "Sun", "type": "DirectionalLight", "properties": { "color": [1, 0.96, 0.9], "intensity": 3.0 } },
  { "command": "set_camera", "position": [0, 3, 7], "target": [0, 1, 0], "fov": 60 },
  { "command": "set_render_settings", "ssao": true, "bloom": true, "tonemap_mode": "uncharted2" },
  { "command": "play" },
  { "command": "wait_frames", "count": 20 },
  { "command": "capture_screenshot", "path": "D:/Temp/render_check.png" },
  { "command": "run_lua", "code": "local e = Ignite.FindEntityByName('HeroCube'); return e and e:get_component('Transform').position.y or -1" },
  { "command": "stop" }
]
```
- **All `AgentController` Actions**:
  - Lifecycle & Timing: `ping`, `get_status`, `play`, `simulate`, `stop`, `pause`, `resume`, `step_frames`, `wait_frames`, `wait_seconds`, `quit`
  - Scene, Assets & Packaging: `load_scene`, `save_scene`, `import_asset`, `list_assets`, `package_game`
  - Entities & Components: `list_entities`, `get_entity`, `create_entity`, `duplicate_entity`, `destroy_entity`, `rename_entity`, `add_component`, `set_component`, `update_component`, `remove_component`
  - Camera, Rendering, Physics & Scripting: `set_camera`, `set_render_settings`, `capture_screenshot`, `physics_raycast`, `run_lua`
  - Networking: `net_start_server`, `net_stop_server`, `net_connect`, `net_disconnect`, `net_send_input`, `net_send_chat`
  - UI & Input Injection: `get_ui_info`, `inject_mouse_move`, `inject_mouse_button`, `inject_key`
- **Supported Primitives**: `Cube`, `Sphere`, `Plane`, `Cylinder`, `Cone`, `Torus`.
- **Supported Components**: `Transform`, `Rendering`, `StaticMesh`, `SkeletalMesh`, `Camera`, `DirectionalLight`, `PointLight`, `SpotLight`, `WorldEnvironment`, `Rigidbody`/`Rigidbody3D`, `Rigidbody2D`, `BoxCollider`, `SphereCollider`, `CapsuleCollider`, `BoxCollider2D`, `CircleCollider2D`, `CharacterController`, `AudioSource`, `LuaScript`, `NetworkIdentity`, `Sprite2D`, `Text`, `PhysicsJoint`/`Joint`, `ParticleSystem`/`Particle`.
