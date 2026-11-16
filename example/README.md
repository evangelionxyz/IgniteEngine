# Ignite Engine — Lua 3D Platformer, Multiplayer, Physics & Debug Font Feature Demo

This example project demonstrates Ignite Engine's Lua 5.4 gameplay scripting, Jolt 3D physics simulation (including constraints and motorized joints), ENet-backed client-server multiplayer networking with live telemetry, GPU particle systems, rigged skeletal animation state machines, and both **World-Space** and **Screen-Space** MSDF font rendering (including live debug entity-name overlays).

---

## Features Showcased

1. **Multiplayer Networking (`Scripts/player_controller.lua`, `Scripts/remote_player.lua`)**
   - Built-in ENet networking with client-side prediction, entity replication, and packet telemetry.
   - `PlayerRobot` is replicated with `NetworkIdentityComponent` (NetId 1001, local owner).
   - Dedicated/Listen server hosting (`F2`), remote connection to `127.0.0.1:8192` (`F3`), and disconnection (`F4`).
   - Replicated state and client chat emotes (`T` key).
   - Simulated `RemotePlayerBot` (NetId 1002, OwnerClient 2) demonstrating smooth remote interpolation, directional patrol AI, and overhead nametag sync.
   - Live network HUD displaying RTT ping, packet loss %, server/client ticks, and connection status.

2. **Physics Joints & Motorized Obstacles (`Scripts/joint_hazard.lua`)**
   - **Swinging Pendulum Hazard**: Dynamic suspended sphere (`Pendulum_Ball`) tethered to `Pendulum_Anchor` via a Jolt `Point` joint with real-time cable drawing via `Ignite.Debug.DrawLine`.
   - **Motorized Sweeper Blade**: Rotating hazard arm (`Sweeper_Arm`) connected to `Sweeper_Pillar` via a motorized Jolt `Hinge` joint driving continuous 45°/s circular rotation with a 10,000 N force limit.
   - Live hazard telemetry (linear velocity and rotational speed) monitored and rendered in the debug HUD.

3. **Particle Systems & Dynamic VFX (`Scripts/effects_manager.lua`)**
   - Dynamic GPU particle burst effects for platformer gameplay events:
     - Footstep dust puffs while sprinting across terrain.
     - Jump cloud puffs upon takeoff and landing shockwave rings.
     - Sparkle burst on collectible coin pickup.
     - High-energy impulse ring when launching off bounce pads.
     - Celebration fireworks burst upon reaching the summit goal.
     - Instant Nova Spark burst trigger via `Q` key.
   - Atmospheric braziers (`Brazier_Torch_01`, `Brazier_Torch_02`) pairing continuous fire particle emitters with dynamic flickering point lights.

4. **Lua-Driven 3D Platformer (`Scripts/player_controller.lua`, `Scripts/follow_camera.lua`)**
   - `CharacterController` movement with coyote time (`0.14s`), jump buffering (`0.14s`), sprinting, camera-relative directional steering, and smooth yaw turning via `cc:set_rotation(...)`.
   - Third-person orbit camera with raycast occlusion avoidance.
   - Moving kinematic platforms (`Scripts/moving_platform.lua`), launch bounce pads (`Scripts/bounce_pad.lua`), and trigger-based collectibles & summit goal (`Scripts/collectible.lua`).

5. **Physics Simulation (`Scripts/physics_playground.lua`)**
   - Interactive pyramid of dynamic `BoxTextured` physics crates, rolling high-restitution spheres, and on-demand dynamic cube/sphere spawning (`E` key) or raycast impulse blasting.
   - Press `R` to reset the crate stack and rolling spheres back to their initial transforms cleanly.

6. **Skeletal Animations (`Assets/Characters/RobotExpressive/RobotAnimator.ac`)**
   - Rigged glTF character (`RobotExpressive.glb`) driven by Lua via `SkeletalMesh` animator bindings (`skel:set_float("Speed", ...)`, `skel:set_bool("IsGrounded" / "IsDancing" / "IsWaving", ...)`, `skel.current_state`).
   - State machine transitions across `Idle`, `Walking`, `Running`, `Jump`, `Wave`, and `Dance` (with cross-fade transitions).
   - Companion NPC (`CompanionBot` + `Scripts/companion_emote.lua`) demonstrating an independent `unique_animator` instance.

7. **World-Space & Screen-Space Font Rendering + Debug Entity Names (`Scripts/debug_hud.lua`)**
   - **World-Space `TextComponent`**: 3D diegetic signs (`WorldSign_Start`, `WorldSign_Physics`, `WorldSign_Fixed3D`, `WorldSign_Summit`) and overhead player nametags with configurable `font_size`, `billboard`, `center_align`, and `screen_space = false`.
   - **Screen-Space `TextComponent`**: Top HUD banner (`HUD_Banner`, `screen_space = true`) positioned in viewport pixel coordinates and updated dynamically from Lua.
   - **Immediate-Mode Debug Rendering (`Ignite.Debug`)**:
     - `Ignite.Debug.DrawTextWorld(pos, text, color, worldScale, billboard, centerAlign)` — floating state/velocity telemetry above the player and remote bots.
     - `Ignite.Debug.DrawTextScreen(x, y, text, color, pixelSize, centerAlign)` — live FPS, position, velocity, grounded status, animation state, joint hazards, and multiplayer metrics panel.
     - `Ignite.Debug.DrawLine(a, b, color)` — pendulum cables, hazard trajectories, forward physics raycast line & gizmo lines.
     - `Ignite.Debug.SetShowEntityNames(true/false)` — toggle world-space entity-name labels above scene entities (also available in Editor viewport visibility settings via `showEntityNames`).

---

## Controls

| Input | Action |
| :--- | :--- |
| `W / A / S / D` or Arrow Keys | Move character (camera-relative) |
| `Left / Right Shift` | Sprint (`Running` animation + dust trail particles) |
| `Space` | Jump (with coyote time, jump buffer & takeoff particle burst) |
| `J / L / I / K` or Hold `Right Mouse` | Orbit camera yaw / pitch |
| `U / O` | Zoom camera in / out |
| `E` | Raycast physics impulse blast (or spawn dynamic physics prop) |
| `Q` | Trigger Nova Spark particle burst at player feet |
| `R` | Reset physics crate pyramid & spheres |
| `F` | Dance emote (when standing still) |
| `G` | Wave emote (when standing still) |
| `F2` | Host Dedicated / Listen Network Server (port 8192) |
| `F3` | Connect to Server (`127.0.0.1:8192`) |
| `F4` | Disconnect from Server |
| `T` | Send Network Chat Emote |
| `Tab` | Toggle debug entity names & `Ignite.Debug` overlays |

---

## Running the Demo

### In `Ignite.Editor.exe`
```powershell
D:\Dev\Ignite\bin\Debug\Ignite.Editor.exe --project D:\Dev\Ignite\example\PlatformerDemo.ixproj
```
Press **Play** (`Scene -> MainDemo.ixscene` loads automatically). You can toggle **Entity Names** in the viewport visibility menu during Edit or Play mode.

### In `Ignite.Runtime.exe` (Standalone)
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
