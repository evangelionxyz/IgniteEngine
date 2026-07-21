// Copyright (c) 2026 Evangelion Manuhutu

#include "pch.hpp"

#ifndef IMGUI_DEFINE_MATH_OPERATORS
#define IMGUI_DEFINE_MATH_OPERATORS
#endif

#include "asset_editor_panel.hpp"
#include "editor_layer.hpp"
#include "ext/editor_ui.hpp"
#include "animation_timeline.hpp"
#include "editors/animator_editor.hpp"
#include "editors/blend_space_editor.hpp"
#include "editors/widget_editor.hpp"
#include "editors/scriptable_object_editor.hpp"
#include "ignite/animation/animator/animator.hpp"
#include "ignite/animation/animator/animator_controller.hpp"
#include "ignite/animation/animator/animator_controller_2d.hpp"
#include "ignite/animation/animation_2d.hpp"
#include "ignite/animation/animation_montage.hpp"
#include "ignite/animation/locomotion.hpp"
#include "ignite/animation/blend_space.hpp"
#include "ignite/animation/skeletal_animation.hpp"
#include "ignite/animation/skeleton.hpp"
#include "ignite/asset/asset_importer.hpp"

#include "ignite/graphics/ui/widget.hpp"
#include "ignite/graphics/ui/widget_container.hpp"
#include "ignite/graphics/ui/widget_canvas.hpp"
#include "ignite/graphics/ui/widget_button.hpp"
#include "ignite/graphics/ui/widget_label.hpp"
#include "ignite/graphics/ui/widget_image.hpp"

#include "ignite/scripting/scriptable_object.hpp"

#include "ignite/graphics/objects/material_2d.hpp"
#include "ignite/graphics/texture.hpp"
#include "ignite/project/project.hpp"
#include "ignite/scene/sprite_sheet.hpp"
#include "ignite/serializer/binary_serializer.hpp"
#include "ignite/serializer/serializer.hpp"
#include "ignite/core/profiler/profiler.hpp"
#include "ignite/graphics/objects/material.hpp"
#include "ignite/graphics/gpu_upload_sync.hpp"
#include "ignite/imgui/gizmo.hpp"
#include "ignite/math/math.hpp"
#include "ignite/graphics/objects/mesh.hpp"
#include "ignite/asset/asset_manager.hpp"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <format>
#include <iterator>
#include <ranges>
#include <unordered_map>
#include <vector>

#include <glm/gtx/quaternion.hpp>

namespace ignite
{
    namespace
    {
        struct TextureEditorState
        {
            TextureCreateInfo createInfo;
            bool initialized = false;
        };

        struct SpriteSheetEditorState
        {
            glm::vec2 selectionStartUV = { 0.0f, 0.0f };
            glm::vec2 selectionEndUV = { 0.0f, 0.0f };
            glm::vec2 dragStartMinUV = { 0.0f, 0.0f };
            glm::vec2 dragStartMaxUV = { 0.0f, 0.0f };
            glm::vec2 dragOffsetUV = { 0.0f, 0.0f };
            glm::vec2 pan = { 0.0f, 0.0f };
            std::vector<std::string> spriteNames;
            int selectedSpriteIndex = -1;
            int renamingSpriteIndex = -1;
            int activeHandle = -1;
            int gridColumns = 2;
            int gridRows = 2;
            float snapStepU = 0.001f;
            float snapStepV = 0.001f;
            float zoom = 1.0f;
            float extractedPanelHeight = 170.0f;
            float previewColumnWidth = 0.0f;
            bool snappingEnabled = true;
            bool snapToGrid = false;
            bool selecting = false;
            char renameBuffer[128] = {};
        };

        struct Animation2DEditorState
        {
            float   playbackTime = 0.0f;
            float   lastRealTime = 0.0f;
            float   previewZoom = 1.0f;
            float   toolsWidth = 280.0f;
            int     previewFrame = 0;
            bool    playing = false;
        };

        struct MaterialEditorState
        {
            float previewColumnWidth = 0.0f;
            int selectedMeshType = 0;
            bool initialized = false;
        };

        struct SkeletonPreviewEditorState
        {
            AssetHandle previewMeshHandle = AssetHandle(0);
            AssetHandle previewAnimationHandle = AssetHandle(0);
            Ref<SkeletalMesh> cachedPreviewMesh;
            std::vector<glm::mat4> previewGlobalTransforms;
            std::vector<glm::mat4> previewFinalTransforms;
            float timeSeconds = 0.0f;
            float animationTimelineHeight = 180.0f;
            int gizmoTarget = 0;
            bool playing = false;
            bool loop = false;
        };

        struct MeshEditorState
        {
            AssetHandle skeletonHandle = AssetHandle(0);
            Ref<Skeleton> cachedSkeleton;
			float previewColumnWidth = 0.0f;
        };

        static std::unordered_map<uint64_t, MaterialEditorState> s_MaterialEditorState;
        static std::unordered_map<uint64_t, SkeletonPreviewEditorState> s_SkeletonPreviewEditorState;
        static std::unordered_map<uint64_t, AnimatorControllerEditorState> s_AnimatorControllerEditorState;

        static std::unordered_map<uint64_t, MeshEditorState> s_MeshEditorState;
        static std::unordered_map<uint64_t, TextureEditorState> s_TextureEditorState;
        static std::unordered_map<uint64_t, SpriteSheetEditorState> s_SpriteSheetEditorState;
        static std::unordered_map<uint64_t, Animation2DEditorState> s_Anim2DEditorState;
        static std::unordered_map<uint64_t, BlendSpaceEditorState> s_BlendSpaceEditorState;

        static Gizmo s_SkeletonPreviewGizmo;

        static Ref<Material> s_SkeletonPreviewMaterial;

        static ignite::Path BuildAssetMetaPath(Project *project, const AssetMetaData &metadata)
        {
            if (!project)
            {
                return {};
            }

            ignite::Path assetPath = project->GetProjectFilepath(metadata.filepath);
            assetPath += ".meta";
            return assetPath;
        }

        static std::string BuildAssetEditorPinOwnerTag(AssetHandle handle)
        {
            return std::format("editor.asset-editor.{}", static_cast<uint64_t>(handle));
        }

        static void SaveAnimatorControllerEditorMeta(Project *project, const Ref<AnimatorController> &controller, const AssetMetaData &metadata,
            const AnimatorControllerEditorState &ui)
        {
            if (!project || !controller || controller->handle == AssetHandle(0))
            {
                return;
            }

            const ignite::Path metaPath = BuildAssetMetaPath(project, metadata);
            if (metaPath.empty())
            {
                return;
            }

            controller->SerializeMetaFile(metaPath, [&](Serializer &sr)
            {
                sr.BeginMap("AnimatorEditor");
                sr.BeginMap("Graph");
                sr.AddKeyValue("PanX", ui.graphState.pan.x);
                sr.AddKeyValue("PanY", ui.graphState.pan.y);
                sr.AddKeyValue("Zoom", ui.graphState.zoom);
                sr.AddKeyValue("AnyStatePosX", ui.anyStateEditorPos.x);
                sr.AddKeyValue("AnyStatePosY", ui.anyStateEditorPos.y);
                sr.EndMap();
                sr.EndMap();
            });
        }

        static bool LoadAnimatorControllerEditorMeta(Project *project, const AssetMetaData &metadata, AnimatorControllerEditorState &ui)
        {
            if (!project)
            {
                return false;
            }

            const ignite::Path metadataPath = BuildAssetMetaPath(project, metadata);
            if (metadataPath.empty() || !ignite::Path::exists(metadataPath))
            {
                return false;
            }

            const YAML::Node root = Serializer::Deserialize(metadataPath);
            const YAML::Node editorNode = root["DATA"]["AnimatorEditor"];
            if (!editorNode)
            {
                return false;
            }

            const YAML::Node graphNode = editorNode["Graph"];
            if (graphNode)
            {
                if (graphNode["PanX"]) ui.graphState.pan.x = graphNode["PanX"].as<float>();
                if (graphNode["PanY"]) ui.graphState.pan.y = graphNode["PanY"].as<float>();
                if (graphNode["Zoom"]) ui.graphState.zoom = graphNode["Zoom"].as<float>();
                if (graphNode["AnyStatePosX"]) ui.anyStateEditorPos.x = graphNode["AnyStatePosX"].as<float>();
                if (graphNode["AnyStatePosY"]) ui.anyStateEditorPos.y = graphNode["AnyStatePosY"].as<float>();
            }

            return true;
        }

        static const char *TextureFormatToString(nvrhi::Format format)
        {
            switch (format)
            {
                case nvrhi::Format::RGBA8_UNORM: return "RGBA8_UNORM";
                case nvrhi::Format::RGBA32_FLOAT: return "RGBA32_FLOAT";
                default: return "UNKNOWN";
            }
        }

        static const char *SamplerAddressModeToString(nvrhi::SamplerAddressMode mode)
        {
            switch (mode)
            {
                case nvrhi::SamplerAddressMode::Repeat: return "Repeat";
                case nvrhi::SamplerAddressMode::ClampToEdge: return "ClampToEdge";
                case nvrhi::SamplerAddressMode::ClampToBorder: return "ClampToBorder";
                default: return "Other";
            }
        }

        static const char *TextureChannelToString(int channel)
        {
            switch (channel)
            {
                case 0: return "R";
                case 1: return "G";
                case 2: return "B";
                case 3: return "A";
                default: return "R";
            }
        }

        template<typename TOnChanged>
        static void DrawTexturePreviewDropTarget(const char *label, AssetHandle &textureHandle, TOnChanged &&onChanged)
        {
            auto assetManager = AssetManager::GetInstance();

            ImGui::PushID(label);
            ImGui::TextUnformatted(label);

            if (ImGui::BeginTable("##texture_preview_table", 2, ImGuiTableFlags_SizingFixedFit))
            {
                ImGui::TableSetupColumn("Preview", ImGuiTableColumnFlags_WidthFixed, 96.0f);
                ImGui::TableSetupColumn("Details", ImGuiTableColumnFlags_WidthStretch);

                ImGui::TableNextRow();
                ImGui::TableNextColumn();

                const ImVec2 previewSize(96.0f, 96.0f);
                const ImVec2 previewMin = ImGui::GetCursorScreenPos();
                const ImVec2 previewMax = ImVec2(previewMin.x + previewSize.x, previewMin.y + previewSize.y);

                ImGui::InvisibleButton("##texture_preview", previewSize);

                ImDrawList *drawList = ImGui::GetWindowDrawList();
                const ImU32 dark = IM_COL32(70, 70, 70, 255);
                const ImU32 light = IM_COL32(100, 100, 100, 255);
                constexpr float tileSize = 16.0f;
                for (float y = previewMin.y; y < previewMax.y; y += tileSize)
                {
                    for (float x = previewMin.x; x < previewMax.x; x += tileSize)
                    {
                        const int ix = static_cast<int>((x - previewMin.x) / tileSize);
                        const int iy = static_cast<int>((y - previewMin.y) / tileSize);
                        const ImU32 color = ((ix + iy) % 2 == 0) ? dark : light;
                        drawList->AddRectFilled(ImVec2(x, y), ImVec2(std::min(x + tileSize, previewMax.x), std::min(y + tileSize, previewMax.y)), color);
                    }
                }

                Ref<Texture> texture = nullptr;
                if (textureHandle != AssetHandle(0))
                {
                    texture = assetManager->GetAsset<Texture>(textureHandle);
                }

                if (texture && texture->GetHandle())
                {
                    drawList->AddImage(
                        reinterpret_cast<ImTextureID>(texture->GetHandle().Get()),
                        previewMin,
                        previewMax,
                        ImVec2(0.0f, 1.0f),
                        ImVec2(1.0f, 0.0f));
                }

                drawList->AddRect(previewMin, previewMax, IM_COL32(20, 20, 20, 255), 0.0f, 0, 1.5f);

                if (ImGui::BeginDragDropTarget())
                {
                    if (const ImGuiPayload *payload = ImGui::AcceptDragDropPayload(DND_PAYLOAD_CONTENT_BROWSER_ITEM))
                    {
                        if (payload->Data && payload->DataSize == sizeof(AssetHandle))
                        {
                            const AssetHandle droppedHandle = *static_cast<const AssetHandle *>(payload->Data);
                            const AssetMetaData &droppedMetadata = assetManager->GetMetaData(droppedHandle);
                            if (droppedMetadata.type == AssetType::Texture)
                            {
                                textureHandle = droppedHandle;
                                onChanged();
                            }
                        }
                    }
                    ImGui::EndDragDropTarget();
                }

                ImGui::TableNextColumn();

                std::string textureDisplayName = assetManager->GetAssetDisplayName(textureHandle);
                ImGui::TextWrapped("%s", textureDisplayName.c_str());
                ImGui::TextDisabled("ID: %llu", static_cast<unsigned long long>(textureHandle));

                if (textureHandle != AssetHandle(0))
                {
                    ImGui::Spacing();
                    if (ImGui::Button("Clear Texture"))
                    {
                        textureHandle = AssetHandle(0);
                        onChanged();
                    }
                }

                ImGui::EndTable();
            }

            ImGui::PopID();
        }

