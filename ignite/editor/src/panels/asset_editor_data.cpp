// Copyright (c) 2026 Evangelion Manuhutu

#include "pch.hpp"

#include "asset_editor_data.hpp"
#include "ignite/graphics/renderer/asset_scene_renderer.hpp"
#include "ignite/graphics/objects/mesh.hpp"

#include "ext/editor_ui.hpp"

namespace ignite::UI
{
    void AssetEditorData::DrawSceneDetails()
    {
        if (ImGui::TreeNode("Scene Details"))
        {
            // Environment Texture
            DrawTexturePreviewDropTarget("Environment", previewEnvTexHandle, [&]()
                {
                    if (sceneData.sceneRenderer)
                        sceneData.sceneRenderer->SetEnvironmentTexture(previewEnvTexHandle);
                });

            // Post Processing Settings
            if (ImGui::TreeNodeEx("Post Processing"))
            {
                auto &pp = sceneData.sceneRenderer->GetPostProcessingSettings();
                auto &lens = sceneData.camera.lens;

                if (ImGui::TreeNodeEx("Anti Aliasing"))
                {
                    if (ImGui::TreeNodeEx("TAA", ImGuiTreeNodeFlags_DefaultOpen))
                    {
                        UI::DrawCheckbox("Enable", &pp.taaProperties.enable);
                        UI::DrawFloatControl("Blend Factor", &pp.taaProperties.blendFactor, 0.025f, 0.01f, 1.0f, 1.0f);
                        ImGui::TreePop();
                    }

                    if (ImGui::TreeNodeEx("MSAA", ImGuiTreeNodeFlags_DefaultOpen))
                    {
                        UI::DrawCheckbox("Enable", &pp.msaaProperties.enable);
                        UI::DrawIntControl("Samples", &pp.msaaProperties.sampleCount, 1, 1, 16);
                        ImGui::TextDisabled("Requires resolved MSAA render targets in this preview path.");
                        ImGui::TreePop();
                    }

                    UI::DrawFloatControl("Render Scale", &pp.renderScale, 0.01f, 0.25f, 1.0f, 1.0f);
                    ImGui::TreePop();
                }

                // Tonemapping & Color correction
                if (ImGui::TreeNodeEx("Color Correction"))
                {
                    const char *tonemapModes[] = { "Reinhard", "Uncharted 2", "Filmic" };
                    int currentTonemap = static_cast<int>(pp.tonemapMode);
                    if (UI::DrawComboBox("Tonemap Mode", tonemapModes, std::size(tonemapModes), &currentTonemap))
                    {
                        pp.tonemapMode = static_cast<TonemapMode>(currentTonemap);
                    }
                    // TODO: Add these controls back in when we have a proper color grading system
                    // UI::DrawFloatControl("Exposure", &pp.exposure, 0.025f, 0.0f, 10.0f, 1.0f);
                    // UI::DrawFloatControl("Gamma", &pp.gamma, 0.025f, 0.1f, 5.0f, 2.2f);
                    ImGui::TreePop();
                }

                if (ImGui::TreeNodeEx("Bloom", ImGuiTreeNodeFlags_DefaultOpen))
                {
                    UI::DrawCheckbox("Enable Bloom", &pp.enableBloom);
                    UI::DrawFloatControl("Bloom Intensity", &pp.bloomIntensity, 0.025f, 0.0f, 100.0f, 1.0f);
                    UI::DrawFloatControl("Bloom Threshold", &pp.bloomThreshold, 0.01f, 0.0f, 10.0f, 0.85f);
                    UI::DrawFloatControl("Bloom Knee", &pp.bloomKnee, 0.01f, 0.0f, 10.0f, 0.5f);
                    UI::DrawFloatControl("Bloom Radius", &pp.bloomRadius, 0.01f, 0.0f, 10.0f, 1.0f);
                    UI::DrawIntControl("Bloom Iterations", &pp.bloomIterations, 1.0f, 1, 16);
                    ImGui::TreePop();
                }

                if (ImGui::TreeNodeEx("Vignette", ImGuiTreeNodeFlags_DefaultOpen))
                {
                    UI::DrawCheckbox("Enable Vignette", &pp.enableVignette);
                    UI::DrawFloatControl("Vignette Radius", &pp.vignetteRadius, 0.005f, 0.0f, 2.0f, 1.0f);
                    UI::DrawFloatControl("Vignette Softness", &pp.vignetteSoftness, 0.005f, 0.0f, 2.0f, 0.5f);
                    UI::DrawFloatControl("Vignette Intensity", &pp.vignetteIntensity, 0.005f, 0.0f, 2.0f, 0.5f);
                    UI::DrawColorVec3("Vignette Color", pp.vignetteColor);
                    ImGui::TreePop();
                }

                if (ImGui::TreeNodeEx("Chromatic Aberration", ImGuiTreeNodeFlags_DefaultOpen))
                {
                    UI::DrawCheckbox("Enable Chromatic Aberration", &pp.enableChromAb);
                    UI::DrawFloatControl("ChromAb Amount", &pp.chromAbAmount, 0.0001f, 0.0f, 0.1f, 0.001f);
                    UI::DrawFloatControl("ChromAb Radial", &pp.chromAbRadial, 0.005f, 0.0f, 5.0f, 1.0f);
                    ImGui::TreePop();
                }

                if (ImGui::TreeNodeEx("HBAO", ImGuiTreeNodeFlags_DefaultOpen))
                {
                    UI::DrawCheckbox("Enable HBAO", &pp.enableSSAO);
                    UI::DrawFloatControl("AO Radius", &pp.aoRadius, 0.01f, 0.0f, 5.0f, 1.2f);
                    UI::DrawFloatControl("AO Bias", &pp.aoBias, 0.001f, 0.0f, 0.5f, 0.03f);
                    UI::DrawFloatControl("AO Intensity", &pp.aoIntensity, 0.025f, 0.0f, 10.0f, 1.0f);
                    UI::DrawFloatControl("AO Power", &pp.aoPower, 0.025f, 0.0f, 5.0f, 1.35f);
                    ImGui::TreePop();
                }

                if (ImGui::TreeNodeEx("Depth of Field", ImGuiTreeNodeFlags_DefaultOpen))
                {
                    UI::DrawCheckbox("Enable DOF", &lens.enabledDOF);
                    UI::DrawFloatControl("Focal Length", &lens.focalLength, 0.5f, 1.0f, 500.0f, 120.0f);
                    UI::DrawFloatControl("Focal Distance", &lens.focalDistance, 0.05f, 0.1f, 100.0f, 5.5f);
                    UI::DrawFloatControl("fStop", &lens.fStop, 0.05f, 0.1f, 22.0f, 1.4f);
                    UI::DrawFloatControl("Focus Range", &lens.focusRange, 0.05f, 0.1f, 100.0f, 5.0f);
                    UI::DrawFloatControl("Blur Amount", &lens.blurAmount, 0.025f, 0.0f, 10.0f, 1.0f);
                    ImGui::TreePop();
                }

                ImGui::TreePop();
            }

            ImGui::TreePop();
        }
    }

