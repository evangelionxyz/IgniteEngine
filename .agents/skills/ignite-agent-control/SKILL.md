---
name: ignite-agent-control
description: Control Ignite.Editor.exe, Ignite.Runtime.exe, packaged games, and Ignite.Network.exe via AgentExecutionContext, CLI command scripts, and scripts/ignite_mcp_server.py.
---

# Ignite Agent Control & MCP Automation

## 1. Batch Script Execution (`--command-script`)
Run reproducible Editor/Runtime automation workflows without manual clicking:
```powershell
D:\Dev\Ignite\bin\Debug\Ignite.Editor.exe `
  --project="D:\Path\To\Project.ixproj" `
  --windowed `
  --command-script "D:\Path\To\commands.json" `
  --command-report "D:\Path\To\report.json" `
  --quit-after-script
```
Notice `Application::Tick` checks `AgentController::IsAutomationActive()`, so rendering and simulation proceed even when the window does not have OS focus.

## 2. Live Interactive Session via MCP Server
Start `python D:\Dev\Ignite\scripts\ignite_mcp_server.py` (stdio MCP JSON-RPC 2.0):
- `ignite_launch_app(target="editor", project_path=..., windowed=True)` spawns the app with `--watch-command-file` / `--response-file` and waits for `ping`.
- `ignite_send_command(session_id=..., command=..., params={...})` dispatches single JSON commands synchronously and returns structured results.
- `ignite_package_game(project_path=..., output_dir=..., include_server=True)` exports a self-contained game build.
- `ignite_start_server(scene_path=..., project_path=..., port=8192)` launches `Ignite.Network.exe`.

## 3. Command JSON Reference
Example command array:
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
Supported primitives: `Cube`, `Sphere`, `Plane`, `Cylinder`, `Cone`, `Torus`.
Supported components for `add_component`/`set_component`/`remove_component`:
`Transform`, `StaticMesh`, `Camera`, `DirectionalLight`, `PointLight`, `SpotLight`, `Rigidbody`/`Rigidbody3D`, `Rigidbody2D`, `BoxCollider`, `SphereCollider`, `CapsuleCollider`, `AudioSource`, `LuaScript`, `NetworkIdentity`, `Text`.