		static void DrawSceneDetails(AssetEditorData &assetData)
		{
			if (ImGui::TreeNode("Scene Details"))
			{
				// Environment Texture
				DrawTexturePreviewDropTarget("Environment", assetData.previewEnvTexHandle, [&]()
				{
					if (assetData.sceneData.sceneRenderer)
						assetData.sceneData.sceneRenderer->SetEnvironmentTexture(assetData.previewEnvTexHandle);
				});

				// Post Processing Settings
				if (ImGui::TreeNodeEx("Post Processing"))
				{
					auto &pp = assetData.sceneData.sceneRenderer->GetPostProcessingSettings();
					auto &lens = assetData.sceneData.camera.lens;

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

        static bool DrawMeshInstanceMaterial(const Ref<MeshInstance> &instance, const char *meshName)
        {
            bool dirty = false;

			std::string materialButtonLabel = "Drop Material Here";
			const AssetHandle materialHandle = instance->GetMaterialAssetHandle();
			if (materialHandle != AssetHandle(0))
			{
				materialButtonLabel = AssetManager::GetInstance()->GetAssetDisplayName(materialHandle);
			}

			constexpr float closeButtonWidth = 24.0f;
			const float materialButtonWidth = ImGui::GetContentRegionAvail().x - closeButtonWidth - 4.0f;

			ImGui::TextUnformatted(meshName);
			ImGui::Button(materialButtonLabel.c_str(), ImVec2(materialButtonWidth, 0.0f));
			if (ImGui::BeginDragDropTarget())
			{
				if (const ImGuiPayload *payload = ImGui::AcceptDragDropPayload(DND_PAYLOAD_CONTENT_BROWSER_ITEM))
				{
					if (payload->Data && payload->DataSize == sizeof(AssetHandle))
					{
						const AssetHandle droppedHandle = *static_cast<const AssetHandle *>(payload->Data);
						const AssetMetaData &metadata = AssetManager::GetInstance()->GetMetaData(droppedHandle);
						if (metadata.type == AssetType::Material)
						{
							instance->SetMaterial(droppedHandle);
							dirty = true;
						}
					}
				}
				ImGui::EndDragDropTarget();
			}

			ImGui::SameLine();
			if (materialHandle != AssetHandle(0) && ImGui::Button("X", ImVec2(closeButtonWidth, 0.0f)))
			{
				instance->SetMaterial(AssetHandle(0));
                dirty = true;
			}

            return dirty;
        }

        static AssetHandle FindFirstMeshHandleForSkeleton(AssetHandle skeletonHandle)
        {
            if (skeletonHandle == AssetHandle(0))
                return AssetHandle(0);

            auto assetManager = AssetManager::GetInstance();
            if (!assetManager)
                return AssetHandle(0);

            for (const auto &[meshHandle, metadata] : assetManager->GetAssetAssetRegistry())
            {
                if (metadata.type != AssetType::SkeletalMesh)
                    continue;

                auto mesh = assetManager->GetAsset<SkeletalMesh>(meshHandle);
                if (mesh && mesh->GetSkeletonHandle() == skeletonHandle)
                    return meshHandle;
            }

            return AssetHandle(0);
        }
    }

    // :Constructor
    AssetEditorPanel::AssetEditorPanel(const char *windowTitle, EditorLayer *editor) : IPanel(windowTitle, editor)
    { }

    AssetEditorPanel::~AssetEditorPanel()
    {
        if (m_EditorLayer)
        {
            m_EditorLayer->m_AssetEditorPanel = nullptr;
        }
    }

    void AssetEditorPanel::OnAttach()
    {
        m_OpenSignalToken = SignalBus::Subscribe<AssetEditorOpenSignal>(
            [this](const AssetEditorOpenSignal& signal)
            {
                OnAssetEditorOpenSignal(signal);
            });

        m_CreateSignalToken = SignalBus::Subscribe<AssetEditorCreateSignal>(
            [this](const AssetEditorCreateSignal& signal)
            {
                OnAssetEditorCreateSignal(signal);
            });
    }

    void AssetEditorPanel::OnDetach()
    {
        SignalBus::Unsubscribe<AssetEditorOpenSignal>(m_OpenSignalToken);
        SignalBus::Unsubscribe<AssetEditorCreateSignal>(m_CreateSignalToken);
        m_OpenSignalToken   = kInvalidSignalToken;
        m_CreateSignalToken = kInvalidSignalToken;

        s_SkeletonPreviewMaterial.reset();
        s_SkeletonPreviewEditorState.clear();

        m_Assets.clear();
    }

    void AssetEditorPanel::OnUpdate(float deltaTime)
    {
        if (!m_EditorLayer || !m_EditorLayer->GetActiveProject())
        {
            return;
        }

        auto assetManager = AssetManager::GetInstance();

        for (auto &assetData : m_Assets)
        {
            if (!assetData.isOpen)
                continue;

            if (!assetData.asset || !assetData.asset->IsReady())
            {
                // Try to retrieve the loaded asset from the manager.
                Ref<Asset> loaded = assetManager->GetAsset(assetData.handle);
                if (loaded && loaded->IsReady())
                {
                    auto &sceneRenderer = assetData.sceneData.sceneRenderer;
                    switch (assetData.metadata.type)
                    {
                        case AssetType::Material:
                        {
                            if (sceneRenderer)
                            {
                                sceneRenderer->SetPreviewStaticMesh(Renderer::GetDefaultMesh(EMeshType::UV_SPHERE));
                                sceneRenderer->SetPreviewMaterial(loaded->As<Material>());
                                assetData.asset = loaded;
                            }
                            break;
                        }
                        case AssetType::StaticMesh:
                        {
                            if (sceneRenderer)
                            {
                                sceneRenderer->SetPreviewStaticMesh(loaded->As<StaticMesh>());
                                assetData.asset = loaded;
                            }
                            break;
                        }
						case AssetType::SkeletalMesh:
						{
							if (sceneRenderer)
							{
								sceneRenderer->SetPreviewSkeletalMesh(loaded->As<SkeletalMesh>());
								assetData.asset = loaded;
							}
							break;
						}
                        case AssetType::Skeleton:
                        {
                            if (sceneRenderer)
                            {
                                assetData.asset = loaded;
                            }
                            break;
                        }
                        case AssetType::Widget:
                        {
                            if (sceneRenderer)
                            {
                                sceneRenderer->SetPreviewWidget(loaded->As<WidgetCanvas>());
                                assetData.asset = loaded;

                            }
                            break;
                        }
                        default:
                        {
                            assetData.asset = loaded;
                            break;
                        }
                    }

                    if (assetData.metadata.type == AssetType::Mesh 
                        || assetData.metadata.type == AssetType::StaticMesh 
                        || assetData.metadata.type == AssetType::SkeletalMesh)
                    {
                        auto mesh = loaded->As<Mesh>();
                        if (mesh)
                        {
							const auto &aabb = mesh->localAABB;
							glm::vec3 focusCenter = (aabb.min + aabb.max) * 0.5f;
							glm::vec3 halfExtents = glm::abs(aabb.max - aabb.min);
							const float radius = glm::max(halfExtents.x, glm::max(halfExtents.y, halfExtents.z));
							const float fov = glm::radians(assetData.sceneData.camera.fov);
							const float distance = radius / std::tan(fov * 0.5f);
							assetData.sceneData.camera.FocusTarget(focusCenter, distance);
                        }
                    }
                }
            }
        }

        // -----------------------------------------------
        // Per-asset animation / skeleton preview update
        // -----------------------------------------------
        for (auto &assetData : m_Assets)
        {
            if (!assetData.sceneData.viewportVisible || !assetData.isOpen || !assetData.asset || !assetData.asset->IsReady())
            {
                continue;
            }

            if (assetData.metadata.type != AssetType::Skeleton && assetData.metadata.type != AssetType::SkeletalMesh)
            {
                continue;
            }

            Ref<Skeleton> skeleton = nullptr;
            if (assetData.metadata.type == AssetType::Skeleton)
            {
                skeleton = assetData.asset->As<Skeleton>();
            }
            else
            {
                auto mesh = assetData.asset->As<SkeletalMesh>();
                if (!mesh)
                {
                    continue;
                }

                const AssetHandle skeletonHandle = mesh->GetSkeletonHandle();
                if (skeletonHandle != AssetHandle(0))
                {
                    skeleton = assetManager->GetAsset<Skeleton>(skeletonHandle);
                }
            }

            if (!skeleton)
            {
                if (assetData.sceneData.sceneRenderer)
                {
                    assetData.sceneData.sceneRenderer->SetBoneTransforms({});
                }
                continue;
            }

            SkeletonPreviewEditorState &previewState = s_SkeletonPreviewEditorState[static_cast<uint64_t>(skeleton->handle)];

            if (assetData.sceneData.sceneRenderer)
            {
                if (previewState.previewMeshHandle == AssetHandle(0))
                {
                    previewState.previewMeshHandle = FindFirstMeshHandleForSkeleton(skeleton->handle);
                    previewState.cachedPreviewMesh.reset();
                }

                if (previewState.previewMeshHandle != AssetHandle(0))
                {
                    if (!previewState.cachedPreviewMesh || previewState.cachedPreviewMesh->handle != previewState.previewMeshHandle)
                    {
                        previewState.cachedPreviewMesh = assetManager->GetAsset<SkeletalMesh>(previewState.previewMeshHandle);
                    }

                    if (previewState.cachedPreviewMesh && previewState.cachedPreviewMesh->IsReady())
                    {
                        assetData.sceneData.sceneRenderer->SetPreviewSkeletalMesh(previewState.cachedPreviewMesh);
                    }
                    else
                    {
                        assetData.sceneData.sceneRenderer->SetPreviewSkeletalMesh(nullptr);
                    }
                }
                else
                {
                    assetData.sceneData.sceneRenderer->SetPreviewSkeletalMesh(nullptr);
                }
            }

            Ref<SkeletalAnimation> previewAnimation = nullptr;
            if (previewState.previewAnimationHandle != AssetHandle(0))
            {
                previewAnimation = assetManager->GetAsset<SkeletalAnimation>(previewState.previewAnimationHandle);
            }

            if (previewAnimation && previewAnimation->duration > 0.0f)
            {
                if (previewState.playing)
                {
                    previewState.timeSeconds += deltaTime;
                    const float animDurationSec = previewAnimation->duration / std::max(previewAnimation->ticksPerSeconds, 0.0001f);
                    if (previewState.loop)
                    {
                        previewState.timeSeconds = std::fmod(previewState.timeSeconds, std::max(animDurationSec, 0.0001f));
                    }
                    else
                    {
                        previewState.timeSeconds = std::min(previewState.timeSeconds, animDurationSec);
                    }
                }
            }

            const size_t jointCount = skeleton->joints.size();
            previewState.previewGlobalTransforms.resize(jointCount, glm::mat4(1.0f));
            previewState.previewFinalTransforms.resize(jointCount, glm::mat4(1.0f));

            const bool hasPreviewAnimation = previewAnimation && previewAnimation->duration > 0.0f;
            const float timeInTicks = hasPreviewAnimation
                ? previewState.timeSeconds * std::max(previewAnimation->ticksPerSeconds, 0.0001f)
                : 0.0f;

            for (size_t jointIndex = 0; jointIndex < jointCount; ++jointIndex)
            {
                const Joint &joint = skeleton->joints[jointIndex];

                glm::mat4 local = joint.defaultTransform.GetMatrix();
                if (hasPreviewAnimation)
                {
                    if (previewAnimation->channels.contains((int)jointIndex))
                    {
                        // defaultTransform as fallback (inside Calculate function)
                        const Transform localTransform = previewAnimation->channels[(int)jointIndex].Calculate(timeInTicks, joint.defaultTransform);
                        local = localTransform.GetMatrix();
                    }
                }

                if (joint.parentJointId < 0 || joint.parentJointId >= static_cast<int32_t>(jointCount))
                {
                    previewState.previewGlobalTransforms[jointIndex] = local;
                }
                else
                {
                    previewState.previewGlobalTransforms[jointIndex] = previewState.previewGlobalTransforms[static_cast<size_t>(joint.parentJointId)] * local;
                }

                if (hasPreviewAnimation)
                {
                    previewState.previewFinalTransforms[jointIndex] = previewState.previewGlobalTransforms[jointIndex] * joint.inverseBindPose;
                }
                else
                {
                    previewState.previewFinalTransforms[jointIndex] = glm::mat4(1.0f);
                }
            }

            if (assetData.sceneData.sceneRenderer)
            {
                assetData.sceneData.sceneRenderer->SetBoneTransforms(previewState.previewFinalTransforms);
            }
        }
    }

    // UI
    void AssetEditorPanel::OnRender(nvrhi::IFramebuffer *framebuffer)
    {
        if (!m_EditorLayer || !m_EditorLayer->GetActiveProject())
        {
            return;
        }

        for (auto &assetData : m_Assets)
        {
            if (!assetData.sceneData.viewportVisible || !assetData.isOpen
                || (assetData.metadata.type != AssetType::Material 
                    && assetData.metadata.type != AssetType::Mesh 
                    && assetData.metadata.type != AssetType::SkeletalMesh 
                    && assetData.metadata.type != AssetType::StaticMesh
                    && assetData.metadata.type != AssetType::Skeleton 
                    && assetData.metadata.type != AssetType::Widget))
            {
                continue;
            }

            if (!assetData.asset || !assetData.asset->IsReady())
            {
                continue;
            }

            EditorSceneData &sceneData = assetData.sceneData;
            if (!sceneData.sceneRenderer || !sceneData.sceneRT || !sceneData.uiRT || !sceneData.compositeRT)
            {
                continue;
            }

            const uint32_t width = std::max(1u, sceneData.viewportWidth);
            const uint32_t height = std::max(1u, sceneData.viewportHeight);

            if (sceneData.sceneRT->GetWidth() != width || sceneData.sceneRT->GetHeight() != height)
            {
                sceneData.sceneRT->Resize(width, height);
                sceneData.uiRT->Resize(width, height);
                sceneData.compositeRT->Resize(width, height);
            }

			FrameContext *frameContext = Renderer::GetCurrentFrameContext();
            sceneData.sceneRenderer->BeginFrame();
            sceneData.sceneRenderer->Render(&sceneData.camera, frameContext, sceneData.sceneRT, sceneData.uiRT, sceneData.compositeRT);
        }
    }

    void AssetEditorPanel::OnGuiRender()
    {
        IGN_PROFILE_FUNCTION();
        UICreateAssetPopup();

        for (auto &assetData : m_Assets)
        {
            if (!assetData.isOpen)
                continue;

            switch (assetData.metadata.type)
            {
                case AssetType::SpriteSheet:
                {
                    UISpriteSheet2DEditor(assetData);
                    break;
                }
                case AssetType::Texture:
                {
                    UITextureEditor(assetData);
                    break;
                }
                case AssetType::Material2D:
                {
                    UIMaterial2DEditor(assetData);
                    break;
                }
                case AssetType::SkeletalMesh:
                {
                    UISkeletalMeshEditor(assetData);
                    break;
                }
				case AssetType::StaticMesh:
				{
					UIStaticMeshEditor(assetData);
					break;
				}
                case AssetType::SkeletalAnimation:
                {
                    UISkeletalAnimationEditor(assetData);
                    break;
                }
                case AssetType::Skeleton:
                {
                    UISkeletonEditor(assetData);
                    break;
                }
                case AssetType::Animation2D:
                {
                    UIAnimation2DEditor(assetData);
                    break;
                }
                case AssetType::AnimatorController2D:
                {
                    UIAnimatorController2DEditor(assetData);
                    break;
                }
                case AssetType::AnimatorController:
                {
                    UIAnimatorControllerEditor(assetData);
                    break;
                }
                case AssetType::BlendSpace:
                {
                    UIBlendSpaceEditor(assetData);
                    break;
                }
                case AssetType::Material:
                {
                    UIMaterialEditor(assetData);
                    break;
                }
                case AssetType::ScriptableObject:
                {
                    UIScriptableObjectEditor(assetData);
                    break;
                }
                case AssetType::Widget:
                {
                    UIWidgetEditor(assetData);
                    break;
                }
                default:
                {
                    break;
                }
            }
        }

        std::erase_if(m_Assets, [this](AssetEditorData &assetData)
        {
            if (assetData.isOpen)
            {
                return false;
            }

            if (m_EditorLayer && m_EditorLayer->GetActiveProject())
            {
                AssetManager::GetInstance()->ClearAssetPins(BuildAssetEditorPinOwnerTag(assetData.handle));
            }

            if (assetData.sceneData.sceneRenderer || assetData.sceneData.sceneRT || assetData.sceneData.uiRT || assetData.sceneData.compositeRT)
            {
                s_MaterialEditorState.erase(static_cast<uint64_t>(assetData.handle));

                auto sceneRenderer = std::move(assetData.sceneData.sceneRenderer);
                auto sceneRT = std::move(assetData.sceneData.sceneRT);
                auto uiRT = std::move(assetData.sceneData.uiRT);
                auto compositeRT = std::move(assetData.sceneData.compositeRT);

                // YES it is empty, to bring the Ref count to end of render thread
                Application::SubmitToRenderThread([sceneRenderer, sceneRT, uiRT, compositeRT]()
                {
                });
            }

            return true;
        });
    }

#pragma region ImGui_Helper
    // :IMGUI Helper
    bool AssetEditorPanel::DrawAssetEditorHeader(AssetEditorData &assetData)
    {
        const bool isDirty = assetData.asset && assetData.asset->IsDirty();
        if (ImGui::Button("Save"))
        {
            if (!SaveAsset(assetData))
            {
                LOG_ERROR("[Asset Editor] Failed to save asset: {}", assetData.metadata.filepath.generic_string());
            }
        }

        ImGui::Separator();
        return true;
    }

    bool AssetEditorPanel::BeginAssetEditorWindow(AssetEditorData &assetData, bool &isOpen, const ImVec2 &windowSize, const ImVec2 &minWindowSize, ImGuiWindowFlags flags)
    {
        if (assetData.requestFocus)
        {
            ImGui::SetNextWindowFocus();
        }

        // Append unsaved indicator
        if (assetData.asset && assetData.asset->IsDirty())
            flags |= ImGuiWindowFlags_UnsavedDocument;

        ImGui::SetNextWindowSize(windowSize, ImGuiCond_Appearing);
        ImGui::SetNextWindowSizeConstraints(minWindowSize, ImVec2(FLT_MAX, FLT_MAX));
        assetData.sceneData.viewportVisible = ImGui::Begin(assetData.windowTitle.c_str(), &isOpen, flags);
        if (!assetData.sceneData.viewportVisible)
        {
            if (!isOpen && assetData.asset && assetData.asset->IsDirty())
            {
                isOpen = true;
                assetData.showUnsavedClosePopup = true;
            }
        }
        return assetData.sceneData.viewportVisible;
    }

    void AssetEditorPanel::UIAssetEditorClosePopup(AssetEditorData &assetData, bool &isOpen)
    {
        const std::string popupId = std::format("Unsaved Changes###asset_unsaved_close_{}", static_cast<uint64_t>(assetData.handle));
        if (assetData.showUnsavedClosePopup)
        {
            ImGui::OpenPopup(popupId.c_str());
        }

        if (ImGui::BeginPopupModal(popupId.c_str(), nullptr, ImGuiWindowFlags_AlwaysAutoResize))
        {
            ImGui::Text("This asset has unsaved changes.");
            ImGui::Separator();

            if (ImGui::Button("Save and Close"))
            {
                if (SaveAsset(assetData))
                {
                    assetData.showUnsavedClosePopup = false;
                    isOpen = false;
                    ImGui::CloseCurrentPopup();
                }
            }
            ImGui::SameLine();
            if (ImGui::Button("Discard"))
            {
                assetData.showUnsavedClosePopup = false;
                isOpen = false;
                ImGui::CloseCurrentPopup();
            }
            ImGui::SameLine();
            if (ImGui::Button("Cancel"))
            {
                assetData.showUnsavedClosePopup = false;
                isOpen = true;
                ImGui::CloseCurrentPopup();
            }
            ImGui::EndPopup();
        }
    }

    void AssetEditorPanel::UICreateAssetPopup()
    {
        if (!m_CreateRequest.open || !m_EditorLayer || !m_EditorLayer->GetActiveProject())
        {
            return;
        }

        Project *project = m_EditorLayer->GetActiveProject().get();
		auto assetManager = AssetManager::GetInstance();

        if (m_CreateRequest.type == AssetType::Material2D && !m_CreateRequest.asset)
        {
            m_CreateRequest.asset = CreateRef<Material2D>();
        }

        if (m_CreateRequest.type == AssetType::SpriteSheet && !m_CreateRequest.asset)
        {
            m_CreateRequest.asset = CreateRef<SpriteSheet>();
        }

        if (m_CreateRequest.type == AssetType::BlendSpace && !m_CreateRequest.asset)
        {
            m_CreateRequest.asset = CreateRef<BlendSpace>();
        }

        if (m_CreateRequest.type == AssetType::LocomotionController && !m_CreateRequest.asset)
        {
            m_CreateRequest.asset = CreateRef<LocomotionController>();
        }

        if (m_CreateRequest.type == AssetType::AnimatorController && !m_CreateRequest.asset)
        {
            m_CreateRequest.asset = AnimatorController::Create();
        }

        if (m_CreateRequest.type == AssetType::Widget && !m_CreateRequest.asset)
        {
            m_CreateRequest.asset = CreateRef<WidgetCanvas>(nullptr);
        }

        // Asset creation pop-up
        ImGui::OpenPopup("Create Asset");
        if (ImGui::BeginPopupModal("Create Asset", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
        {
            ImGui::Text("Asset Type: %s", AssetTypeToString(m_CreateRequest.type).c_str());

            ignite::Path targetDirectory = m_CreateRequest.targetDirectory.empty() ? project->GetAssetDirectory() : m_CreateRequest.targetDirectory;
            if (!ignite::Path::exists(targetDirectory))
            {
                ignite::Path::create_directories(targetDirectory);
            }

            const std::string extension = GetAssetExtensionFromType(m_CreateRequest.type);
            const bool submitByEnter = ImGui::InputText("Name", m_CreateRequest.nameBuffer, sizeof(m_CreateRequest.nameBuffer), ImGuiInputTextFlags_EnterReturnsTrue);

            std::string assetName = m_CreateRequest.nameBuffer;
            if (!assetName.empty())
            {
                ignite::Path proposed(assetName);
                assetName = proposed.stem().string();
            }

            const bool hasValidName = !assetName.empty();
            const ignite::Path requestedPath = targetDirectory / (assetName + extension);
            const bool isNameAvailable = hasValidName && !ignite::Path::exists(requestedPath);

            if (!hasValidName)
            {
                ImGui::TextColored(ImVec4(1.0f, 0.65f, 0.2f, 1.0f), "Name cannot be empty.");
            }
            else if (!isNameAvailable)
            {
                ImGui::TextColored(ImVec4(1.0f, 0.3f, 0.3f, 1.0f), "Name is not available.");
            }

            auto tryCreateAsset = [&]()
            {
                std::string finalAssetName = m_CreateRequest.nameBuffer;
                if (finalAssetName.empty())
                {
                    return;
                }

                ignite::Path proposed(finalAssetName);
                finalAssetName = proposed.stem().string();
                if (finalAssetName.empty())
                {
                    return;
                }

                const ignite::Path fullAssetPath = targetDirectory / (finalAssetName + extension);
                if (ignite::Path::exists(fullAssetPath))
                {
                    return;
                }

                bool created = false;
                Ref<Asset> createdAsset = m_CreateRequest.asset;
                if (m_CreateRequest.type == AssetType::Material2D)
                {
                    Ref<Material2D> asset = createdAsset->As<Material2D>();
                    if (asset)
                    {
                        asset->name = finalAssetName;
                        created = asset->Serialize(fullAssetPath);
                        if (created)
                        {
                            asset->SetDirtyFlag(false);
                            asset->SetReadyFlag(true);
                            createdAsset = asset;
                        }
                    }
                }
                else if (m_CreateRequest.type == AssetType::Widget)
                {
                    Ref<WidgetCanvas> asset = createdAsset->As<WidgetCanvas>();
                    if (asset)
                    {
                        asset->SetName(finalAssetName);
                        created = asset->Serialize(fullAssetPath);
                        if (created)
                        {
                            asset->SetDirtyFlag(false);
                            asset->SetReadyFlag(true);
                            createdAsset = asset;
                        }
                    }
                }
                else if (m_CreateRequest.type == AssetType::AnimatorController)
                {
                    Ref<AnimatorController> asset = createdAsset->As<AnimatorController>();
                    if (asset)
                    {
                        created = asset->Serialize(fullAssetPath);
                        if (created)
                        {
                            asset->SetDirtyFlag(false);
                            asset->SetReadyFlag(true);
                            createdAsset = asset;
                        }
                    }
                }
                else if (m_CreateRequest.type == AssetType::BlendSpace)
                {
                    Ref<BlendSpace> asset = createdAsset->As<BlendSpace>();
                    if (asset)
                    {
                        created = asset->Serialize(fullAssetPath);
                        if (created)
                        {
                            asset->SetDirtyFlag(false);
                            asset->SetReadyFlag(true);
                            createdAsset = asset;
                        }
                    }
                }
                else if (m_CreateRequest.type == AssetType::LocomotionController)
                {
                    Ref<LocomotionController> asset = createdAsset->As<LocomotionController>();
                    if (asset)
                    {
                        asset->name = finalAssetName;
                        created = asset->Serialize(fullAssetPath);
                        if (created)
                        {
                            asset->SetDirtyFlag(false);
                            asset->SetReadyFlag(true);
                            createdAsset = asset;
                        }
                    }
                }
                else if (m_CreateRequest.type == AssetType::SpriteSheet)
                {
                    Ref<SpriteSheet> asset = createdAsset->As<SpriteSheet>();
                    if (asset)
                    {
                        created = asset->Serialize(fullAssetPath);
                        if (created)
                        {
                            asset->SetDirtyFlag(false);
                            asset->SetReadyFlag(true);
                            createdAsset = asset;
                        }
                    }
                }
                else if (m_CreateRequest.type == AssetType::Animation2D)
                {
                    Ref<Animation2D> asset = createdAsset->As<Animation2D>();
                    if (asset)
                    {
                        created = asset->Serialize(fullAssetPath);
                        if (created)
                        {
                            asset->SetDirtyFlag(false);
                            asset->SetReadyFlag(true);
                            createdAsset = asset;
                        }
                    }
                }
                else if (m_CreateRequest.type == AssetType::AnimatorController2D)
                {
                    Ref<AnimatorController2D> asset = createdAsset->As<AnimatorController2D>();
                    if (asset)
                    {
                        created = asset->Serialize(fullAssetPath);
                        if (created)
                        {
                            asset->SetDirtyFlag(false);
                            asset->SetReadyFlag(true);
                            createdAsset = asset;
                        }
                    }
                }

                if (created && createdAsset)
                {
                    Application::SubmitToMainThread([createdAsset, fullAssetPath, project, assetManager, this]()
                    {
                        AssetHandle handle = AssetHandle();
                        AssetMetaData metadata;
                        metadata.type = m_CreateRequest.type;
                        metadata.filepath = project->GetProjectFilepath(fullAssetPath);

                        createdAsset->handle = handle;
                        assetManager->AssignMetaData(handle, metadata);
                        assetManager->AssignAsset(handle, createdAsset);

                        // TODO: Fix save project assets
                        m_EditorLayer->SaveProject();

                        AssetEditorData data;
                        data.asset = createdAsset;
                        data.metadata = metadata;
                        data.handle = handle;
                        data.isOpen = true;
                        data.requestFocus = true;
                        data.windowTitle = std::format("{} - {}###asset_editor_{}", AssetTypeToString(metadata.type), fullAssetPath.filename().string(), static_cast<uint64_t>(handle));

                        assetManager->AddAssetPin(handle, BuildAssetEditorPinOwnerTag(handle));

                        InitializeSceneData(data);

                        m_Assets.push_back(std::move(data));
                        m_CreateRequest = {};

                        // Refresh content browser
                        m_EditorLayer->RefreshContentBrowsers();
                    });
                }
            };

            const bool canCreate = hasValidName && isNameAvailable;
            if (!canCreate)
            {
                ImGui::BeginDisabled();
            }

            const bool submitByButton = ImGui::Button("Create");

            if (!canCreate)
            {
                ImGui::EndDisabled();
            }

            if ((submitByEnter || submitByButton) && canCreate)
            {
                tryCreateAsset();
            }

            ImGui::SameLine();

            if (ImGui::Button("Cancel"))
            {
                m_CreateRequest = {};
                ImGui::CloseCurrentPopup();
            }

            ImGui::EndPopup();
        }

        if (!m_CreateRequest.open)
        {
            m_CreateRequest = {};
        }
    }
#pragma endregion !ImGui_Helper

#pragma region 2D_STUFF
    void AssetEditorPanel::UISpriteSheet2DEditor(AssetEditorData &assetData)
    {
        bool isOpen = assetData.isOpen;

        if (BeginAssetEditorWindow(assetData, isOpen, ImVec2(1500.0f, 1000.0f), ImVec2(900.0f, 700.0f), ImGuiWindowFlags_NoScrollWithMouse))
        {
            if (DrawAssetEditorHeader(assetData))
            {
                if (assetData.asset && assetData.asset->IsReady())
                {
                    if (Ref<SpriteSheet> spriteSheet = assetData.asset->As<SpriteSheet>())
                    {
                        if (!spriteSheet)
                        {
                            return;
                        }

                        Project *project = m_EditorLayer->GetActiveProject().get();
                        auto assetManager = AssetManager::GetInstance();

                        const auto stateKey = static_cast<uint64_t>(spriteSheet->handle);
                        SpriteSheetEditorState &state = s_SpriteSheetEditorState[stateKey];
                        auto &sprites = spriteSheet->GetSprites();

                        if (state.spriteNames.size() < sprites.size())
                        {
                            for (size_t i = state.spriteNames.size(); i < sprites.size(); ++i)
                            {
                                state.spriteNames.push_back(std::format("Sprite {}", i));
                            }
                        }
                        else if (state.spriteNames.size() > sprites.size())
                        {
                            state.spriteNames.resize(sprites.size());
                        }

                        if (state.selectedSpriteIndex >= static_cast<int>(sprites.size()))
                        {
                            state.selectedSpriteIndex = -1;
                        }

                        Ref<Texture> texture = nullptr;
                        if (spriteSheet->GetTextureHandle() != AssetHandle(0))
                        {
                            texture = assetManager->GetAsset<Texture>(spriteSheet->GetTextureHandle());
                        }

                        const ImVec2 contentSize = ImGui::GetContentRegionAvail();
                        const float horizontalSplitterWidth = 6.0f;
                        const float minPreviewColumnWidth = 260.0f;
                        const float minToolsColumnWidth = 320.0f;
                        const float maxPreviewColumnWidth = std::max(minPreviewColumnWidth, contentSize.x - minToolsColumnWidth - horizontalSplitterWidth);

                        if (state.previewColumnWidth <= 0.0f)
                        {
                            const float defaultToolsWidth = std::clamp(contentSize.x * 0.36f, 320.0f, 460.0f);
                            state.previewColumnWidth = std::max(minPreviewColumnWidth, contentSize.x - defaultToolsWidth - horizontalSplitterWidth);
                        }
                        state.previewColumnWidth = std::clamp(state.previewColumnWidth, minPreviewColumnWidth, maxPreviewColumnWidth);

                        // Sprite sheet - Preview
                        ImGui::BeginChild("##sprite_sheet_preview_column", ImVec2(state.previewColumnWidth, 0.0f), ImGuiChildFlags_None);
                        const float splitterThickness = 6.0f;
                        const float minViewportHeight = 120.0f;
                        const float minExtractedHeight = 90.0f;
                        const float totalLeftHeight = ImGui::GetContentRegionAvail().y;
                        state.extractedPanelHeight = std::clamp(state.extractedPanelHeight, minExtractedHeight, std::max(minExtractedHeight, totalLeftHeight - minViewportHeight - splitterThickness));
                        const float viewportHeight = std::max(minViewportHeight, totalLeftHeight - state.extractedPanelHeight - splitterThickness);

                        ImGui::BeginChild("##sprite_sheet_viewport", ImVec2(0.0f, viewportHeight), ImGuiChildFlags_Borders);
                        {
                            const ImVec2 viewportPos = ImGui::GetCursorScreenPos();
                            const ImVec2 viewportSize = ImGui::GetContentRegionAvail();

                            bool hovered = false;
                            if (viewportSize.x > 0.0f && viewportSize.y > 0.0f)
                            {
                                ImGui::InvisibleButton("##sprite_sheet_view_btn", viewportSize);
                                hovered = ImGui::IsItemHovered();
                                if (hovered)
                                {
                                    ImGui::SetItemKeyOwner(ImGuiKey_MouseWheelY);
                                    ImGui::SetItemKeyOwner(ImGuiKey_MouseWheelX);
                                }
                            }

                            if (hovered && ImGui::IsMouseDragging(ImGuiMouseButton_Middle))
                            {
                                state.pan += glm::vec2(ImGui::GetIO().MouseDelta.x, ImGui::GetIO().MouseDelta.y);
                            }

                            if (texture && texture->GetHandle())
                            {
                                const float texW = static_cast<float>(texture->GetWidth());
                                const float texH = static_cast<float>(texture->GetHeight());
                                const float fitScale = glm::min(viewportSize.x / std::max(texW, 1.0f), viewportSize.y / std::max(texH, 1.0f));
                                ImVec2 imageSize = { texW * fitScale * state.zoom, texH * fitScale * state.zoom };
                                ImVec2 imagePos =
                                {
                                    viewportPos.x + (viewportSize.x - imageSize.x) * 0.5f + state.pan.x,
                                    viewportPos.y + (viewportSize.y - imageSize.y) * 0.5f + state.pan.y
                                };

                                if (hovered)
                                {
                                    const float wheel = ImGui::GetIO().MouseWheel;
                                    if (wheel != 0.0f)
                                    {
                                        const float newZoom = std::clamp(state.zoom + wheel * 0.1f, 0.2f, 8.0f);
                                        if (newZoom != state.zoom)
                                        {
                                            const ImVec2 mousePos = ImGui::GetMousePos();
                                            const ImVec2 uvAtMouse =
                                            {
                                                (mousePos.x - imagePos.x) / std::max(imageSize.x, 1.0f),
                                                (mousePos.y - imagePos.y) / std::max(imageSize.y, 1.0f)
                                            };

                                            state.zoom = newZoom;

                                            imageSize =
                                            {
                                                texW * fitScale * state.zoom,
                                                texH * fitScale * state.zoom
                                            };

                                            const ImVec2 centeredPos =
                                            {
                                                viewportPos.x + (viewportSize.x - imageSize.x) * 0.5f,
                                                viewportPos.y + (viewportSize.y - imageSize.y) * 0.5f
                                            };

                                            state.pan.x = mousePos.x - uvAtMouse.x * imageSize.x - centeredPos.x;
                                            state.pan.y = mousePos.y - uvAtMouse.y * imageSize.y - centeredPos.y;

                                            imagePos =
                                            {
                                                viewportPos.x + (viewportSize.x - imageSize.x) * 0.5f + state.pan.x,
                                                viewportPos.y + (viewportSize.y - imageSize.y) * 0.5f + state.pan.y
                                            };
                                        }
                                    }
                                }

                                ImDrawList *drawList = ImGui::GetWindowDrawList();
                                drawList->AddImage(reinterpret_cast<ImTextureID>(texture->GetHandle().Get()), imagePos, ImVec2(imagePos.x + imageSize.x, imagePos.y + imageSize.y));

                                auto toUV = [&](const ImVec2 &mouse)
                                {
                                    glm::vec2 uv;
                                    uv.x = (mouse.x - imagePos.x) / std::max(imageSize.x, 1.0f);
                                    uv.y = (mouse.y - imagePos.y) / std::max(imageSize.y, 1.0f);
                                    uv = glm::clamp(uv, glm::vec2(0.0f), glm::vec2(1.0f));
                                    return uv;
                                };

                                auto applySnap = [&](glm::vec2 uv)
                                {
                                    if (!state.snappingEnabled)
                                    {
                                        return uv;
                                    }

                                    const float gridStepU = 1.0f / static_cast<float>(std::max(state.gridColumns, 1));
                                    const float gridStepV = 1.0f / static_cast<float>(std::max(state.gridRows, 1));
                                    const float stepU = state.snapToGrid ? gridStepU : std::max(state.snapStepU, 0.0001f);
                                    const float stepV = state.snapToGrid ? gridStepV : std::max(state.snapStepV, 0.0001f);

                                    uv.x = std::round(uv.x / stepU) * stepU;
                                    uv.y = std::round(uv.y / stepV) * stepV;
                                    return glm::clamp(uv, glm::vec2(0.0f), glm::vec2(1.0f));
                                };

                                const bool mouseInsideImage = hovered && ImGui::GetMousePos().x >= imagePos.x && ImGui::GetMousePos().x <= imagePos.x + imageSize.x &&
                                    ImGui::GetMousePos().y >= imagePos.y && ImGui::GetMousePos().y <= imagePos.y + imageSize.y;

                                const glm::vec2 currentUVMin = glm::min(state.selectionStartUV, state.selectionEndUV);
                                const glm::vec2 currentUVMax = glm::max(state.selectionStartUV, state.selectionEndUV);
                                const ImVec2 currentSelMin =
                                {
                                    imagePos.x + currentUVMin.x * imageSize.x,
                                    imagePos.y + currentUVMin.y * imageSize.y
                                };
                                const ImVec2 currentSelMax =
                                {
                                    imagePos.x + currentUVMax.x * imageSize.x,
                                    imagePos.y + currentUVMax.y * imageSize.y
                                };

                                const bool hasSelection = (currentUVMax.x - currentUVMin.x) > 0.0001f && (currentUVMax.y - currentUVMin.y) > 0.0001f;

                                ImVec2 handles[8] {};
                                if (hasSelection)
                                {
                                    handles[0] = currentSelMin;
                                    handles[1] = ImVec2(currentSelMax.x, currentSelMin.y);
                                    handles[2] = currentSelMax;
                                    handles[3] = ImVec2(currentSelMin.x, currentSelMax.y);
                                    handles[4] = ImVec2((currentSelMin.x + currentSelMax.x) * 0.5f, currentSelMin.y);
                                    handles[5] = ImVec2(currentSelMax.x, (currentSelMin.y + currentSelMax.y) * 0.5f);
                                    handles[6] = ImVec2((currentSelMin.x + currentSelMax.x) * 0.5f, currentSelMax.y);
                                    handles[7] = ImVec2(currentSelMin.x, (currentSelMin.y + currentSelMax.y) * 0.5f);
                                }

                                int hoveredHandle = -1;
                                const float handleHalfSize = 6.0f;
                                if (mouseInsideImage && hasSelection)
                                {
                                    const ImVec2 mousePos = ImGui::GetMousePos();
                                    for (int i = 0; i < 8; ++i)
                                    {
                                        const ImRect handleRect(ImVec2(handles[i].x - handleHalfSize, handles[i].y - handleHalfSize), ImVec2(handles[i].x + handleHalfSize, handles[i].y + handleHalfSize));
                                        if (handleRect.Contains(mousePos))
                                        {
                                            hoveredHandle = i;
                                            break;
                                        }
                                    }
                                }

                                if (hoveredHandle == 0 || hoveredHandle == 2)
                                    ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeNWSE);
                                else if (hoveredHandle == 1 || hoveredHandle == 3)
                                    ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeNESW);
                                else if (hoveredHandle == 4 || hoveredHandle == 6)
                                    ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeNS);
                                else if (hoveredHandle == 5 || hoveredHandle == 7)
                                    ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeEW);
                                else if (mouseInsideImage && hasSelection && hoveredHandle == -1 && ImRect(currentSelMin, currentSelMax).Contains(ImGui::GetMousePos()))
                                    ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeAll);

                                int clickedSpriteIndex = -1;
                                if (mouseInsideImage && ImGui::IsMouseClicked(ImGuiMouseButton_Left))
                                {
                                    const ImVec2 mousePos = ImGui::GetMousePos();
                                    for (int i = static_cast<int>(sprites.size()) - 1; i >= 0; --i)
                                    {
                                        const auto &sprite = sprites[static_cast<size_t>(i)];
                                        const ImRect spriteRect(
                                            ImVec2(imagePos.x + sprite.uv0.x * imageSize.x, imagePos.y + sprite.uv0.y * imageSize.y),
                                            ImVec2(imagePos.x + sprite.uv1.x * imageSize.x, imagePos.y + sprite.uv1.y * imageSize.y)
                                        );

                                        if (spriteRect.Contains(mousePos))
                                        {
                                            clickedSpriteIndex = i;
                                            break;
                                        }
                                    }
                                }

                                if (mouseInsideImage && ImGui::IsMouseClicked(ImGuiMouseButton_Left))
                                {
                                    const glm::vec2 mouseUV = applySnap(toUV(ImGui::GetMousePos()));
                                    if (hoveredHandle != -1)
                                    {
                                        state.activeHandle = hoveredHandle;
                                        state.dragStartMinUV = currentUVMin;
                                        state.dragStartMaxUV = currentUVMax;
                                        state.selecting = false;
                                    }
                                    else if (hasSelection && ImRect(currentSelMin, currentSelMax).Contains(ImGui::GetMousePos()))
                                    {
                                        state.activeHandle = 8;
                                        state.dragStartMinUV = currentUVMin;
                                        state.dragStartMaxUV = currentUVMax;
                                        state.dragOffsetUV = mouseUV - currentUVMin;
                                        state.selecting = false;
                                    }
                                    else if (clickedSpriteIndex != -1)
                                    {
                                        state.selectedSpriteIndex = clickedSpriteIndex;
                                        const auto &selectedSprite = sprites[static_cast<size_t>(clickedSpriteIndex)];
                                        state.selectionStartUV = selectedSprite.uv0;
                                        state.selectionEndUV = selectedSprite.uv1;
                                        state.activeHandle = -1;
                                        state.selecting = false;
                                    }
                                    else
                                    {
                                        state.activeHandle = -1;
                                        state.selectedSpriteIndex = -1;
                                        state.selecting = true;
                                        state.selectionStartUV = state.selectionEndUV = mouseUV;
                                    }
                                }

                                if (state.activeHandle != -1)
                                {
                                    if (ImGui::IsMouseDown(ImGuiMouseButton_Left))
                                    {
                                        const glm::vec2 mouseUV = applySnap(toUV(ImGui::GetMousePos()));
                                        glm::vec2 minUV = state.dragStartMinUV;
                                        glm::vec2 maxUV = state.dragStartMaxUV;

                                        if (state.activeHandle == 8)
                                        {
                                            const glm::vec2 sizeUV = state.dragStartMaxUV - state.dragStartMinUV;
                                            minUV = mouseUV - state.dragOffsetUV;
                                            minUV.x = std::clamp(minUV.x, 0.0f, 1.0f - sizeUV.x);
                                            minUV.y = std::clamp(minUV.y, 0.0f, 1.0f - sizeUV.y);
                                            maxUV = minUV + sizeUV;
                                        }
                                        else
                                        {
                                            constexpr float epsilon = 0.0001f;
                                            switch (state.activeHandle)
                                            {
                                                case 0: // top-left
                                                minUV.x = std::clamp(mouseUV.x, 0.0f, state.dragStartMaxUV.x - epsilon);
                                                minUV.y = std::clamp(mouseUV.y, 0.0f, state.dragStartMaxUV.y - epsilon);
                                                break;
                                                case 1: // top-right
                                                maxUV.x = std::clamp(mouseUV.x, state.dragStartMinUV.x + epsilon, 1.0f);
                                                minUV.y = std::clamp(mouseUV.y, 0.0f, state.dragStartMaxUV.y - epsilon);
                                                break;
                                                case 2: // bottom-right
                                                maxUV.x = std::clamp(mouseUV.x, state.dragStartMinUV.x + epsilon, 1.0f);
                                                maxUV.y = std::clamp(mouseUV.y, state.dragStartMinUV.y + epsilon, 1.0f);
                                                break;
                                                case 3: // bottom-left
                                                minUV.x = std::clamp(mouseUV.x, 0.0f, state.dragStartMaxUV.x - epsilon);
                                                maxUV.y = std::clamp(mouseUV.y, state.dragStartMinUV.y + epsilon, 1.0f);
                                                break;
                                                case 4: // top
                                                minUV.y = std::clamp(mouseUV.y, 0.0f, state.dragStartMaxUV.y - epsilon);
                                                break;
                                                case 5: // right
                                                maxUV.x = std::clamp(mouseUV.x, state.dragStartMinUV.x + epsilon, 1.0f);
                                                break;
                                                case 6: // bottom
                                                maxUV.y = std::clamp(mouseUV.y, state.dragStartMinUV.y + epsilon, 1.0f);
                                                break;
                                                case 7: // left
                                                minUV.x = std::clamp(mouseUV.x, 0.0f, state.dragStartMaxUV.x - epsilon);
                                                break;
                                                default:
                                                break;
                                            }
                                        }

                                        state.selectionStartUV = minUV;
                                        state.selectionEndUV = maxUV;
                                    }
                                    else
                                    {
                                        state.activeHandle = -1;
                                    }
                                }

                                bool selectionFinishedThisFrame = false;
                                if (state.selecting)
                                {
                                    state.selectionEndUV = applySnap(toUV(ImGui::GetMousePos()));
                                    if (!ImGui::IsMouseDown(ImGuiMouseButton_Left))
                                    {
                                        state.selecting = false;
                                        selectionFinishedThisFrame = true;
                                    }
                                }

                                const glm::vec2 uvMin = glm::min(state.selectionStartUV, state.selectionEndUV);
                                const glm::vec2 uvMax = glm::max(state.selectionStartUV, state.selectionEndUV);

                                if (selectionFinishedThisFrame && state.selectedSpriteIndex == -1)
                                {
                                    if ((uvMax.x - uvMin.x) > 0.0001f && (uvMax.y - uvMin.y) > 0.0001f)
                                    {
                                        sprites.push_back({ uvMin, uvMax });
                                        state.spriteNames.push_back(std::format("Sprite {}", sprites.size() - 1));
                                        state.selectedSpriteIndex = static_cast<int>(sprites.size()) - 1;
                                        spriteSheet->SetDirtyFlag(true);
                                    }
                                }

                                if (state.selectedSpriteIndex >= 0 && state.selectedSpriteIndex < static_cast<int>(sprites.size()) && state.activeHandle != -1)
                                {
                                    auto &selectedSprite = sprites[static_cast<size_t>(state.selectedSpriteIndex)];
                                    selectedSprite.uv0 = uvMin;
                                    selectedSprite.uv1 = uvMax;
                                    spriteSheet->SetDirtyFlag(true);
                                }

                                const ImVec2 selMin = { imagePos.x + uvMin.x * imageSize.x, imagePos.y + uvMin.y * imageSize.y };
                                const ImVec2 selMax = { imagePos.x + uvMax.x * imageSize.x, imagePos.y + uvMax.y * imageSize.y };
                                drawList->AddRectFilled(selMin, selMax, IM_COL32(255, 220, 50, 30));
                                drawList->AddRect(selMin, selMax, IM_COL32(255, 220, 50, 255), 0.0f, 0, 2.0f);

                                if ((uvMax.x - uvMin.x) > 0.0001f && (uvMax.y - uvMin.y) > 0.0001f)
                                {
                                    const ImVec2 midTop = ImVec2((selMin.x + selMax.x) * 0.5f, selMin.y);
                                    const ImVec2 midRight = ImVec2(selMax.x, (selMin.y + selMax.y) * 0.5f);
                                    const ImVec2 midBottom = ImVec2((selMin.x + selMax.x) * 0.5f, selMax.y);
                                    const ImVec2 midLeft = ImVec2(selMin.x, (selMin.y + selMax.y) * 0.5f);

                                    const ImVec2 handlesDraw[8] = { selMin, ImVec2(selMax.x, selMin.y), selMax, ImVec2(selMin.x, selMax.y), midTop, midRight, midBottom, midLeft };

                                    for (const ImVec2 &handlePos : handlesDraw)
                                    {
                                        const ImVec2 hMin = ImVec2(handlePos.x - 4.0f, handlePos.y - 4.0f);
                                        const ImVec2 hMax = ImVec2(handlePos.x + 4.0f, handlePos.y + 4.0f);
                                        drawList->AddRectFilled(hMin, hMax, IM_COL32(255, 255, 255, 230));
                                        drawList->AddRect(hMin, hMax, IM_COL32(20, 20, 20, 255), 0.0f, 0, 1.0f);
                                    }
                                }

                                for (size_t i = 0; i < sprites.size(); ++i)
                                {
                                    const auto &sprite = sprites[i];
                                    const ImVec2 blockMin = { imagePos.x + sprite.uv0.x * imageSize.x,
                                                             imagePos.y + sprite.uv0.y * imageSize.y };
                                    const ImVec2 blockMax = { imagePos.x + sprite.uv1.x * imageSize.x,
                                                             imagePos.y + sprite.uv1.y * imageSize.y };
                                    const bool isSelectedSprite = state.selectedSpriteIndex == static_cast<int>(i);
                                    drawList->AddRect(blockMin, blockMax, isSelectedSprite
                                        ? IM_COL32(255, 128, 0, 255)
                                        : IM_COL32(0, 220, 255, 200), 0.0f, 0, isSelectedSprite
                                        ? 2.0f : 1.5f);
                                }
                            }
                            else
                            {
                                ImGui::SetCursorScreenPos(ImVec2(viewportPos.x + 12.0f, viewportPos.y + 12.0f));
                                ImGui::Text("Drop a texture to preview SpriteSheet");
                            }
                        }
                        ImGui::EndChild();

                        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.20f, 0.20f, 0.20f, 1.0f));
                        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.28f, 0.28f, 0.28f, 1.0f));
                        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.32f, 0.32f, 0.32f, 1.0f));
                        ImGui::Button("##sprite_sheet_vertical_splitter", ImVec2(-1.0f, splitterThickness));

                        if (ImGui::IsItemHovered() || ImGui::IsItemActive())
                        {
                            ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeNS);
                        }

                        if (ImGui::IsItemActive())
                        {
                            state.extractedPanelHeight -= ImGui::GetIO().MouseDelta.y;
                            state.extractedPanelHeight = std::clamp(state.extractedPanelHeight, minExtractedHeight,
                                std::max(minExtractedHeight, totalLeftHeight - minViewportHeight - splitterThickness));
                        }
                        ImGui::PopStyleColor(3);

                        ImGui::Text("Extracted Sprites");
                        ImGui::BeginChild("##sprite_sheet_extracted_preview", ImVec2(0.0f, state.extractedPanelHeight), ImGuiChildFlags_Borders);
                        if (texture && texture->GetHandle())
                        {
                            const float previewSize = 56.0f;
                            const float spacing = ImGui::GetStyle().ItemSpacing.x;
                            const float contentWidth = ImGui::GetContentRegionAvail().x;
                            const int spritesPerRow = std::max(1, static_cast<int>((contentWidth + spacing) / (previewSize + spacing + horizontalSplitterWidth)));

                            for (size_t i = 0; i < sprites.size(); ++i)
                            {
                                const auto &sprite = sprites[i];
                                ImGui::PushID(static_cast<int>(i));

                                ImTextureID texId = reinterpret_cast<ImTextureID>(texture->GetHandle().Get());
                                ImGui::ImageButton("##sprite_preview", texId, ImVec2(previewSize, previewSize), ImVec2(sprite.uv0.x, sprite.uv0.y), ImVec2(sprite.uv1.x, sprite.uv1.y));

                                if (ImGui::BeginDragDropSource())
                                {
                                    SpriteSheetSpritePayload payload;
                                    payload.spriteSheetHandle = spriteSheet->handle;
                                    payload.textureHandle = spriteSheet->GetTextureHandle();
                                    payload.spriteIndex = static_cast<uint32_t>(i);
                                    payload.uv0 = sprite.uv0;
                                    payload.uv1 = sprite.uv1;

                                    ImGui::SetDragDropPayload("sprite_sheet_item", &payload, sizeof(payload));
                                    ImGui::Text("Sprite %zu", i);
                                    ImGui::EndDragDropSource();
                                }

                                if (state.selectedSpriteIndex == static_cast<int>(i))
                                {
                                    ImDrawList *selectionDrawList = ImGui::GetWindowDrawList();
                                    selectionDrawList->AddRect(ImGui::GetItemRectMin(), ImGui::GetItemRectMax(), IM_COL32(255, 180, 0, 255), 2.0f, 0, 2.0f);
                                }

                                if ((i + 1) % spritesPerRow != 0)
                                {
                                    ImGui::SameLine();
                                }

                                ImGui::PopID();
                            }
                        }
                        else
                        {
                            ImGui::TextDisabled("Assign a texture to preview extracted sprites.");
                        }
                        ImGui::EndChild();
                        ImGui::EndChild();

                        ImGui::SameLine(0.0f, 0.0f);
                        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.20f, 0.20f, 0.20f, 1.0f));
                        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.28f, 0.28f, 0.28f, 1.0f));
                        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.32f, 0.32f, 0.32f, 1.0f));
                        ImGui::BeginChild("##sprite_sheet_horizontal_splitter", ImVec2(horizontalSplitterWidth, 0.0f), ImGuiChildFlags_None);
                        ImGui::Button("##sprite_sheet_horizontal_splitter_btn", ImVec2(-1.0f, -1.0f));

                        if (ImGui::IsItemHovered() || ImGui::IsItemActive())
                        {
                            ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeEW);
                        }

                        if (ImGui::IsItemActive())
                        {
                            state.previewColumnWidth += ImGui::GetIO().MouseDelta.x;
                            state.previewColumnWidth = std::clamp(state.previewColumnWidth, minPreviewColumnWidth, maxPreviewColumnWidth);
                        }

                        ImGui::EndChild();
                        ImGui::PopStyleColor(3);

                        ImGui::SameLine(0.0f, 0.0f);
                        ImGui::BeginChild("##sprite_sheet_tools_column", ImVec2(0.0f, 0.0f), ImGuiChildFlags_None);

                        ImGui::Spacing();
                        std::string textureLabel = spriteSheet->GetTextureHandle() == AssetHandle(0) ? "Drop Texture Here" : "Texture Loaded";

                        ImGui::Button(textureLabel.c_str(), ImVec2(220.0f, 0.0f));
                        if (ImGui::BeginDragDropTarget())
                        {
                            if (const ImGuiPayload *payload = ImGui::AcceptDragDropPayload("content_browser_item"))
                            {
                                if (payload->Data && payload->DataSize == sizeof(AssetHandle))
                                {
                                    const AssetHandle droppedHandle = *static_cast<const AssetHandle *>(payload->Data);
                                    const AssetMetaData &droppedMetadata = assetManager->GetMetaData(droppedHandle);
                                    if (droppedMetadata.type == AssetType::Texture)
                                    {
                                        spriteSheet->SetTextureHandle(droppedHandle);
                                        spriteSheet->SetDirtyFlag(true);
                                    }
                                }
                            }
                            ImGui::EndDragDropTarget();
                        }

                        ImGui::SameLine();
                        if (ImGui::Button("Clear Texture"))
                        {
                            spriteSheet->SetTextureHandle(AssetHandle(0));
                            spriteSheet->SetDirtyFlag(true);
                        }

                        ImGui::Text("Texture Handle: %llu", static_cast<unsigned long long>(static_cast<uint64_t>(spriteSheet->GetTextureHandle())));

                        glm::vec2 atlasSize = spriteSheet->GetAtlasSize();
                        if (ImGui::DragFloat2("Atlas Cell Size", &atlasSize.x, 1.0f, 1.0f, 8192.0f, "%.0f"))
                        {
                            spriteSheet->SetAtlasSize(atlasSize);
                            spriteSheet->SetDirtyFlag(true);
                        }

                        const glm::vec2 uvMin = glm::min(state.selectionStartUV, state.selectionEndUV);
                        const glm::vec2 uvMax = glm::max(state.selectionStartUV, state.selectionEndUV);

                        ImGui::Text("Selected UV0: %.3f, %.3f", uvMin.x, uvMin.y);
                        ImGui::Text("Selected UV1: %.3f, %.3f", uvMax.x, uvMax.y);

                        const bool hasValidSelectionArea = uvMax.x > uvMin.x && uvMax.y > uvMin.y;
                        const bool editingSelectedSprite = state.selectedSpriteIndex >= 0 && state.selectedSpriteIndex < static_cast<int>(sprites.size());
                        if (editingSelectedSprite && ImGui::Button("Apply To Selected"))
                        {
                            if (hasValidSelectionArea)
                            {
                                auto &selectedSprite = sprites[static_cast<size_t>(state.selectedSpriteIndex)];
                                selectedSprite.uv0 = uvMin;
                                selectedSprite.uv1 = uvMax;
                                spriteSheet->SetDirtyFlag(true);
                            }
                        }
                        else if (!editingSelectedSprite)
                        {
                            ImGui::TextDisabled("Selection auto-adds on mouse release");
                        }

                        ImGui::SameLine();
                        if (ImGui::Button("Remove Selected") && state.selectedSpriteIndex >= 0 && state.selectedSpriteIndex < static_cast<int>(sprites.size()))
                        {
                            sprites.erase(sprites.begin() + state.selectedSpriteIndex);
                            state.spriteNames.erase(state.spriteNames.begin() + state.selectedSpriteIndex);
                            state.selectedSpriteIndex = -1;
                            state.renamingSpriteIndex = -1;
                            spriteSheet->SetDirtyFlag(true);
                        }

                        ImGui::BeginChild("##sprite_sheet_sprite_list", ImVec2(0.0f, 130.0f), ImGuiChildFlags_Borders);
                        for (size_t i = 0; i < sprites.size(); ++i)
                        {
                            const auto &sprite = sprites[i];
                            const bool selected = state.selectedSpriteIndex == static_cast<int>(i);
                            const std::string rowLabel = std::format("{}##sprite_row_{}", state.spriteNames[i], i);
                            if (ImGui::Selectable(rowLabel.c_str(), selected))
                            {
                                state.selectedSpriteIndex = static_cast<int>(i);
                                state.selectionStartUV = sprite.uv0;
                                state.selectionEndUV = sprite.uv1;
                                state.renamingSpriteIndex = state.selectedSpriteIndex;
                                std::strncpy(state.renameBuffer, state.spriteNames[i].c_str(), sizeof(state.renameBuffer) - 1);
                                state.renameBuffer[sizeof(state.renameBuffer) - 1] = '\0';
                            }

                            if (ImGui::BeginDragDropSource())
                            {
                                SpriteSheetSpritePayload payload;
                                payload.spriteSheetHandle = spriteSheet->handle;
                                payload.textureHandle = spriteSheet->GetTextureHandle();
                                payload.spriteIndex = static_cast<uint32_t>(i);
                                payload.uv0 = sprite.uv0;
                                payload.uv1 = sprite.uv1;

                                ImGui::SetDragDropPayload("sprite_sheet_item", &payload, sizeof(payload));
                                ImGui::Text("Sprite %zu", i);
                                ImGui::EndDragDropSource();
                            }

                            ImGui::SameLine();
                            ImGui::TextDisabled("(%.3f, %.3f) -> (%.3f, %.3f)", sprite.uv0.x, sprite.uv0.y, sprite.uv1.x, sprite.uv1.y);
                        }
                        ImGui::EndChild();

                        const bool canMoveUp = state.selectedSpriteIndex > 0 && state.selectedSpriteIndex < static_cast<int>(sprites.size());
                        if (!canMoveUp)
                        {
                            ImGui::BeginDisabled();
                        }

                        if (ImGui::Button("Move Up"))
                        {
                            const int i = state.selectedSpriteIndex;
                            std::swap(sprites[static_cast<size_t>(i)], sprites[static_cast<size_t>(i - 1)]);
                            std::swap(state.spriteNames[static_cast<size_t>(i)], state.spriteNames[static_cast<size_t>(i - 1)]);
                            state.selectedSpriteIndex = i - 1;
                            spriteSheet->SetDirtyFlag(true);
                        }

                        if (!canMoveUp)
                        {
                            ImGui::EndDisabled();
                        }

                        ImGui::SameLine();
                        const bool canMoveDown = state.selectedSpriteIndex >= 0 && state.selectedSpriteIndex < static_cast<int>(sprites.size()) - 1;
                        if (!canMoveDown)
                        {
                            ImGui::BeginDisabled();
                        }

                        if (ImGui::Button("Move Down"))
                        {
                            const int i = state.selectedSpriteIndex;
                            std::swap(sprites[static_cast<size_t>(i)], sprites[static_cast<size_t>(i + 1)]);
                            std::swap(state.spriteNames[static_cast<size_t>(i)], state.spriteNames[static_cast<size_t>(i + 1)]);
                            state.selectedSpriteIndex = i + 1;
                            spriteSheet->SetDirtyFlag(true);
                        }
                        if (!canMoveDown)
                        {
                            ImGui::EndDisabled();
                        }

                        if (state.selectedSpriteIndex >= 0 && state.selectedSpriteIndex < static_cast<int>(sprites.size()))
                        {
                            if (state.renamingSpriteIndex != state.selectedSpriteIndex)
                            {
                                state.renamingSpriteIndex = state.selectedSpriteIndex;
                                std::strncpy(state.renameBuffer, state.spriteNames[static_cast<size_t>(state.selectedSpriteIndex)].c_str(),
                                    sizeof(state.renameBuffer) - 1);

                                state.renameBuffer[sizeof(state.renameBuffer) - 1] = '\0';
                            }

                            if (ImGui::InputText("Sprite Name", state.renameBuffer, sizeof(state.renameBuffer)))
                            {
                                state.spriteNames[static_cast<size_t>(state.selectedSpriteIndex)] = state.renameBuffer;
                            }

                            auto &sprite = sprites[static_cast<size_t>(state.selectedSpriteIndex)];
                            glm::vec2 selectedUv0 = sprite.uv0;
                            glm::vec2 selectedUv1 = sprite.uv1;
                            if (ImGui::DragFloat2("Sprite UV0", &selectedUv0.x, 0.001f, 0.0f, 1.0f, "%.3f")
                                || ImGui::DragFloat2("Sprite UV1", &selectedUv1.x, 0.001f, 0.0f, 1.0f, "%.3f"))
                            {
                                sprite.uv0 = glm::clamp(glm::min(selectedUv0, selectedUv1), glm::vec2(0.0f), glm::vec2(1.0f));
                                sprite.uv1 = glm::clamp(glm::max(selectedUv0, selectedUv1), glm::vec2(0.0f), glm::vec2(1.0f));
                                state.selectionStartUV = sprite.uv0;
                                state.selectionEndUV = sprite.uv1;
                                spriteSheet->SetDirtyFlag(true);
                            }
                        }

                        ImGui::Separator();
                        ImGui::DragInt("Grid Columns", &state.gridColumns, 1.0f, 1, 512);
                        ImGui::DragInt("Grid Rows", &state.gridRows, 1.0f, 1, 512);
                        ImGui::Checkbox("Enable Snapping", &state.snappingEnabled);

                        if (state.snappingEnabled)
                        {
                            ImGui::Checkbox("Snap To Grid", &state.snapToGrid);
                            if (!state.snapToGrid)
                            {
                                ImGui::DragFloat("Snap Step U", &state.snapStepU, 0.001f, 0.001f, 1.0f, "%.3f");
                                ImGui::DragFloat("Snap Step V", &state.snapStepV, 0.001f, 0.001f, 1.0f, "%.3f");
                            }
                        }

                        if (ImGui::Button("Auto Slice Grid"))
                        {
                            sprites.clear();
                            state.spriteNames.clear();
                            state.selectedSpriteIndex = -1;

                            const int cols = std::max(state.gridColumns, 1);
                            const int rows = std::max(state.gridRows, 1);
                            const float cellU = 1.0f / static_cast<float>(cols);
                            const float cellV = 1.0f / static_cast<float>(rows);

                            for (int y = 0; y < rows; ++y)
                            {
                                for (int x = 0; x < cols; ++x)
                                {
                                    const glm::vec2 blockUV0 = { x * cellU, y * cellV };
                                    const glm::vec2 blockUV1 = { (x + 1) * cellU, (y + 1) * cellV };
                                    sprites.push_back({ blockUV0, blockUV1 });
                                    state.spriteNames.push_back(std::format("Sprite {}", sprites.size() - 1));
                                }
                            }

                            if (texture && texture->GetWidth() > 0 && texture->GetHeight() > 0)
                            {
                                spriteSheet->SetAtlasSize({ static_cast<float>(texture->GetWidth()) / static_cast<float>(cols),
                                     static_cast<float>(texture->GetHeight()) / static_cast<float>(rows) });
                            }

                            spriteSheet->SetDirtyFlag(true);
                        }

                        ImGui::SameLine();
                        if (ImGui::Button("Clear Sprites"))
                        {
                            sprites.clear();
                            state.spriteNames.clear();
                            state.selectedSpriteIndex = -1;
                            spriteSheet->SetDirtyFlag(true);
                        }

                        ImGui::Text("Total Sprites: %zu", sprites.size());
                        ImGui::EndChild();
                    }
                    else
                    {
                        ImGui::Text("Invalid asset!");
                    }
                }
                else
                {
                    ImGui::Text("Loading asset...");
                }
            }
        }

        UIAssetEditorClosePopup(assetData, isOpen);
        ImGui::End();
        assetData.isOpen = isOpen;
        assetData.requestFocus = false;
    }

    void AssetEditorPanel::UIWidgetEditor(AssetEditorData &assetData)
    {
        // Delegate the inner widget editor rendering to WidgetEditor to keep the AssetEditorPanel slim.
        bool isOpen = assetData.isOpen;
        if (BeginAssetEditorWindow(assetData, isOpen, ImVec2(1100.0f, 900.0f), ImVec2(420.0f, 560.0f), ImGuiWindowFlags_NoScrollWithMouse))
        {
            if (DrawAssetEditorHeader(assetData))
            {
                WidgetEditor::UIWidgetEditor(assetData, m_EditorLayer);
            }
        }

        UIAssetEditorClosePopup(assetData, isOpen);
        ImGui::End();
        assetData.isOpen = isOpen;
        assetData.requestFocus = false;
    }

    void AssetEditorPanel::UIMaterial2DEditor(AssetEditorData &assetData)
    {
        bool isOpen = assetData.isOpen;
        if (BeginAssetEditorWindow(assetData, isOpen, ImVec2(1100.0f, 900.0f), ImVec2(420.0f, 560.0f), ImGuiWindowFlags_NoScrollWithMouse))
        {
            if (DrawAssetEditorHeader(assetData))
            {
                if (assetData.asset && assetData.asset->IsReady())
                {
                    if (Ref<Material2D> material2D = assetData.asset->As<Material2D>())
                    {
                        if (!material2D || !m_EditorLayer || !m_EditorLayer->GetActiveProject())
                        {
                            return;
                        }

                        const char *materialTypeLabel = material2D->data.type == MATERIAL_2D_TYPE_LIT ? "Lit" : "Unlit";
                        if (ImGui::BeginCombo("Material Type", materialTypeLabel))
                        {
                            if (ImGui::Selectable("Unlit", material2D->data.type == MATERIAL_2D_TYPE_UNLIT))
                            {
                                material2D->data.type = MATERIAL_2D_TYPE_UNLIT;
                                material2D->SetDirtyFlag(true);
                            }

                            if (ImGui::Selectable("Lit",
                                material2D->data.type == MATERIAL_2D_TYPE_LIT))
                            {
                                material2D->data.type = MATERIAL_2D_TYPE_LIT;
                                material2D->SetDirtyFlag(true);
                            }

                            ImGui::EndCombo();
                        }

                        if (ImGui::ColorEdit4("Base Color", &material2D->data.baseColor.x))
                        {
                            material2D->SetDirtyFlag(true);
                        }

                        if (ImGui::ColorEdit4("Additive Color", &material2D->data.additiveColor.x))
                        {
                            material2D->SetDirtyFlag(true);
                        }

                        if (ImGui::DragFloat2("Tiling", &material2D->data.tilingFactor.x, 0.025f))
                        {
                            material2D->SetDirtyFlag(true);
                        }

                        DrawTexturePreviewDropTarget("Texture Preview", material2D->textureHandle, [&]()
                        {
                            material2D->SetDirtyFlag(true);
                        });
                    }
                    else
                    {
                        ImGui::Text("Invalid asset!");
                    }
                }
                else
                {
                    ImGui::Text("Loading asset...");
                }
            }
        }

        UIAssetEditorClosePopup(assetData, isOpen);
        ImGui::End();
        assetData.isOpen = isOpen;
        assetData.requestFocus = false;
    }


    void AssetEditorPanel::UIAnimation2DEditor(AssetEditorData &assetData)
    {
        bool isOpen = assetData.isOpen;
        if (BeginAssetEditorWindow(assetData, isOpen, ImVec2(1280.0f, 1080.0f), ImVec2(420.0f, 640.0f), ImGuiWindowFlags_NoScrollWithMouse))
        {
            if (DrawAssetEditorHeader(assetData))
            {
                if (assetData.asset && assetData.asset->IsReady())
                {
                    if (Ref<Animation2D> anim = assetData.asset->As<Animation2D>())
                    {
                        auto assetManager = AssetManager::GetInstance();

                        const auto stateKey = static_cast<uint64_t>(anim->handle);
                        Animation2DEditorState &st = s_Anim2DEditorState[stateKey];

                        // Resolve texture
                        Ref<Texture> texture = nullptr;
                        if (anim->textureHandle != AssetHandle(0))
                        {
                            texture = assetManager->GetAsset<Texture>(anim->textureHandle);
                        }

                        const int frameCount = static_cast<int>(anim->frames.size());
                        const float fps = glm::max(anim->fps, 0.001f);
                        const float totalDur = (frameCount > 0) ? (static_cast<float>(frameCount) / fps) : 0.0f;

                        // ---- Advance internal preview clock ----
                        const float now = static_cast<float>(ImGui::GetTime());
                        if (st.playing && frameCount > 0)
                        {
                            const float dt = now - st.lastRealTime;
                            st.playbackTime += dt;
                            if (st.playbackTime >= totalDur)
                            {
                                if (anim->loop)
                                {
                                    st.playbackTime = std::fmod(st.playbackTime, totalDur);
                                }
                                else
                                {
                                    st.playbackTime = totalDur;
                                    st.playing = false;
                                }
                            }
                            st.previewFrame = std::min(static_cast<int>(st.playbackTime * fps), frameCount - 1);
                        }
                        st.lastRealTime = now;

                        // ================================================================
                        // Layout: LEFT = preview viewport | RIGHT = tools
                        // ================================================================
                        const ImVec2 available = ImGui::GetContentRegionAvail();
                        const float  splitter = 6.0f;
                        const float  minLeft = 120.0f;
                        const float  minRight = 240.0f;
                        st.toolsWidth = std::clamp(st.toolsWidth, minRight, available.x - minLeft - splitter);
                        const float leftW = available.x - st.toolsWidth - splitter;

                        // ----- LEFT: preview -----
                        ImGui::BeginChild("##anim2d_preview_col", ImVec2(leftW, 0.0f), ImGuiChildFlags_None);
                        {
                            // Timeline height reservation
                            const float timelineH = 80.0f;
                            const float controlsH = 28.0f;
                            const float previewH = ImGui::GetContentRegionAvail().y - timelineH - controlsH - 12.0f;

                            // -- Preview viewport --
                            ImGui::BeginChild("##anim2d_tex_view", ImVec2(0.0f, previewH), ImGuiChildFlags_Borders);
                            {
                                const ImVec2 vpPos = ImGui::GetCursorScreenPos();
                                const ImVec2 vpSize = ImGui::GetContentRegionAvail();

                                if (vpSize.x > 0 && vpSize.y > 0)
                                {
                                    ImGui::InvisibleButton("##anim2d_view_ibt", vpSize);
                                }

                                // Mouse-wheel zoom
                                if (ImGui::IsItemHovered())
                                {
                                    const float w = ImGui::GetIO().MouseWheel;
                                    if (w != 0.0f) st.previewZoom = std::clamp(st.previewZoom + w * 0.1f, 0.1f, 8.0f);
                                }

                                ImDrawList *dl = ImGui::GetWindowDrawList();

                                // Checkerboard background
                                {
                                    constexpr float cs = 12.0f;
                                    const ImU32 cA = IM_COL32(60, 60, 60, 255);
                                    const ImU32 cB = IM_COL32(80, 80, 80, 255);
                                    const int cols = static_cast<int>(vpSize.x / cs) + 2;
                                    const int rows = static_cast<int>(vpSize.y / cs) + 2;
                                    for (int ry = 0; ry < rows; ++ry)
                                    {
                                        for (int cx = 0; cx < cols; ++cx)
                                        {
                                            const ImVec2 tMin = { vpPos.x + cx * cs, vpPos.y + ry * cs };
                                            const ImVec2 tMax = { tMin.x + cs, tMin.y + cs };
                                            dl->AddRectFilled(tMin, tMax, ((cx + ry) % 2 == 0) ? cA : cB);
                                        }
                                    }
                                }

                                if (texture && texture->GetHandle() && frameCount > 0)
                                {
                                    const int fi = std::clamp(st.previewFrame, 0, frameCount - 1);
                                    const auto &fr = anim->frames[static_cast<size_t>(fi)];

                                    const float texW = static_cast<float>(texture->GetWidth());
                                    const float texH = static_cast<float>(texture->GetHeight());
                                    // Aspect of the sprite region
                                    const float uvW = std::max(std::abs(fr.uv1.x - fr.uv0.x), 0.001f);
                                    const float uvH = std::max(std::abs(fr.uv1.y - fr.uv0.y), 0.001f);
                                    const float sprW = texW * uvW;
                                    const float sprH = texH * uvH;
                                    const float fit = std::min(vpSize.x / sprW, vpSize.y / sprH);
                                    const float imgW = sprW * fit * st.previewZoom;
                                    const float imgH = sprH * fit * st.previewZoom;
                                    const ImVec2 imgPos =
                                    {
                                        vpPos.x + (vpSize.x - imgW) * 0.5f,
                                        vpPos.y + (vpSize.y - imgH) * 0.5f
                                    };

                                    dl->AddImage(reinterpret_cast<ImTextureID>(texture->GetHandle().Get()),
                                        imgPos, ImVec2(imgPos.x + imgW, imgPos.y + imgH), ImVec2(fr.uv0.x, fr.uv1.y), ImVec2(fr.uv1.x, fr.uv0.y));

                                    // Frame label
                                    const std::string lbl = std::format("Frame {} / {}", fi, frameCount - 1);
                                    dl->AddText(ImVec2(vpPos.x + 6, vpPos.y + 6), IM_COL32(255, 255, 255, 220), lbl.c_str());
                                }
                                else
                                {
                                    const char *msg = texture ? "No frames" : "No texture";
                                    const ImVec2 ts = ImGui::CalcTextSize(msg);
                                    dl->AddText(ImVec2(vpPos.x + (vpSize.x - ts.x) * 0.5f, vpPos.y + (vpSize.y - ts.y) * 0.5f),
                                        IM_COL32(160, 160, 160, 200), msg);
                                }
                            }
                            ImGui::EndChild();

                            // -- Playback controls --
                            ImGui::Spacing();
                            const float btnW = 60.0f;

                            // Play
                            bool playDisabled = (frameCount == 0);
                            if (playDisabled) ImGui::BeginDisabled();
                            if (ImGui::Button(st.playing ? "Pause##anim2d_pause" : "Play##anim2d_play", ImVec2(btnW, 0)))
                            {
                                if (st.playing)
                                {
                                    st.playing = false;
                                }
                                else
                                {
                                    // If at end and not looping, restart
                                    if (!anim->loop && st.playbackTime >= totalDur)
                                    {
                                        st.playbackTime = 0.0f;
                                    }
                                    st.playing = true;
                                }
                            }

                            if (playDisabled)
                            {
                                ImGui::EndDisabled();
                            }

                            ImGui::SameLine();
                            if (ImGui::Button("Stop##anim2d_stop", ImVec2(btnW, 0)))
                            {
                                st.playing = false;
                                st.playbackTime = 0.0f;
                                st.previewFrame = 0;
                            }

                            ImGui::SameLine();
                            ImGui::SetNextItemWidth(-1.0f);
                            // Zoom slider
                            ImGui::SliderFloat("##anim2d_zoom", &st.previewZoom, 0.1f, 4.0f, "Zoom %.2fx");

                            // -- Timeline --
                            ImGui::Spacing();
                            {
                                UI::TimelineState timelineState { frameCount, fps, totalDur, st.playbackTime };
                                UI::Timeline::Draw(ImGui::GetWindowDrawList(), 54.0f, timelineState, &st.playbackTime, &st.previewFrame, &st.playing, "No frames – drop sprites here");
                            }
                        }
                        ImGui::EndChild();

                        // ---- Splitter handle ----
                        ImGui::SameLine(0.0f, 0.0f);
                        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.18f, 0.18f, 0.20f, 1));
                        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.26f, 0.26f, 0.30f, 1));
                        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.30f, 0.30f, 0.36f, 1));
                        ImGui::BeginChild("##anim2d_splitter", ImVec2(splitter, 0.0f), ImGuiChildFlags_None);
                        ImGui::Button("##anim2d_splitter_btn", ImVec2(-1.0f, -1.0f));
                        if (ImGui::IsItemHovered() || ImGui::IsItemActive())
                        {
                            ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeEW);
                        }
                        if (ImGui::IsItemActive())
                        {
                            st.toolsWidth = std::clamp(st.toolsWidth - ImGui::GetIO().MouseDelta.x, minRight, available.x - minLeft - splitter);
                        }

                        ImGui::EndChild();
                        ImGui::PopStyleColor(3);

                        // ================================================================
                        // RIGHT: tools
                        // ================================================================
                        ImGui::SameLine(0.0f, 0.0f);
                        ImGui::BeginChild("##anim2d_tools_col", ImVec2(0.0f, 0.0f), ImGuiChildFlags_None);
                        {
                            // Name
                            {
                                char nameBuf[256];
                                std::strncpy(nameBuf, anim->name.c_str(), sizeof(nameBuf));
                                ImGui::SetNextItemWidth(-1.0f);
                                if (ImGui::InputText("##anim2d_name", nameBuf, sizeof(nameBuf)))
                                {
                                    anim->name = nameBuf;
                                    anim->SetDirtyFlag(true);
                                }
                                ImGui::SameLine(0.0f, 4.0f);
                                ImGui::TextDisabled("Name");
                            }

                            // FPS + Loop
                            ImGui::Spacing();
                            float fps2 = anim->fps;
                            ImGui::SetNextItemWidth(-60.0f);
                            if (ImGui::DragFloat("##anim2d_fps", &fps2, 0.5f, 0.1f, 240.0f, "FPS: %.1f"))
                            {
                                anim->fps = fps2;
                                anim->SetDirtyFlag(true);
                            }
                            ImGui::SameLine();
                            bool loop2 = anim->loop;
                            if (ImGui::Checkbox("Loop##anim2d_loop", &loop2))
                            {
                                anim->loop = loop2;
                                anim->SetDirtyFlag(true);
                            }

                            ImGui::Separator();

                            // Texture drop zone
                            ImGui::TextDisabled("Texture");
                            const std::string texLabel = (anim->textureHandle != AssetHandle(0)) ? "Texture Loaded" : "Drop Texture Here";
                            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.18f, 0.18f, 0.24f, 1.0f));
                            ImGui::Button(texLabel.c_str(), ImVec2(-1.0f, 28.0f));
                            ImGui::PopStyleColor();
                            if (ImGui::BeginDragDropTarget())
                            {
                                if (const ImGuiPayload *payload = ImGui::AcceptDragDropPayload(DND_PAYLOAD_CONTENT_BROWSER_ITEM))
                                {
                                    const AssetHandle handle = *static_cast<const AssetHandle *>(payload->Data);
                                    const AssetMetaData &md = AssetManager::GetInstance()->GetMetaData(handle);
                                    if (md.type == AssetType::Texture)
                                    {
                                        anim->textureHandle = handle;
                                        anim->SetDirtyFlag(true);
                                    }
                                }
                                ImGui::EndDragDropTarget();
                            }
                            ImGui::TextDisabled("Handle: %llu", static_cast<uint64_t>(anim->textureHandle));

                            ImGui::Separator();

                            // Frame drop zone
                            ImGui::TextDisabled("Frames (%d)", frameCount);
                            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.12f, 0.14f, 0.28f, 1.0f));
                            ImGui::Button("Drop Sprites / SpriteSheet##anim2d_drop", ImVec2(-1.0f, 32.0f));
                            ImGui::PopStyleColor();
                            if (ImGui::BeginDragDropTarget())
                            {
                                if (const ImGuiPayload *payload = ImGui::AcceptDragDropPayload(DND_PAYLOAD_SPRITE_SHEET_ITEM))
                                {
                                    if (payload->Data && payload->DataSize == sizeof(SpriteSheetSpritePayload))
                                    {
                                        const auto sp = *static_cast<const SpriteSheetSpritePayload *>(payload->Data);
                                        Animation2D::Frame f; f.uv0 = sp.uv0; f.uv1 = sp.uv1;
                                        std::swap(f.uv0.y, f.uv1.y);
                                        anim->frames.push_back(f);
                                        if (anim->textureHandle == AssetHandle(0)) anim->textureHandle = sp.textureHandle;
                                        anim->SetDirtyFlag(true);
                                    }
                                }
                                else if (const ImGuiPayload *payload = ImGui::AcceptDragDropPayload(DND_PAYLOAD_CONTENT_BROWSER_ITEM))
                                {
                                    if (payload->Data && payload->DataSize == sizeof(AssetHandle))
                                    {
                                        const AssetHandle handle = *static_cast<AssetHandle *>(payload->Data);
                                        const auto &md = AssetManager::GetInstance()->GetMetaData(handle);
                                        if (md.type == AssetType::SpriteSheet)
                                        {
                                            Ref<SpriteSheet> ss = assetManager->GetAsset<SpriteSheet>(handle);
                                            if (ss)
                                            {
                                                for (const auto &sp : ss->GetSprites())
                                                {
                                                    Animation2D::Frame f; f.uv0 = sp.uv0; f.uv1 = sp.uv1;
                                                    std::swap(f.uv0.y, f.uv1.y);
                                                    anim->frames.push_back(f);
                                                }
                                                if (anim->textureHandle == AssetHandle(0)) anim->textureHandle = ss->GetTextureHandle();
                                                anim->SetDirtyFlag(true);
                                            }
                                        }
                                    }
                                }
                                ImGui::EndDragDropTarget();
                            }

                            // Frame list
                            if (ImGui::BeginChild("##anim2d_frame_list", ImVec2(0.0f, -36.0f), ImGuiChildFlags_Borders))
                            {
                                for (int i = 0; i < frameCount; ++i)
                                {
                                    auto &f = anim->frames[static_cast<size_t>(i)];
                                    ImGui::PushID(i);
                                    const bool sel = (i == st.previewFrame);
                                    ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(0.2f, 0.5f, 0.3f, 0.7f));
                                    if (ImGui::Selectable(std::format("Frame {}##anim2d_fsel", i).c_str(), sel))
                                    {
                                        st.previewFrame = i;
                                        st.playbackTime = i / fps;
                                        st.playing = false;
                                    }
                                    ImGui::PopStyleColor();
                                    ImGui::SameLine();
                                    ImGui::TextDisabled("(%.2f,%.2f)→(%.2f,%.2f)", f.uv0.x, f.uv0.y, f.uv1.x, f.uv1.y);
                                    ImGui::SameLine();
                                    if (ImGui::SmallButton("X##anim2d_del"))
                                    {
                                        anim->frames.erase(anim->frames.begin() + i);
                                        anim->SetDirtyFlag(true);
                                        if (st.previewFrame >= static_cast<int>(anim->frames.size()))
                                            st.previewFrame = std::max(0, static_cast<int>(anim->frames.size()) - 1);
                                        ImGui::PopID();
                                        break;
                                    }
                                    ImGui::PopID();
                                }
                            }
                            ImGui::EndChild();

                            if (ImGui::Button("+ Empty##anim2d_add")) { anim->frames.push_back({}); anim->SetDirtyFlag(true); }
                            ImGui::SameLine();
                            if (ImGui::Button("Clear##anim2d_clear")) { anim->frames.clear(); anim->SetDirtyFlag(true); st.previewFrame = 0; st.playbackTime = 0.0f; st.playing = false; }
                        }
                        ImGui::EndChild();
                    }
                    else
                    {
                        ImGui::Text("Invalid asset!");
                    }
                }
                else
                {
                    ImGui::Text("Loading asset...");
                }
            }
        }

        UIAssetEditorClosePopup(assetData, isOpen);
        ImGui::End();
        assetData.isOpen = isOpen;
        assetData.requestFocus = false;
    }

    void AssetEditorPanel::UIAnimatorController2DEditor(AssetEditorData &assetData)
    {
        bool isOpen = assetData.isOpen;
        if (BeginAssetEditorWindow(assetData, isOpen, ImVec2(1600.0f, 1000.0f), ImVec2(520.0f, 700.0f), ImGuiWindowFlags_NoScrollWithMouse))
        {
            if (DrawAssetEditorHeader(assetData))
            {
                if (assetData.asset && assetData.asset->IsReady())
                {
                    if (Ref<AnimatorController2D> ctrl = assetData.asset->As<AnimatorController2D>())
                    {
                        auto project = m_EditorLayer->GetActiveProject();

                        ImGui::Text("Animator Controller 2D");
                        ImGui::Separator();

                        // Default State
                        {
                            char defBuf[256];
                            std::strncpy(defBuf, ctrl->defaultState.c_str(), sizeof(defBuf));
                            if (ImGui::InputText("Default State##ac2d_default", defBuf, sizeof(defBuf)))
                            {
                                ctrl->defaultState = defBuf;
                                ctrl->SetDirtyFlag(true);
                            }
                        }

                        // ==== States ====
                        if (ImGui::CollapsingHeader("States##ac2d_states", ImGuiTreeNodeFlags_DefaultOpen))
                        {
                            for (int i = 0; i < static_cast<int>(ctrl->states.size()); ++i)
                            {
                                auto &state = ctrl->states[static_cast<size_t>(i)];
                                ImGui::PushID(i);

                                ImGui::SetNextItemWidth(150);
                                char nameBuf[256];
                                std::strncpy(nameBuf, state.name.c_str(), sizeof(nameBuf));
                                if (ImGui::InputText("Name##s_name", nameBuf, sizeof(nameBuf)))
                                {
                                    state.name = nameBuf;
                                    ctrl->SetDirtyFlag(true);
                                }
                                ImGui::SameLine();

                                // Animation2D drag-drop
                                ImGui::Text("Anim: %llu", static_cast<uint64_t>(state.GetAnimationAssetHandle()));
                                ImGui::SameLine();
                                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.15f, 0.2f, 0.35f, 1.0f));
                                ImGui::Button("Drop Anim2D##s_drop", ImVec2(90, 20));
                                ImGui::PopStyleColor();
                                if (ImGui::BeginDragDropTarget())
                                {
                                    if (const ImGuiPayload *payload = ImGui::AcceptDragDropPayload(DND_PAYLOAD_CONTENT_BROWSER_ITEM))
                                    {
                                        const AssetHandle handle = *static_cast<const AssetHandle *>(payload->Data);
                                        const AssetMetaData &md = AssetManager::GetInstance()->GetMetaData(handle);
                                        if (md.type == AssetType::Animation2D)
                                        {
                                            state.SetAnimationHandle(handle);
                                            ctrl->SetDirtyFlag(true);
                                        }
                                    }
                                    ImGui::EndDragDropTarget();
                                }
                                ImGui::SameLine();
                                if (ImGui::SmallButton("Remove##s_rem"))
                                {
                                    ctrl->states.erase(ctrl->states.begin() + i);
                                    ctrl->SetDirtyFlag(true);
                                    ImGui::PopID();
                                    break;
                                }

                                ImGui::PopID();
                            }

                            if (ImGui::Button("+ Add State##ac2d_add_state"))
                            {
                                AnimState2D s;
                                s.name = "NewState";
                                s.editorPos = { 100.0f + 120.0f * static_cast<float>(ctrl->states.size()), 100.0f };
                                ctrl->states.push_back(s);
                                ctrl->SetDirtyFlag(true);
                            }
                        }

                        // ==== Parameters ====
                        if (ImGui::CollapsingHeader("Parameters##ac2d_params", ImGuiTreeNodeFlags_DefaultOpen))
                        {
                            for (int i = 0; i < static_cast<int>(ctrl->params.size()); ++i)
                            {
                                auto &p = ctrl->params[static_cast<size_t>(i)];
                                ImGui::PushID(i);

                                ImGui::SetNextItemWidth(120);
                                char pNameBuf[256];
                                std::strncpy(pNameBuf, p.name.c_str(), sizeof(pNameBuf));
                                if (ImGui::InputText("##p_name", pNameBuf, sizeof(pNameBuf)))
                                {
                                    p.name = pNameBuf;
                                    ctrl->SetDirtyFlag(true);
                                }
                                ImGui::SameLine();

                                // Type selector
                                int typeIdx = static_cast<int>(p.type);
                                ImGui::SetNextItemWidth(70);
                                if (ImGui::Combo("##p_type", &typeIdx, s_ParamTypeNames, 4))
                                {
                                    p.type = static_cast<AnimParam::Type>(typeIdx);
                                    ctrl->SetDirtyFlag(true);
                                }
                                ImGui::SameLine();

                                // Value
                                switch (p.type)
                                {
                                    case AnimParam::Type::Float:
                                    ImGui::SetNextItemWidth(80);
                                    if (ImGui::DragFloat("##p_float", &p.floatVal, 0.01f)) ctrl->SetDirtyFlag(true);
                                    break;
                                    case AnimParam::Type::Int:
                                    ImGui::SetNextItemWidth(80);
                                    if (ImGui::DragInt("##p_int", &p.intVal)) ctrl->SetDirtyFlag(true);
                                    break;
                                    case AnimParam::Type::Bool:
                                    if (ImGui::Checkbox("##p_bool", &p.boolVal)) ctrl->SetDirtyFlag(true);
                                    break;
                                    case AnimParam::Type::String:
                                    {
                                        char strbuf[256];
                                        std::strncpy(strbuf, p.strVal.c_str(), sizeof(strbuf));
                                        ImGui::SetNextItemWidth(100);
                                        if (ImGui::InputText("##p_str", strbuf, sizeof(strbuf)))
                                        {
                                            p.strVal = strbuf;
                                            ctrl->SetDirtyFlag(true);
                                        }
                                    }
                                    break;
                                    default: break;
                                }
                                ImGui::SameLine();
                                if (ImGui::SmallButton("X##p_del"))
                                {
                                    ctrl->params.erase(ctrl->params.begin() + i);
                                    ctrl->SetDirtyFlag(true);
                                    ImGui::PopID();
                                    break;
                                }

                                ImGui::PopID();
                            }

                            if (ImGui::Button("+ Float##p_addf"))
                            {
                                ctrl->params.push_back({ .name = "NewFloat", .type = AnimParam::Type::Float }); ctrl->SetDirtyFlag(true);
                            }
                            ImGui::SameLine();
                            if (ImGui::Button("+ Bool##p_addb"))
                            {
                                ctrl->params.push_back({ .name = "NewBool", .type = AnimParam::Type::Bool }); ctrl->SetDirtyFlag(true);
                            }
                            ImGui::SameLine();
                            if (ImGui::Button("+ Int##p_addi"))
                            {
                                ctrl->params.push_back({ .name = "NewInt", .type = AnimParam::Type::Int }); ctrl->SetDirtyFlag(true);
                            }
                            ImGui::SameLine();
                            if (ImGui::Button("+ String##p_adds"))
                            {
                                ctrl->params.push_back({ .name = "NewString", .type = AnimParam::Type::String }); ctrl->SetDirtyFlag(true);
                            }
                        }

                        // ==== Transitions ====
                        if (ImGui::CollapsingHeader("Transitions##ac2d_trans"))
                        {
                            for (int i = 0; i < static_cast<int>(ctrl->transitions.size()); ++i)
                            {
                                auto &tr = ctrl->transitions[static_cast<size_t>(i)];
                                ImGui::PushID(i);

                                char fromBuf[256], toBuf[256];
                                std::strncpy(fromBuf, tr.fromState.c_str(), sizeof(fromBuf));
                                std::strncpy(toBuf, tr.toState.c_str(), sizeof(toBuf));
                                ImGui::SetNextItemWidth(110);
                                if (ImGui::InputText("From##tr_from", fromBuf, sizeof(fromBuf)))
                                {
                                    tr.fromState = fromBuf; ctrl->SetDirtyFlag(true);
                                }

                                ImGui::SameLine();
                                ImGui::TextUnformatted("->");
                                ImGui::SameLine();
                                ImGui::SetNextItemWidth(110);

                                if (ImGui::InputText("To##tr_to", toBuf, sizeof(toBuf)))
                                {
                                    tr.toState = toBuf; ctrl->SetDirtyFlag(true);
                                }

                                ImGui::SameLine();
                                if (ImGui::Checkbox("Exit Time##tr_et", &tr.hasExitTime)) ctrl->SetDirtyFlag(true);
                                if (tr.hasExitTime)
                                {
                                    ImGui::SameLine();
                                    ImGui::SetNextItemWidth(60);
                                    if (ImGui::DragFloat("##tr_etv", &tr.exitTime, 0.01f, 0.0f, 1.0f))
                                    {
                                        ctrl->SetDirtyFlag(true);
                                    }
                                }

                                ImGui::SameLine();
                                if (ImGui::SmallButton("X##tr_del"))
                                {
                                    ctrl->transitions.erase(ctrl->transitions.begin() + i);
                                    ctrl->SetDirtyFlag(true);
                                    ImGui::PopID();
                                    break;
                                }

                                // Conditions
                                ImGui::Indent(16.0f);
                                for (int ci = 0; ci < static_cast<int>(tr.conditions.size()); ++ci)
                                {
                                    auto &cond = tr.conditions[static_cast<size_t>(ci)];
                                    ImGui::PushID(ci);

                                    char cParamBuf[256];
                                    std::strncpy(cParamBuf, cond.paramName.c_str(), sizeof(cParamBuf));
                                    ImGui::SetNextItemWidth(110);
                                    if (ImGui::InputText("Param##cond_p", cParamBuf, sizeof(cParamBuf)))
                                    {
                                        cond.paramName = cParamBuf; ctrl->SetDirtyFlag(true);
                                    }
                                    ImGui::SameLine();
                                    int  opIdx = static_cast<int>(cond.op);
                                    ImGui::SetNextItemWidth(50);
                                    if (ImGui::Combo("##cond_op", &opIdx, s_ConditionOpNames, 6))
                                    {
                                        cond.op = static_cast<AnimCondition::Op>(opIdx);
                                        ctrl->SetDirtyFlag(true);
                                    }
                                    ImGui::SameLine();

                                    const AnimParam *param = ctrl->GetParam(cond.paramName);
                                    if (param)
                                    {
                                        switch (param->type)
                                        {
                                            case AnimParam::Type::Float:
                                            ImGui::SetNextItemWidth(70);
                                            if (ImGui::DragFloat("##cond_fval", &cond.floatThreshold, 0.01f)) ctrl->SetDirtyFlag(true);
                                            break;
                                            case AnimParam::Type::Int:
                                            ImGui::SetNextItemWidth(70);
                                            if (ImGui::DragInt("##cond_ival", &cond.intThreshold)) ctrl->SetDirtyFlag(true);
                                            break;
                                            case AnimParam::Type::Bool:
                                            if (ImGui::Checkbox("##cond_bval", &cond.boolThreshold)) ctrl->SetDirtyFlag(true);
                                            break;
                                            case AnimParam::Type::String:
                                            {
                                                char sbuf[256];
                                                std::strncpy(sbuf, cond.strThreshold.c_str(), sizeof(sbuf));
                                                ImGui::SetNextItemWidth(90);
                                                if (ImGui::InputText("##cond_sval", sbuf, sizeof(sbuf)))
                                                {
                                                    cond.strThreshold = sbuf; ctrl->SetDirtyFlag(true);
                                                }
                                            }
                                            break;
                                            default: break;
                                        }
                                    }
                                    else
                                    {
                                        ImGui::SetNextItemWidth(70);
                                        if (ImGui::DragFloat("##cond_fval2", &cond.floatThreshold, 0.01f))
                                        {
                                            ctrl->SetDirtyFlag(true);
                                        }
                                    }

                                    ImGui::SameLine();
                                    if (ImGui::SmallButton("X##cond_del"))
                                    {
                                        tr.conditions.erase(tr.conditions.begin() + ci);
                                        ctrl->SetDirtyFlag(true);
                                        ImGui::PopID();
                                        break;
                                    }
                                    ImGui::PopID();
                                }
                                if (ImGui::SmallButton("+ Condition##cond_add"))
                                {
                                    tr.conditions.push_back({});
                                    ctrl->SetDirtyFlag(true);
                                }
                                ImGui::Unindent(16.0f);

                                ImGui::PopID();
                                ImGui::Separator();
                            }

                            if (ImGui::Button("+ Add Transition##tr_add"))
                            {
                                AnimTransition tr;
                                if (!ctrl->states.empty())
                                {
                                    tr.fromState = ctrl->states[0].name;
                                    tr.toState = ctrl->states.size() > 1 ? ctrl->states[1].name : "";
                                }
                                ctrl->transitions.push_back(tr);
                                ctrl->SetDirtyFlag(true);
                            }
                        }
                    }
                    else
                    {
                        ImGui::Text("Invalid asset!");
                    }
                }
                else
                {

                    ImGui::Text("Loading asset...");
                }
            }
        }

        UIAssetEditorClosePopup(assetData, isOpen);
        ImGui::End();
        assetData.isOpen = isOpen;
        assetData.requestFocus = false;
    }
