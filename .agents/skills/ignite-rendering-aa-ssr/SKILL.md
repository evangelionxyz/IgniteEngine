---
name: ignite-rendering-aa-ssr
description: Maintain Ignite Engine's TAA, SSR (Screen-Space Reflections), and decoupled Anti-Aliasing (NONE/MSAA/FXAA/TAA) + GraphicsScalability pipeline without introducing oil-painting TAA smearing, noisy reflections, or Vulkan sync hazards.
---

# Ignite Rendering: TAA, SSR & Decoupled Anti-Aliasing

Use this skill whenever editing `composite.pixel.hlsl`, `fxaa.pixel.hlsl`, `ssr_trace.compute.hlsl`, `ssr_denoise.compute.hlsl`, `ssr_hiz.compute.hlsl`, `ssr.hpp/.cpp`, `scene_renderer.hpp/.cpp`, `scene_renderer_targets.cpp`, or `iscene_renderer.hpp/.cpp`.

---

## 1. TAA Invariants (Preventing "Oil Painting" Smearing & Posterization)

Every one of the bugs below has previously caused TAA to look like an oil painting or smear on static/moving scenes. **Never regress these invariants:**

1. **Dilated Closest-Depth MUST Compute Velocity, NOT Direct `historyUV`** (`ignite/editor/resources/shaders/composite.pixel.hlsl`):
   - After finding `closestUV` / `closestDepth` in the 3x3 cross, reproject `closestUV` through `taaReprojection` to get `prevClosestUV`, then compute:
     ```hlsl
     float2 velocityUV = prevClosestUV - closestUV;
     historyUV = input.uv + velocityUV;
     ```
   - **NEVER** set `historyUV = prevClosestUV` directly! When the camera is static (`taaReprojection == Identity`), `prevClosestUV == closestUV`, so setting `historyUV = prevClosestUV` shifts every pixel on every angled wall/floor/column by `(±1, ±1)` px per frame, causing recursive directional advection (literal oil-painting brushstrokes).

2. **`taaHistoryRT` MUST Be `RGBA16_SFLOAT` (Never `RGBA8_UNORM`)** (`scene_renderer_targets.cpp`, `scene_renderer.cpp`):
   - Blending with low `blendFactor` (`0.05`–`0.10`) in 8-bit `RGBA8_UNORM` quantizes small deltas (`< 10/255`) to `0`, freezing history into flat posterized blotches.
   - Keep `taaHistoryRT[0..2]` as `nri::Format::RGBA16_SFLOAT` (`COLOR_ATTACHMENT`), render `CompositePass` directly into `taaHistoryRT[frameIndexInFlight]`, and present it to `finalCompositeFB` / `compositeRT` (`RGBA8_UNORM`, required by ImGui & `capture_screenshot`) via `FXAAPass(cmd, destFB, historyColor, /*bypass=*/true)`.
   - Reset `target->taaHistoryValid = false` whenever `taaHistoryRT` is created or resized.

3. **Do NOT Pre-Blur the Current Frame and Do NOT Sharpen Inside the Feedback Loop** (`composite.pixel.hlsl`):
   - Use the exact center `tonemappedScene` texel as `centerRGB` — do **not** apply a 3x3 Gaussian/Blackman-Harris blur to the current frame before blending into history.
   - **Never** apply unsharp-mask/CAS sharpening inside `CompositePass` when writing to `taaHistoryRT`; feedback sharpening compounds every frame and creates high-frequency grain at low `blendFactor`. (If sharpening is ever added, apply it strictly in the non-feedback presentation pass.)

4. **Neighborhood Clamping & Specular Stability** (`composite.pixel.hlsl`):
   - Compute 3x3 YCoCg mean `mu`, std `sigma` (small floor `float3(0.001, 0.0005, 0.0005)`), and `minYCoCg`/`maxYCoCg`.
   - Use tight variance clipping `varianceGamma = lerp(1.25f, 0.75f, saturate(motionPixels * 0.5f))`, intersected with `[minYCoCg, maxYCoCg]`, and ray-box `ClipAABB` toward `mu`.
   - Blend `centerRGB` and `clampedHistory` using Karis luma weights `1.0 / (1.0 + luma)` to prevent specular highlight flicker without softening detail.

5. **Always Increment `m_TAAFrameIndex` Every Frame** (`scene_renderer.cpp`):
   - Increment `++m_TAAFrameIndex` unconditionally every rendered frame (not only when TAA is enabled) so GTAO and SSR temporal dithering animate cleanly under FXAA and NONE too.

