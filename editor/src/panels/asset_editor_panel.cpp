// Copyright (c) 2026 Evangelion Manuhutu

#include "asset_editor_panel.hpp"

#include "../editor_layer.hpp"

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

#include "ignite/graphics/objects/material_2d.hpp"
#include "ignite/graphics/texture.hpp"
#include "ignite/project/project.hpp"
#include "ignite/scene/sprite_sheet.hpp"
#include "ignite/serializer/binary_serializer.hpp"
#include "ignite/serializer/serializer.hpp"
#include "ignite/core/profiler/profiler.hpp"
#include "ignite/graphics/objects/material.hpp"
#include "ignite/graphics/gpu_upload_sync.hpp"

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
        enum MeshType
        {
            CUBE,
            SPHERE,
            ICO_SPHERE
        };

        struct TextureEditorState
        {
            TextureCreateInfo createInfo;
            bool initialized = false;
        };

        static std::unordered_map<uint64_t, TextureEditorState> s_TextureEditorState;

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

        static std::unordered_map<uint64_t, SpriteSheetEditorState> s_SpriteSheetEditorState;
        static std::unordered_map<uint64_t, Animation2DEditorState> s_Anim2DEditorState;
        static const char *s_ParamTypeNames[] = { "Float", "Int", "Bool", "String" };
        static const char *s_ConditionOpNames[] = { "==", "!=", ">", "<", ">=", "<=" };

        struct MaterialPreviewEditorState
        {
            AssetHandle environmentTextureHandle = AssetHandle(0);
            float previewColumnWidth = 0.0f;
            int selectedMeshType = 1;
            bool initialized = false;
        };

        static std::unordered_map<uint64_t, MaterialPreviewEditorState> s_MaterialPreviewEditorState;

        // Default static meshes
        static std::unordered_map<MeshType, Ref<StaticMesh>> s_DefaultMeshes;

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
        static void DrawTexturePreviewDropTarget(Project *project, const char *label, AssetHandle &textureHandle, TOnChanged &&onChanged)
        {
            if (!project)
            {
                return;
            }

            auto assetManager = project->GetAssetManager();

            ImGui::PushID(label);
            ImGui::TextUnformatted(label);

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
                texture = project->GetAsset<Texture>(textureHandle);
                if (!texture)
                {
                    texture = project->GetAssetImmediate<Texture>(textureHandle);
                }
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

            ImGui::SameLine();
            if (textureHandle != AssetHandle(0))
            {
                if (ImGui::Button("Clear Texture"))
                {
                    textureHandle = AssetHandle(0);
                    onChanged();
                }
            }

            ImGui::TextDisabled("Handle: %llu", static_cast<unsigned long long>(static_cast<uint64_t>(textureHandle)));
            ImGui::PopID();
        }
    }

    // :Constructor
    AssetEditorPanel::AssetEditorPanel(const char *windowTitle, EditorLayer *editor) : IPanel(windowTitle, editor)
    {
    }

    // :Desctructor
    AssetEditorPanel::~AssetEditorPanel()
    {
    }

    void AssetEditorPanel::OnAttach()
    {
        s_DefaultMeshes[CUBE] = BinarySerializer::DeserializeStaticMesh("resources/staticmeshes/cube.ixsm");
        s_DefaultMeshes[SPHERE] = BinarySerializer::DeserializeStaticMesh("resources/staticmeshes/sphere.ixsm");
        s_DefaultMeshes[ICO_SPHERE] = BinarySerializer::DeserializeStaticMesh("resources/staticmeshes/ico_sphere.ixsm");
    }

    void AssetEditorPanel::OnDetach()
    {
        s_DefaultMeshes.clear();
    }

    // Render
    void AssetEditorPanel::OnRender(nvrhi::IFramebuffer *framebuffer)
    {
        if (!m_EditorLayer || !m_EditorLayer->GetActiveProject())
        {
            return;
        }

        for (auto &assetData : m_Assets)
        {
            if (!assetData.isOpen || assetData.metadata.type != AssetType::Material)
            {
                continue;
            }

            if (!assetData.asset || !assetData.asset->IsReady())
            {
                continue;
            }

            Ref<Material> material = assetData.asset->As<Material>();
            if (!material)
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

            sceneData.camera.UpdateProjection(static_cast<float>(width), static_cast<float>(height));
            sceneData.camera.UpdateView();

            sceneData.sceneRenderer->SetProject(m_EditorLayer->GetActiveProject().get());
            sceneData.sceneRenderer->SetMaterial(material);
            sceneData.sceneRenderer->BeginFrame();
            sceneData.sceneRenderer->Render(&sceneData.camera, sceneData.sceneRT, sceneData.uiRT, sceneData.compositeRT);
        }
    }

    void AssetEditorPanel::OnGuiRender()
    {
        IGN_PROFILE_FUNCTION();
        RenderCreateAssetPopup();

        for (auto &assetData : m_Assets)
        {
            if (!assetData.isOpen)
                continue;
            switch (assetData.metadata.type)
            {
                case AssetType::SpriteSheet:
                {
                    RenderSpriteSheet2DEditor(assetData);
                    break;
                }

                case AssetType::Texture:
                {
                    RenderTextureEditor(assetData);

                    break;
                }

                case AssetType::Material2D:
                {
                    RenderMaterial2DEditor(assetData);
                    break;
                }

                case AssetType::SkeletalAnimation:
                {
                    RenderSkeletalAnimationEditor(assetData);
                    break;
                }

                case AssetType::Skeleton:
                {
                    RenderSkeletalSkeletonEditor(assetData);
                    break;
                }

                case AssetType::AnimationMontage:
                {
                    RenderAnimationMontageEditor(assetData);
                    break;
                }

                case AssetType::Animation2D:
                {
                    RenderAnimation2DEditor(assetData);
                    break;
                }

                case AssetType::AnimatorController2D:
                {
                    RenderAnimatorController2DEditor(assetData);
                    break;
                }

                case AssetType::AnimatorController:
                {
                    RenderAnimatorControllerEditor(assetData);
                    break;
                }

                case AssetType::BlendSpace:
                {
                    RenderBlendSpaceEditor(assetData);
                    break;
                }

                case AssetType::LocomotionController:
                {
                    RenderLocomotionControllerEditor(assetData);
                    break;
                }

                case AssetType::Material:
                {
                    RenderMaterialEditor(assetData);
                    break;
                }

                default:
                {
                    break;
                }
            }
        }

        std::erase_if(m_Assets, [](AssetEditorData &assetData)
        {
            if (assetData.isOpen)
            {
                return false;
            }

            if (assetData.sceneData.sceneRenderer || assetData.sceneData.sceneRT || assetData.sceneData.uiRT || assetData.sceneData.compositeRT)
            {
                nvrhi::IDevice *device = DeviceManager::GetInstance()->GetDevice();
                GPUUploadSync::DeviceWaitIdle(device);

                s_MaterialPreviewEditorState.erase(static_cast<uint64_t>(assetData.handle));

                assetData.sceneData.sceneRenderer.reset();
                assetData.sceneData.sceneRT.reset();
                assetData.sceneData.uiRT.reset();
                assetData.sceneData.compositeRT.reset();
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
        if (assetData.asset->IsDirty())
            flags |= ImGuiWindowFlags_UnsavedDocument;

        ImGui::SetNextWindowSize(windowSize, ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowSizeConstraints(minWindowSize, ImVec2(FLT_MAX, FLT_MAX));
        if (!ImGui::Begin(assetData.windowTitle.c_str(), &isOpen, flags))
        {
            if (!isOpen && assetData.asset && assetData.asset->IsDirty())
            {
                isOpen = true;
                assetData.showUnsavedClosePopup = true;
            }

            return false;
        }

        return true;
    }

    void AssetEditorPanel::RenderAssetEditorClosePopup(AssetEditorData &assetData, bool &isOpen)
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

    void AssetEditorPanel::RenderCreateAssetPopup()
    {
        if (!m_CreateRequest.open || !m_EditorLayer || !m_EditorLayer->GetActiveProject())
        {
            return;
        }

        Project *project = m_EditorLayer->GetActiveProject().get();
        auto assetManager = project->GetAssetManager();

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

        // Asset creation pop-up
        ImGui::SetNextWindowSize(ImVec2(1200.0f, 760.0f), ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowSizeConstraints(ImVec2(900.0f, 640.0f), ImVec2(FLT_MAX, FLT_MAX));
        if (ImGui::Begin("Create Asset", &m_CreateRequest.open, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse))
        {
            ImGui::Text("Asset Type: %s", AssetTypeToString(m_CreateRequest.type).c_str());
            ImGui::InputText("Name", m_CreateRequest.nameBuffer, sizeof(m_CreateRequest.nameBuffer));

            auto tryCreateAsset = [&]()
            {
                std::string assetName = m_CreateRequest.nameBuffer;
                if (assetName.empty())
                {
                    assetName = "NewAsset";
                }

                std::filesystem::path targetDirectory = m_CreateRequest.targetDirectory.empty() ? project->GetAssetDirectory() : m_CreateRequest.targetDirectory;
                if (!std::filesystem::exists(targetDirectory))
                {
                    std::filesystem::create_directories(targetDirectory);
                }

                const std::string extension = GetAssetExtensionFromType(m_CreateRequest.type);
                const std::filesystem::path fullAssetPath = BuildUniqueAssetPath(targetDirectory, assetName, extension);

                bool created = false;
                Ref<Asset> createdAsset = m_CreateRequest.asset;
                if (m_CreateRequest.type == AssetType::Material2D)
                {
                    Ref<Material2D> asset = createdAsset->As<Material2D>();
                    if (asset)
                    {
                        asset->name = assetName;
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
                        asset->name = assetName;
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
                        asset->name = assetName;
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
                    AssetHandle handle = AssetHandle();
                    AssetMetaData metadata;
                    metadata.type = m_CreateRequest.type;
                    metadata.filepath = project->GetAssetRelativeFilepath(fullAssetPath);

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

                    InitializeSceneData(data);
                    
                    m_Assets.push_back(std::move(data));
                    m_CreateRequest = {};
                }
            };

            if (ImGui::Button("Create"))
            {
                tryCreateAsset();
            }
            ImGui::SameLine();

            if (ImGui::Button("Cancel"))
            {
                m_CreateRequest = {};
            }
            ImGui::Separator();

            if (m_CreateRequest.type == AssetType::Material2D)
            {
                Ref<Material2D> asset = m_CreateRequest.asset ? m_CreateRequest.asset->As<Material2D>() : nullptr;
                if (!asset)
                {
                    ImGui::TextColored(ImVec4(1.0f, 0.3f, 0.3f, 1.0f), "Invalid asset instance for Material2D creation.");
                    ImGui::End();
                    return;
                }

                ImGui::Separator();
                RenderMaterial2DEditor(asset);
            }

            if (m_CreateRequest.type == AssetType::SpriteSheet)
            {
                Ref<SpriteSheet> asset = m_CreateRequest.asset ? m_CreateRequest.asset->As<SpriteSheet>() : nullptr;
                if (!asset)
                {
                    ImGui::TextColored(ImVec4(1.0f, 0.3f, 0.3f, 1.0f), "Invalid asset instance for SpriteSheet creation.");
                    ImGui::End();
                    return;
                }

                ImGui::Separator();
                RenderSpriteSheet2DEditor(asset);
            }

            if (m_CreateRequest.type == AssetType::Animation2D)
            {
                Ref<Animation2D> asset = m_CreateRequest.asset ? m_CreateRequest.asset->As<Animation2D>() : nullptr;
                if (!asset)
                {
                    ImGui::TextColored(ImVec4(1.0f, 0.3f, 0.3f, 1.0f), "Invalid asset instance for Animation2D creation.");
                    ImGui::End();
                    return;
                }

                ImGui::Separator();
                RenderAnimation2DEditor(asset);
            }

            if (m_CreateRequest.type == AssetType::AnimatorController2D)
            {
                Ref<AnimatorController2D> asset = m_CreateRequest.asset ? m_CreateRequest.asset->As<AnimatorController2D>() : nullptr;
                if (!asset)
                {
                    ImGui::TextColored(ImVec4(1.0f, 0.3f, 0.3f, 1.0f), "Invalid asset instance for AnimatorController2D creation.");
                    ImGui::End();
                    return;
                }

                ImGui::Separator();
                RenderAnimatorController2DEditor(asset);
            }

            if (m_CreateRequest.type == AssetType::AnimatorController)
            {
                Ref<AnimatorController> asset = m_CreateRequest.asset ? m_CreateRequest.asset->As<AnimatorController>() : nullptr;
                if (!asset)
                {
                    ImGui::TextColored(ImVec4(1.0f, 0.3f, 0.3f, 1.0f), "Invalid asset instance for AnimatorController creation.");
                    ImGui::End();
                    return;
                }

                ImGui::Separator();
                RenderAnimatorControllerEditor(asset);
            }

            if (m_CreateRequest.type == AssetType::BlendSpace)
            {
                Ref<BlendSpace> asset = m_CreateRequest.asset ? m_CreateRequest.asset->As<BlendSpace>() : nullptr;
                if (!asset)
                {
                    ImGui::TextColored(ImVec4(1.0f, 0.3f, 0.3f, 1.0f), "Invalid asset instance for BlendSpace creation.");
                    ImGui::End();
                    return;
                }

                ImGui::Separator();
                RenderBlendSpaceEditor(asset);
            }

            if (m_CreateRequest.type == AssetType::LocomotionController)
            {
                Ref<LocomotionController> asset = m_CreateRequest.asset ? m_CreateRequest.asset->As<LocomotionController>() : nullptr;
                if (!asset)
                {
                    ImGui::TextColored(ImVec4(1.0f, 0.3f, 0.3f, 1.0f), "Invalid asset instance for LocomotionController creation.");
                    ImGui::End();
                    return;
                }

                ImGui::Separator();
                RenderLocomotionControllerEditor(asset);
            }

            ImGui::End();
        }
        else
        {
            ImGui::End();
        }

        if (!m_CreateRequest.open)
        {
            m_CreateRequest = {};
        }
    }
#pragma endregion !ImGui_Helper

#pragma region 2D_STUFF
    void AssetEditorPanel::RenderSpriteSheet2DEditor(AssetEditorData &assetData)
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
                        RenderSpriteSheet2DEditor(spriteSheet);
                    }
                    else
                    {
                        ImGui::Text("Loading asset...");
                    }
                }
                else
                {
                    ImGui::Text("Loading asset...");
                }
            }
        }

        RenderAssetEditorClosePopup(assetData, isOpen);
        ImGui::End();
        assetData.isOpen = isOpen;
        assetData.requestFocus = false;
    }

    void AssetEditorPanel::RenderSpriteSheet2DEditor(const Ref<SpriteSheet> &spriteSheet)
    {
        if (!spriteSheet)
        {
            return;
        }

        Project *project = m_EditorLayer->GetActiveProject().get();
        auto assetManager = project->GetAssetManager();

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
            texture = project->GetAsset<Texture>(spriteSheet->GetTextureHandle());
            if (!texture)
            {
                texture = project->GetAssetImmediate<Texture>(spriteSheet->GetTextureHandle());
            }
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
        std::string textureLabel = spriteSheet->GetTextureHandle() == AssetHandle(0)
            ? "Drop Texture Here"
            : "Texture Loaded";

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


    void AssetEditorPanel::RenderMaterial2DEditor(AssetEditorData &assetData)
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
                        RenderMaterial2DEditor(material2D);
                    }
                    else
                    {
                        ImGui::Text("Loading asset...");
                    }
                }
                else
                {
                    ImGui::Text("Loading asset...");
                }
            }
        }

        RenderAssetEditorClosePopup(assetData, isOpen);
        ImGui::End();
        assetData.isOpen = isOpen;
        assetData.requestFocus = false;
    }

    void AssetEditorPanel::RenderMaterial2DEditor(const Ref<Material2D> &material2D)
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

        DrawTexturePreviewDropTarget(m_EditorLayer->GetActiveProject().get(), "Texture Preview", material2D->textureHandle,
            [&]()
        {
            material2D->SetDirtyFlag(true);
        });
    }


    void AssetEditorPanel::RenderAnimation2DEditor(AssetEditorData &assetData)
    {
        bool isOpen = assetData.isOpen;
        if (BeginAssetEditorWindow(assetData, isOpen, ImVec2(1280.0f, 1080.0f), ImVec2(420.0f, 640.0f), ImGuiWindowFlags_NoScrollWithMouse))
        {
            if (DrawAssetEditorHeader(assetData))
            {
                if (assetData.asset && assetData.asset->IsReady())
                {
                    if (Ref<Animation2D> animation = assetData.asset->As<Animation2D>())
                    {
                        RenderAnimation2DEditor(animation);
                    }
                    else
                    {
                        ImGui::Text("Loading asset...");
                    }
                }
                else
                {
                    ImGui::Text("Loading asset...");
                }
            }
        }

        RenderAssetEditorClosePopup(assetData, isOpen);
        ImGui::End();
        assetData.isOpen = isOpen;
        assetData.requestFocus = false;
    }

    void AssetEditorPanel::RenderAnimation2DEditor(const Ref<Animation2D> &anim)
    {
        auto project = m_EditorLayer->GetActiveProject();

        const uint64_t stateKey = static_cast<uint64_t>(anim->handle);
        Animation2DEditorState &st = s_Anim2DEditorState[stateKey];

        // Resolve texture
        Ref<Texture> texture;
        if (anim->textureHandle != AssetHandle(0))
        {
            texture = project->GetAsset<Texture>(anim->textureHandle);
            if (!texture) texture = project->GetAssetImmediate<Texture>(anim->textureHandle);
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
                const float tlH = 54.0f;
                const float rulerH = 16.0f;   // time label strip
                const float dotAreaH = tlH - rulerH;
                const ImVec2 tlPos = ImGui::GetCursorScreenPos();
                const float  tlW = ImGui::GetContentRegionAvail().x;
                ImGui::InvisibleButton("##anim2d_timeline", ImVec2(tlW, tlH));
                const bool tlHov = ImGui::IsItemHovered();
                const bool tlActive = ImGui::IsItemActive();

                ImDrawList *dl = ImGui::GetWindowDrawList();

                // Background
                dl->AddRectFilled(tlPos, ImVec2(tlPos.x + tlW, tlPos.y + tlH), IM_COL32(30, 30, 35, 255));
                dl->AddRect(tlPos, ImVec2(tlPos.x + tlW, tlPos.y + tlH), IM_COL32(70, 70, 80, 255));

                const float frameDur = (fps > 0) ? (1.0f / fps) : 0.0f;

                if (frameCount > 0)
                {
                    const float cellW = tlW / static_cast<float>(frameCount);

                    // Ruler: time labels
                    for (int i = 0; i < frameCount; ++i)
                    {
                        const float cx = tlPos.x + (i + 0.5f) * cellW;
                        const float timeAtFrame = i * frameDur;
                        const std::string ts = std::format("{:.2f}s", timeAtFrame);
                        const ImVec2 tsz = ImGui::CalcTextSize(ts.c_str());
                        if (cellW >= tsz.x + 4.0f || i % std::max(1, static_cast<int>(tsz.x / cellW) + 1) == 0)
                        {
                            dl->AddText(ImVec2(cx - tsz.x * 0.5f, tlPos.y + 1.0f), IM_COL32(160, 160, 180, 200), ts.c_str());
                        }
                    }

                    // Horizontal divider under ruler
                    dl->AddLine(ImVec2(tlPos.x, tlPos.y + rulerH), ImVec2(tlPos.x + tlW, tlPos.y + rulerH), IM_COL32(70, 70, 80, 255));

                    // Frame dots
                    const float dotY = tlPos.y + rulerH + dotAreaH * 0.5f;
                    for (int i = 0; i < frameCount; ++i)
                    {
                        const float cx = tlPos.x + (i + 0.5f) * cellW;

                        // Cell separator tick
                        dl->AddLine(ImVec2(cx - cellW * 0.5f, tlPos.y + rulerH), ImVec2(cx - cellW * 0.5f, tlPos.y + tlH), IM_COL32(55, 55, 65, 255));

                        const bool isActive = (i == st.previewFrame);
                        const float dotR = isActive ? 7.0f : 5.0f;
                        const ImU32 dotCol = isActive ? IM_COL32(80, 200, 120, 255) : IM_COL32(120, 130, 160, 220);
                        dl->AddCircleFilled(ImVec2(cx, dotY), dotR, dotCol);
                        dl->AddCircle(ImVec2(cx, dotY), dotR, IM_COL32(200, 200, 220, 255), 0, 1.3f);
                    }

                    // Playhead line
                    if (totalDur > 0.0f)
                    {
                        const float phX = tlPos.x + (st.playbackTime / totalDur) * tlW;
                        dl->AddLine(ImVec2(phX, tlPos.y), ImVec2(phX, tlPos.y + tlH), IM_COL32(255, 100, 60, 230), 2.0f);

                        // Playhead handle triangle
                        dl->AddTriangleFilled(ImVec2(phX - 5, tlPos.y), ImVec2(phX + 5, tlPos.y), ImVec2(phX, tlPos.y + 10), IM_COL32(255, 100, 60, 230));
                    }

                    // Scrubbing
                    if ((tlHov || tlActive) && ImGui::IsMouseDown(ImGuiMouseButton_Left))
                    {
                        const float mx = std::clamp(ImGui::GetMousePos().x - tlPos.x, 0.0f, tlW);
                        st.playbackTime = (mx / tlW) * totalDur;
                        st.previewFrame = std::min(static_cast<int>(st.playbackTime * fps), frameCount - 1);
                        st.playing = false;
                    }
                }
                else
                {
                    const char *noMsg = "No frames – drop sprites here";
                    const ImVec2 ns = ImGui::CalcTextSize(noMsg);
                    dl->AddText(ImVec2(tlPos.x + (tlW - ns.x) * 0.5f, tlPos.y + (tlH - ns.y) * 0.5f),
                        IM_COL32(110, 110, 120, 200), noMsg);
                }
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
                    const AssetMetaData &md = project->GetAssetManager()->GetMetaData(handle);
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
                        const auto &md = project->GetAssetManager()->GetMetaData(handle);
                        if (md.type == AssetType::SpriteSheet)
                        {
                            Ref<SpriteSheet> ss = project->GetAsset<SpriteSheet>(handle);
                            if (!ss) ss = project->GetAssetImmediate<SpriteSheet>(handle);
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


    void AssetEditorPanel::RenderAnimatorController2DEditor(AssetEditorData &assetData)
    {
        bool isOpen = assetData.isOpen;
        if (BeginAssetEditorWindow(assetData, isOpen, ImVec2(1600.0f, 1000.0f), ImVec2(520.0f, 700.0f), ImGuiWindowFlags_NoScrollWithMouse))
        {
            if (DrawAssetEditorHeader(assetData))
            {
                if (assetData.asset && assetData.asset->IsReady())
                {
                    if (Ref<AnimatorController2D> controller = assetData.asset->As<AnimatorController2D>())
                    {
                        RenderAnimatorController2DEditor(controller);
                    }
                    else
                    {
                        ImGui::Text("Loading asset...");
                    }
                }
                else
                {
                    ImGui::Text("Loading asset...");
                }
            }
        }

        RenderAssetEditorClosePopup(assetData, isOpen);
        ImGui::End();
        assetData.isOpen = isOpen;
        assetData.requestFocus = false;
    }

    void AssetEditorPanel::RenderAnimatorController2DEditor(const Ref<AnimatorController2D> &ctrl)
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
                ImGui::Text("Anim: %llu", static_cast<uint64_t>(state.animHandle));
                ImGui::SameLine();
                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.15f, 0.2f, 0.35f, 1.0f));
                ImGui::Button("Drop Anim2D##s_drop", ImVec2(90, 20));
                ImGui::PopStyleColor();
                if (ImGui::BeginDragDropTarget())
                {
                    if (const ImGuiPayload *payload = ImGui::AcceptDragDropPayload(DND_PAYLOAD_CONTENT_BROWSER_ITEM))
                    {
                        const AssetHandle handle = *static_cast<const AssetHandle *>(payload->Data);
                        const AssetMetaData &md = project->GetAssetManager()->GetMetaData(handle);
                        if (md.type == AssetType::Animation2D)
                        {
                            state.animHandle = handle;
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
#pragma endregion !2D_STUFF


#pragma region 3D_STUFF
    void AssetEditorPanel::RenderMaterialEditor(AssetEditorData &assetData)
    {
        bool isOpen = assetData.isOpen;
        if (BeginAssetEditorWindow(assetData, isOpen, ImVec2(1100.0f, 900.0f), ImVec2(420.0f, 560.0f), 0))
        {
            if (DrawAssetEditorHeader(assetData))
            {
                if (assetData.asset && assetData.asset->IsReady())
                {
                    if (Ref<Material> material = assetData.asset->As<Material>())
                    {
                        EditorSceneData &sceneData = assetData.sceneData;
                        const uint64_t stateKey = static_cast<uint64_t>(assetData.handle);
                        MaterialPreviewEditorState &previewState = s_MaterialPreviewEditorState[stateKey];
                        if (!previewState.initialized)
                        {
                            previewState.selectedMeshType = static_cast<int>(CUBE);
                            previewState.initialized = true;
                        }

                        const ImVec2 contentSize = ImGui::GetContentRegionAvail();
                        const float splitterWidth = 6.0f;
                        const float minPreviewColumnWidth = 260.0f;
                        const float minToolsColumnWidth = 320.0f;
                        const float maxPreviewColumnWidth = std::max(minPreviewColumnWidth, contentSize.x - minToolsColumnWidth - splitterWidth);

                        if (previewState.previewColumnWidth <= 0.0f)
                        {
                            const float defaultToolsWidth = std::clamp(contentSize.x * 0.36f, 320.0f, 460.0f);
                            previewState.previewColumnWidth = std::max(minPreviewColumnWidth, contentSize.x - defaultToolsWidth - splitterWidth);
                        }
                        previewState.previewColumnWidth = std::clamp(previewState.previewColumnWidth, minPreviewColumnWidth, maxPreviewColumnWidth);

                        ImGui::BeginChild("##material_preview_column", ImVec2(previewState.previewColumnWidth, 0.0f), ImGuiChildFlags_None);
                        ImGui::BeginChild("##material_preview_viewport", ImVec2(0.0f, 0.0f), ImGuiChildFlags_Borders);
                        {
                            const ImVec2 viewportSize = ImGui::GetContentRegionAvail();
                            sceneData.viewportWidth = std::max(1u, static_cast<uint32_t>(viewportSize.x));
                            sceneData.viewportHeight = std::max(1u, static_cast<uint32_t>(viewportSize.y));

                            Ref<Texture> previewTexture = sceneData.compositeRT ? sceneData.compositeRT->GetColorAttachment(0) : nullptr;
                            if (previewTexture && previewTexture->GetHandle())
                            {
                                ImGui::Image(reinterpret_cast<ImTextureID>(previewTexture->GetHandle().Get()), viewportSize, ImVec2(0.0f, 0.0f), ImVec2(1.0f, 1.0f));
                                sceneData.viewportHovered = ImGui::IsItemHovered();
                            }
                            else
                            {
                                ImGui::Dummy(viewportSize);
                                sceneData.viewportHovered = ImGui::IsItemHovered();
                            }
                        }
                        ImGui::EndChild();
                        ImGui::EndChild();

                        UpdateMaterialPreviewCamera(sceneData, ImGui::GetIO().DeltaTime);
                        if (sceneData.sceneRenderer)
                        {
                            sceneData.sceneRenderer->SetMaterial(material);

                            const Ref<StaticMesh> previewMesh = s_DefaultMeshes.contains(static_cast<MeshType>(previewState.selectedMeshType))
                                ? s_DefaultMeshes[static_cast<MeshType>(previewState.selectedMeshType)]
                                : nullptr;
                            sceneData.sceneRenderer->SetPreviewMesh(previewMesh);

                            if (previewState.environmentTextureHandle != AssetHandle(0) && m_EditorLayer && m_EditorLayer->GetActiveProject())
                            {
                                Project *project = m_EditorLayer->GetActiveProject().get();
                                Ref<Texture> envTexture = project->GetAsset<Texture>(previewState.environmentTextureHandle);
                                if (!envTexture)
                                {
                                    envTexture = project->GetAssetImmediate<Texture>(previewState.environmentTextureHandle);
                                }
                                sceneData.sceneRenderer->SetEnvironmentTexture(envTexture);
                            }
                        }

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
                            previewState.previewColumnWidth += ImGui::GetIO().MouseDelta.x;
                            previewState.previewColumnWidth = std::clamp(previewState.previewColumnWidth, minPreviewColumnWidth, maxPreviewColumnWidth);
                        }
                        ImGui::EndChild();
                        ImGui::PopStyleColor(3);

                        ImGui::SameLine(0.0f, 0.0f);
                        ImGui::BeginChild("##material_controls_column", ImVec2(0.0f, 0.0f), ImGuiChildFlags_None);

                        const char *meshNames[] = { "Cube", "Sphere", "Ico Sphere" };
                        int meshSelection = previewState.selectedMeshType;
                        if (ImGui::Combo("Preview Mesh", &meshSelection, meshNames, IM_ARRAYSIZE(meshNames)))
                        {
                            previewState.selectedMeshType = meshSelection;
                        }

                        DrawTexturePreviewDropTarget(m_EditorLayer->GetActiveProject().get(), "Environment", previewState.environmentTextureHandle,
                            [&]()
                        {
                            if (sceneData.sceneRenderer)
                            {
                                if (previewState.environmentTextureHandle == AssetHandle(0))
                                {
                                    sceneData.sceneRenderer->SetEnvironmentTexture(nullptr);
                                }
                                else
                                {
                                    Project *project = m_EditorLayer->GetActiveProject().get();
                                    Ref<Texture> envTexture = project->GetAsset<Texture>(previewState.environmentTextureHandle);
                                    if (!envTexture)
                                    {
                                        envTexture = project->GetAssetImmediate<Texture>(previewState.environmentTextureHandle);
                                    }
                                    sceneData.sceneRenderer->SetEnvironmentTexture(envTexture);
                                }
                            }
                        });

                        ImGui::Separator();
                        RenderMaterialEditor(material);
                        ImGui::EndChild();
                    }
                    else
                    {
                        ImGui::Text("Loading asset...");
                    }
                }
                else
                {
                    ImGui::Text("Loading asset...");
                }
            }
        }

        RenderAssetEditorClosePopup(assetData, isOpen);
        ImGui::End();
        assetData.isOpen = isOpen;
        assetData.requestFocus = false;
    }

    void AssetEditorPanel::RenderMaterialEditor(const Ref<Material> &material)
    {
        if (!material || !m_EditorLayer || !m_EditorLayer->GetActiveProject())
            return;

        if (ImGui::ColorEdit4("Base Color", &material->gpuData.baseColorFactor.x))
        {
            material->SetDirtyFlag(true);
        }

        if (ImGui::ColorEdit4("Emissive Color", &material->gpuData.emissiveFactor.x))
        {
            material->SetDirtyFlag(true);
        }

        if (ImGui::DragFloat("Metallic Factor", &material->gpuData.metallicFactor, 0.025f, 0.0f, 1.0f))
        {
            material->SetDirtyFlag(true);
        }

        if (ImGui::DragFloat("Roughness Factor", &material->gpuData.roughnessFactor, 0.025f, 0.0f, 1.0f))
        {
            material->SetDirtyFlag(true);
        }

        if (ImGui::DragFloat("Occlusion Strength", &material->gpuData.occlusionStrength, 0.025f, 0.0f, 1.0f))
        {
            material->SetDirtyFlag(true);
        }

        ImGui::Separator();

        // Base texture
        DrawTexturePreviewDropTarget(m_EditorLayer->GetActiveProject().get(), "Base Color Texture", material->baseColorTextureHandle,
            [&]() { material->SetDirtyFlag(true); });

        // Normal texture
        DrawTexturePreviewDropTarget(m_EditorLayer->GetActiveProject().get(), "Normal Texture", material->normalTextureHandle,
            [&]() { material->SetDirtyFlag(true); });
        
        // Emissive texture
        DrawTexturePreviewDropTarget(m_EditorLayer->GetActiveProject().get(), "Emissive Texture", material->emissiveTextureHandle,
            [&]() { material->SetDirtyFlag(true); });
        
        // Metallic Texture
        DrawTexturePreviewDropTarget(m_EditorLayer->GetActiveProject().get(), "Metallic Texture", material->metallicTextureHandle,
            [&]() { material->SetDirtyFlag(true); });

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
        DrawTexturePreviewDropTarget(m_EditorLayer->GetActiveProject().get(), "Roughness Texture", material->roughnessTextureHandle,
            [&]() { material->SetDirtyFlag(true); });

        // Rougness texture channel
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

        // Occlussion texture
        DrawTexturePreviewDropTarget(m_EditorLayer->GetActiveProject().get(), "Occlusion Texture", material->occlusionTextureHandle,
            [&]() { material->SetDirtyFlag(true); });
    }
    

    void AssetEditorPanel::RenderTextureEditor(AssetEditorData &assetData)
    {
        bool isOpen = assetData.isOpen;
        if (BeginAssetEditorWindow(assetData, isOpen, ImVec2(1000.0f, 900.0f), ImVec2(420.0f, 640.0f), ImGuiWindowFlags_NoScrollWithMouse))
        {
            if (DrawAssetEditorHeader(assetData))
            {
                if (assetData.asset && assetData.asset->IsReady())
                {
                    if (Ref<Texture> texture = assetData.asset->As<Texture>())
                    {
                        RenderTextureEditor(assetData, texture);
                    }
                    else
                    {
                        ImGui::Text("Loading asset...");
                    }
                }
                else
                {
                    ImGui::Text("Loading asset...");
                }
            }
        }

        RenderAssetEditorClosePopup(assetData, isOpen);
        ImGui::End();
        assetData.isOpen = isOpen;
        assetData.requestFocus = false;
    }

    void AssetEditorPanel::RenderTextureEditor(AssetEditorData &assetData, const Ref<Texture> &texture)
    {
        if (!texture || !m_EditorLayer || !m_EditorLayer->GetActiveProject())
        {
            return;
        }

        Project *project = m_EditorLayer->GetActiveProject().get();
        auto assetManager = project->GetAssetManager();

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

        const float previewMaxWidth = std::min(320.0f, ImGui::GetContentRegionAvail().x);
        if (previewMaxWidth > 0.0f && texture->GetWidth() > 0 && texture->GetHeight() > 0)
        {
            const float aspectRatio = static_cast<float>(texture->GetWidth()) / static_cast<float>(texture->GetHeight());
            ImVec2 previewSize(previewMaxWidth, previewMaxWidth);
            if (aspectRatio > 1.0f)
            {
                previewSize.y = previewMaxWidth / aspectRatio;
            }
            else
            {
                previewSize.x = previewMaxWidth * aspectRatio;
            }

            ImGui::Text("Preview");
            ImTextureID textureId = reinterpret_cast<ImTextureID>(texture->GetHandle().Get());
            ImGui::Image(textureId, previewSize);
        }

        ImGui::Separator();

        int mipLevels = static_cast<int>(state.createInfo.mipLevels);
        if (ImGui::DragInt("Mip Levels", &mipLevels, 1.0f, 1, 16))
        {
            state.createInfo.mipLevels = static_cast<uint32_t>(std::max(mipLevels, 1));
        }

        int arraySize = static_cast<int>(state.createInfo.arraySize);
        if (ImGui::DragInt("Array Size", &arraySize, 1.0f, 1, 64))
        {
            state.createInfo.arraySize = static_cast<uint32_t>(std::max(arraySize, 1));
        }

        int sampleCount = static_cast<int>(state.createInfo.sampleCount);
        if (ImGui::DragInt("Sample Count", &sampleCount, 1.0f, 1, 16))
        {
            state.createInfo.sampleCount = static_cast<uint32_t>(std::max(sampleCount, 1));
        }

        int sampleQuality = static_cast<int>(state.createInfo.sampleQuality);
        if (ImGui::DragInt("Sample Quality", &sampleQuality, 1.0f, 0, 16))
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

        if (ImGui::BeginCombo("Import Format", TextureFormatToString(formatOptions[currentFormatIndex])))
        {
            for (int i = 0; i < static_cast<int>(std::size(formatOptions)); ++i)
            {
                const bool selected = i == currentFormatIndex;
                if (ImGui::Selectable(TextureFormatToString(formatOptions[i]),
                    selected))
                {
                    state.createInfo.format = formatOptions[i];
                }

                if (selected)
                {
                    ImGui::SetItemDefaultFocus();
                }
            }
            ImGui::EndCombo();
        }

        const nvrhi::SamplerAddressMode addressModeOptions[] =
        {
            nvrhi::SamplerAddressMode::Repeat,
            nvrhi::SamplerAddressMode::ClampToEdge,
            nvrhi::SamplerAddressMode::ClampToBorder
        };

        auto drawAddressModeCombo = [&addressModeOptions](const char *label, nvrhi::SamplerAddressMode &mode)
        {
            if (ImGui::BeginCombo(label, SamplerAddressModeToString(mode)))
            {
                for (const auto option : addressModeOptions)
                {
                    const bool selected = option == mode;
                    if (ImGui::Selectable(SamplerAddressModeToString(option), selected))
                    {
                        mode = option;
                    }

                    if (selected)
                    {
                        ImGui::SetItemDefaultFocus();
                    }
                }
                ImGui::EndCombo();
            }
        };

        drawAddressModeCombo("Wrap U", state.createInfo.samplerAddressU);
        drawAddressModeCombo("Wrap V", state.createInfo.samplerAddressV);
        drawAddressModeCombo("Wrap W", state.createInfo.samplerAddressW);

        ImGui::Checkbox("Linear Filtering", &state.createInfo.samplerLinearFiltering);
        ImGui::Checkbox("Flip Vertically", &state.createInfo.flip);
        ImGui::Checkbox("Keep CPU Data", &state.createInfo.keepCpuData);
        ImGui::Checkbox("Keep Initial State", &state.createInfo.keepInitialState);

        ImGui::Separator();
        if (ImGui::Button("ReImport"))
        {
            assetManager->SetTextureCreateInfo(assetData.handle, state.createInfo);

            AssetMetaData importMetadata = assetData.metadata;
            importMetadata.filepath = project->GetAssetFilepath(assetData.metadata.filepath);

            Ref<Texture> reimportedTexture = AssetImporter::ImportTexture(assetData.handle, importMetadata, state.createInfo, assetManager);
            if (reimportedTexture)
            {
                reimportedTexture->handle = assetData.handle;
                assetManager->AssignAsset(assetData.handle, reimportedTexture);
                assetManager->SetTextureCreateInfo(assetData.handle, reimportedTexture->GetCreateInfo());
                assetData.asset = reimportedTexture;
                state.createInfo = reimportedTexture->GetCreateInfo();
                reimportedTexture->SetDirtyFlag(false);
            }
        }
    }


    void AssetEditorPanel::RenderAnimatorControllerEditor(const Ref<AnimatorController> &animator)
    {
        if (!animator || !m_EditorLayer || !m_EditorLayer->GetActiveProject())
        {
            return;
        }

        auto project = m_EditorLayer->GetActiveProject();
        auto resetRuntimeForController = [&]()
        {
            Ref<Scene> activeScene = m_EditorLayer->GetActiveScene();
            if (!activeScene || !activeScene->registry)
            {
                return;
            }

            auto view = activeScene->registry->view<SkeletalMeshComponent>();
            for (auto entity : view)
            {
                auto &sm = view.get<SkeletalMeshComponent>(entity);
                if (sm.animatorHandle != animator->handle)
                {
                    continue;
                }

                sm.currentStateName.clear();
                sm.stateElapsed = 0.0f;
                sm.stateNormalized = 0.0f;
            }
        };

        ImGui::Text("Animator Controller");
        ImGui::Separator();

        char defaultBuf[256]{};
        std::strncpy(defaultBuf, animator->defaultState.c_str(), sizeof(defaultBuf) - 1);
        if (ImGui::InputText("Default State##ac_default", defaultBuf, sizeof(defaultBuf)))
        {
            animator->defaultState = defaultBuf;
            animator->SetDirtyFlag(true);
        }

        ImGui::Button("Drop Skeleton##ac_drop_skel", ImVec2(220.0f, 0.0f));
        if (ImGui::BeginDragDropTarget())
        {
            if (const ImGuiPayload *payload = ImGui::AcceptDragDropPayload(DND_PAYLOAD_CONTENT_BROWSER_ITEM))
            {
                if (payload->Data && payload->DataSize == sizeof(AssetHandle))
                {
                    const AssetHandle handle = *static_cast<const AssetHandle *>(payload->Data);
                    const AssetMetaData &md = project->GetAssetManager()->GetMetaData(handle);
                    if (md.type == AssetType::Skeleton)
                    {
                        animator->skeletonHandle = handle;
                        animator->SetDirtyFlag(true);
                    }
                }
            }
            ImGui::EndDragDropTarget();
        }

        ImGui::TextDisabled("Skeleton Handle: %llu", static_cast<uint64_t>(animator->skeletonHandle));

        if (ImGui::CollapsingHeader("States##ac_states", ImGuiTreeNodeFlags_DefaultOpen))
        {
            for (int i = 0; i < static_cast<int>(animator->states.size()); ++i)
            {
                auto &state = animator->states[static_cast<size_t>(i)];
                ImGui::PushID(i);

                char nameBuf[256]{};
                std::strncpy(nameBuf, state.name.c_str(), sizeof(nameBuf) - 1);
                ImGui::SetNextItemWidth(180.0f);
                if (ImGui::InputText("Name##ac_state_name", nameBuf, sizeof(nameBuf)))
                {
                    state.name = nameBuf;
                    animator->SetDirtyFlag(true);
                }

                ImGui::SameLine();
                ImGui::Text("Anim: %llu", static_cast<uint64_t>(state.animHandle));
                ImGui::SameLine();
                ImGui::Button("Drop Anim##ac_state_drop", ImVec2(100.0f, 20.0f));
                if (ImGui::BeginDragDropTarget())
                {
                    if (const ImGuiPayload *payload = ImGui::AcceptDragDropPayload(DND_PAYLOAD_CONTENT_BROWSER_ITEM))
                    {
                        if (payload->Data && payload->DataSize == sizeof(AssetHandle))
                        {
                            const AssetHandle handle = *static_cast<const AssetHandle *>(payload->Data);
                            const AssetMetaData &md = project->GetAssetManager()->GetMetaData(handle);
                            if (md.type == AssetType::SkeletalAnimation)
                            {
                                Ref<SkeletalAnimation> droppedAnim = project->GetAsset<SkeletalAnimation>(handle);
                                if (!droppedAnim)
                                {
                                    droppedAnim = project->GetAssetImmediate<SkeletalAnimation>(handle);
                                }

                                if (droppedAnim)
                                {
                                    state.animHandle = handle;
                                    animator->SetDirtyFlag(true);
                                    resetRuntimeForController();
                                }
                            }
                        }
                    }
                    ImGui::EndDragDropTarget();
                }

                ImGui::SameLine();
                if (ImGui::SmallButton("X##ac_state_remove"))
                {
                    animator->states.erase(animator->states.begin() + i);
                    animator->SetDirtyFlag(true);
                    ImGui::PopID();
                    break;
                }

                ImGui::PopID();
            }

            if (ImGui::Button("+ Add State##ac_state_add"))
            {
                animator->states.push_back({ "NewState", AssetHandle(0) });
                animator->SetDirtyFlag(true);
            }
        }

        if (ImGui::CollapsingHeader("Parameters##ac_params", ImGuiTreeNodeFlags_DefaultOpen))
        {
            for (int i = 0; i < static_cast<int>(animator->params.size()); ++i)
            {
                auto &p = animator->params[static_cast<size_t>(i)];
                ImGui::PushID(i);

                ImGui::SetNextItemWidth(140.0f);
                char pNameBuf[256]{};
                std::strncpy(pNameBuf, p.name.c_str(), sizeof(pNameBuf) - 1);
                if (ImGui::InputText("##ac_param_name", pNameBuf, sizeof(pNameBuf)))
                {
                    p.name = pNameBuf;
                    animator->SetDirtyFlag(true);
                }

                ImGui::SameLine();
                int typeIdx = static_cast<int>(p.type);
                ImGui::SetNextItemWidth(70.0f);
                if (ImGui::Combo("##ac_param_type", &typeIdx, s_ParamTypeNames, 4))
                {
                    p.type = static_cast<AnimParam::Type>(typeIdx);
                    animator->SetDirtyFlag(true);
                }

                ImGui::SameLine();
                switch (p.type)
                {
                    case AnimParam::Type::Float: if (ImGui::DragFloat("##ac_param_f", &p.floatVal, 0.01f)) animator->SetDirtyFlag(true); break;
                    case AnimParam::Type::Int: if (ImGui::DragInt("##ac_param_i", &p.intVal)) animator->SetDirtyFlag(true); break;
                    case AnimParam::Type::Bool: if (ImGui::Checkbox("##ac_param_b", &p.boolVal)) animator->SetDirtyFlag(true); break;
                    case AnimParam::Type::String:
                    {
                        char sBuf[256]{};
                        std::strncpy(sBuf, p.strVal.c_str(), sizeof(sBuf) - 1);
                        if (ImGui::InputText("##ac_param_s", sBuf, sizeof(sBuf)))
                        {
                            p.strVal = sBuf;
                            animator->SetDirtyFlag(true);
                        }
                        break;
                    }
                    default: break;
                }

                ImGui::SameLine();
                if (ImGui::SmallButton("X##ac_param_remove"))
                {
                    animator->params.erase(animator->params.begin() + i);
                    animator->SetDirtyFlag(true);
                    ImGui::PopID();
                    break;
                }

                ImGui::PopID();
            }

            if (ImGui::Button("+ Float##ac_add_float")) { animator->params.push_back({ .name = "NewFloat", .type = AnimParam::Type::Float }); animator->SetDirtyFlag(true); }
            ImGui::SameLine();
            if (ImGui::Button("+ Bool##ac_add_bool")) { animator->params.push_back({ .name = "NewBool", .type = AnimParam::Type::Bool }); animator->SetDirtyFlag(true); }
            ImGui::SameLine();
            if (ImGui::Button("+ Int##ac_add_int")) { animator->params.push_back({ .name = "NewInt", .type = AnimParam::Type::Int }); animator->SetDirtyFlag(true); }
            ImGui::SameLine();
            if (ImGui::Button("+ String##ac_add_str")) { animator->params.push_back({ .name = "NewString", .type = AnimParam::Type::String }); animator->SetDirtyFlag(true); }
        }

        if (ImGui::CollapsingHeader("Transitions##ac_trans"))
        {
            for (int i = 0; i < static_cast<int>(animator->transitions.size()); ++i)
            {
                auto &tr = animator->transitions[static_cast<size_t>(i)];
                ImGui::PushID(i);

                char fromBuf[256]{};
                char toBuf[256]{};
                std::strncpy(fromBuf, tr.fromState.c_str(), sizeof(fromBuf) - 1);
                std::strncpy(toBuf, tr.toState.c_str(), sizeof(toBuf) - 1);
                ImGui::SetNextItemWidth(120.0f);
                if (ImGui::InputText("From##ac_tr_from", fromBuf, sizeof(fromBuf))) { tr.fromState = fromBuf; animator->SetDirtyFlag(true); }
                ImGui::SameLine();
                ImGui::SetNextItemWidth(120.0f);
                if (ImGui::InputText("To##ac_tr_to", toBuf, sizeof(toBuf))) { tr.toState = toBuf; animator->SetDirtyFlag(true); }
                ImGui::SameLine();
                if (ImGui::Checkbox("Exit Time##ac_tr_exit", &tr.hasExitTime)) animator->SetDirtyFlag(true);
                if (tr.hasExitTime)
                {
                    ImGui::SameLine();
                    ImGui::SetNextItemWidth(70.0f);
                    if (ImGui::DragFloat("##ac_tr_exit_v", &tr.exitTime, 0.01f, 0.0f, 1.0f)) animator->SetDirtyFlag(true);
                }

                ImGui::SameLine();
                if (ImGui::SmallButton("X##ac_tr_remove"))
                {
                    animator->transitions.erase(animator->transitions.begin() + i);
                    animator->SetDirtyFlag(true);
                    ImGui::PopID();
                    break;
                }

                ImGui::Indent(16.0f);
                for (int ci = 0; ci < static_cast<int>(tr.conditions.size()); ++ci)
                {
                    auto &cond = tr.conditions[static_cast<size_t>(ci)];
                    ImGui::PushID(ci);

                    char cParamBuf[256]{};
                    std::strncpy(cParamBuf, cond.paramName.c_str(), sizeof(cParamBuf) - 1);
                    ImGui::SetNextItemWidth(120.0f);
                    if (ImGui::InputText("Param##ac_cond_param", cParamBuf, sizeof(cParamBuf))) { cond.paramName = cParamBuf; animator->SetDirtyFlag(true); }
                    ImGui::SameLine();

                    int opIdx = static_cast<int>(cond.op);
                    ImGui::SetNextItemWidth(70.0f);
                    if (ImGui::Combo("##ac_cond_op", &opIdx, s_ConditionOpNames, 6))
                    {
                        cond.op = static_cast<AnimCondition::Op>(opIdx);
                        animator->SetDirtyFlag(true);
                    }

                    ImGui::SameLine();
                    const AnimParam *param = animator->GetParam(cond.paramName);
                    if (param)
                    {
                        switch (param->type)
                        {
                            case AnimParam::Type::Float: if (ImGui::DragFloat("##ac_cond_f", &cond.floatThreshold, 0.01f)) animator->SetDirtyFlag(true); break;
                            case AnimParam::Type::Int: if (ImGui::DragInt("##ac_cond_i", &cond.intThreshold)) animator->SetDirtyFlag(true); break;
                            case AnimParam::Type::Bool: if (ImGui::Checkbox("##ac_cond_b", &cond.boolThreshold)) animator->SetDirtyFlag(true); break;
                            case AnimParam::Type::String:
                            {
                                char tBuf[256]{};
                                std::strncpy(tBuf, cond.strThreshold.c_str(), sizeof(tBuf) - 1);
                                if (ImGui::InputText("##ac_cond_s", tBuf, sizeof(tBuf)))
                                {
                                    cond.strThreshold = tBuf;
                                    animator->SetDirtyFlag(true);
                                }
                                break;
                            }
                            default: break;
                        }
                    }
                    else
                    {
                        if (ImGui::DragFloat("##ac_cond_f2", &cond.floatThreshold, 0.01f)) animator->SetDirtyFlag(true);
                    }

                    ImGui::SameLine();
                    if (ImGui::SmallButton("X##ac_cond_remove"))
                    {
                        tr.conditions.erase(tr.conditions.begin() + ci);
                        animator->SetDirtyFlag(true);
                        ImGui::PopID();
                        break;
                    }

                    ImGui::PopID();
                }

                if (ImGui::SmallButton("+ Condition##ac_cond_add"))
                {
                    tr.conditions.push_back({});
                    animator->SetDirtyFlag(true);
                }
                ImGui::Unindent(16.0f);
                ImGui::Separator();
                ImGui::PopID();
            }

            if (ImGui::Button("+ Add Transition##ac_add_transition"))
            {
                AnimTransition tr;
                if (!animator->states.empty())
                {
                    tr.fromState = animator->states[0].name;
                    tr.toState = animator->states.size() > 1 ? animator->states[1].name : "";
                }
                animator->transitions.push_back(tr);
                animator->SetDirtyFlag(true);
            }
        }
    }

    void AssetEditorPanel::RenderAnimatorControllerEditor(AssetEditorData &assetData)
    {
        bool isOpen = assetData.isOpen;
        if (BeginAssetEditorWindow(assetData, isOpen, ImVec2(1600.0f, 1000.0f), ImVec2(520.0f, 700.0f), ImGuiWindowFlags_NoScrollWithMouse))
        {
            if (DrawAssetEditorHeader(assetData))
            {
                if (assetData.asset && assetData.asset->IsReady())
                {
                    if (Ref<AnimatorController> controller = assetData.asset->As<AnimatorController>())
                    {
                        RenderAnimatorControllerEditor(controller);
                    }
                    else
                    {
                        ImGui::Text("Loading asset...");
                    }
                }
                else
                {
                    ImGui::Text("Loading asset...");
                }
            }
        }

        RenderAssetEditorClosePopup(assetData, isOpen);
        ImGui::End();
        assetData.isOpen = isOpen;
        assetData.requestFocus = false;
    }

    void AssetEditorPanel::RenderAnimationMontageEditor(AssetEditorData &assetData)
    {
        bool isOpen = assetData.isOpen;
        if (BeginAssetEditorWindow(assetData, isOpen, ImVec2(1100.0f, 840.0f), ImVec2(480.0f, 560.0f), 0))
        {
            if (DrawAssetEditorHeader(assetData))
            {
                if (assetData.asset && assetData.asset->IsReady())
                {
                    if (Ref<AnimationMontage> montage = assetData.asset->As<AnimationMontage>())
                    {
                        RenderAnimationMontageEditor(montage);
                    }
                    else
                    {
                        ImGui::Text("Loading asset...");
                    }
                }
                else
                {
                    ImGui::Text("Loading asset...");
                }
            }
        }

        RenderAssetEditorClosePopup(assetData, isOpen);
        ImGui::End();
        assetData.isOpen = isOpen;
        assetData.requestFocus = false;
    }

    void AssetEditorPanel::RenderAnimationMontageEditor(const Ref<AnimationMontage> &montage)
    {
        if (!montage || !m_EditorLayer || !m_EditorLayer->GetActiveProject())
        {
            return;
        }

        Project *project = m_EditorLayer->GetActiveProject().get();
        auto assetManager = project->GetAssetManager();

        if (montage->name.empty())
        {
            montage->name = "NewMontage";
        }

        char nameBuffer[256] {};
        std::strncpy(nameBuffer, montage->name.c_str(), sizeof(nameBuffer) - 1);
        if (ImGui::InputText("Montage Name", nameBuffer, sizeof(nameBuffer)))
        {
            montage->name = nameBuffer;
            montage->SetDirtyFlag(true);
        }

        ImGui::Button("Drop Animation (.ixanim)", ImVec2(240.0f, 0.0f));
        if (ImGui::BeginDragDropTarget())
        {
            if (const ImGuiPayload *payload = ImGui::AcceptDragDropPayload(DND_PAYLOAD_CONTENT_BROWSER_ITEM))
            {
                if (payload->Data && payload->DataSize == sizeof(AssetHandle))
                {
                    const AssetHandle droppedHandle = *static_cast<const AssetHandle *>(payload->Data);
                    const AssetMetaData &metadata = assetManager->GetMetaData(droppedHandle);
                    if (metadata.type == AssetType::SkeletalAnimation)
                    {
                        montage->SetAnimationHandle(droppedHandle);

                        Ref<SkeletalAnimation> animation = project->GetAsset<SkeletalAnimation>(droppedHandle);
                        if (!animation)
                        {
                            animation = project->GetAssetImmediate<SkeletalAnimation>(droppedHandle);
                        }

                        if (animation)
                        {
                            montage->SetSkeletonHandle(animation->GetSkeletonHandle());
                        }

                        montage->SetDirtyFlag(true);
                    }
                }
            }
            ImGui::EndDragDropTarget();
        }

        ImGui::TextDisabled("Animation Handle: %llu", static_cast<unsigned long long>(static_cast<uint64_t>(montage->GetAnimationHandle())));
        ImGui::TextDisabled("Skeleton Handle: %llu", static_cast<unsigned long long>(static_cast<uint64_t>(montage->GetSkeletonHandle())));

        static char notifyNameBuffer[128] = "Footstep";
        static float notifyStart = 0.0f;
        static float notifyEnd = 0.2f;

        ImGui::SeparatorText("Notifies");
        ImGui::InputText("Notify Name", notifyNameBuffer, sizeof(notifyNameBuffer));
        ImGui::DragFloat("Start Time", &notifyStart, 0.01f, 0.0f, 999.0f);
        ImGui::DragFloat("End Time", &notifyEnd, 0.01f, 0.0f, 999.0f);
        if (ImGui::Button("Add Notify"))
        {
            montage->AddNotif(notifyNameBuffer, std::min(notifyStart, notifyEnd), std::max(notifyStart, notifyEnd));
            montage->SetDirtyFlag(true);
        }

        std::vector<std::string> removeQueue;
        if (ImGui::BeginChild("##montage_notify_list", ImVec2(0.0f, 260.0f), ImGuiChildFlags_Borders))
        {
            for (auto &[notifyName, notify] : montage->GetAnimNotifies())
            {
                ImGui::PushID(notifyName.c_str());
                float start = notify.startTime;
                float end = notify.endTime;

                if (ImGui::DragFloat("Start", &start, 0.01f, 0.0f, 999.0f)
                    || ImGui::DragFloat("End", &end, 0.01f, 0.0f, 999.0f))
                {
                    montage->SetNotif(notifyName, AnimNotif(std::min(start, end), std::max(start, end)));
                    montage->SetDirtyFlag(true);
                }

                ImGui::SameLine();
                ImGui::TextUnformatted(notifyName.c_str());
                ImGui::SameLine();
                if (ImGui::SmallButton("Remove"))
                {
                    removeQueue.push_back(notifyName);
                }

                ImGui::Separator();
                ImGui::PopID();
            }
            ImGui::EndChild();
        }

        for (const std::string &notifyName : removeQueue)
        {
            montage->RemoveNotif(notifyName);
            montage->SetDirtyFlag(true);
        }
    }
    

    void AssetEditorPanel::RenderLocomotionControllerEditor(AssetEditorData &assetData)
    {
        bool isOpen = assetData.isOpen;
        if (BeginAssetEditorWindow(assetData, isOpen, ImVec2(1200.0f, 900.0f), ImVec2(560.0f, 640.0f), 0))
        {
            if (DrawAssetEditorHeader(assetData))
            {
                if (assetData.asset && assetData.asset->IsReady())
                {
                    if (Ref<LocomotionController> controller = assetData.asset->As<LocomotionController>())
                    {
                        RenderLocomotionControllerEditor(controller);
                    }
                    else
                    {
                        ImGui::Text("Loading asset...");
                    }
                }
                else
                {
                    ImGui::Text("Loading asset...");
                }
            }
        }

        RenderAssetEditorClosePopup(assetData, isOpen);
        ImGui::End();
        assetData.isOpen = isOpen;
        assetData.requestFocus = false;
    }

    void AssetEditorPanel::RenderLocomotionControllerEditor(const Ref<LocomotionController> &controller)
    {
        if (!controller || !m_EditorLayer || !m_EditorLayer->GetActiveProject())
        {
            return;
        }

        Project *project = m_EditorLayer->GetActiveProject().get();
        auto assetManager = project->GetAssetManager();

        if (controller->name.empty())
        {
            controller->name = "NewLocomotion";
        }

        char nameBuffer[256] {};
        std::strncpy(nameBuffer, controller->name.c_str(), sizeof(nameBuffer) - 1);
        if (ImGui::InputText("Controller Name", nameBuffer, sizeof(nameBuffer)))
        {
            controller->name = nameBuffer;
            controller->SetDirtyFlag(true);
        }

        ImGui::Button("Drop Skeleton", ImVec2(220.0f, 0.0f));
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
                        controller->skeletonHandle = droppedHandle;
                        controller->SetDirtyFlag(true);
                    }
                }
            }
            ImGui::EndDragDropTarget();
        }

        ImGui::TextDisabled("Skeleton Handle: %llu", static_cast<unsigned long long>(static_cast<uint64_t>(controller->skeletonHandle)));

        char defaultStateBuffer[256] {};
        std::strncpy(defaultStateBuffer, controller->defaultState.c_str(), sizeof(defaultStateBuffer) - 1);
        if (ImGui::InputText("Default State", defaultStateBuffer, sizeof(defaultStateBuffer)))
        {
            controller->defaultState = defaultStateBuffer;
            controller->SetDirtyFlag(true);
        }

        if (ImGui::Button("Add State"))
        {
            controller->states.push_back({ "NewState", false, AssetHandle(0) });
            controller->SetDirtyFlag(true);
        }

        if (ImGui::BeginChild("##locomotion_states", ImVec2(0.0f, 300.0f), ImGuiChildFlags_Borders))
        {
            int removeIndex = -1;
            for (size_t i = 0; i < controller->states.size(); ++i)
            {
                auto &state = controller->states[i];
                ImGui::PushID(static_cast<int>(i));

                char stateName[128] {};
                std::strncpy(stateName, state.name.c_str(), sizeof(stateName) - 1);
                if (ImGui::InputText("State Name", stateName, sizeof(stateName)))
                {
                    state.name = stateName;
                    controller->SetDirtyFlag(true);
                }

                if (ImGui::Checkbox("Use BlendSpace", &state.useBlendSpace))
                {
                    state.assetHandle = AssetHandle(0);
                    controller->SetDirtyFlag(true);
                }

                ImGui::Button(state.useBlendSpace ? "Drop BlendSpace (.bsp)" : "Drop Animation (.ixanim)", ImVec2(240.0f, 0.0f));
                if (ImGui::BeginDragDropTarget())
                {
                    if (const ImGuiPayload *payload = ImGui::AcceptDragDropPayload(DND_PAYLOAD_CONTENT_BROWSER_ITEM))
                    {
                        if (payload->Data && payload->DataSize == sizeof(AssetHandle))
                        {
                            const AssetHandle droppedHandle = *static_cast<const AssetHandle *>(payload->Data);
                            const AssetMetaData &metadata = assetManager->GetMetaData(droppedHandle);

                            const bool typeMatch = (state.useBlendSpace && metadata.type == AssetType::BlendSpace)
                                || (!state.useBlendSpace && metadata.type == AssetType::SkeletalAnimation);

                            if (typeMatch)
                            {
                                state.assetHandle = droppedHandle;
                                controller->SetDirtyFlag(true);
                            }
                        }
                    }
                    ImGui::EndDragDropTarget();
                }

                ImGui::TextDisabled("Asset Handle: %llu", static_cast<unsigned long long>(static_cast<uint64_t>(state.assetHandle)));

                if (ImGui::SmallButton("Remove State"))
                {
                    removeIndex = static_cast<int>(i);
                }

                ImGui::Separator();
                ImGui::PopID();
            }

            if (removeIndex >= 0)
            {
                controller->states.erase(controller->states.begin() + removeIndex);
                controller->SetDirtyFlag(true);
            }

            ImGui::EndChild();
        }
    }


    void AssetEditorPanel::RenderSkeletalSkeletonEditor(const Ref<Skeleton> &animation)
    {
        if (!animation)
        {
            return;
        }

        const uint64_t key = static_cast<uint64_t>(animation->handle);
        static std::unordered_map<uint64_t, int32_t> s_SelectedJoint;
        static std::unordered_map<uint64_t, int32_t> s_SelectedSocket;

        int32_t &selectedJoint = s_SelectedJoint[key];
        int32_t &selectedSocket = s_SelectedSocket[key];

        if (selectedJoint >= static_cast<int32_t>(animation->joints.size()))
        {
            selectedJoint = animation->joints.empty() ? -1 : 0;
        }
        if (selectedSocket >= static_cast<int32_t>(animation->sockets.size()))
        {
            selectedSocket = animation->sockets.empty() ? -1 : 0;
        }

        ImGui::Text("Joints: %zu", animation->joints.size());
        ImGui::Text("Sockets: %zu", animation->sockets.size());
        ImGui::Separator();

        std::vector<std::vector<int32_t>> children(animation->joints.size());
        for (const Joint &joint : animation->joints)
        {
            if (joint.parentJointId >= 0 && joint.parentJointId < static_cast<int32_t>(animation->joints.size()))
            {
                children[static_cast<size_t>(joint.parentJointId)].push_back(joint.id);
            }
        }

        // Left Side
        if (ImGui::BeginChild("##skeleton_joint_tree", ImVec2(0.0f, 280.0f), ImGuiChildFlags_Borders))
        {
            std::function<void(int32_t)> drawJointNode = [&](int32_t jointId)
            {
                if (jointId < 0 || jointId >= static_cast<int32_t>(animation->joints.size()))
                {
                    return;
                }

                const Joint &joint = animation->joints[static_cast<size_t>(jointId)];
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

                if (opened)
                {
                    for (int32_t childJointId : children[static_cast<size_t>(jointId)])
                    {
                        drawJointNode(childJointId);
                    }
                    ImGui::TreePop();
                }
            };

            for (const Joint &joint : animation->joints)
            {
                if (joint.parentJointId == -1)
                {
                    drawJointNode(joint.id);
                }
            }
        }
        ImGui::EndChild();


        ImGui::SameLine();

        if (selectedJoint >= 0 && selectedJoint < static_cast<int32_t>(animation->joints.size()))
        {
            const Joint &joint = animation->joints[static_cast<size_t>(selectedJoint)];
            ImGui::TextDisabled("Selected Joint: %s (id: %d)", joint.name.c_str(), joint.id);
        }

        ImGui::SeparatorText("Joint Sockets");
        if (ImGui::Button("Add Socket") && selectedJoint >= 0)
        {
            JointSocket socket;
            socket.name = std::format("Socket_{}", animation->sockets.size());
            socket.parentJointId = selectedJoint;
            animation->sockets.push_back(std::move(socket));
            animation->RebuildSocketMap();
            selectedSocket = static_cast<int32_t>(animation->sockets.size()) - 1;
            animation->SetDirtyFlag(true);
        }

        if (ImGui::BeginChild("##skeleton_socket_list", ImVec2(0.0f, 180.0f), ImGuiChildFlags_Borders))
        {
            for (int32_t i = 0; i < static_cast<int32_t>(animation->sockets.size()); ++i)
            {
                const JointSocket &socket = animation->sockets[static_cast<size_t>(i)];
                std::string row = socket.name;
                if (socket.parentJointId >= 0 && socket.parentJointId < static_cast<int32_t>(animation->joints.size()))
                {
                    row += std::format(" -> {}", animation->joints[static_cast<size_t>(socket.parentJointId)].name);
                }

                if (ImGui::Selectable(std::format("{}##socket_row_{}", row, i).c_str(), selectedSocket == i))
                {
                    selectedSocket = i;
                }
            }
        }
        ImGui::EndChild();

        if (selectedSocket >= 0 && selectedSocket < static_cast<int32_t>(animation->sockets.size()))
        {
            JointSocket &socket = animation->sockets[static_cast<size_t>(selectedSocket)];

            char nameBuf[256]{};
            std::strncpy(nameBuf, socket.name.c_str(), sizeof(nameBuf) - 1);
            if (ImGui::InputText("Socket Name", nameBuf, sizeof(nameBuf)))
            {
                socket.name = nameBuf;
                animation->RebuildSocketMap();
                animation->SetDirtyFlag(true);
            }

            int parentJoint = socket.parentJointId;
            std::string parentJointLabel = "None";
            if (parentJoint >= 0 && parentJoint < static_cast<int32_t>(animation->joints.size()))
            {
                parentJointLabel = animation->joints[static_cast<size_t>(parentJoint)].name;
            }

            if (ImGui::BeginCombo("Parent Joint", parentJointLabel.c_str()))
            {
                for (const Joint &joint : animation->joints)
                {
                    const bool selected = parentJoint == joint.id;
                    if (ImGui::Selectable(joint.name.c_str(), selected))
                    {
                        socket.parentJointId = joint.id;
                        animation->SetDirtyFlag(true);
                    }
                }
                ImGui::EndCombo();
            }

            if (ImGui::DragFloat3("Socket Translation", &socket.localTranslation.x, 0.01f))
            {
                animation->SetDirtyFlag(true);
            }

            glm::vec3 eulerDeg = glm::degrees(glm::eulerAngles(socket.localRotation));
            if (ImGui::DragFloat3("Socket Rotation", &eulerDeg.x, 0.1f))
            {
                socket.localRotation = glm::quat(glm::radians(eulerDeg));
                animation->SetDirtyFlag(true);
            }

            if (ImGui::DragFloat3("Socket Scale", &socket.localScale.x, 0.01f, 0.001f, 1000.0f))
            {
                animation->SetDirtyFlag(true);
            }

            if (ImGui::Button("Remove Socket"))
            {
                animation->sockets.erase(animation->sockets.begin() + selectedSocket);
                animation->RebuildSocketMap();
                selectedSocket = animation->sockets.empty() ? -1 : std::min(selectedSocket, static_cast<int32_t>(animation->sockets.size()) - 1);
                animation->SetDirtyFlag(true);
            }
        }
    }

    void AssetEditorPanel::RenderSkeletalSkeletonEditor(AssetEditorData &assetData)
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
                        RenderSkeletalSkeletonEditor(skeleton);
                    }
                    else
                    {
                        ImGui::Text("Loading asset...");
                    }
                }
                else
                {
                    ImGui::Text("Loading asset...");
                }
            }
        }

        RenderAssetEditorClosePopup(assetData, isOpen);
        ImGui::End();
        assetData.isOpen = isOpen;
        assetData.requestFocus = false;
    }

    void AssetEditorPanel::RenderSkeletalAnimationEditor(AssetEditorData &assetData)
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
                        RenderSkeletalAnimationEditor(animation);
                    }
                    else
                    {
                        ImGui::Text("Loading asset...");
                    }
                }
                else
                {
                    ImGui::Text("Loading asset...");
                }
            }
        }

        RenderAssetEditorClosePopup(assetData, isOpen);
        ImGui::End();
        assetData.isOpen = isOpen;
        assetData.requestFocus = false;
    }

    void AssetEditorPanel::RenderSkeletalAnimationEditor(const Ref<SkeletalAnimation> &animation)
    {
        ImGui::Text("Name: %s", animation->name.c_str());
        ImGui::Text("Duration: %.3f", animation->duration);
        ImGui::Text("Ticks Per Second: %.3f", animation->ticksPerSeconds);
        ImGui::Text("Channels: %zu", animation->channels.size());

        Project *project = m_EditorLayer ? m_EditorLayer->GetActiveProject().get() : nullptr;
        if (!project)
        {
            return;
        }

        auto assetManager = project->GetAssetManager();

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
    

    void AssetEditorPanel::RenderBlendSpaceEditor(AssetEditorData &assetData)
    {
        bool isOpen = assetData.isOpen;
        if (BeginAssetEditorWindow(assetData, isOpen, ImVec2(1100.0f, 840.0f), ImVec2(520.0f, 620.0f), 0))
        {
            if (DrawAssetEditorHeader(assetData))
            {
                if (assetData.asset && assetData.asset->IsReady())
                {
                    if (Ref<BlendSpace> blendSpace = assetData.asset->As<BlendSpace>())
                    {
                        RenderBlendSpaceEditor(blendSpace);
                    }
                    else
                    {
                        ImGui::Text("Loading asset...");
                    }
                }
                else
                {
                    ImGui::Text("Loading asset...");
                }
            }
        }

        RenderAssetEditorClosePopup(assetData, isOpen);
        ImGui::End();
        assetData.isOpen = isOpen;
        assetData.requestFocus = false;
    }

    void AssetEditorPanel::RenderBlendSpaceEditor(const Ref<BlendSpace> &blendSpace)
    {
        if (!blendSpace || !m_EditorLayer || !m_EditorLayer->GetActiveProject())
        {
            return;
        }

        Project *project = m_EditorLayer->GetActiveProject().get();
        auto assetManager = project->GetAssetManager();

        if (blendSpace->name.empty())
        {
            blendSpace->name = "NewBlendSpace";
        }

        char nameBuffer[256]{};
        std::strncpy(nameBuffer, blendSpace->name.c_str(), sizeof(nameBuffer) - 1);
        if (ImGui::InputText("BlendSpace Name", nameBuffer, sizeof(nameBuffer)))
        {
            blendSpace->name = nameBuffer;
            blendSpace->SetDirtyFlag(true);
        }

        ImGui::Button("Drop Skeleton", ImVec2(220.0f, 0.0f));
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
                        blendSpace->skeletonHandle = droppedHandle;
                        blendSpace->SetDirtyFlag(true);
                    }
                }
            }
            ImGui::EndDragDropTarget();
        }

        ImGui::TextDisabled("Skeleton Handle: %llu", static_cast<unsigned long long>(static_cast<uint64_t>(blendSpace->skeletonHandle)));
        char axisXBuffer[128]{};
        char axisYBuffer[128]{};
        std::strncpy(axisXBuffer, blendSpace->axisXName.c_str(), sizeof(axisXBuffer) - 1);
        std::strncpy(axisYBuffer, blendSpace->axisYName.c_str(), sizeof(axisYBuffer) - 1);
        if (ImGui::InputText("Axis X", axisXBuffer, sizeof(axisXBuffer)))
        {
            blendSpace->axisXName = axisXBuffer;
            blendSpace->SetDirtyFlag(true);
        }
        if (ImGui::InputText("Axis Y", axisYBuffer, sizeof(axisYBuffer)))
        {
            blendSpace->axisYName = axisYBuffer;
            blendSpace->SetDirtyFlag(true);
        }
        if (ImGui::DragFloat2("Axis Min", &blendSpace->axisMin.x, 0.1f)
            || ImGui::DragFloat2("Axis Max", &blendSpace->axisMax.x, 0.1f))
        {
            blendSpace->SetDirtyFlag(true);
        }

        ImGui::SeparatorText("Samples");
        if (ImGui::Button("Drop Animation (.ixanim)", ImVec2(220.0f, 0.0f))) {}
        if (ImGui::BeginDragDropTarget())
        {
            if (const ImGuiPayload *payload = ImGui::AcceptDragDropPayload(DND_PAYLOAD_CONTENT_BROWSER_ITEM))
            {
                if (payload->Data && payload->DataSize == sizeof(AssetHandle))
                {
                    const AssetHandle droppedHandle = *static_cast<const AssetHandle *>(payload->Data);
                    const AssetMetaData &metadata = assetManager->GetMetaData(droppedHandle);
                    if (metadata.type == AssetType::SkeletalAnimation)
                    {
                        bool validSkeleton = blendSpace->skeletonHandle == AssetHandle(0);
                        Ref<SkeletalAnimation> animation = project->GetAsset<SkeletalAnimation>(droppedHandle);
                        if (!animation)
                        {
                            animation = project->GetAssetImmediate<SkeletalAnimation>(droppedHandle);
                        }

                        if (animation)
                        {
                            if (blendSpace->skeletonHandle == AssetHandle(0))
                            {
                                blendSpace->skeletonHandle = animation->GetSkeletonHandle();
                                validSkeleton = true;
                            }
                            else
                            {
                                validSkeleton = animation->GetSkeletonHandle() == blendSpace->skeletonHandle;
                            }
                        }

                        if (validSkeleton)
                        {
                            blendSpace->samples.push_back({ droppedHandle, glm::vec2(0.0f, 0.0f) });
                            blendSpace->SetDirtyFlag(true);
                        }
                    }
                }
            }
            ImGui::EndDragDropTarget();
        }

        if (ImGui::BeginChild("##blend_space_samples", ImVec2(0.0f, 260.0f), ImGuiChildFlags_Borders))
        {
            int removeIndex = -1;
            for (size_t i = 0; i < blendSpace->samples.size(); ++i)
            {
                auto &sample = blendSpace->samples[i];
                ImGui::PushID(static_cast<int>(i));
                ImGui::Text("Anim: %llu", static_cast<unsigned long long>(static_cast<uint64_t>(sample.animationHandle)));
                if (ImGui::DragFloat2("Position", &sample.position.x, 0.1f))
                {
                    blendSpace->SetDirtyFlag(true);
                }

                if (ImGui::SmallButton("Remove"))
                {
                    removeIndex = static_cast<int>(i);
                }
                ImGui::Separator();
                ImGui::PopID();
            }

            if (removeIndex >= 0)
            {
                blendSpace->samples.erase(blendSpace->samples.begin() + removeIndex);
                blendSpace->SetDirtyFlag(true);
            }
            ImGui::EndChild();
        }
    }