#pragma endregion !2D_STUFF


#pragma region 3D_STUFF
    void AssetEditorPanel::UIMaterialEditor(AssetEditorData &assetData)
    {
        bool isOpen = assetData.isOpen;
        if (BeginAssetEditorWindow(assetData, isOpen, ImVec2(1080.0f, 480.0f), ImVec2(420.0f, 560.0f), 0))
        {
            if (DrawAssetEditorHeader(assetData))
            {
                if (assetData.asset && assetData.asset->IsReady())
                {
                    if (Ref<Material> material = assetData.asset->As<Material>())
                    {
                        const auto stateKey = static_cast<uint64_t>(assetData.handle);
                        MaterialEditorState &editorState = s_MaterialEditorState[stateKey];
                        if (!editorState.initialized)
                        {
                            editorState.selectedMeshType = static_cast<int>(EMeshType::UV_SPHERE);
                            editorState.initialized = true;
                        }

                        const ImVec2 contentSize = ImGui::GetContentRegionAvail();
                        const float splitterWidth = 6.0f;
                        const float minPreviewColumnWidth = 260.0f;
                        const float minToolsColumnWidth = 320.0f;
                        const float maxPreviewColumnWidth = std::max(minPreviewColumnWidth, contentSize.x - minToolsColumnWidth - splitterWidth);

                        if (editorState.previewColumnWidth <= 0.0f)
                        {
                            const float defaultToolsWidth = std::clamp(contentSize.x * 0.36f, 320.0f, 460.0f);
                            editorState.previewColumnWidth = std::max(minPreviewColumnWidth, contentSize.x - defaultToolsWidth - splitterWidth);
                        }
                        editorState.previewColumnWidth = std::clamp(editorState.previewColumnWidth, minPreviewColumnWidth, maxPreviewColumnWidth);

                        ImGui::BeginChild("##material_preview_column", ImVec2(editorState.previewColumnWidth, 0.0f), ImGuiChildFlags_None);
                        ImGui::BeginChild("##material_preview_viewport", ImVec2(0.0f, 0.0f), ImGuiChildFlags_Borders);
                        {
                            const ImVec2 viewportSize = ImGui::GetContentRegionAvail();
                            assetData.sceneData.viewportWidth = std::max(1u, static_cast<uint32_t>(viewportSize.x));
                            assetData.sceneData.viewportHeight = std::max(1u, static_cast<uint32_t>(viewportSize.y));

                            Ref<Texture> previewTexture = assetData.sceneData.compositeRT ? assetData.sceneData.compositeRT->GetColorAttachment(0) : nullptr;
                            if (previewTexture && previewTexture->GetHandle())
                            {
                                ImGui::Image(reinterpret_cast<ImTextureID>(previewTexture->GetHandle().Get()), viewportSize, ImVec2(0.0f, 0.0f), ImVec2(1.0f, 1.0f));
                                assetData.sceneData.viewportHovered = ImGui::IsItemHovered();
                            }
                            else
                            {
                                ImGui::Dummy(viewportSize);
                                assetData.sceneData.viewportHovered = ImGui::IsItemHovered();
                            }
                        }
                        ImGui::EndChild();
                        ImGui::EndChild();

                        UpdateSceneCamera(assetData.sceneData, ImGui::GetIO().DeltaTime);

						// Horizontal splitter
                        ImGui::SameLine(0.0f, 0.0f);
                        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.20f, 0.20f, 0.20f, 1.0f));
                        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.28f, 0.28f, 0.28f, 1.0f));
                        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.32f, 0.32f, 0.32f, 1.0f));
                        ImGui::BeginChild("##material_horizontal_splitter", ImVec2(splitterWidth, 0.0f), ImGuiChildFlags_None);
                        ImGui::Button("##material_horizontal_splitter_btn", ImVec2(-1.0f, -1.0f));
                        if (ImGui::IsItemHovered() || ImGui::IsItemActive())
                        {
                            ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeEW);
                        }
                        if (ImGui::IsItemActive())
                        {
                            editorState.previewColumnWidth += ImGui::GetIO().MouseDelta.x;
                            editorState.previewColumnWidth = std::clamp(editorState.previewColumnWidth, minPreviewColumnWidth, maxPreviewColumnWidth);
                        }
                        ImGui::EndChild();
                        ImGui::PopStyleColor(3);

                        ImGui::SameLine(0.0f, 0.0f);
                        ImGui::BeginChild("##material_controls_column", ImVec2(0.0f, 0.0f), ImGuiChildFlags_None);
                        {
							const char *meshNames[] = { "UV Sphere" };
							int meshSelection = editorState.selectedMeshType;
							if (ImGui::Combo("Preview Mesh", &meshSelection, meshNames, IM_ARRAYSIZE(meshNames)))
							{
								editorState.selectedMeshType = meshSelection;
								assetData.sceneData.sceneRenderer->SetPreviewStaticMesh(Renderer::GetDefaultMesh((EMeshType)meshSelection));
							}

							DrawSceneDetails(assetData);

							if (ImGui::TreeNodeEx("Material Properties", ImGuiTreeNodeFlags_DefaultOpen))
							{
								if (UI::DrawColorVec4("Base Color", material->gpuData.baseColorFactor))
								{
									material->SetDirtyFlag(true);
								}

								if (UI::DrawColorVec4("Emissive Color", material->gpuData.emissiveFactor))
								{
									material->SetDirtyFlag(true);
								}

								if (UI::DrawFloatControl("Metallic Factor", &material->gpuData.metallicFactor, 0.025f, 0.0f, 1.0f))
								{
									material->SetDirtyFlag(true);
								}

								if (UI::DrawFloatControl("Roughness Factor", &material->gpuData.roughnessFactor, 0.025f, 0.0f, 1.0f))
								{
									material->SetDirtyFlag(true);
								}

								if (UI::DrawFloatControl("Occlusion Strength", &material->gpuData.occlusionStrength, 0.025f, 0.0f, 1.0f))
								{
									material->SetDirtyFlag(true);
								}

								// Blend Mode
								{
									const char *blendModeNames[] = { "Opaque", "Transparent" };
									int currentBlendMode = static_cast<int>(material->GetType());
									if (currentBlendMode > 1) currentBlendMode = 0; // clamp Masked to Opaque for display
									if (ImGui::Combo("Blend Mode", &currentBlendMode, blendModeNames, IM_ARRAYSIZE(blendModeNames)))
									{
										material->SetType(static_cast<MaterialType>(currentBlendMode));
										material->SetDirtyFlag(true);
										material->InvalidateBindingSet();
									}
								}

								// Tiling Factor
								if (ImGui::DragFloat2("Tiling Factor", &material->gpuData.tilingFactor.x, 0.05f, 0.01f, 100.0f))
								{
									material->SetDirtyFlag(true);
								}

								ImGui::Separator();

								if (ImGui::TreeNodeEx("Textures", ImGuiTreeNodeFlags_DefaultOpen))
								{
									// Base texture
									DrawTexturePreviewDropTarget("Base Color Texture", material->baseColorTextureHandle, [&]() { material->SetDirtyFlag(true); });

									// Normal texture
									DrawTexturePreviewDropTarget("Normal Texture", material->normalTextureHandle, [&]() { material->SetDirtyFlag(true); });

									// Emissive texture
									DrawTexturePreviewDropTarget("Emissive Texture", material->emissiveTextureHandle, [&]() { material->SetDirtyFlag(true); });

									// Occlussion texture
									DrawTexturePreviewDropTarget("Occlusion Texture", material->occlusionTextureHandle, [&]() { material->SetDirtyFlag(true); });

									// Metallic Texture
									DrawTexturePreviewDropTarget("Metallic Texture", material->metallicTextureHandle, [&]() { material->SetDirtyFlag(true); });

									// Metallic texture channel
									ImGui::SameLine();
									ImGui::SetNextItemWidth(70);
									if (ImGui::BeginCombo("Metallic Channel", TextureChannelToString(material->gpuData.metallicChannel)))
									{
										for (int channel = 0; channel < 4; ++channel)
										{
											const bool selected = material->gpuData.metallicChannel == channel;
											if (ImGui::Selectable(TextureChannelToString(channel), selected))
											{
												material->gpuData.metallicChannel = channel;
												material->SetDirtyFlag(true);
											}

											if (selected)
											{
												ImGui::SetItemDefaultFocus();
											}
										}

										ImGui::EndCombo();
									}

									// Roughness texture
									DrawTexturePreviewDropTarget("Roughness Texture", material->roughnessTextureHandle, [&]() { material->SetDirtyFlag(true); });

									// Roughness texture channel
									ImGui::SameLine();
									ImGui::SetNextItemWidth(70);
									if (ImGui::BeginCombo("Roughness Channel", TextureChannelToString(material->gpuData.roughnessChannel)))
									{
										for (int channel = 0; channel < 4; ++channel)
										{
											const bool selected = material->gpuData.roughnessChannel == channel;
											if (ImGui::Selectable(TextureChannelToString(channel), selected))
											{
												material->gpuData.roughnessChannel = channel;
												material->SetDirtyFlag(true);
											}

											if (selected)
											{
												ImGui::SetItemDefaultFocus();
											}
										}

										ImGui::EndCombo();
									}


									ImGui::TreePop();
								}

								ImGui::TreePop();
							}
                        }
                        ImGui::EndChild();
                    }
                    else
                    {
                        ImGui::Text("Invalid asset!");
                    }
                }
                else
                {
                    ImGui::Text("Loading asset...");
                }
            }
        }

        UIAssetEditorClosePopup(assetData, isOpen);
        ImGui::End();
        assetData.isOpen = isOpen;
        assetData.requestFocus = false;
    }

    void AssetEditorPanel::UIScriptableObjectEditor(AssetEditorData &assetData)
    {
        bool isOpen = assetData.isOpen;
        if (BeginAssetEditorWindow(assetData, isOpen, ImVec2(600.0f, 400.0f), ImVec2(400.0f, 300.0f), 0))
        {
            if (DrawAssetEditorHeader(assetData))
            {
                if (assetData.asset && assetData.asset->IsReady())
                {
                    if (Ref<ScriptableObject> so = assetData.asset->As<ScriptableObject>())
                    {
                        bool classAvails = ScriptableObjectEditor::DrawEditor(so, m_EditorLayer);
                        if (!classAvails)
                            ImGui::TextColored(ImVec4(0.9f, 0.2f, 0.2f, 1.0f), "C# Class '%s' not found!", so->GetClassName().c_str());
                    }
                    else
                    {
                        ImGui::Text("Invalid asset...");
                    }
                }
                else
                {
                    ImGui::Text("Loading asset...");
                }
            }

            UIAssetEditorClosePopup(assetData, isOpen);
            ImGui::End();
            assetData.isOpen = isOpen;
            assetData.requestFocus = false;
        }
    }

    void AssetEditorPanel::UITextureEditor(AssetEditorData &assetData)
    {
        bool isOpen = assetData.isOpen;
        if (BeginAssetEditorWindow(assetData, isOpen, ImVec2{ 420.0f, 720.0f }, ImVec2(420.0f, 720.0f), ImGuiWindowFlags_NoScrollbar))
        {
            if (DrawAssetEditorHeader(assetData))
            {
                if (assetData.asset && assetData.asset->IsReady())
                {
                    if (Ref<Texture> texture = assetData.asset->As<Texture>())
                    {
                        auto project = m_EditorLayer->GetActiveProject().get();
                        auto assetManager = AssetManager::GetInstance();

                        const auto stateKey = static_cast<uint64_t>(assetData.handle);
                        TextureEditorState &state = s_TextureEditorState[stateKey];
                        if (!state.initialized)
                        {
                            state.createInfo = texture->GetCreateInfo();
                            state.initialized = true;
                        }

                        ImGui::Text("Path: %s", assetData.metadata.filepath.generic_string().c_str());
                        ImGui::Text("Resolution: %d x %d", texture->GetWidth(), texture->GetHeight());
                        ImGui::Text("Channels: %d", texture->GetChannels());
                        ImGui::Text("Current Format: %s", TextureFormatToString(texture->GetFormat()));

                        const float previewMaxWidth = std::max(420.0f, ImGui::GetContentRegionAvail().x);
                        if (previewMaxWidth > 0.0f && texture->GetWidth() > 0 && texture->GetHeight() > 0)
                        {
                            const float aspectRatio = static_cast<float>(texture->GetWidth()) / static_cast<float>(texture->GetHeight());
                            ImVec2 previewSize(previewMaxWidth, previewMaxWidth);
                            if (aspectRatio > 1.0f)
                                previewSize.y = previewMaxWidth / aspectRatio;
                            else
                                previewSize.x = previewMaxWidth * aspectRatio;

                            ImGui::Text("Preview");
                            ImTextureID textureId = reinterpret_cast<ImTextureID>(texture->GetHandle().Get());
                            ImGui::Image(textureId, previewSize);
                        }

                        ImGui::Separator();

                        auto mipLevels = static_cast<int>(state.createInfo.mipLevels);
                        if (UI::DrawIntControl("Mip Levels", &mipLevels, 1.0f, 0, 16))
                        {
                            state.createInfo.mipLevels = static_cast<uint32_t>(std::max(mipLevels, 1));
                        }

                        auto arraySize = static_cast<int>(state.createInfo.arraySize);
                        if (UI::DrawIntControl("Array Size", &arraySize, 1.0f, 1, 64))
                        {
                            state.createInfo.arraySize = static_cast<uint32_t>(std::max(arraySize, 1));
                        }

                        auto sampleCount = static_cast<int>(state.createInfo.sampleCount);
                        if (UI::DrawIntControl("Sample Count", &sampleCount, 1.0f, 1, 16))
                        {
                            state.createInfo.sampleCount = static_cast<uint32_t>(std::max(sampleCount, 1));
                        }

                        auto sampleQuality = static_cast<int>(state.createInfo.sampleQuality);
                        if (UI::DrawIntControl("Sample Quality", &sampleQuality, 1.0f, 0, 16))
                        {
                            state.createInfo.sampleQuality = static_cast<uint32_t>(std::max(sampleQuality, 0));
                        }

                        const nvrhi::Format formatOptions[] = { nvrhi::Format::RGBA8_UNORM, nvrhi::Format::RGBA32_FLOAT };

                        int currentFormatIndex = 0;
                        for (int i = 0; i < static_cast<int>(std::size(formatOptions)); ++i)
                        {
                            if (state.createInfo.format == formatOptions[i])
                            {
                                currentFormatIndex = i;
                                break;
                            }
                        }

                        const char *formatOptionLabels[] =
                        {
                            TextureFormatToString(formatOptions[0]),
                            TextureFormatToString(formatOptions[1])
                        };

                        if (UI::DrawComboBox("Import Format", formatOptionLabels, static_cast<int>(std::size(formatOptionLabels)), &currentFormatIndex))
                        {
                            state.createInfo.format = formatOptions[currentFormatIndex];
                        }

                        const std::array<nvrhi::SamplerAddressMode, 3> addressModeOptions =
                        {
                            nvrhi::SamplerAddressMode::Repeat,
                            nvrhi::SamplerAddressMode::ClampToEdge,
                            nvrhi::SamplerAddressMode::ClampToBorder
                        };

                        std::array<const char *, 3> addressModeOptionsStr = { "Repeat", "Clamp To Edge" , "Clamp To Border" };

                        // Wrap U
                        auto drawAddressModeCombo = [&addressModeOptions, &addressModeOptionsStr](const char *label, nvrhi::SamplerAddressMode &mode)
                        {
                            int currentIdx = 0;
                            switch (mode)
                            {
                                case nvrhi::SamplerAddressMode::Repeat: currentIdx = 0; break;
                                case nvrhi::SamplerAddressMode::ClampToEdge: currentIdx = 1; break;
                                case nvrhi::SamplerAddressMode::ClampToBorder: currentIdx = 2; break;
                            }

                            if (UI::DrawComboBox(label, addressModeOptionsStr.data(), static_cast<int>(addressModeOptions.size()), &currentIdx))
                            {
                                mode = addressModeOptions[currentIdx];
                            }
                        };

                        drawAddressModeCombo("Wrap U", state.createInfo.samplerAddressU);
                        drawAddressModeCombo("Wrap V", state.createInfo.samplerAddressV);
                        drawAddressModeCombo("Wrap W", state.createInfo.samplerAddressW);

                        UI::DrawCheckbox("Linear Filtering", &state.createInfo.samplerLinearFiltering);
                        UI::DrawCheckbox("Flip Vertically", &state.createInfo.flip);
                        UI::DrawCheckbox("Keep CPU Data", &state.createInfo.keepCpuData);
                        UI::DrawCheckbox("Keep Initial State", &state.createInfo.keepInitialState);

                        ImGui::Separator();
                        if (ImGui::Button("ReImport"))
                        {
                            Texture::SerializeMetaFile(BuildAssetMetaPath(project, assetData.metadata), assetData.handle, state.createInfo);

                            AssetMetaData importMetadata = assetData.metadata;
                            importMetadata.filepath = project->GetProjectFilepath(assetData.metadata.filepath);

                            Ref<Texture> reimportedTexture = AssetImporter::ImportTexture(assetData.handle, importMetadata, state.createInfo, assetManager);
                            if (reimportedTexture)
                            {
                                reimportedTexture->handle = assetData.handle;
                                assetManager->AssignAsset(assetData.handle, reimportedTexture);
                                SaveAsset(assetData);
                            }
                        }
                    }
                    else
                    {
                        ImGui::Text("Invalid asset!");
                    }
                }
                else
                {
                    ImGui::Text("Loading asset...");
                }
            }
        }

        UIAssetEditorClosePopup(assetData, isOpen);
        ImGui::End();
        assetData.isOpen = isOpen;
        assetData.requestFocus = false;
    }

    void AssetEditorPanel::UIAnimatorControllerEditor(AssetEditorData &assetData)
    {
        bool isOpen = assetData.isOpen;
        if (BeginAssetEditorWindow(assetData, isOpen, ImVec2(1600.0f, 1000.0f), ImVec2(520.0f, 700.0f), ImGuiWindowFlags_NoScrollWithMouse))
        {
            if (DrawAssetEditorHeader(assetData))
            {
                if (assetData.asset && assetData.asset->IsReady())
                {
                    if (Ref<AnimatorController> animator = assetData.asset->As<AnimatorController>())
                    {
                        auto project = m_EditorLayer->GetActiveProject().get();
                        auto assetManager = AssetManager::GetInstance();
                        AnimatorControllerEditorState &ui = s_AnimatorControllerEditorState[static_cast<uint64_t>(animator->handle)];
                        if (!ui.initialized)
                        {
                            ui.initialized = true;
                            ui.selectedState = animator->states.empty() ? -1 : 0;
                            LoadAnimatorControllerEditorMeta(project, assetManager->GetMetaData(animator->handle), ui);
                        }

                        if (ui.selectedState >= static_cast<int>(animator->states.size())) ui.selectedState = animator->states.empty() ? -1 : static_cast<int>(animator->states.size()) - 1;
                        if (ui.selectedTransition >= static_cast<int>(animator->transitions.size())) ui.selectedTransition = animator->transitions.empty() ? -1 : static_cast<int>(animator->transitions.size()) - 1;

                        const float totalWidth = ImGui::GetContentRegionAvail().x;
                        const float spacing = ImGui::GetStyle().ItemSpacing.x;
                        const float leftWidth = std::max(220.0f, totalWidth * 0.22f);
                        const float rightWidth = std::max(360.0f, totalWidth * 0.30f);
                        const float graphWidth = std::max(320.0f, totalWidth - leftWidth - rightWidth - spacing * 2.0f);

                        ImGui::BeginChild("##ac_left", ImVec2(leftWidth, 0.0f), ImGuiChildFlags_Borders | ImGuiChildFlags_ResizeX);
                        AnimatorEditor::DrawAnimatorControllerLeftPanel(animator, assetManager, ui);
                        ImGui::EndChild();

                        ImGui::SameLine();
                        ImGui::BeginChild("##ac_graph", ImVec2(graphWidth, 0.0f), ImGuiChildFlags_Borders | ImGuiChildFlags_ResizeX, ImGuiWindowFlags_NoScrollWithMouse | ImGuiWindowFlags_NoScrollbar);
                        AnimatorEditor::DrawAnimatorControllerGraph(animator, assetManager, ui);
                        ImGui::EndChild();

                        ImGui::SameLine();
                        ImGui::BeginChild("##ac_right", ImVec2(0.0f, 0.0f), ImGuiChildFlags_Borders);
                        if (ImGui::BeginTabBar("##ac_tabs"))
                        {
                            if (ImGui::BeginTabItem("Inspector"))
                            {
                                AnimatorEditor::DrawAnimatorControllerInspectorTab(animator, assetManager, ui);
                                AnimatorEditor::DrawAnimatorControllerConditions(animator, ui);
                                ImGui::EndTabItem();
                            }
                            if (ImGui::BeginTabItem("Parameters"))
                            {
                                AnimatorEditor::DrawAnimatorControllerParamsTab(animator);
                                ImGui::EndTabItem();
                            }
                            ImGui::EndTabBar();
                        }
                        ImGui::EndChild();
                    }
                    else
                    {
                        ImGui::Text("Invalid asset!");
                    }
                }
                else
                {
                    ImGui::Text("Loading asset...");
                }
            }
        }

        UIAssetEditorClosePopup(assetData, isOpen);
        ImGui::End();
        assetData.isOpen = isOpen;
        assetData.requestFocus = false;
    }

    void AssetEditorPanel::UIBlendSpaceEditor(AssetEditorData &assetData)
    {
        bool isOpen = assetData.isOpen;
        if (BeginAssetEditorWindow(assetData, isOpen, ImVec2(1200.0f, 800.0f), ImVec2(520.0f, 500.0f), ImGuiWindowFlags_NoScrollWithMouse))
        {
            if (DrawAssetEditorHeader(assetData))
            {
                if (assetData.asset && assetData.asset->IsReady())
                {
                    if (Ref<BlendSpace> blendSpace = assetData.asset->As<BlendSpace>())
                    {
                        auto assetManager = AssetManager::GetInstance();
                        BlendSpaceEditorState &state = s_BlendSpaceEditorState[static_cast<uint64_t>(blendSpace->handle)];
                        BlendSpaceEditor::DrawBlendSpaceEditor(blendSpace, assetManager, state);
                    }
                    else
                    {
                        ImGui::Text("Invalid asset!");
                    }
                }
                else
                {
                    ImGui::Text("Loading asset...");
                }
            }
        }

        UIAssetEditorClosePopup(assetData, isOpen);
        ImGui::End();
        assetData.isOpen = isOpen;
        assetData.requestFocus = false;
    }

	void AssetEditorPanel::UIStaticMeshEditor(AssetEditorData &assetData)
	{
		bool isOpen = assetData.isOpen;
		if (BeginAssetEditorWindow(assetData, isOpen, ImVec2(1080.0f, 480.0f), ImVec2(420.0f, 560.0f), 0))
		{
			if (DrawAssetEditorHeader(assetData))
			{
				if (assetData.asset && assetData.asset->IsReady())
				{
					if (Ref<StaticMesh> mesh = assetData.asset->As<StaticMesh>())
					{
						auto project = m_EditorLayer->GetActiveProject().get();
						auto assetManager = AssetManager::GetInstance();

						const auto meshKey = static_cast<uint64_t>(mesh->handle);
						MeshEditorState &editorState = s_MeshEditorState[meshKey];

						const ImVec2 contentSize = ImGui::GetContentRegionAvail();
						const float splitterWidth = 6.0f;
						const float minPreviewColumnWidth = 260.0f;
						const float minToolsColumnWidth = 320.0f;
						const float maxPreviewColumnWidth = std::max(minPreviewColumnWidth, contentSize.x - minToolsColumnWidth - splitterWidth);

						if (editorState.previewColumnWidth <= 0.0f)
						{
							const float defaultToolsWidth = std::clamp(contentSize.x * 0.36f, 320.0f, 460.0f);
                            editorState.previewColumnWidth = std::max(minPreviewColumnWidth, contentSize.x - defaultToolsWidth - splitterWidth);
						}
                        editorState.previewColumnWidth = std::clamp(editorState.previewColumnWidth, minPreviewColumnWidth, maxPreviewColumnWidth);

						ImGui::BeginChild("##mesh_preview_column", ImVec2(editorState.previewColumnWidth, 0.0f), ImGuiChildFlags_None);
						ImGui::BeginChild("##mesh_preview_viewport", ImVec2(0.0f, 0.0f), ImGuiChildFlags_Borders);
						{
							const ImVec2 viewportSize = ImGui::GetContentRegionAvail();
							assetData.sceneData.viewportWidth = std::max(1u, static_cast<uint32_t>(viewportSize.x));
							assetData.sceneData.viewportHeight = std::max(1u, static_cast<uint32_t>(viewportSize.y));
							Ref<Texture> previewTexture = assetData.sceneData.compositeRT ? assetData.sceneData.compositeRT->GetColorAttachment(0) : nullptr;
							if (previewTexture && previewTexture->GetHandle())
							{
								ImGui::Image(reinterpret_cast<ImTextureID>(previewTexture->GetHandle().Get()), viewportSize, ImVec2(0.0f, 0.0f), ImVec2(1.0f, 1.0f));
								assetData.sceneData.viewportHovered = ImGui::IsItemHovered();
							}
							else
							{
								ImGui::Dummy(viewportSize);
								assetData.sceneData.viewportHovered = ImGui::IsItemHovered();
							}
						}
                        ImGui::EndChild();
                        ImGui::EndChild();

						UpdateSceneCamera(assetData.sceneData, ImGui::GetIO().DeltaTime);

						// Horizontal splitter
						ImGui::SameLine(0.0f, 0.0f);
						ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.20f, 0.20f, 0.20f, 1.0f));
						ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.28f, 0.28f, 0.28f, 1.0f));
						ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.32f, 0.32f, 0.32f, 1.0f));
						ImGui::BeginChild("##material_horizontal_splitter", ImVec2(splitterWidth, 0.0f), ImGuiChildFlags_None);
						ImGui::Button("##material_horizontal_splitter_btn", ImVec2(-1.0f, -1.0f));
						if (ImGui::IsItemHovered() || ImGui::IsItemActive())
						{
							ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeEW);
						}
						if (ImGui::IsItemActive())
						{
							editorState.previewColumnWidth += ImGui::GetIO().MouseDelta.x;
							editorState.previewColumnWidth = std::clamp(editorState.previewColumnWidth, minPreviewColumnWidth, maxPreviewColumnWidth);
						}
						ImGui::EndChild();
						ImGui::PopStyleColor(3);

                        ImGui::SameLine(0.0f, 0.0f);
                        ImGui::BeginChild("##mesh_controls_column", ImVec2(0.0f, 0.0f), ImGuiChildFlags_Borders, ImGuiWindowFlags_NoScrollbar);
						{
							DrawSceneDetails(assetData);

							// Asset details
							if (ImGui::TreeNodeEx("Static Mesh Details", ImGuiTreeNodeFlags_DefaultOpen))
							{
								const auto &meshInstances = mesh->GetMeshInstances();
								for (size_t i = 0; i < meshInstances.size(); ++i)
								{
									Ref<StaticMeshInstance> instance = meshInstances[i];
									if (!instance)
										continue;

									ImGui::PushID(static_cast<int>(i));
                                    const std::string meshInstanceName = std::format("{} (Submesh {})", instance->GetName(), i);
									
									if (DrawMeshInstanceMaterial(instance, meshInstanceName.c_str()))
										mesh->SetDirtyFlag(true);

                                    ImGui::Spacing();

									ImGui::PopID();
								}

								ImGui::TreePop();
							}

						    ImGui::EndChild();
						}
					}
					else
					{
						ImGui::Text("Invalid asset!");
					}
				}
				else
				{
					ImGui::Text("Loading asset...");
				}
			}
		}

		UIAssetEditorClosePopup(assetData, isOpen);
		ImGui::End();
		assetData.isOpen = isOpen;
		assetData.requestFocus = false;
	}

    void AssetEditorPanel::UISkeletalMeshEditor(AssetEditorData &assetData)
    {
        bool isOpen = assetData.isOpen;
        if (BeginAssetEditorWindow(assetData, isOpen, ImVec2(1080.0f, 480.0f), ImVec2(420.0f, 560.0f), 0))
        {
            if (DrawAssetEditorHeader(assetData))
            {
                if (assetData.asset && assetData.asset->IsReady())
                {
                    if (Ref<SkeletalMesh> mesh = assetData.asset->As<SkeletalMesh>())
                    {
                        auto project = m_EditorLayer->GetActiveProject().get();
                        auto assetManager = AssetManager::GetInstance();

                        const auto meshKey = static_cast<uint64_t>(mesh->handle);
                        MeshEditorState &meshEditorState = s_MeshEditorState[meshKey];
                        AssetHandle skeletonHandle = mesh->GetSkeletonHandle();
                        if (meshEditorState.skeletonHandle != skeletonHandle)
                        {
                            meshEditorState.skeletonHandle = skeletonHandle;
                            meshEditorState.cachedSkeleton.reset();
                        }

                        static std::unordered_map<uint64_t, int32_t> s_SelectedJoint;
                        static std::unordered_map<uint64_t, int32_t> s_SelectedSocket;
                        static std::unordered_map<uint64_t, int32_t> s_LastAutoScrolledJoint;

                        const auto key = static_cast<uint64_t>(skeletonHandle);
                        SkeletonPreviewEditorState &previewState = s_SkeletonPreviewEditorState[key];

                        int32_t &selectedJoint = s_SelectedJoint[key];
                        int32_t &selectedSocket = s_SelectedSocket[key];
                        int32_t &lastAutoScrolledJoint = s_LastAutoScrolledJoint[key];

                        if (skeletonHandle != AssetHandle(0) && !meshEditorState.cachedSkeleton)
                        {
                            meshEditorState.cachedSkeleton = assetManager->GetAsset<Skeleton>(skeletonHandle);
                        }

                        Ref<Skeleton> skeleton = meshEditorState.cachedSkeleton;
                        size_t jointCount = 0;
                        if (skeleton)
                        {
                            jointCount = skeleton->joints.size();
                            if (selectedJoint >= static_cast<int32_t>(skeleton->joints.size()))
                            {
                                selectedJoint = skeleton->joints.empty() ? -1 : 0;
                            }
                            if (selectedJoint < 0)
                            {
                                lastAutoScrolledJoint = -1;
                            }
                            if (selectedSocket >= static_cast<int32_t>(skeleton->sockets.size()))
                            {
                                selectedSocket = skeleton->sockets.empty() ? -1 : 0;
                            }
                        }

                        std::vector<std::vector<int32_t>> children(jointCount);
                        if (skeleton)
                        {
                            for (const Joint &joint : skeleton->joints)
                            {
                                if (joint.parentJointId >= 0 && joint.parentJointId < static_cast<int32_t>(jointCount))
                                {
                                    children[static_cast<size_t>(joint.parentJointId)].push_back(joint.id);
                                }
                            }
                        }

                        bool isViewSkeletonTree = false;
                        const bool isAnimLoaded = previewState.previewAnimationHandle != AssetHandle(0);
                        Ref<SkeletalAnimation> previewAnimation = nullptr;
                        if (previewState.previewAnimationHandle != AssetHandle(0))
                        {
                            previewAnimation = assetManager->GetAsset<SkeletalAnimation>(previewState.previewAnimationHandle);
                        }

                        const float timelineFps = previewAnimation ? std::max(previewAnimation->ticksPerSeconds, 0.001f) : 0.001f;
                        const float timelineTotalDuration = (previewAnimation && previewAnimation->duration > 0.0f)
                            ? (previewAnimation->duration / timelineFps)
                            : 0.0f;
                        const int timelineFrameCount = (previewAnimation && previewAnimation->duration > 0.0f)
                            ? std::max(1, static_cast<int>(std::ceil(previewAnimation->duration)))
                            : 0;

                        const ImVec2 contentSize = ImGui::GetContentRegionAvail();
                        const float splitterWidth = 6.0f;
                        const float minPreviewColumnWidth = 260.0f;
                        const float minToolsColumnWidth = 320.0f;
                        const float maxPreviewColumnWidth = std::max(minPreviewColumnWidth, contentSize.x - minToolsColumnWidth - splitterWidth);

                        if (meshEditorState.previewColumnWidth <= 0.0f)
                        {
                            const float defaultToolsWidth = std::clamp(contentSize.x * 0.36f, 320.0f, 460.0f);
                            meshEditorState.previewColumnWidth = std::max(minPreviewColumnWidth, contentSize.x - defaultToolsWidth - splitterWidth);
                        }
                        meshEditorState.previewColumnWidth = std::clamp(meshEditorState.previewColumnWidth, minPreviewColumnWidth, maxPreviewColumnWidth);

                        // Viewport Column
                        ImGui::BeginChild("##mesh_preview_column", ImVec2(meshEditorState.previewColumnWidth, 0.0f), ImGuiChildFlags_None);
                        {
                            const float splitterThickness = 6.0f;
                            const float minViewportHeight = 120.0f;
                            const float minTimelineHeight = 120.0f;
                            const ImVec2 previewRegion = ImGui::GetContentRegionAvail();
                            float maxTimelineHeight = minTimelineHeight;
                            float meshScenePreviewHeight = previewRegion.y;
                            if (skeleton)
                            {
                                maxTimelineHeight = std::max(minTimelineHeight, previewRegion.y - minViewportHeight - splitterThickness);
                                previewState.animationTimelineHeight = std::clamp(previewState.animationTimelineHeight, minTimelineHeight, maxTimelineHeight);
                                meshScenePreviewHeight = std::max(minViewportHeight, previewRegion.y - previewState.animationTimelineHeight - splitterThickness);
                            }

                            // Scene viewport child
                            if (ImGui::BeginChild("##mesh_scene_preview", { 0.0f, meshScenePreviewHeight }, ImGuiChildFlags_None))
                            {
                                const ImVec2 viewportSize = ImGui::GetContentRegionAvail();
                                assetData.sceneData.viewportWidth = std::max(1u, static_cast<uint32_t>(viewportSize.x));
                                assetData.sceneData.viewportHeight = std::max(1u, static_cast<uint32_t>(viewportSize.y));
                                UpdateSceneCamera(assetData.sceneData, ImGui::GetIO().DeltaTime);

                                Ref<Texture> previewTexture = assetData.sceneData.compositeRT ? assetData.sceneData.compositeRT->GetColorAttachment(0) : nullptr;
                                if (previewTexture && previewTexture->GetHandle())
                                {
                                    const ImVec2 viewportPos = ImGui::GetCursorScreenPos();
                                    ImGui::Image(reinterpret_cast<ImTextureID>(previewTexture->GetHandle().Get()), viewportSize, ImVec2(0.0f, 0.0f), ImVec2(1.0f, 1.0f));
                                    assetData.sceneData.viewportHovered = ImGui::IsItemHovered();

                                    const glm::mat4 viewProjection = assetData.sceneData.camera.GetProjection() * assetData.sceneData.camera.GetView();
                                    const Rect viewportRect { viewportPos.x, viewportPos.y, viewportPos.x + viewportSize.x, viewportPos.y + viewportSize.y };

                                    if (skeleton)
                                    {
                                        const bool hasSelectedJoint = selectedJoint >= 0 && selectedJoint < static_cast<int32_t>(skeleton->joints.size());
                                        const bool hasSelectedSocket = selectedSocket >= 0 && selectedSocket < static_cast<int32_t>(skeleton->sockets.size());
                                        const bool useSocketGizmo = previewState.gizmoTarget == 1 && hasSelectedSocket;
                                        const bool previewViewportFocused = assetData.sceneData.viewportHovered && ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows);

                                        const bool hasPreviewPose = previewState.previewGlobalTransforms.size() == skeleton->joints.size();

                                        auto getJointGlobal = [&](int32_t jointId) -> const glm::mat4 &
                                        {
                                            if (hasPreviewPose && jointId >= 0 && jointId < static_cast<int32_t>(previewState.previewGlobalTransforms.size()))
                                            {
                                                return previewState.previewGlobalTransforms[static_cast<size_t>(jointId)];
                                            }
                                            return skeleton->joints[static_cast<size_t>(jointId)].globalTransform;
                                        };

                                        bool isPreviewGizmoManipulating = false;
                                        bool isPreviewGizmoHovered = false;

                                        if (previewViewportFocused && (useSocketGizmo || hasSelectedJoint))
                                        {
                                            GizmoInfo gizmoInfo;
                                            gizmoInfo.cameraView = assetData.sceneData.camera.GetView();
                                            gizmoInfo.cameraProjection = assetData.sceneData.camera.GetProjection();
                                            gizmoInfo.cameraType = assetData.sceneData.camera.projectionType;
                                            gizmoInfo.snapValue = 0.05f;
                                            gizmoInfo.isSnapping = false;
                                            gizmoInfo.viewRect = viewportRect;

                                            s_SkeletonPreviewGizmo.SetInfo(gizmoInfo);
                                            s_SkeletonPreviewGizmo.SetOperation(ImGuizmo::OPERATION::TRANSLATE);
                                            s_SkeletonPreviewGizmo.SetMode(ImGuizmo::MODE::LOCAL);

                                            glm::mat4 gizmoTransform = glm::mat4(1.0f);
                                            if (useSocketGizmo)
                                            {
                                                const JointSocket &socket = skeleton->sockets[static_cast<size_t>(selectedSocket)];
                                                const glm::mat4 socketLocal = socket.local.GetMatrix();
                                                if (socket.parentJointId >= 0 && socket.parentJointId < static_cast<int32_t>(skeleton->joints.size()))
                                                {
                                                    gizmoTransform = getJointGlobal(socket.parentJointId) * socketLocal;
                                                }
                                                else
                                                {
                                                    gizmoTransform = socketLocal;
                                                }
                                            }
                                            else
                                            {
                                                gizmoTransform = getJointGlobal(selectedJoint);
                                            }

                                            s_SkeletonPreviewGizmo.Manipulate(gizmoTransform);
                                            isPreviewGizmoManipulating = s_SkeletonPreviewGizmo.IsManipulating();
                                            isPreviewGizmoHovered = s_SkeletonPreviewGizmo.IsHovered();

                                            if (isPreviewGizmoManipulating)
                                            {
                                                if (useSocketGizmo)
                                                {
                                                    JointSocket &socket = skeleton->sockets[static_cast<size_t>(selectedSocket)];
                                                    glm::mat4 parentWorld = glm::mat4(1.0f);
                                                    if (socket.parentJointId >= 0 && socket.parentJointId < static_cast<int32_t>(skeleton->joints.size()))
                                                    {
                                                        parentWorld = getJointGlobal(socket.parentJointId);
                                                    }

                                                    const glm::mat4 localMatrix = glm::inverse(parentWorld) * gizmoTransform;
                                                    glm::vec3 localTranslation, localEuler, localScale;
                                                    Math::DecomposeTransformEuler(localMatrix, localTranslation, localEuler, localScale);
                                                    socket.local.translation = localTranslation;
                                                    socket.local.rotation = glm::quat(localEuler);
                                                    socket.local.scale = localScale;
                                                }
                                                else if (hasSelectedJoint)
                                                {
                                                    Joint &joint = skeleton->joints[static_cast<size_t>(selectedJoint)];
                                                    glm::mat4 parentWorld = glm::mat4(1.0f);
                                                    if (joint.parentJointId >= 0 && joint.parentJointId < static_cast<int32_t>(skeleton->joints.size()))
                                                    {
                                                        parentWorld = getJointGlobal(joint.parentJointId);
                                                    }

                                                    const glm::mat4 localMatrix = glm::inverse(parentWorld) * gizmoTransform;
                                                    glm::vec3 localTranslation, localEuler, localScale;
                                                    Math::DecomposeTransformEuler(localMatrix, localTranslation, localEuler, localScale);
                                                    joint.defaultTransform.translation = localTranslation;
                                                    joint.defaultTransform.rotation = glm::quat(localEuler);
                                                    joint.defaultTransform.scale = localScale;
                                                    joint.localTransform = joint.defaultTransform.GetMatrix();
                                                }

                                                skeleton->SetDirtyFlag(true);
                                            }
                                        }

                                        if (assetData.sceneData.viewportHovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left) && !isPreviewGizmoManipulating && !isPreviewGizmoHovered)
                                        {
                                            const ImVec2 mousePos = ImGui::GetMousePos();
                                            int32_t pickedJoint = -1;
                                            float bestDistanceSq = 64.0f;

                                            for (const Joint &joint : skeleton->joints)
                                            {
                                                ImVec2 jointPos;
                                                if (!Math::ProjectWorldToScreen(glm::vec3(getJointGlobal(joint.id)[3]), viewProjection, viewportRect, jointPos))
                                                {
                                                    continue;
                                                }

                                                const float dx = jointPos.x - mousePos.x;
                                                const float dy = jointPos.y - mousePos.y;
                                                const float distanceSq = dx * dx + dy * dy;
                                                if (distanceSq < bestDistanceSq)
                                                {
                                                    bestDistanceSq = distanceSq;
                                                    pickedJoint = joint.id;
                                                }
                                            }

                                            if (pickedJoint >= 0)
                                            {
                                                selectedJoint = pickedJoint;
                                            }
                                        }

                                        ImDrawList *drawList = ImGui::GetWindowDrawList();
                                        for (const Joint &joint : skeleton->joints)
                                        {
                                            if (joint.parentJointId < 0 || joint.parentJointId >= static_cast<int32_t>(skeleton->joints.size()))
                                            {
                                                continue;
                                            }

                                            const Joint &parent = skeleton->joints[static_cast<size_t>(joint.parentJointId)];

                                            ImVec2 childPos, parentPos;
                                            if (Math::ProjectWorldToScreen(glm::vec3(getJointGlobal(joint.id)[3]), viewProjection, viewportRect, childPos)
                                                && Math::ProjectWorldToScreen(glm::vec3(getJointGlobal(parent.id)[3]), viewProjection, viewportRect, parentPos))
                                            {
                                                const bool selectedLink = (joint.id == selectedJoint || parent.id == selectedJoint);
                                                drawList->AddLine(parentPos, childPos, selectedLink ? IM_COL32(255, 180, 30, 255) : IM_COL32(50, 220, 255, 190), selectedLink ? 2.5f : 1.5f);
                                            }
                                        }

                                        for (const Joint &joint : skeleton->joints)
                                        {
                                            ImVec2 jointPos;
                                            if (Math::ProjectWorldToScreen(glm::vec3(getJointGlobal(joint.id)[3]), viewProjection, viewportRect, jointPos))
                                            {
                                                const bool selected = joint.id == selectedJoint;
                                                drawList->AddCircleFilled(jointPos, selected ? 5.0f : 3.0f, selected ? IM_COL32(255, 120, 20, 255) : IM_COL32(255, 255, 255, 230));
                                            }
                                        }
                                    }
                                }
                                else
                                {
                                    ImGui::Dummy(viewportSize);
                                    assetData.sceneData.viewportHovered = ImGui::IsItemHovered();
                                }
                            }
                            ImGui::EndChild(); // !scene viewport child

                            if (skeleton)
                            {
                                // Timeline Splitter
                                {
                                    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.18f, 0.18f, 0.20f, 1.0f));
                                    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.26f, 0.26f, 0.30f, 1.0f));
                                    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.30f, 0.30f, 0.36f, 1.0f));
                                    ImGui::BeginChild("##animation_timeline_splitter", { 0.0f, splitterThickness });
                                    ImGui::Button("##animation_timeline_splitter_btn", ImVec2(-1.0f, -1.0f));
                                    if (ImGui::IsItemHovered() || ImGui::IsItemActive())
                                    {
                                        ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeNS);
                                    }
                                    if (ImGui::IsItemActive())
                                    {
                                        previewState.animationTimelineHeight -= ImGui::GetIO().MouseDelta.y;
                                        previewState.animationTimelineHeight = std::clamp(previewState.animationTimelineHeight, minTimelineHeight, maxTimelineHeight);
                                    }
                                    ImGui::PopStyleColor(3);
                                    ImGui::EndChild();
                                }

                                if (ImGui::BeginChild("##animation_preview_timeline", { 0.0f, previewState.animationTimelineHeight }, ImGuiChildFlags_Borders, ImGuiWindowFlags_NoScrollbar))
                                {
                                    // Animation playback tools
                                    if (ImGui::BeginChild("##animation_timeline_toolbar", { 0.0f, 36.0f }))
                                    {
                                        ImGui::BeginDisabled(!isAnimLoaded);
                                        if (ImGui::Button(previewState.playing ? "Pause" : "Play"))
                                        {
                                            previewState.playing = !previewState.playing;
                                        }
                                        ImGui::SameLine();
                                        if (ImGui::Button("Stop"))
                                        {
                                            previewState.playing = false;
                                            previewState.timeSeconds = 0.0f;
                                        }
                                        ImGui::SameLine();
                                        ImGui::Checkbox("Loop", &previewState.loop);
                                        ImGui::EndDisabled();
                                    }
                                    ImGui::EndChild(); // !Animation playback tools

                                    ImGui::Spacing();

                                    // Animation timeline
                                    if (ImGui::BeginChild("##animation_timeline", { 0.0f, 0.0f }))
                                    {
                                        const float timelineCurrentTime = previewState.timeSeconds;
                                        int previewFrame = (timelineFrameCount > 0)
                                            ? std::clamp(static_cast<int>(timelineCurrentTime * timelineFps), 0, timelineFrameCount - 1)
                                            : 0;
                                        bool playing = previewState.playing;
                                        UI::TimelineState timelineState { timelineFrameCount, timelineFps, timelineTotalDuration, timelineCurrentTime };
                                        UI::Timeline::Draw(ImGui::GetWindowDrawList(), 54.0f, timelineState, &previewState.timeSeconds, &previewFrame, &playing, isAnimLoaded ? "No frames – drop sprites here" : "Select animation");
                                        previewState.playing = playing;
                                    }
                                    ImGui::EndChild(); // !Animation timeline
                                }
                                ImGui::EndChild(); // !animation preview timeline
                            }
                        }
                        ImGui::EndChild(); // !mesh preview column

                        // Horizontal Splitter
                        ImGui::SameLine(0.0f, 0.0f);
                        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.20f, 0.20f, 0.20f, 1.0f));
                        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.28f, 0.28f, 0.28f, 1.0f));
                        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.32f, 0.32f, 0.32f, 1.0f));
                        ImGui::BeginChild("##mesh_horizontal_splitter", ImVec2(splitterWidth, 0.0f), ImGuiChildFlags_None);
                        ImGui::Button("##mesh_horizontal_splitter_btn", ImVec2(-1.0f, -1.0f));
                        if (ImGui::IsItemHovered() || ImGui::IsItemActive())
                        {
                            ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeEW);
                        }
                        if (ImGui::IsItemActive())
                        {
                            meshEditorState.previewColumnWidth += ImGui::GetIO().MouseDelta.x;
                            meshEditorState.previewColumnWidth = std::clamp(meshEditorState.previewColumnWidth, minPreviewColumnWidth, maxPreviewColumnWidth);
                        }
                        ImGui::EndChild();
                        ImGui::PopStyleColor(3);

                        ImGui::SameLine(0.0f, 0.0f);

                        // Asset Details Column (Right Column)
                        if (ImGui::BeginChild("##mesh_controls_column", ImVec2(0.0f, 0.0f), ImGuiChildFlags_Borders, ImGuiWindowFlags_NoScrollbar))
                        {
                            if (ImGui::BeginTabBar("##tab_bar_right_details"))
                            {
                                if (ImGui::BeginTabItem("Asset Details"))
                                {
                                    if (ImGui::BeginChild("##asset_details", { 0.0f, 0.0f }, ImGuiChildFlags_None))
                                    {
                                        std::string skeletonLabel = "Drop Skeleton Here";
                                        if (skeletonHandle != AssetHandle(0))
                                        {
                                            const AssetMetaData &metadata = assetManager->GetMetaData(skeletonHandle);
                                            if (metadata.type == AssetType::Skeleton)
                                            {
                                                skeletonLabel = metadata.filepath.filename().string();
                                            }
                                            else
                                            {
                                                skeletonLabel = "Skeleton Assigned";
                                            }
                                        }

                                        ImGui::Button(skeletonLabel.c_str(), ImVec2(-1.0f, 0.0f));
                                        if (ImGui::BeginDragDropTarget())
                                        {
                                            if (const ImGuiPayload *payload = ImGui::AcceptDragDropPayload(DND_PAYLOAD_CONTENT_BROWSER_ITEM))
                                            {
                                                if (payload->Data && payload->DataSize == sizeof(AssetHandle))
                                                {
                                                    const AssetHandle droppedHandle = *static_cast<const AssetHandle *>(payload->Data);
                                                    const AssetMetaData &metadata = assetManager->GetMetaData(droppedHandle);
                                                    if (metadata.type == AssetType::Skeleton)
                                                    {
                                                        mesh->SetSkeleton(droppedHandle);
                                                        mesh->SetDirtyFlag(true);
                                                        skeletonHandle = droppedHandle;
                                                        meshEditorState.skeletonHandle = droppedHandle;
                                                        meshEditorState.cachedSkeleton = assetManager->GetAsset<Skeleton>(droppedHandle);
                                                        skeleton = meshEditorState.cachedSkeleton;
                                                    }
                                                }
                                            }
                                            ImGui::EndDragDropTarget();
                                        }

                                        ImGui::SameLine();
                                        if (ImGui::Button("Clear Skeleton"))
                                        {
                                            mesh->SetSkeleton(AssetHandle(0));
                                            mesh->SetDirtyFlag(true);
                                            skeletonHandle = AssetHandle(0);
                                            meshEditorState.skeletonHandle = AssetHandle(0);
                                            meshEditorState.cachedSkeleton.reset();
                                            skeleton.reset();
                                        }

                                        ImGui::TextDisabled("Skeleton Handle: %llu", static_cast<unsigned long long>(static_cast<uint64_t>(skeletonHandle)));

                                        ImGui::SeparatorText("Details");
                                        const auto &meshInstances = mesh->GetMeshInstances();
                                        if (meshInstances.empty())
                                        {
                                            ImGui::TextDisabled("No mesh instances available.");
                                        }
                                        else
                                        {
                                            for (size_t i = 0; i < meshInstances.size(); ++i)
                                            {
                                                Ref<SkeletalMeshInstance> instance = meshInstances[i];
                                                if (!instance)
                                                    continue;

                                                ImGui::PushID(static_cast<int>(i));
                                                const std::string meshInstanceName = std::format("{} (Submesh {})", instance->GetName(), i);

                                                if (DrawMeshInstanceMaterial(instance, meshInstanceName.c_str()))
													mesh->SetDirtyFlag(true);

                                                ImGui::Spacing();

                                                ImGui::PopID();
                                            }
                                        }
                                    }
                                    ImGui::EndChild();
                                    ImGui::EndTabItem();
                                }

                                if (skeleton && ImGui::BeginTabItem("Skeleton Preview"))
                                {
                                    if (ImGui::BeginChild("##skeleton_preview", { 0.0f, 0.0f }, ImGuiChildFlags_None))
                                    {
                                        ImGui::TextUnformatted("Skeleton Tree");
                                        std::function<void(int32_t)> drawJointNode = [&](int32_t jointId)
                                        {
                                            if (jointId < 0 || jointId >= static_cast<int32_t>(jointCount))
                                            {
                                                return;
                                            }

                                            const Joint &joint = skeleton->joints[static_cast<size_t>(jointId)];
                                            const bool isSelected = selectedJoint == jointId;
                                            const bool hasChildren = !children[static_cast<size_t>(jointId)].empty();

                                            ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_SpanAvailWidth | ImGuiTreeNodeFlags_DefaultOpen;
                                            if (!hasChildren)
                                            {
                                                flags |= ImGuiTreeNodeFlags_Leaf;
                                            }
                                            if (isSelected)
                                            {
                                                flags |= ImGuiTreeNodeFlags_Selected;
                                            }

                                            const bool opened = ImGui::TreeNodeEx(reinterpret_cast<void *>(static_cast<intptr_t>(jointId + 1)), flags, "%s", joint.name.c_str());
                                            if (ImGui::IsItemClicked())
                                            {
                                                selectedJoint = jointId;
                                            }

                                            if (selectedJoint == jointId && lastAutoScrolledJoint != selectedJoint)
                                            {
                                                if (!ImGui::IsItemVisible())
                                                {
                                                    ImGui::SetScrollHereY(0.5f);
                                                }
                                                lastAutoScrolledJoint = selectedJoint;
                                            }

                                            if (opened)
                                            {
                                                for (int32_t childJointId : children[static_cast<size_t>(jointId)])
                                                {
                                                    drawJointNode(childJointId);
                                                }
                                                ImGui::TreePop();
                                            }
                                        };

                                        for (const Joint &joint : skeleton->joints)
                                        {
                                            if (joint.parentJointId == -1)
                                            {
                                                drawJointNode(joint.id);
                                            }
                                        }
                                    }
                                    ImGui::EndChild();
                                    ImGui::EndTabItem();
                                }

                                if (skeleton && ImGui::BeginTabItem("Skeleton Details"))
                                {
                                    if (ImGui::BeginChild("##details_preview", { 0.0f, 0.0f }, ImGuiChildFlags_None))
                                    {
                                        ImGui::TextUnformatted("Joint / Socket Details");
                                        ImGui::Separator();

                                        const char *gizmoTargets[] = { "Joint", "Socket" };
                                        if (ImGui::Combo("Gizmo Target", &previewState.gizmoTarget, gizmoTargets, IM_ARRAYSIZE(gizmoTargets)))
                                        {
                                            if (previewState.gizmoTarget == 1 && (selectedSocket < 0 || selectedSocket >= static_cast<int32_t>(skeleton->sockets.size())))
                                            {
                                                previewState.gizmoTarget = 0;
                                            }
                                        }

                                        if (selectedJoint >= 0 && selectedJoint < static_cast<int32_t>(skeleton->joints.size()))
                                        {
                                            const Joint &joint = skeleton->joints[static_cast<size_t>(selectedJoint)];
                                            ImGui::TextDisabled("Selected Joint: %s (id: %d)", joint.name.c_str(), joint.id);
                                        }

                                        if (previewState.gizmoTarget == 1)
                                        {
                                            if (selectedSocket >= 0 && selectedSocket < static_cast<int32_t>(skeleton->sockets.size()))
                                            {
                                                ImGui::TextDisabled("Selected Socket: %s", skeleton->sockets[static_cast<size_t>(selectedSocket)].name.c_str());
                                            }
                                            else
                                            {
                                                ImGui::TextDisabled("Selected Socket: <none>");
                                            }
                                        }

                                        ImGui::SeparatorText("Joint Sockets");

                                        if (ImGui::Button("Add Socket") && selectedJoint >= 0)
                                        {
                                            JointSocket socket;
                                            socket.name = std::format("Socket_{}", skeleton->sockets.size());
                                            socket.parentJointId = selectedJoint;
                                            skeleton->sockets.push_back(std::move(socket));
                                            skeleton->RebuildSocketMap();
                                            selectedSocket = static_cast<int32_t>(skeleton->sockets.size()) - 1;
                                            skeleton->SetDirtyFlag(true);
                                        }

                                        if (ImGui::BeginChild("##skeleton_socket_list", ImVec2(0.0f, 180.0f), ImGuiChildFlags_Borders))
                                        {
                                            for (int32_t i = 0; i < static_cast<int32_t>(skeleton->sockets.size()); ++i)
                                            {
                                                const JointSocket &socket = skeleton->sockets[static_cast<size_t>(i)];
                                                std::string row = socket.name;
                                                if (socket.parentJointId >= 0 && socket.parentJointId < static_cast<int32_t>(skeleton->joints.size()))
                                                {
                                                    row += std::format(" -> {}", skeleton->joints[static_cast<size_t>(socket.parentJointId)].name);
                                                }

                                                if (ImGui::Selectable(std::format("{}##socket_row_{}", row, i).c_str(), selectedSocket == i))
                                                {
                                                    selectedSocket = i;
                                                    if (socket.parentJointId >= 0 && socket.parentJointId < static_cast<int32_t>(skeleton->joints.size()))
                                                    {
                                                        selectedJoint = socket.parentJointId;
                                                    }
                                                }
                                            }
                                        }
                                        ImGui::EndChild();

                                        if (selectedSocket >= 0 && selectedSocket < static_cast<int32_t>(skeleton->sockets.size()))
                                        {
                                            JointSocket &socket = skeleton->sockets[static_cast<size_t>(selectedSocket)];

                                            char nameBuf[256] {};
                                            std::strncpy(nameBuf, socket.name.c_str(), sizeof(nameBuf) - 1);
                                            if (ImGui::InputText("Socket Name", nameBuf, sizeof(nameBuf)))
                                            {
                                                socket.name = nameBuf;
                                                skeleton->RebuildSocketMap();
                                                skeleton->SetDirtyFlag(true);
                                            }

                                            int parentJoint = socket.parentJointId;
                                            std::string parentJointLabel = "None";
                                            if (parentJoint >= 0 && parentJoint < static_cast<int32_t>(skeleton->joints.size()))
                                            {
                                                parentJointLabel = skeleton->joints[static_cast<size_t>(parentJoint)].name;
                                            }

                                            if (ImGui::BeginCombo("Parent Joint", parentJointLabel.c_str()))
                                            {
                                                for (const Joint &joint : skeleton->joints)
                                                {
                                                    const bool selected = parentJoint == joint.id;
                                                    if (ImGui::Selectable(joint.name.c_str(), selected))
                                                    {
                                                        socket.parentJointId = joint.id;
                                                        skeleton->SetDirtyFlag(true);
                                                    }
                                                }
                                                ImGui::EndCombo();
                                            }

                                            if (ImGui::DragFloat3("Socket Translation", &socket.local.translation.x, 0.01f))
                                            {
                                                skeleton->SetDirtyFlag(true);
                                            }

                                            glm::vec3 eulerDeg = glm::degrees(glm::eulerAngles(socket.local.rotation));
                                            if (ImGui::DragFloat3("Socket Rotation", &eulerDeg.x, 0.1f))
                                            {
                                                socket.local.rotation = glm::quat(glm::radians(eulerDeg));
                                                skeleton->SetDirtyFlag(true);
                                            }

                                            if (ImGui::DragFloat3("Socket Scale", &socket.local.scale.x, 0.01f, 0.001f, 1000.0f))
                                            {
                                                skeleton->SetDirtyFlag(true);
                                            }

                                            if (ImGui::Button("Remove Socket"))
                                            {
                                                skeleton->sockets.erase(skeleton->sockets.begin() + selectedSocket);
                                                skeleton->RebuildSocketMap();
                                                selectedSocket = skeleton->sockets.empty() ? -1 : std::min(selectedSocket, static_cast<int32_t>(skeleton->sockets.size()) - 1);
                                                skeleton->SetDirtyFlag(true);
                                            }
                                        }
                                    }
                                    ImGui::EndChild();
                                    ImGui::EndTabItem();
                                }

                                if (ImGui::BeginTabItem("Scene Preview"))
                                {
                                    if (ImGui::BeginChild("##scene_details_preview", { 0.0f, 0.0f }, ImGuiChildFlags_None))
                                    {
                                        DrawSceneDetails(assetData);

                                        if (skeleton)
                                        {
                                            std::string animLabel = isAnimLoaded ? assetManager->GetAssetDisplayName(previewState.previewAnimationHandle) : "Drag Here";
                                            UI::DrawButtonWithColumn("Preview Anim", animLabel.c_str(), nullptr, [&previewState, &assetManager]()
                                            {
                                                if (ImGui::BeginDragDropTarget())
                                                {
                                                    if (const ImGuiPayload *payload = ImGui::AcceptDragDropPayload(DND_PAYLOAD_CONTENT_BROWSER_ITEM))
                                                    {
                                                        if (payload->Data && payload->DataSize == sizeof(AssetHandle))
                                                        {
                                                            const AssetHandle droppedHandle = *static_cast<const AssetHandle *>(payload->Data);
                                                            const AssetMetaData &md = assetManager->GetMetaData(droppedHandle);
                                                            if (md.type == AssetType::SkeletalAnimation)
                                                            {
                                                                previewState.previewAnimationHandle = droppedHandle;
                                                                previewState.timeSeconds = 0.0f;
                                                                previewState.playing = true;
                                                            }
                                                        }
                                                    }
                                                    ImGui::EndDragDropTarget();
                                                }
                                            });
                                        }
                                    }
                                    ImGui::EndChild();
                                    ImGui::EndTabItem();
                                }
                                ImGui::EndTabBar();
                            }
                        }
                        ImGui::EndChild();
                    }
                    else
                    {
                        ImGui::Text("Invalid asset!");
                    }
                }
                else
                {
                    ImGui::Text("Loading asset...");
                }
            }
        }

        UIAssetEditorClosePopup(assetData, isOpen);
        ImGui::End();
        assetData.isOpen = isOpen;
        assetData.requestFocus = false;
    }

    void AssetEditorPanel::UISkeletonEditor(AssetEditorData &assetData)
    {
        bool isOpen = assetData.isOpen;
        if (BeginAssetEditorWindow(assetData, isOpen, ImVec2(1100.0f, 900.0f), ImVec2(560.0f, 620.0f), 0))
        {
            if (DrawAssetEditorHeader(assetData))
            {
                if (assetData.asset && assetData.asset->IsReady())
                {
                    if (Ref<Skeleton> skeleton = assetData.asset->As<Skeleton>())
                    {
                        auto project = m_EditorLayer->GetActiveProject().get();
                        auto assetManager = AssetManager::GetInstance();

                        const uint64_t key = static_cast<uint64_t>(skeleton->handle);
                        static std::unordered_map<uint64_t, int32_t> s_SelectedJoint;
                        static std::unordered_map<uint64_t, int32_t> s_SelectedSocket;
                        static std::unordered_map<uint64_t, int32_t> s_LastAutoScrolledJoint;
                        SkeletonPreviewEditorState &previewState = s_SkeletonPreviewEditorState[key];

                        if (previewState.previewMeshHandle == AssetHandle(0))
                        {
                            const AssetHandle matchedMeshHandle = FindFirstMeshHandleForSkeleton(skeleton->handle);
                            if (matchedMeshHandle != AssetHandle(0))
                            {
                                previewState.previewMeshHandle = matchedMeshHandle;
                                previewState.cachedPreviewMesh.reset();
                            }
                        }

                        int32_t &selectedJoint = s_SelectedJoint[key];
                        int32_t &selectedSocket = s_SelectedSocket[key];
                        int32_t &lastAutoScrolledJoint = s_LastAutoScrolledJoint[key];

                        if (selectedJoint >= static_cast<int32_t>(skeleton->joints.size()))
                        {
                            selectedJoint = skeleton->joints.empty() ? -1 : 0;
                        }
                        if (selectedJoint < 0)
                        {
                            lastAutoScrolledJoint = -1;
                        }
                        if (selectedSocket >= static_cast<int32_t>(skeleton->sockets.size()))
                        {
                            selectedSocket = skeleton->sockets.empty() ? -1 : 0;
                        }

                        ImGui::Text("Joints: %zu | Sockets: %zu", skeleton->joints.size(), skeleton->sockets.size());
                        ImGui::Separator();

                        std::vector<std::vector<int32_t>> children(skeleton->joints.size());
                        for (const Joint &joint : skeleton->joints)
                        {
                            if (joint.parentJointId >= 0 && joint.parentJointId < static_cast<int32_t>(skeleton->joints.size()))
                            {
                                children[static_cast<size_t>(joint.parentJointId)].push_back(joint.id);
                            }
                        }

                        const float totalWidth = ImGui::GetContentRegionAvail().x;
                        const float spacing = ImGui::GetStyle().ItemSpacing.x;
                        const float listWidth = std::max(260.0f, totalWidth * 0.28f);
                        const float detailsWidth = std::max(320.0f, totalWidth * 0.30f);
                        const float viewportWidth = std::max(220.0f, totalWidth - listWidth - detailsWidth - spacing * 2.0f);

                        if (ImGui::BeginChild("##skeleton_left_list", ImVec2(listWidth, 0.0f), ImGuiChildFlags_Borders | ImGuiChildFlags_ResizeX))
                        {
                            ImGui::TextUnformatted("Skeleton Tree");
                            ImGui::Separator();

                            std::function<void(int32_t)> drawJointNode = [&](int32_t jointId)
                            {
                                if (jointId < 0 || jointId >= static_cast<int32_t>(skeleton->joints.size()))
                                {
                                    return;
                                }

                                const Joint &joint = skeleton->joints[static_cast<size_t>(jointId)];
                                const bool isSelected = selectedJoint == jointId;
                                const bool hasChildren = !children[static_cast<size_t>(jointId)].empty();

                                ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_SpanAvailWidth | ImGuiTreeNodeFlags_DefaultOpen;
                                if (!hasChildren)
                                {
                                    flags |= ImGuiTreeNodeFlags_Leaf;
                                }
                                if (isSelected)
                                {
                                    flags |= ImGuiTreeNodeFlags_Selected;
                                }

                                const bool opened = ImGui::TreeNodeEx(reinterpret_cast<void *>(static_cast<intptr_t>(jointId + 1)), flags, "%s", joint.name.c_str());
                                if (ImGui::IsItemClicked())
                                {
                                    selectedJoint = jointId;
                                }

                                if (selectedJoint == jointId && lastAutoScrolledJoint != selectedJoint)
                                {
                                    if (!ImGui::IsItemVisible())
                                    {
                                        ImGui::SetScrollHereY(0.5f);
                                    }

                                    lastAutoScrolledJoint = selectedJoint;
                                }

                                if (opened)
                                {
                                    for (int32_t childJointId : children[static_cast<size_t>(jointId)])
                                    {
                                        drawJointNode(childJointId);
                                    }
                                    ImGui::TreePop();
                                }
                            };

                            for (const Joint &joint : skeleton->joints)
                            {
                                if (joint.parentJointId == -1)
                                {
                                    drawJointNode(joint.id);
                                }
                            }
                        }
                        ImGui::EndChild();

                        ImGui::SameLine();

                        if (ImGui::BeginChild("##skeleton_preview_view", ImVec2(viewportWidth, 0.0f), ImGuiChildFlags_Borders | ImGuiChildFlags_ResizeX))
                        {
                            ImGui::TextUnformatted("Mesh + Skeleton Preview");
                            ImGui::Separator();

                            const ImVec2 viewportSize = ImGui::GetContentRegionAvail();
                            assetData.sceneData.viewportWidth = std::max(1u, static_cast<uint32_t>(viewportSize.x));
                            assetData.sceneData.viewportHeight = std::max(1u, static_cast<uint32_t>(viewportSize.y));
                            UpdateSceneCamera(assetData.sceneData, ImGui::GetIO().DeltaTime);

                            Ref<Texture> previewTexture = assetData.sceneData.compositeRT ? assetData.sceneData.compositeRT->GetColorAttachment(0) : nullptr;
                            if (previewTexture && previewTexture->GetHandle())
                            {
                                const ImVec2 viewportPos = ImGui::GetCursorScreenPos();
                                ImGui::Image(reinterpret_cast<ImTextureID>(previewTexture->GetHandle().Get()), viewportSize, ImVec2(0.0f, 0.0f), ImVec2(1.0f, 1.0f));
                                assetData.sceneData.viewportHovered = ImGui::IsItemHovered();

                                ImGui::SetCursorScreenPos(ImVec2(viewportPos.x + 8.0f, viewportPos.y + 8.0f));
                                ImGui::TextDisabled(
                                    "RTTex=%p | RT=%p | %ux%u",
                                    reinterpret_cast<ImTextureID>(previewTexture->GetHandle().Get()),
                                    assetData.sceneData.compositeRT.get(),
                                    assetData.sceneData.viewportWidth,
                                    assetData.sceneData.viewportHeight);

                                const glm::mat4 viewProjection = assetData.sceneData.camera.GetProjection() * assetData.sceneData.camera.GetView();
                                const Rect viewportRect { viewportPos.x, viewportPos.y, viewportPos.x + viewportSize.x, viewportPos.y + viewportSize.y };
                                const bool hasPreviewPose = previewState.previewGlobalTransforms.size() == skeleton->joints.size();

                                auto getJointGlobal = [&](int32_t jointId) -> const glm::mat4 &
                                {
                                    if (hasPreviewPose && jointId >= 0 && jointId < static_cast<int32_t>(previewState.previewGlobalTransforms.size()))
                                    {
                                        return previewState.previewGlobalTransforms[static_cast<size_t>(jointId)];
                                    }
                                    return skeleton->joints[static_cast<size_t>(jointId)].globalTransform;
                                };

                                const bool hasSelectedJoint = selectedJoint >= 0 && selectedJoint < static_cast<int32_t>(skeleton->joints.size());
                                const bool hasSelectedSocket = selectedSocket >= 0 && selectedSocket < static_cast<int32_t>(skeleton->sockets.size());
                                const bool useSocketGizmo = previewState.gizmoTarget == 1 && hasSelectedSocket;
                                const bool previewViewportFocused = assetData.sceneData.viewportHovered && ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows);

                                bool isPreviewGizmoManipulating = false;
                                bool isPreviewGizmoHovered = false;

                                if (previewViewportFocused && (useSocketGizmo || hasSelectedJoint))
                                {
                                    GizmoInfo gizmoInfo;
                                    gizmoInfo.cameraView = assetData.sceneData.camera.GetView();
                                    gizmoInfo.cameraProjection = assetData.sceneData.camera.GetProjection();
                                    gizmoInfo.cameraType = assetData.sceneData.camera.projectionType;
                                    gizmoInfo.snapValue = 0.05f;
                                    gizmoInfo.isSnapping = false;
                                    gizmoInfo.viewRect = viewportRect;

                                    s_SkeletonPreviewGizmo.SetInfo(gizmoInfo);
                                    s_SkeletonPreviewGizmo.SetOperation(ImGuizmo::OPERATION::TRANSLATE);
                                    s_SkeletonPreviewGizmo.SetMode(ImGuizmo::MODE::LOCAL);

                                    glm::mat4 gizmoTransform = glm::mat4(1.0f);
                                    if (useSocketGizmo)
                                    {
                                        const JointSocket &socket = skeleton->sockets[static_cast<size_t>(selectedSocket)];
                                        const glm::mat4 socketLocal = socket.local.GetMatrix();
                                        if (socket.parentJointId >= 0 && socket.parentJointId < static_cast<int32_t>(skeleton->joints.size()))
                                        {
                                            gizmoTransform = getJointGlobal(socket.parentJointId) * socketLocal;
                                        }
                                        else
                                        {
                                            gizmoTransform = socketLocal;
                                        }
                                    }
                                    else
                                    {
                                        gizmoTransform = getJointGlobal(selectedJoint);
                                    }

                                    s_SkeletonPreviewGizmo.Manipulate(gizmoTransform);
                                    isPreviewGizmoManipulating = s_SkeletonPreviewGizmo.IsManipulating();
                                    isPreviewGizmoHovered = s_SkeletonPreviewGizmo.IsHovered();

                                    if (isPreviewGizmoManipulating)
                                    {
                                        if (useSocketGizmo)
                                        {
                                            JointSocket &socket = skeleton->sockets[static_cast<size_t>(selectedSocket)];
                                            glm::mat4 parentWorld = glm::mat4(1.0f);
                                            if (socket.parentJointId >= 0 && socket.parentJointId < static_cast<int32_t>(skeleton->joints.size()))
                                            {
                                                parentWorld = getJointGlobal(socket.parentJointId);
                                            }

                                            const glm::mat4 localMatrix = glm::inverse(parentWorld) * gizmoTransform;
                                            glm::vec3 localTranslation, localEuler, localScale;
                                            Math::DecomposeTransformEuler(localMatrix, localTranslation, localEuler, localScale);
                                            socket.local = Transform(localTranslation, glm::quat(localEuler), localScale);
                                        }
                                        else if (hasSelectedJoint)
                                        {
                                            Joint &joint = skeleton->joints[static_cast<size_t>(selectedJoint)];
                                            glm::mat4 parentWorld = glm::mat4(1.0f);
                                            if (joint.parentJointId >= 0 && joint.parentJointId < static_cast<int32_t>(skeleton->joints.size()))
                                            {
                                                parentWorld = getJointGlobal(joint.parentJointId);
                                            }

                                            const glm::mat4 localMatrix = glm::inverse(parentWorld) * gizmoTransform;
                                            glm::vec3 localTranslation, localEuler, localScale;
                                            Math::DecomposeTransformEuler(localMatrix, localTranslation, localEuler, localScale);
                                            joint.defaultTransform = Transform(localTranslation, glm::quat(localEuler), localScale);
                                            joint.localTransform = joint.defaultTransform.GetMatrix();
                                        }

                                        skeleton->SetDirtyFlag(true);
                                    }
                                }

                                if (assetData.sceneData.viewportHovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left)
                                    && !isPreviewGizmoManipulating && !isPreviewGizmoHovered)
                                {
                                    const ImVec2 mousePos = ImGui::GetMousePos();
                                    int32_t pickedJoint = -1;
                                    float bestDistanceSq = 64.0f;

                                    for (const Joint &joint : skeleton->joints)
                                    {
                                        ImVec2 jointPos;
                                        if (!Math::ProjectWorldToScreen(glm::vec3(getJointGlobal(joint.id)[3]), viewProjection, viewportRect, jointPos))
                                        {
                                            continue;
                                        }

                                        const float dx = jointPos.x - mousePos.x;
                                        const float dy = jointPos.y - mousePos.y;
                                        const float distanceSq = dx * dx + dy * dy;
                                        if (distanceSq < bestDistanceSq)
                                        {
                                            bestDistanceSq = distanceSq;
                                            pickedJoint = joint.id;
                                        }
                                    }

                                    if (pickedJoint >= 0)
                                    {
                                        selectedJoint = pickedJoint;
                                    }
                                }

                                ImDrawList *drawList = ImGui::GetWindowDrawList();
                                for (const Joint &joint : skeleton->joints)
                                {
                                    if (joint.parentJointId < 0 || joint.parentJointId >= static_cast<int32_t>(skeleton->joints.size()))
                                    {
                                        continue;
                                    }

                                    const Joint &parent = skeleton->joints[static_cast<size_t>(joint.parentJointId)];

                                    ImVec2 childPos, parentPos;
                                    if (Math::ProjectWorldToScreen(glm::vec3(getJointGlobal(joint.id)[3]), viewProjection, viewportRect, childPos)
                                        && Math::ProjectWorldToScreen(glm::vec3(getJointGlobal(parent.id)[3]), viewProjection, viewportRect, parentPos))
                                    {
                                        const bool selectedLink = (joint.id == selectedJoint || parent.id == selectedJoint);
                                        drawList->AddLine(parentPos, childPos, selectedLink ? IM_COL32(255, 180, 30, 255) : IM_COL32(50, 220, 255, 190), selectedLink ? 2.5f : 1.5f);
                                    }
                                }

                                for (const Joint &joint : skeleton->joints)
                                {
                                    ImVec2 jointPos;
                                    if (Math::ProjectWorldToScreen(glm::vec3(getJointGlobal(joint.id)[3]), viewProjection, viewportRect, jointPos))
                                    {
                                        const bool selected = joint.id == selectedJoint;
                                        drawList->AddCircleFilled(jointPos, selected ? 5.0f : 3.0f, selected ? IM_COL32(255, 120, 20, 255) : IM_COL32(255, 255, 255, 230));
                                    }
                                }
                            }
                            else
                            {
                                ImGui::Dummy(viewportSize);
                                assetData.sceneData.viewportHovered = ImGui::IsItemHovered();
                            }
                        }
                        ImGui::EndChild();

                        ImGui::SameLine();

                        if (ImGui::BeginChild("##skeleton_right_details", ImVec2(0.0f, 0.0f), ImGuiChildFlags_Borders))
                        {
                            ImGui::TextUnformatted("Joint / Socket Details");
                            ImGui::Separator();

                            const char *gizmoTargets[] = { "Joint", "Socket" };
                            if (ImGui::Combo("Gizmo Target", &previewState.gizmoTarget, gizmoTargets, IM_ARRAYSIZE(gizmoTargets)))
                            {
                                if (previewState.gizmoTarget == 1 && (selectedSocket < 0 || selectedSocket >= static_cast<int32_t>(skeleton->sockets.size())))
                                {
                                    previewState.gizmoTarget = 0;
                                }
                            }

                            if (selectedJoint >= 0 && selectedJoint < static_cast<int32_t>(skeleton->joints.size()))
                            {
                                const Joint &joint = skeleton->joints[static_cast<size_t>(selectedJoint)];
                                ImGui::TextDisabled("Selected Joint: %s (id: %d)", joint.name.c_str(), joint.id);
                            }

                            if (previewState.gizmoTarget == 1)
                            {
                                if (selectedSocket >= 0 && selectedSocket < static_cast<int32_t>(skeleton->sockets.size()))
                                {
                                    ImGui::TextDisabled("Selected Socket: %s", skeleton->sockets[static_cast<size_t>(selectedSocket)].name.c_str());
                                }
                                else
                                {
                                    ImGui::TextDisabled("Selected Socket: <none>");
                                }
                            }

                            ImGui::SeparatorText("Joint Sockets");

                            ImGui::Button("Drop Preview Mesh (.mesh)", ImVec2(-1.0f, 0.0f));
                            if (ImGui::BeginDragDropTarget())
                            {
                                if (const ImGuiPayload *payload = ImGui::AcceptDragDropPayload(DND_PAYLOAD_CONTENT_BROWSER_ITEM))
                                {
                                    if (payload->Data && payload->DataSize == sizeof(AssetHandle))
                                    {
                                        const AssetHandle droppedHandle = *static_cast<const AssetHandle *>(payload->Data);
                                        const AssetMetaData &md = assetManager->GetMetaData(droppedHandle);
                                        if (md.type == AssetType::Mesh || md.type == AssetType::SkeletalMesh)
                                        {
                                            previewState.previewMeshHandle = droppedHandle;
                                            previewState.cachedPreviewMesh.reset();
                                        }
                                    }
                                }
                                ImGui::EndDragDropTarget();
                            }
                            ImGui::TextDisabled("Preview Mesh Handle: %llu", static_cast<uint64_t>(previewState.previewMeshHandle));
                            if (ImGui::SmallButton("Clear Preview Mesh"))
                            {
                                previewState.previewMeshHandle = AssetHandle(0);
                                previewState.cachedPreviewMesh.reset();
                            }

                            ImGui::Separator();
                            ImGui::Button("Drop Preview Animation (.ixanim)", ImVec2(-1.0f, 0.0f));
                            if (ImGui::BeginDragDropTarget())
                            {
                                if (const ImGuiPayload *payload = ImGui::AcceptDragDropPayload(DND_PAYLOAD_CONTENT_BROWSER_ITEM))
                                {
                                    if (payload->Data && payload->DataSize == sizeof(AssetHandle))
                                    {
                                        const AssetHandle droppedHandle = *static_cast<const AssetHandle *>(payload->Data);
                                        const AssetMetaData &md = assetManager->GetMetaData(droppedHandle);
                                        if (md.type == AssetType::SkeletalAnimation)
                                        {
                                            previewState.previewAnimationHandle = droppedHandle;
                                            previewState.timeSeconds = 0.0f;
                                            assetManager->AddAssetPin(droppedHandle, BuildAssetEditorPinOwnerTag(droppedHandle));
                                        }
                                    }
                                }
                                ImGui::EndDragDropTarget();
                            }
                            ImGui::TextDisabled("Preview Animation Handle: %llu", static_cast<unsigned long long>(static_cast<uint64_t>(previewState.previewAnimationHandle)));

                            if (ImGui::Button(previewState.playing ? "Pause" : "Play"))
                            {
                                previewState.playing = !previewState.playing;
                            }
                            ImGui::SameLine();
                            if (ImGui::Button("Stop"))
                            {
                                previewState.playing = false;
                                previewState.timeSeconds = 0.0f;
                            }
                            ImGui::SameLine();
                            ImGui::Checkbox("Loop", &previewState.loop);

                            if (ImGui::Button("Add Socket") && selectedJoint >= 0)
                            {
                                JointSocket socket;
                                socket.name = std::format("Socket_{}", skeleton->sockets.size());
                                socket.parentJointId = selectedJoint;
                                skeleton->sockets.push_back(std::move(socket));
                                skeleton->RebuildSocketMap();
                                selectedSocket = static_cast<int32_t>(skeleton->sockets.size()) - 1;
                                skeleton->SetDirtyFlag(true);
                            }

                            if (ImGui::BeginChild("##skeleton_socket_list", ImVec2(0.0f, 180.0f), ImGuiChildFlags_Borders))
                            {
                                for (int32_t i = 0; i < static_cast<int32_t>(skeleton->sockets.size()); ++i)
                                {
                                    const JointSocket &socket = skeleton->sockets[static_cast<size_t>(i)];
                                    std::string row = socket.name;
                                    if (socket.parentJointId >= 0 && socket.parentJointId < static_cast<int32_t>(skeleton->joints.size()))
                                    {
                                        row += std::format(" -> {}", skeleton->joints[static_cast<size_t>(socket.parentJointId)].name);
                                    }

                                    if (ImGui::Selectable(std::format("{}##socket_row_{}", row, i).c_str(), selectedSocket == i))
                                    {
                                        selectedSocket = i;
                                        if (socket.parentJointId >= 0 && socket.parentJointId < static_cast<int32_t>(skeleton->joints.size()))
                                        {
                                            selectedJoint = socket.parentJointId;
                                        }
                                    }
                                }
                            }
                            ImGui::EndChild();

                            if (selectedSocket >= 0 && selectedSocket < static_cast<int32_t>(skeleton->sockets.size()))
                            {
                                JointSocket &socket = skeleton->sockets[static_cast<size_t>(selectedSocket)];

                                char nameBuf[256] {};
                                std::strncpy(nameBuf, socket.name.c_str(), sizeof(nameBuf) - 1);
                                if (ImGui::InputText("Socket Name", nameBuf, sizeof(nameBuf)))
                                {
                                    socket.name = nameBuf;
                                    skeleton->RebuildSocketMap();
                                    skeleton->SetDirtyFlag(true);
                                }

                                int parentJoint = socket.parentJointId;
                                std::string parentJointLabel = "None";
                                if (parentJoint >= 0 && parentJoint < static_cast<int32_t>(skeleton->joints.size()))
                                {
                                    parentJointLabel = skeleton->joints[static_cast<size_t>(parentJoint)].name;
                                }

                                if (ImGui::BeginCombo("Parent Joint", parentJointLabel.c_str()))
                                {
                                    for (const Joint &joint : skeleton->joints)
                                    {
                                        const bool selected = parentJoint == joint.id;
                                        if (ImGui::Selectable(joint.name.c_str(), selected))
                                        {
                                            socket.parentJointId = joint.id;
                                            skeleton->SetDirtyFlag(true);
                                        }
                                    }
                                    ImGui::EndCombo();
                                }

                                if (ImGui::DragFloat3("Socket Translation", &socket.local.translation.x, 0.01f))
                                {
                                    skeleton->SetDirtyFlag(true);
                                }

                                glm::vec3 eulerDeg = glm::degrees(glm::eulerAngles(socket.local.rotation));
                                if (ImGui::DragFloat3("Socket Rotation", &eulerDeg.x, 0.1f))
                                {
                                    socket.local.rotation = glm::quat(glm::radians(eulerDeg));
                                    skeleton->SetDirtyFlag(true);
                                }

                                if (ImGui::DragFloat3("Socket Scale", &socket.local.scale.x, 0.01f, 0.001f, 1000.0f))
                                {
                                    skeleton->SetDirtyFlag(true);
                                }

                                if (ImGui::Button("Remove Socket"))
                                {
                                    skeleton->sockets.erase(skeleton->sockets.begin() + selectedSocket);
                                    skeleton->RebuildSocketMap();
                                    selectedSocket = skeleton->sockets.empty() ? -1 : std::min(selectedSocket, static_cast<int32_t>(skeleton->sockets.size()) - 1);
                                    skeleton->SetDirtyFlag(true);
                                }
                            }
                        }
                        ImGui::EndChild();
                    }
                    else
                    {
                        ImGui::Text("Invalid asset!");
                    }
                }
                else
                {
                    ImGui::Text("Loading asset...");
                }
            }
        }

        UIAssetEditorClosePopup(assetData, isOpen);
        ImGui::End();
        assetData.isOpen = isOpen;
        assetData.requestFocus = false;
    }

    void AssetEditorPanel::UISkeletalAnimationEditor(AssetEditorData &assetData)
    {
        bool isOpen = assetData.isOpen;
        if (BeginAssetEditorWindow(assetData, isOpen, ImVec2(900.0f, 680.0f), ImVec2(420.0f, 560.0f), 0))
        {
            if (DrawAssetEditorHeader(assetData))
            {
                if (assetData.asset && assetData.asset->IsReady())
                {
                    if (Ref<SkeletalAnimation> animation = assetData.asset->As<SkeletalAnimation>())
                    {
                        auto project = m_EditorLayer->GetActiveProject().get();
                        auto assetManager = AssetManager::GetInstance();

                        ImGui::Text("Name: %s", animation->name.c_str());
                        ImGui::Text("Duration: %.3f", animation->duration);
                        ImGui::Text("Ticks Per Second: %.3f", animation->ticksPerSeconds);
                        ImGui::Text("Channels: %zu", animation->channels.size());

                        const AssetHandle skeletonHandle = AssetHandle(animation->GetSkeletonHandle());
                        if (skeletonHandle != AssetHandle(0))
                        {
                            const AssetMetaData &skeletonMetadata = assetManager->GetMetaData(skeletonHandle);
                            if (skeletonMetadata.type == AssetType::Skeleton)
                            {
                                ImGui::Text("Skeleton: %s", skeletonMetadata.filepath.generic_string().c_str());
                            }
                            else
                            {
                                ImGui::Text("Skeleton Handle: %llu", static_cast<uint64_t>(skeletonHandle));
                            }
                        }
                        else
                        {
                            ImGui::Text("Skeleton: <none>");
                        }

                        ImGui::Button("Drop Skeleton Here", ImVec2(220.0f, 0.0f));
                        if (ImGui::BeginDragDropTarget())
                        {
                            if (const ImGuiPayload *payload = ImGui::AcceptDragDropPayload("content_browser_item"))
                            {
                                if (payload->Data && payload->DataSize == sizeof(AssetHandle))
                                {
                                    const AssetHandle droppedHandle = *static_cast<const AssetHandle *>(payload->Data);
                                    const AssetMetaData &droppedMetadata = assetManager->GetMetaData(droppedHandle);
                                    if (droppedMetadata.type == AssetType::Skeleton)
                                    {
                                        animation->SetSkeletonHandle(UUID(static_cast<uint64_t>(droppedHandle)));
                                        animation->SetDirtyFlag(true);
                                    }
                                }
                            }
                            ImGui::EndDragDropTarget();
                        }
                    }
                    else
                    {
                        ImGui::Text("Invalid asset!");
                    }
                }
                else
                {
                    ImGui::Text("Loading asset...");
                }
            }
        }

        UIAssetEditorClosePopup(assetData, isOpen);
        ImGui::End();
        assetData.isOpen = isOpen;
        assetData.requestFocus = false;
    }