	void AssetEditorData::UpdateCamera(float deltaTime)
	{
		sceneData.camera.UpdateMouseState();
		sceneData.camera.mouse.scroll = { ImGui::GetIO().MouseWheelH, ImGui::GetIO().MouseWheel };

		if (sceneData.viewportHovered)
		{
			sceneData.camera.HandleOrbit(deltaTime);
			sceneData.camera.HandlePan(deltaTime);
			sceneData.camera.HandleZoom(deltaTime);
		}

		sceneData.camera.ApplyInertia(deltaTime);
		sceneData.camera.UpdateCameraPosition(deltaTime);

		if (sceneData.viewportWidth > 0 && sceneData.viewportHeight > 0)
		{
			sceneData.camera.UpdateProjection(sceneData.viewportWidth, sceneData.viewportHeight);
		}

		sceneData.camera.UpdateView();
	}

	void AssetEditorData::FocusCameraOnMesh(const Ref<Mesh> &mesh)
	{
		if (!mesh || sceneData.cameraFocused)
			return;

		const auto &aabb = mesh->localAABB;
		glm::vec3 focusCenter = (aabb.min + aabb.max) * 0.5f;
		glm::vec3 halfExtents = glm::abs(aabb.max - aabb.min);
		const float radius = glm::max(halfExtents.x, glm::max(halfExtents.y, halfExtents.z));
		const float fov = glm::radians(sceneData.camera.fov);
		const float distance = radius / std::tan(fov * 0.5f);
		sceneData.camera.FocusTarget(focusCenter, distance);
		sceneData.cameraFocused = true;
	}

}
