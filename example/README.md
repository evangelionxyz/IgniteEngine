# Ignite Engine — Lua 3D Platformer & Debug Font Feature Demo

This example project demonstrates Ignite Engine's Lua 5.4 gameplay scripting, Jolt 3D physics simulation, rigged skeletal animation state machines, and both **World-Space** and **Screen-Space** MSDF font rendering (including live debug entity-name overlays).

---

## Features Showcased

1. **Lua-Driven 3D Platformer (`Scripts/player_controller.lua`, `Scripts/follow_camera.lua`)**
   - `CharacterController` movement with coyote time (`0.14s`), jump buffering (`0.14s`), sprinting, camera-relative directional steering, and smooth yaw turning via `cc:set_rotation(...)`.
   - Third-person orbit camera with raycast occlusion avoidance.
   - Moving kinematic platforms (`Scripts/moving_platform.lua`), launch bounce pads (`Scripts/bounce_pad.lua`), and trigger-based collectibles & summit goal (`Scripts/collectible.lua`).
2. **Physics Simulation (`Scripts/physics_playground.lua`)**
   - Interactive pyramid of dynamic `BoxTextured` physics crates, rolling high-restitution spheres, and on-demand dynamic cube/sphere spawning (`E` key) or raycast impulse blasting.
   - Press `R` to reset the crate stack and rolling spheres back to their initial transforms cleanly.
3. **Skeletal Animations (`Assets/Characters/RobotExpressive/RobotAnimator.ac`)**
   - Rigged glTF character (`RobotExpressive.glb`) driven by Lua via `SkeletalMesh` animator bindings (`skel:set_float("Speed", ...)`, `skel:set_bool("IsGrounded" / "IsDancing" / "IsWaving", ...)`, `skel.current_state`).
   - State machine transitions across `Idle`, `Walking`, `Running`, `Jump`, `Wave`, and `Dance` (with cross-fade transitions).
   - Companion NPC (`CompanionBot` + `Scripts/companion_emote.lua`) demonstrating an independent `unique_animator` instance.
4. **World-Space & Screen-Space Font Rendering + Debug Entity Names (`Scripts/debug_hud.lua`)**
   - **World-Space `TextComponent`**: 3D diegetic signs (`WorldSign_Start`, `WorldSign_Physics`, `WorldSign_Fixed3D`, `WorldSign_Summit`) with configurable `font_size`, `billboard`, `center_align`, and `screen_space = false`.
   - **Screen-Space `TextComponent`**: Top HUD banner (`HUD_Banner`, `screen_space = true`) positioned in viewport pixel coordinates and updated dynamically from Lua.
   - **Immediate-Mode Debug Rendering (`Ignite.Debug`)**:
     - `Ignite.Debug.DrawTextWorld(pos, text, color, worldScale, billboard, centerAlign)` — floating state/velocity telemetry above the player.
     - `Ignite.Debug.DrawTextScreen(x, y, text, color, pixelSize, centerAlign)` — live FPS, position, velocity, grounded status, animation state, and raycast target HUD.
     - `Ignite.Debug.DrawLine(a, b, color)` — forward physics raycast line & gizmo lines.
     - `Ignite.Debug.SetShowEntityNames(true/false)` — toggle world-space entity-name labels above scene entities (also available in Editor viewport visibility settings via `showEntityNames`).

---

## Controls

| Input | Action |
| :--- | :--- |
| `W / A / S / D` or Arrow Keys | Move character (camera-relative) |
| `Left / Right Shift` | Sprint (`Running` animation) |
| `Space` | Jump (with coyote time & jump buffer) |
| `J / L / I / K` or Hold `Right Mouse` | Orbit camera yaw / pitch |
| `U / O` | Zoom camera in / out |
| `E` | Raycast physics impulse blast (or spawn dynamic physics prop) |
| `R` | Reset physics crate pyramid & spheres |
| `F` | Dance emote (when standing still) |
| `G` | Wave emote (when standing still) |
| `Tab` | Toggle debug entity names & `Ignite.Debug` overlays |

---

## Running the Demo

### In `Ignite.Editor.exe`
```powershell
D:\Dev\Ignite\bin\Debug\Ignite.Editor.exe --project D:\Dev\Ignite\example\PlatformerDemo.ixproj
```
Press **Play** (`Scene -> MainDemo.ixscene` loads automatically). You can also toggle **Entity Names** in the viewport visibility menu during Edit or Play mode.

### In `Ignite.Runtime.exe`
```powershell
D:\Dev\Ignite\bin\Debug\Ignite.Runtime.exe --project D:\Dev\Ignite\example\PlatformerDemo.ixproj
```

---

## Downloaded Open-Source Assets & Attribution

- **`RobotExpressive.glb`** (`Assets/Characters/RobotExpressive/`)
  - Source: [Three.js examples (`RobotExpressive.glb`)](https://github.com/mrdoob/three.js/blob/dev/examples/models/gltf/RobotExpressive/RobotExpressive.glb), originally created by **Tomas Laulhe (Quaternius)** — **CC0 1.0 Universal**.
- **`BoxTextured.glb` & `Duck.glb`** (`Assets/Props/`)
  - Source: [KhronosGroup `glTF-Sample-Assets`](https://github.com/KhronosGroup/glTF-Sample-Assets) — CC0 / Apache-2.0 / public sample models.
- **`JetBrainsMono-Bold.ttf`** (`Assets/Fonts/`)
  - Source: [JetBrains Mono](https://github.com/JetBrains/JetBrainsMono) — **SIL Open Font License 1.1 (OFL-1.1)**.