#pragma endregion !3D_STUFF

    bool AssetEditorPanel::SaveAsset(AssetEditorData &assetData)
    {
        if (!m_EditorLayer || !m_EditorLayer->GetActiveProject() || !assetData.asset)
        {
            return false;
        }

        Project *project = m_EditorLayer->GetActiveProject().get();
        const ignite::Path savePath = project->GetProjectFilepath(assetData.metadata.filepath);
        const ignite::Path metaPath = BuildAssetMetaPath(project, assetData.metadata);

        auto saveDefaultMeta = [&]()
        {
            return assetData.asset->SerializeMetaFile(metaPath);
        };

        switch (assetData.metadata.type)
        {
            case AssetType::Texture:
            {
                Ref<Texture> asset = assetData.asset->As<Texture>();
                if (!asset)
                    return false;

                TextureCreateInfo createInfo = asset->GetCreateInfo();
                const auto stateIt = s_TextureEditorState.find(static_cast<uint64_t>(assetData.handle));
                if (stateIt != s_TextureEditorState.end() && stateIt->second.initialized)
                    createInfo = stateIt->second.createInfo;

                const bool metaSaved = Texture::SerializeMetaFile(metaPath, assetData.handle, createInfo);
                if (!metaSaved)
                    return false;

                asset->SetDirtyFlag(false);
                return true;
            }

            case AssetType::SpriteSheet:
            {
                Ref<SpriteSheet> asset = assetData.asset->As<SpriteSheet>();
                if (!asset)
                    return false;
                if (!asset->Serialize(savePath))
                    return false;
                asset->NotifyChange();
                return saveDefaultMeta();
            }

            case AssetType::Material2D:
            {
                Ref<Material2D> asset = assetData.asset->As<Material2D>();
                if (!asset)
                    return false;
                if (!asset->Serialize(savePath))
                    return false;
                asset->NotifyChange();
                return saveDefaultMeta();
            }

            case AssetType::SkeletalAnimation:
            {
                Ref<SkeletalAnimation> asset = assetData.asset->As<SkeletalAnimation>();
                if (!asset)
                    return false;
                if (!asset->Serialize(savePath))
                    return false;

                asset->NotifyChange();
                return saveDefaultMeta();
            }

            case AssetType::Skeleton:
            {
                Ref<Skeleton> asset = assetData.asset->As<Skeleton>();
                if (!asset)
                    return false;
                if (!asset->Serialize(savePath))
                    return false;
                asset->NotifyChange();
                return saveDefaultMeta();
            }

            case AssetType::AnimationMontage:
            {
                Ref<AnimationMontage> asset = assetData.asset->As<AnimationMontage>();
                if (!asset)
                    return false;
                if (!asset->Serialize(savePath))
                    return false;
                asset->NotifyChange();
                return saveDefaultMeta();
            }

            case AssetType::BlendSpace:
            {
                Ref<BlendSpace> asset = assetData.asset->As<BlendSpace>();
                if (!asset)
                    return false;
                if (!asset->Serialize(savePath))
                    return false;
                asset->NotifyChange();
                return saveDefaultMeta();
            }

            case AssetType::LocomotionController:
            {
                Ref<LocomotionController> asset = assetData.asset->As<LocomotionController>();
                if (!asset)
                    return false;
                if (!asset->Serialize(savePath))
                    return false;
                asset->NotifyChange();
                return saveDefaultMeta();
            }

			case AssetType::StaticMesh:
			{
				Ref<StaticMesh> asset = assetData.asset->As<StaticMesh>();
				if (!asset)
					return false;
				if (!asset->Serialize(savePath))
					return false;
				asset->NotifyChange();
				return saveDefaultMeta();
			}

            case AssetType::SkeletalMesh:
            {
                Ref<SkeletalMesh> asset = assetData.asset->As<SkeletalMesh>();
                if (!asset)
                    return false;
                if (!asset->Serialize(savePath))
                    return false;
                asset->NotifyChange();
                return saveDefaultMeta();
            }

            case AssetType::Animation2D:
            {
                Ref<Animation2D> asset = assetData.asset->As<Animation2D>();
                if (!asset)
                    return false;
                if (!asset->Serialize(savePath))
                    return false;
                asset->SetDirtyFlag(false);
                asset->NotifyChange();
                return saveDefaultMeta();
            }
            case AssetType::AnimatorController2D:
            {
                Ref<AnimatorController2D> asset = assetData.asset->As<AnimatorController2D>();
                if (!asset)
                    return false;
                if (!asset->Serialize(savePath))
                    return false;
                asset->SetDirtyFlag(false);
                asset->NotifyChange();
                return saveDefaultMeta();
            }
            case AssetType::AnimatorController:
            {
                Ref<AnimatorController> asset = assetData.asset->As<AnimatorController>();
                if (!asset)
                    return false;

                if (!asset->Serialize(savePath))
                    return false;
                
                const auto key = static_cast<uint64_t>(asset->handle);
                if (s_AnimatorControllerEditorState.contains(key))
                    SaveAnimatorControllerEditorMeta(project, asset, assetData.metadata, s_AnimatorControllerEditorState[key]);

                asset->NotifyChange();
                return true;
            }
            case AssetType::Material:
            {
                Ref<Material> asset = assetData.asset->As<Material>();
                if (!asset)
                    return false;
                asset->InvalidateBindingSet();
                if (!asset->Serialize(savePath))
                    return false;
                asset->NotifyChange();
                return saveDefaultMeta();
            }
            case AssetType::ScriptableObject:
            {
                Ref<ScriptableObject> asset = assetData.asset->As<ScriptableObject>();
                if (!asset)
                    return false;

                if (!asset->Serialize(savePath))
                    return false;

                asset->SetDirtyFlag(false);
                asset->NotifyChange();
                return saveDefaultMeta();
            }
            case AssetType::Widget:
            {
                Ref<WidgetCanvas> asset = assetData.asset->As<WidgetCanvas>();
                if (!asset)
                    return false;

                if (!asset->Serialize(savePath))
                    return false;
                asset->NotifyChange();
                return saveDefaultMeta();
            }
            default:
            return false;
        }
    }

    void AssetEditorPanel::InitializeSceneData(AssetEditorData &assetData)
    {
        // Only some asset type can use scene renderer
        const AssetType assetType = assetData.metadata.type;
        if (assetType != AssetType::Material && assetType != AssetType::Mesh && assetType != AssetType::StaticMesh
            && assetType != AssetType::SkeletalMesh && assetType != AssetType::Skeleton && assetType != AssetType::Widget
            && assetType != AssetType::AnimatorController && assetType != AssetType::BlendSpace)
        {
            return;
        }

        const uint32_t width = assetData.sceneData.viewportWidth > 0 ? assetData.sceneData.viewportWidth : 1280;
        const uint32_t height = assetData.sceneData.viewportHeight > 0 ? assetData.sceneData.viewportHeight : 720;

        assetData.sceneData.camera = EditorCamera(std::format("AssetEditorCamera-{}", static_cast<uint64_t>(assetData.handle)));
        assetData.sceneData.camera.SetTarget(glm::vec3(0.0f, 0.0f, 0.0f));
        assetData.sceneData.camera.SetDistance(5.5f);
        assetData.sceneData.camera.yaw = glm::radians(90.0f);
        assetData.sceneData.camera.pitch = 0.0f;
        assetData.sceneData.camera.UpdateSphericalPosition();
        assetData.sceneData.camera.UpdateView();
        assetData.sceneData.camera.UpdateProjection(width, height);

        auto assetManager = AssetManager::GetInstance();

        Ref<Mesh> meshResult;
        Ref<Material> material;

        AssetHandle handle = assetData.handle;

        if (assetType == AssetType::Material)
        {
            material = assetManager->GetAsset<Material>(handle);
        }
        else if (assetType == AssetType::SkeletalMesh)
        {
            meshResult = assetManager->GetAsset<SkeletalMesh>(handle);
        }
		else if (assetType == AssetType::StaticMesh)
		{
			meshResult = assetManager->GetAsset<StaticMesh>(handle);
		}
        else if (assetType == AssetType::Skeleton)
        {
            const AssetHandle matchedMeshHandle = FindFirstMeshHandleForSkeleton(assetData.handle);
            if (matchedMeshHandle != AssetHandle(0))
            {
                auto matchedMesh = assetManager->GetAsset<SkeletalMesh>(matchedMeshHandle);
                if (matchedMesh)
                {
                    meshResult = matchedMesh;
                    SkeletonPreviewEditorState &previewState = s_SkeletonPreviewEditorState[static_cast<uint64_t>(assetData.handle)];
                    previewState.previewMeshHandle = matchedMeshHandle;
                    previewState.cachedPreviewMesh.reset();
                }
            }
        }

        Application::SubmitToRenderThread([this, handle, width, height, assetType, material, assetData]() mutable
        {
            RenderTargetCreateInfo rtCreateInfo = {};
            rtCreateInfo.width = width;
            rtCreateInfo.height = height;
            rtCreateInfo.attachments =
            {
                FramebufferAttachments{ "[Asset Preview DepthAttachment]", nvrhi::Format::D32S8, nvrhi::ResourceStates::DepthWrite},
                FramebufferAttachments{ "[Asset Preview ColorAttachment]", nvrhi::Format::RGBA8_UNORM, nvrhi::ResourceStates::RenderTarget},
                FramebufferAttachments{ "[Asset Preview ObjectIDAttachment]", nvrhi::Format::R32_UINT, nvrhi::ResourceStates::RenderTarget}
            };

            auto sceneRT = RenderTarget::Create(rtCreateInfo, "[Asset Preview Scene RT]");
            auto uiRT = RenderTarget::Create(rtCreateInfo, "[Asset Preview UI RT]");

            RenderTargetCreateInfo compositeInfo = {};
            compositeInfo.width = width;
            compositeInfo.height = height;
            compositeInfo.attachments =
            {
                FramebufferAttachments{ "[Asset Preview Composite ColorAttachment]", nvrhi::Format::RGBA8_UNORM, nvrhi::ResourceStates::RenderTarget}
            };
            auto compositeRT = RenderTarget::Create(compositeInfo, "[Asset Preview Composite RT]");

            Application::SubmitToMainThread([this, handle, sceneRT, uiRT, compositeRT]()
            {
                auto it = std::ranges::find(m_Assets, handle, &AssetEditorData::handle);
                if (it != m_Assets.end())
                {
                    it->sceneData.sceneRT = sceneRT;
                    it->sceneData.uiRT = uiRT;
                    it->sceneData.compositeRT = compositeRT;
                    it->sceneData.sceneRenderer = CreateRef<AssetSceneRenderer>();
                    if (it->sceneData.sceneRenderer)
                    {
					    // Set tonemapping to Filmic for asset preview
					    auto &postProcessing = it->sceneData.sceneRenderer->GetPostProcessingSettings();
					    postProcessing.tonemapMode = TonemapMode::Filmic;
                    }
                }
            });
        });
    }

    void AssetEditorPanel::UpdateSceneCamera(EditorSceneData &sceneData, float deltaTime)
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

    ignite::Path AssetEditorPanel::BuildUniqueAssetPath(const ignite::Path &baseDirectory, const std::string &baseName, const std::string &extension) const
    {
        ignite::Path candidate = baseDirectory / (baseName + extension);
        if (!ignite::Path::exists(candidate))
        {
            return candidate;
        }

        uint32_t suffix = 1;
        while (true)
        {
            candidate = baseDirectory / std::format("{}_{}{}", baseName, suffix, extension);
            if (!ignite::Path::exists(candidate))
            {
                return candidate;
            }
            ++suffix;
        }
    }

    // :EVENTS
    void AssetEditorPanel::OnEvent(Event &event)
    {
        EventDispatcher dispatcher(event);
        dispatcher.Dispatch<MouseScrolledEvent>(BIND_CLASS_EVENT_FN(AssetEditorPanel::OnMouseScrollEvent));
    }

    bool AssetEditorPanel::OnAssetEditorOpenSignal(const AssetEditorOpenSignal &signal)
    {
        auto handle = signal.handle;
        const auto &metadata = signal.metadata;
        if (metadata.type == AssetType::Invalid || handle == AssetHandle(0) || !m_EditorLayer || !m_EditorLayer->GetActiveProject())
        {
            return false;
        }

        // Check if the asset window is already open.
        auto it = std::ranges::find(m_Assets, handle, &AssetEditorData::handle);
        if (it != m_Assets.end())
        {
            it->isOpen = true;
            it->requestFocus = true;
            return true;
        }

        AssetManager::GetInstance()->AddAssetPin(handle, BuildAssetEditorPinOwnerTag(handle));

        std::string assetName = metadata.filepath.filename().string();
        if (assetName.empty())
        {
            assetName = metadata.filepath.generic_string();
        }

        Application::SubmitToMainThread([metadata, handle, assetName, this]()
        {
            AssetEditorData data;
            data.asset = nullptr;
            data.metadata = metadata;
            data.handle = handle;
            data.isOpen = true;
            data.requestFocus = true;
            data.windowTitle = std::format("{} - {}###asset_editor_{}", AssetTypeToString(metadata.type), assetName, static_cast<uint64_t>(handle));

            InitializeSceneData(data);

            m_Assets.push_back(std::move(data));
        });

        return true;
    }

    bool AssetEditorPanel::OnAssetEditorCreateSignal(const AssetEditorCreateSignal &signal)
    {
        if (!m_EditorLayer || !m_EditorLayer->GetActiveProject())
        {
            return false;
        }

        if (signal.type == AssetType::Invalid)
        {
            return false;
        }

        m_CreateRequest = {};
        m_CreateRequest.type = signal.type;
        m_CreateRequest.targetDirectory = signal.targetDirectory;
        m_CreateRequest.open = true;

        if (m_CreateRequest.type == AssetType::Material2D)
        {
            m_CreateRequest.asset = CreateRef<Material2D>();
            std::memcpy(m_CreateRequest.nameBuffer, "NewMaterial2D", sizeof("NewMaterial2D"));
        }

        if (m_CreateRequest.type == AssetType::SpriteSheet)
        {
            m_CreateRequest.asset = CreateRef<SpriteSheet>();
            std::memcpy(m_CreateRequest.nameBuffer, "NewSpriteSheet", sizeof("NewSpriteSheet"));
        }

        if (m_CreateRequest.type == AssetType::Animation2D)
        {
            m_CreateRequest.asset = Animation2D::Create("NewAnimation2D");
            std::memcpy(m_CreateRequest.nameBuffer, "NewAnimation2D", sizeof("NewAnimation2D"));
        }

        if (m_CreateRequest.type == AssetType::AnimatorController2D)
        {
            m_CreateRequest.asset = AnimatorController2D::Create();
            std::memcpy(m_CreateRequest.nameBuffer, "NewAnimatorController", sizeof("NewAnimatorController"));
        }

        if (m_CreateRequest.type == AssetType::AnimatorController)
        {
            m_CreateRequest.asset = AnimatorController::Create();
            std::memcpy(m_CreateRequest.nameBuffer, "NewAnimatorController3D", sizeof("NewAnimatorController3D"));
        }

        if (m_CreateRequest.type == AssetType::BlendSpace)
        {
            m_CreateRequest.asset = CreateRef<BlendSpace>();
            std::memcpy(m_CreateRequest.nameBuffer, "NewBlendSpace", sizeof("NewBlendSpace"));
        }

        if (m_CreateRequest.type == AssetType::LocomotionController)
        {
            m_CreateRequest.asset = CreateRef<LocomotionController>();
            std::memcpy(m_CreateRequest.nameBuffer, "NewLocomotion", sizeof("NewLocomotion"));
        }

        if (m_CreateRequest.type == AssetType::Widget)
        {
            m_CreateRequest.asset = CreateRef<WidgetCanvas>(nullptr);
            std::memcpy(m_CreateRequest.nameBuffer, "NewWidget", sizeof("NewWidget"));
        }

        return true;
    }

    bool AssetEditorPanel::OnMouseScrollEvent(MouseScrolledEvent &event)
    {
        return false;
    }

    void AssetEditorPanel::CloseAllAssetEditors()
    {
		if (auto assetManager = AssetManager::GetInstance())
		{
			for (const auto &assetData : m_Assets)
			{
				assetManager->ClearAssetPins(BuildAssetEditorPinOwnerTag(assetData.handle));
			}
		}
        m_Assets.clear();
    }
}