#pragma endregion !3D_STUFF

    bool AssetEditorPanel::SaveAsset(AssetEditorData &assetData)
    {
        if (!m_EditorLayer || !m_EditorLayer->GetActiveProject() || !assetData.asset)
        {
            return false;
        }

        Project *project = m_EditorLayer->GetActiveProject().get();
        const std::filesystem::path savePath = project->GetAssetFilepath(assetData.metadata.filepath);

        switch (assetData.metadata.type)
        {
            case AssetType::SpriteSheet:
            {
                Ref<SpriteSheet> spriteSheet = assetData.asset->As<SpriteSheet>();
                if (!spriteSheet)
                {
                    return false;
                }

                if (!spriteSheet->Serialize(savePath))
                {
                    return false;
                }

                spriteSheet->SetDirtyFlag(false);
                return true;
            }

            case AssetType::Material2D:
            {
                Ref<Material2D> material2D = assetData.asset->As<Material2D>();
                if (!material2D)
                {
                    return false;
                }

                if (!material2D->Serialize(savePath))
                {
                    return false;
                }

                material2D->SetDirtyFlag(false);
                return true;
            }

            case AssetType::SkeletalAnimation:
            {
                Ref<SkeletalAnimation> animation = assetData.asset->As<SkeletalAnimation>();
                if (!animation)
                {
                    return false;
                }
                animation->Serialize(savePath);
                animation->SetDirtyFlag(false);
                return true;
            }

            case AssetType::Skeleton:
            {
                Ref<Skeleton> skeleton = assetData.asset->As<Skeleton>();
                if (!skeleton)
                {
                    return false;
                }

                if (!skeleton->Serialize(savePath))
                {
                    return false;
                }

                skeleton->SetDirtyFlag(false);
                return true;
            }

            case AssetType::AnimationMontage:
            {
                Ref<AnimationMontage> montage = assetData.asset->As<AnimationMontage>();
                if (!montage)
                {
                    return false;
                }

                if (!montage->Serialize(savePath))
                {
                    return false;
                }

                montage->SetDirtyFlag(false);
                return true;
            }

            case AssetType::BlendSpace:
            {
                Ref<BlendSpace> blendSpace = assetData.asset->As<BlendSpace>();
                if (!blendSpace)
                {
                    return false;
                }

                if (!blendSpace->Serialize(savePath))
                {
                    return false;
                }

                blendSpace->SetDirtyFlag(false);
                return true;
            }

            case AssetType::LocomotionController:
            {
                Ref<LocomotionController> controller = assetData.asset->As<LocomotionController>();
                if (!controller)
                {
                    return false;
                }

                if (!controller->Serialize(savePath))
                {
                    return false;
                }

                controller->SetDirtyFlag(false);
                return true;
            }

            case AssetType::Animation2D:
            {
                Ref<Animation2D> anim = assetData.asset->As<Animation2D>();
                if (!anim)
                {
                    return false;
                }
                std::filesystem::path fullPath = m_EditorLayer->GetActiveProject()->GetAssetDirectory() / assetData.metadata.filepath;
                return anim->Serialize(fullPath);
            }
            case AssetType::AnimatorController2D:
            {
                Ref<AnimatorController2D> ctrl = assetData.asset->As<AnimatorController2D>();
                if (!ctrl)
                {
                    return false;
                }
                std::filesystem::path fullPath = m_EditorLayer->GetActiveProject()->GetAssetDirectory() / assetData.metadata.filepath;
                return ctrl->Serialize(fullPath);
            }
            case AssetType::AnimatorController:
            {
                Ref<AnimatorController> ctrl = assetData.asset->As<AnimatorController>();
                if (!ctrl)
                {
                    return false;
                }
                std::filesystem::path fullPath = m_EditorLayer->GetActiveProject()->GetAssetDirectory() / assetData.metadata.filepath;
                return ctrl->Serialize(fullPath);
            }
            case AssetType::Material:
            {
                Ref<Material> mat = assetData.asset->As<Material>();
                if (!mat)
                {
                    return false;
                }

                std::filesystem::path fullPath = m_EditorLayer->GetActiveProject()->GetAssetDirectory() / assetData.metadata.filepath;
                mat->InvalidateBindingSet();
                return mat->Serialize(fullPath);
            }
            default:
            return false;
        }
    }

    void AssetEditorPanel::InitializeSceneData(AssetEditorData &assetData)
    {
        // Only some asset type can use scene renderer
        if (assetData.metadata.type != AssetType::Material)
            return;

        assetData.sceneData.sceneRenderer = CreateRef<AssetSceneRenderer>();
        assetData.sceneData.sceneRenderer->SetProject(m_EditorLayer->GetActiveProject().get());

        assetData.sceneData.camera = EditorCamera(std::format("AssetEditorCamera-{}", static_cast<uint64_t>(assetData.handle)));
        assetData.sceneData.camera.SetTarget(glm::vec3(0.0f));
        assetData.sceneData.camera.SetDistance(5.5f);
        assetData.sceneData.camera.yaw = glm::radians(90.0f);
        assetData.sceneData.camera.pitch = 0.0f;
        assetData.sceneData.camera.UpdateSphericalPosition();
        assetData.sceneData.camera.UpdateView();
        assetData.sceneData.camera.UpdateProjection(static_cast<float>(assetData.sceneData.viewportWidth), static_cast<float>(assetData.sceneData.viewportHeight));

        RenderTargetCreateInfo rtCreateInfo = {};
        rtCreateInfo.width = assetData.sceneData.viewportWidth;
        rtCreateInfo.height = assetData.sceneData.viewportHeight;
        rtCreateInfo.attachments =
        {
            FramebufferAttachments{ "[Asset Preview DepthAttachment]", nvrhi::Format::D32S8, nvrhi::ResourceStates::DepthWrite},
            FramebufferAttachments{ "[Asset Preview ColorAttachment]", nvrhi::Format::RGBA8_UNORM, nvrhi::ResourceStates::RenderTarget},
            FramebufferAttachments{ "[Asset Preview ObjectIDAttachment]", nvrhi::Format::R32_UINT, nvrhi::ResourceStates::RenderTarget}
        };

        assetData.sceneData.sceneRT = RenderTarget::Create(rtCreateInfo, "[Asset Preview Scene RT]");
        assetData.sceneData.uiRT = RenderTarget::Create(rtCreateInfo, "[Asset Preview UI RT]");

        RenderTargetCreateInfo compositeInfo = {};
        compositeInfo.width = assetData.sceneData.viewportWidth;
        compositeInfo.height = assetData.sceneData.viewportHeight;
        compositeInfo.attachments =
        {
            FramebufferAttachments{ "[Asset Preview Composite ColorAttachment]", nvrhi::Format::RGBA8_UNORM, nvrhi::ResourceStates::RenderTarget}
        };
        assetData.sceneData.compositeRT = RenderTarget::Create(compositeInfo, "[Asset Preview Composite RT]");

        if (assetData.sceneData.sceneRenderer)
        {
            Ref<StaticMesh> previewMesh = s_DefaultMeshes[CUBE];
            assetData.sceneData.sceneRenderer->SetPreviewMesh(previewMesh);
        }
    }

    void AssetEditorPanel::UpdateMaterialPreviewCamera(EditorSceneData &sceneData, float deltaTime)
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
        sceneData.camera.UpdateView();
    }

    std::filesystem::path AssetEditorPanel::BuildUniqueAssetPath(const std::filesystem::path &baseDirectory, const std::string &baseName, const std::string &extension) const
    {
        std::filesystem::path candidate = baseDirectory / (baseName + extension);
        if (!std::filesystem::exists(candidate))
        {
            return candidate;
        }

        uint32_t suffix = 1;
        while (true)
        {
            candidate = baseDirectory / std::format("{}_{}{}", baseName, suffix, extension);
            if (!std::filesystem::exists(candidate))
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
        dispatcher.Dispatch<AssetEditorOpenEvent>(BIND_CLASS_EVENT_FN(AssetEditorPanel::OnAssetEditorOpenEvent));
        dispatcher.Dispatch<AssetEditorCreateEvent>(BIND_CLASS_EVENT_FN(AssetEditorPanel::OnAssetEditorCreateEvent));
        dispatcher.Dispatch<MouseScrolledEvent>(BIND_CLASS_EVENT_FN(AssetEditorPanel::OnMouseScrollEvent));
    }

    bool AssetEditorPanel::OnAssetEditorOpenEvent(AssetEditorOpenEvent &event)
    {
        auto handle = event.GetAssetHandle();
        auto &metadata = event.GetAssetMetaData();
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

        auto assetManager = m_EditorLayer->GetActiveProject()->GetAssetManager();
        Ref<Asset> asset = assetManager->GetAsset(handle);
        if (!asset)
        {
            asset = assetManager->GetAssetImmediate(handle);
        }

        if (asset)
        {
            std::string assetName = metadata.filepath.filename().string();
            if (assetName.empty())
            {
                assetName = metadata.filepath.generic_string();
            }

            AssetEditorData data;
            data.asset = asset;
            data.metadata = metadata;
            data.handle = handle;
            data.isOpen = true;
            data.requestFocus = true;
            data.windowTitle = std::format("{} - {}###asset_editor_{}", AssetTypeToString(metadata.type), assetName, static_cast<uint64_t>(handle));

            InitializeSceneData(data);

            m_Assets.push_back(std::move(data));
            return true;
        }

        return false;
    }

    bool AssetEditorPanel::OnAssetEditorCreateEvent(AssetEditorCreateEvent &event)
    {
        if (!m_EditorLayer || !m_EditorLayer->GetActiveProject())
        {
            return false;
        }

        if (event.GetAssetType() == AssetType::Invalid)
        {
            return false;
        }

        m_CreateRequest = {};
        m_CreateRequest.type = event.GetAssetType();
        m_CreateRequest.targetDirectory = event.GetTargetDirectory();
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

        return true;
    }

    bool AssetEditorPanel::OnMouseScrollEvent(MouseScrolledEvent &event)
    {
        return false;
    }
}