---

## 2. SSR Invariants (Preventing Noisy / Speckled Reflections on FXAA & TAA)

1. **Hi-Z Raymarch Must Rewind `t = tPrev` Before Descending Mips** (`ssr_trace.compute.hlsl`):
   - When `currSS.z >= hizDepth && mip > 0`, **rewind `t = tPrev`**, decrement `mip = max(mip - 1, 0)`, and step `t += float(1 << mip)`.
   - Never decrement `mip` in place at an overshot `t` — coarse strides (`1 << mip`) overshoot thin walls (`diff > thickness`) and cause salt-and-pepper hit/miss noise.
   - Cap Hi-Z traversal mip to `min(int(maxDepthMip), 3)` and use `thickness = max(0.12f, surfLinearZ * 0.05f)`.

2. **Temporally Animated Dither + Radiance Clamping** (`ssr_trace.compute.hlsl`):
   - Offset `Bayer4x4(DTid.xy)` each frame with `frac(float(frameIndex & 255u) * 0.61803399f)` so static 4x4 half-res checkerboard blocks never lock onto reflections.
   - Clamp sampled HDR radiance (`<= 8.0`) and apply smooth screen-edge + ray-distance (`distFade`) fadeouts.

3. **5x5 Karis-Weighted Bilateral Spatial Filter + Temporal Accumulation** (`ssr_denoise.compute.hlsl`, `ssr.hpp/.cpp`, `scene_renderer.cpp`):
   - Filter `t_RawSSR` with a 5x5 Gaussian-weighted depth/normal bilateral kernel (`pow(saturate(dot(sn, centerN)), 8.0)`) multiplied by Karis firefly weight `1.0 / (1.0 + Luma(s.rgb))`.
   - Reproject `centerUV` via `projectionInv` -> `viewInv` -> `prevViewProj` into the previous frame's `m_SSRFinal` (`prevSsr = ssrVec[(slotIdx + ssrCount - 1) % ssrCount]`, zero extra VRAM), clamp history against the 3x3 valid-neighbor variance box (`mu ± 1.5 * sigma`) and `[minAlpha, maxAlpha]`, and blend with `lerp(0.12f, 0.60f, saturate(motionPx * 0.25f))`.
   - Pass the active frame `projection` (jittered when TAA is active, unjittered otherwise) into `SSR::Build(...)` so depth reconstruction matches `depthTexture`.
   - Reset `target->ssrHistoryValid = false` on framebuffer resize or whenever `prevSceneColor` resets.

---

## 3. Decoupled Anti-Aliasing & Scalability (`GameSettings.ign`)

- Anti-Aliasing is **independent** of `GraphicsScalability` (`Low`/`Medium`/`High`/`Ultra`). Changing scalability presets must never overwrite `sampleMethod`, `msaaProperties`, `fxaaProperties`, or `taaProperties`.
- Supported modes: `NONE`, `MSAA` (`sampleCount`), `FXAA` (`subpixQuality`, `edgeThreshold`, `edgeThresholdMin`), and `TAA` (`blendFactor`).
- `ISceneRenderer::SerializeToFile` / `DeserializeFromFile` persist a dedicated `"AntiAliasing"` JSON section in `GameSettings.ign` (with fallback reading of legacy `SceneRenderer` keys).

---

## 4. Verification Checklist

1. **Clear Stale SPIR-V Cache Before Testing Shader Edits**:
   Compiled SPIR-V lives in `resources/shaders/bin/*.spirv` (not `resources/shaders/`). After syncing HLSL to `bin/Release/resources/shaders/` and `bin/Debug/resources/shaders/`, delete the modified `.spirv` files in `bin/<Config>/resources/shaders/bin/` so they recompile.
2. **Visual Verification**: Capture `NONE`, `FXAA`, `TAA (0.05)`, and `TAA (0.10)` screenshots on `D:\Dev\IgniteProject\Hello\Hello.ixproj` with SSR enabled and inspect them side-by-side.
3. **Debug Vulkan Sync Validation**: Run `Ignite.Editor.exe` (`Debug`) with `$env:IGNITE_VK_SYNC_VALIDATION = "1"` across both FXAA+SSR and TAA+SSR and verify **0** `SYNC-HAZARD` / `Validation Error` / `VUID-` messages.
