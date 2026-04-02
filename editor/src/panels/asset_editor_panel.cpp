// Copyright (c) 2026 Evangelion Manuhutu

#include "asset_editor_panel.hpp"

#include "../editor_layer.hpp"
#include "ignite/animation/animation_2d.hpp"
#include "ignite/animation/animator_controller_2d.hpp"
#include "ignite/animation/skeletal_animation.hpp"
#include "ignite/asset/asset_importer.hpp"
#include "ignite/graphics/objects/material_2d.hpp"
#include "ignite/graphics/texture.hpp"
#include "ignite/project/project.hpp"
#include "ignite/scene/sprite_sheet.hpp"
#include "ignite/serializer/binary_serializer.hpp"
#include "ignite/serializer/serializer.hpp"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <format>
#include <iterator>
#include <ranges>
#include <unordered_map>
#include <vector>


namespace ignite
{
    namespace
    {
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
            std::vector<std::string> spriteNames;
        };

        static std::unordered_map<uint64_t, SpriteSheetEditorState> s_SpriteSheetEditorState;

        // ---- Animation2D per-asset playback preview state ----
        struct Animation2DEditorState
        {
            float   playbackTime  = 0.0f;  // seconds into animation
            float   lastRealTime  = 0.0f;  // ImGui time stamp when last ticked
            bool    playing       = false;
            int     previewFrame  = 0;
            float   previewZoom   = 1.0f;
            float   toolsWidth    = 280.0f;
        };

        static std::unordered_map<uint64_t, Animation2DEditorState> s_Anim2DEditorState;

        static const char *TextureFormatToString(nvrhi::Format format) 
        {
            switch (format) {
            case nvrhi::Format::RGBA8_UNORM:
                return "RGBA8_UNORM";
            case nvrhi::Format::RGBA32_FLOAT:
                return "RGBA32_FLOAT";
            default:
                return "UNKNOWN";
            }
        }

        static const char *SamplerAddressModeToString(nvrhi::SamplerAddressMode mode) 
        {
            switch (mode) {
            case nvrhi::SamplerAddressMode::Repeat:
                return "Repeat";
            case nvrhi::SamplerAddressMode::ClampToEdge:
                return "ClampToEdge";
            case nvrhi::SamplerAddressMode::ClampToBorder:
                return "ClampToBorder";
            default:
                return "Other";
            }
        }
    } // namespace

    AssetEditorPanel::AssetEditorPanel(const char *windowTitle, EditorLayer *editor)
        : IPanel(windowTitle, editor)
    {
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

    void AssetEditorPanel::RenderCreateAssetPopup()
    {
        if (!m_CreateRequest.open || !m_EditorLayer || !m_EditorLayer->GetActiveProject())
        {
            return;
        }

        Project *project = m_EditorLayer->GetActiveProject().get();
        auto &assetManager = project->GetAssetManager();

        if (m_CreateRequest.type == AssetType::Material2D && !m_CreateRequest.asset)
        {
            m_CreateRequest.asset = CreateRef<Material2D>();
        }

        if (m_CreateRequest.type == AssetType::SpriteSheet && !m_CreateRequest.asset)
        {
            m_CreateRequest.asset = CreateRef<SpriteSheet>();
        }

        ImGui::SetNextWindowSize(ImVec2(1200.0f, 760.0f), ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowSizeConstraints(ImVec2(900.0f, 640.0f),
            ImVec2(FLT_MAX, FLT_MAX));
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
                Ref<Asset> createdAsset = nullptr;
                if (m_CreateRequest.type == AssetType::Material2D)
                {
                    Ref<Material2D> asset = std::dynamic_pointer_cast<Material2D>(m_CreateRequest.asset);
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
                    Ref<SpriteSheet> asset = std::dynamic_pointer_cast<SpriteSheet>(m_CreateRequest.asset);
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
                    Ref<Animation2D> asset = std::dynamic_pointer_cast<Animation2D>(m_CreateRequest.asset);
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
                    Ref<AnimatorController2D> asset = std::dynamic_pointer_cast<AnimatorController2D>(m_CreateRequest.asset);
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
                    assetManager.AssignMetaData(handle, metadata);
                    assetManager.AssignAsset(handle, createdAsset);

                    // TODO: Fix save project assets
                    m_EditorLayer->SaveProject();

                    AssetEditorData data;
                    data.asset = createdAsset;
                    data.metadata = metadata;
                    data.handle = handle;
                    data.isOpen = true;
                    data.requestFocus = true;
                    data.windowTitle = std::format("{} - {}###asset_editor_{}", AssetTypeToString(metadata.type), fullAssetPath.filename().string(), static_cast<uint64_t>(handle));
                    
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
                Ref<Material2D> asset = std::dynamic_pointer_cast<Material2D>(m_CreateRequest.asset);
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
                Ref<SpriteSheet> asset = std::dynamic_pointer_cast<SpriteSheet>(m_CreateRequest.asset);
                if (!asset)
                {
                    ImGui::TextColored(ImVec4(1.0f, 0.3f, 0.3f, 1.0f), "Invalid asset instance for SpriteSheet creation.");
                    ImGui::End();
                    return;
                }

                ImGui::Separator();
                RenderSpriteSheetEditor(asset);
            }

            if (m_CreateRequest.type == AssetType::Animation2D)
            {
                Ref<Animation2D> asset = std::dynamic_pointer_cast<Animation2D>(m_CreateRequest.asset);
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
                Ref<AnimatorController2D> asset = std::dynamic_pointer_cast<AnimatorController2D>(m_CreateRequest.asset);
                if (!asset)
                {
                    ImGui::TextColored(ImVec4(1.0f, 0.3f, 0.3f, 1.0f), "Invalid asset instance for AnimatorController2D creation.");
                    ImGui::End();
                    return;
                }

                ImGui::Separator();
                RenderAnimatorController2DEditor(asset);
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

    void AssetEditorPanel::RenderSpriteSheetEditor(const Ref<SpriteSheet> &spriteSheet)
    {
        if (!spriteSheet)
        {
            return;
        }

        Project *project = m_EditorLayer->GetActiveProject().get();
        auto &assetManager = project->GetAssetManager();

        const uint64_t stateKey = static_cast<uint64_t>(spriteSheet->handle);
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

                const bool mouseInsideImage = hovered && ImGui::GetMousePos().x >= imagePos.x &&
                    ImGui::GetMousePos().x <= imagePos.x + imageSize.x &&
                    ImGui::GetMousePos().y >= imagePos.y &&
                    ImGui::GetMousePos().y <= imagePos.y + imageSize.y;

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
                else if (mouseInsideImage && hasSelection && hoveredHandle == -1 &&
                    ImRect(currentSelMin, currentSelMax)
                    .Contains(ImGui::GetMousePos()))
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
                    else if (hasSelection && ImRect(currentSelMin, currentSelMax)
                        .Contains(ImGui::GetMousePos()))
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
                        const auto &selectedSprite =
                            sprites[static_cast<size_t>(clickedSpriteIndex)];
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
                            const glm::vec2 sizeUV =
                                state.dragStartMaxUV - state.dragStartMinUV;
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
                                minUV.x =
                                    std::clamp(mouseUV.x, 0.0f, state.dragStartMaxUV.x - epsilon);
                                minUV.y =
                                    std::clamp(mouseUV.y, 0.0f, state.dragStartMaxUV.y - epsilon);
                                break;
                                case 1: // top-right
                                maxUV.x =
                                    std::clamp(mouseUV.x, state.dragStartMinUV.x + epsilon, 1.0f);
                                minUV.y =
                                    std::clamp(mouseUV.y, 0.0f, state.dragStartMaxUV.y - epsilon);
                                break;
                                case 2: // bottom-right
                                maxUV.x =
                                    std::clamp(mouseUV.x, state.dragStartMinUV.x + epsilon, 1.0f);
                                maxUV.y =
                                    std::clamp(mouseUV.y, state.dragStartMinUV.y + epsilon, 1.0f);
                                break;
                                case 3: // bottom-left
                                minUV.x =
                                    std::clamp(mouseUV.x, 0.0f, state.dragStartMaxUV.x - epsilon);
                                maxUV.y =
                                    std::clamp(mouseUV.y, state.dragStartMinUV.y + epsilon, 1.0f);
                                break;
                                case 4: // top
                                minUV.y =
                                    std::clamp(mouseUV.y, 0.0f, state.dragStartMaxUV.y - epsilon);
                                break;
                                case 5: // right
                                maxUV.x =
                                    std::clamp(mouseUV.x, state.dragStartMinUV.x + epsilon, 1.0f);
                                break;
                                case 6: // bottom
                                maxUV.y =
                                    std::clamp(mouseUV.y, state.dragStartMinUV.y + epsilon, 1.0f);
                                break;
                                case 7: // left
                                minUV.x =
                                    std::clamp(mouseUV.x, 0.0f, state.dragStartMaxUV.x - epsilon);
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

                const glm::vec2 uvMin =
                    glm::min(state.selectionStartUV, state.selectionEndUV);
                const glm::vec2 uvMax =
                    glm::max(state.selectionStartUV, state.selectionEndUV);

                if (selectionFinishedThisFrame && state.selectedSpriteIndex == -1)
                {
                    if ((uvMax.x - uvMin.x) > 0.0001f && (uvMax.y - uvMin.y) > 0.0001f)
                    {
                        sprites.push_back({ uvMin, uvMax });
                        state.spriteNames.push_back(
                            std::format("Sprite {}", sprites.size() - 1));
                        state.selectedSpriteIndex = static_cast<int>(sprites.size()) - 1;
                        spriteSheet->SetDirtyFlag(true);
                    }
                }

                if (state.selectedSpriteIndex >= 0 &&
                    state.selectedSpriteIndex < static_cast<int>(sprites.size()) &&
                    state.activeHandle != -1)
                {
                    auto &selectedSprite =
                        sprites[static_cast<size_t>(state.selectedSpriteIndex)];
                    selectedSprite.uv0 = uvMin;
                    selectedSprite.uv1 = uvMax;
                    spriteSheet->SetDirtyFlag(true);
                }

                const ImVec2 selMin = { imagePos.x + uvMin.x * imageSize.x,
                                       imagePos.y + uvMin.y * imageSize.y };
                const ImVec2 selMax = { imagePos.x + uvMax.x * imageSize.x,
                                       imagePos.y + uvMax.y * imageSize.y };
                drawList->AddRectFilled(selMin, selMax, IM_COL32(255, 220, 50, 30));
                drawList->AddRect(selMin, selMax, IM_COL32(255, 220, 50, 255), 0.0f, 0,
                    2.0f);

                if ((uvMax.x - uvMin.x) > 0.0001f && (uvMax.y - uvMin.y) > 0.0001f)
                {
                    const ImVec2 midTop = ImVec2((selMin.x + selMax.x) * 0.5f, selMin.y);
                    const ImVec2 midRight = ImVec2(selMax.x, (selMin.y + selMax.y) * 0.5f);
                    const ImVec2 midBottom = ImVec2((selMin.x + selMax.x) * 0.5f, selMax.y);
                    const ImVec2 midLeft = ImVec2(selMin.x, (selMin.y + selMax.y) * 0.5f);

                    const ImVec2 handlesDraw[8] = { selMin,    ImVec2(selMax.x, selMin.y),
                                                   selMax,    ImVec2(selMin.x, selMax.y),
                                                   midTop,    midRight,
                                                   midBottom, midLeft };

                    for (const ImVec2 &handlePos : handlesDraw)
                    {
                        const ImVec2 hMin = ImVec2(handlePos.x - 4.0f, handlePos.y - 4.0f);
                        const ImVec2 hMax = ImVec2(handlePos.x + 4.0f, handlePos.y + 4.0f);
                        drawList->AddRectFilled(hMin, hMax, IM_COL32(255, 255, 255, 230));
                        drawList->AddRect(hMin, hMax, IM_COL32(20, 20, 20, 255), 0.0f, 0,
                            1.0f);
                    }
                }

                for (size_t i = 0; i < sprites.size(); ++i)
                {
                    const auto &sprite = sprites[i];
                    const ImVec2 blockMin = { imagePos.x + sprite.uv0.x * imageSize.x,
                                             imagePos.y + sprite.uv0.y * imageSize.y };
                    const ImVec2 blockMax = { imagePos.x + sprite.uv1.x * imageSize.x,
                                             imagePos.y + sprite.uv1.y * imageSize.y };
                    const bool isSelectedSprite =
                        state.selectedSpriteIndex == static_cast<int>(i);
                    drawList->AddRect(blockMin, blockMax,
                        isSelectedSprite ? IM_COL32(255, 128, 0, 255)
                        : IM_COL32(0, 220, 255, 200),
                        0.0f, 0, isSelectedSprite ? 2.0f : 1.5f);
                }
            }
            else
            {
                ImGui::SetCursorScreenPos(
                    ImVec2(viewportPos.x + 12.0f, viewportPos.y + 12.0f));
                ImGui::Text("Drop a texture to preview SpriteSheet");
            }
        }
        ImGui::EndChild();

        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.20f, 0.20f, 0.20f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered,
            ImVec4(0.28f, 0.28f, 0.28f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive,
            ImVec4(0.32f, 0.32f, 0.32f, 1.0f));
        ImGui::Button("##sprite_sheet_vertical_splitter",
            ImVec2(-1.0f, splitterThickness));
        if (ImGui::IsItemHovered() || ImGui::IsItemActive())
        {
            ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeNS);
        }
        if (ImGui::IsItemActive())
        {
            state.extractedPanelHeight -= ImGui::GetIO().MouseDelta.y;
            state.extractedPanelHeight = std::clamp(
                state.extractedPanelHeight, minExtractedHeight,
                std::max(minExtractedHeight,
                    totalLeftHeight - minViewportHeight - splitterThickness));
        }
        ImGui::PopStyleColor(3);

        ImGui::Text("Extracted Sprites");
        ImGui::BeginChild("##sprite_sheet_extracted_preview",
            ImVec2(0.0f, state.extractedPanelHeight),
            ImGuiChildFlags_Borders);
        if (texture && texture->GetHandle())
        {
            const float previewSize = 56.0f;
            const float spacing = ImGui::GetStyle().ItemSpacing.x;
            const float contentWidth = ImGui::GetContentRegionAvail().x;
            const int spritesPerRow = std::max(
                1, static_cast<int>((contentWidth + spacing) /
                    (previewSize + spacing + horizontalSplitterWidth)));
            for (size_t i = 0; i < sprites.size(); ++i)
            {
                const auto &sprite = sprites[i];
                ImGui::PushID(static_cast<int>(i));

                ImTextureID texId =
                    reinterpret_cast<ImTextureID>(texture->GetHandle().Get());
                ImGui::ImageButton("##sprite_preview", texId,
                    ImVec2(previewSize, previewSize),
                    ImVec2(sprite.uv0.x, sprite.uv0.y),
                    ImVec2(sprite.uv1.x, sprite.uv1.y));

                if (ImGui::BeginDragDropSource())
                {
                    SpriteSheetSpritePayload payload;
                    payload.spriteSheetHandle = spriteSheet->handle;
                    payload.textureHandle = spriteSheet->GetTextureHandle();
                    payload.spriteIndex = static_cast<uint32_t>(i);
                    payload.uv0 = sprite.uv0;
                    payload.uv1 = sprite.uv1;

                    ImGui::SetDragDropPayload("sprite_sheet_item", &payload,
                        sizeof(payload));
                    ImGui::Text("Sprite %zu", i);
                    ImGui::EndDragDropSource();
                }

                if (state.selectedSpriteIndex == static_cast<int>(i))
                {
                    ImDrawList *selectionDrawList = ImGui::GetWindowDrawList();
                    selectionDrawList->AddRect(ImGui::GetItemRectMin(),
                        ImGui::GetItemRectMax(),
                        IM_COL32(255, 180, 0, 255), 2.0f, 0, 2.0f);
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
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered,
            ImVec4(0.28f, 0.28f, 0.28f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive,
            ImVec4(0.32f, 0.32f, 0.32f, 1.0f));
        ImGui::BeginChild("##sprite_sheet_horizontal_splitter",
            ImVec2(horizontalSplitterWidth, 0.0f),
            ImGuiChildFlags_None);
        ImGui::Button("##sprite_sheet_horizontal_splitter_btn", ImVec2(-1.0f, -1.0f));

        if (ImGui::IsItemHovered() || ImGui::IsItemActive())
        {
            ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeEW);
        }

        if (ImGui::IsItemActive())
        {
            state.previewColumnWidth += ImGui::GetIO().MouseDelta.x;
            state.previewColumnWidth = std::clamp(
                state.previewColumnWidth, minPreviewColumnWidth, maxPreviewColumnWidth);
        }

        ImGui::EndChild();
        ImGui::PopStyleColor(3);

        ImGui::SameLine(0.0f, 0.0f);
        ImGui::BeginChild("##sprite_sheet_tools_column", ImVec2(0.0f, 0.0f),
            ImGuiChildFlags_None);

        ImGui::Spacing();
        std::string textureLabel = spriteSheet->GetTextureHandle() == AssetHandle(0)
            ? "Drop Texture Here"
            : "Texture Loaded";
        ImGui::Button(textureLabel.c_str(), ImVec2(220.0f, 0.0f));
        if (ImGui::BeginDragDropTarget())
        {
            if (const ImGuiPayload *payload =
                ImGui::AcceptDragDropPayload("content_browser_item"))
            {
                if (payload->Data && payload->DataSize == sizeof(AssetHandle))
                {
                    const AssetHandle droppedHandle =
                        *static_cast<const AssetHandle *>(payload->Data);
                    const AssetMetaData &droppedMetadata =
                        assetManager.GetMetaData(droppedHandle);
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

        ImGui::Text("Texture Handle: %llu",
            static_cast<unsigned long long>(
                static_cast<uint64_t>(spriteSheet->GetTextureHandle())));

        glm::vec2 atlasSize = spriteSheet->GetAtlasSize();
        if (ImGui::DragFloat2("Atlas Cell Size", &atlasSize.x, 1.0f, 1.0f, 8192.0f,
            "%.0f"))
        {
            spriteSheet->SetAtlasSize(atlasSize);
            spriteSheet->SetDirtyFlag(true);
        }

        const glm::vec2 uvMin =
            glm::min(state.selectionStartUV, state.selectionEndUV);
        const glm::vec2 uvMax =
            glm::max(state.selectionStartUV, state.selectionEndUV);

        ImGui::Text("Selected UV0: %.3f, %.3f", uvMin.x, uvMin.y);
        ImGui::Text("Selected UV1: %.3f, %.3f", uvMax.x, uvMax.y);

        const bool hasValidSelectionArea = uvMax.x > uvMin.x && uvMax.y > uvMin.y;
        const bool editingSelectedSprite =
            state.selectedSpriteIndex >= 0 &&
            state.selectedSpriteIndex < static_cast<int>(sprites.size());
        if (editingSelectedSprite && ImGui::Button("Apply To Selected"))
        {
            if (hasValidSelectionArea)
            {
                auto &selectedSprite =
                    sprites[static_cast<size_t>(state.selectedSpriteIndex)];
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
        if (ImGui::Button("Remove Selected") && state.selectedSpriteIndex >= 0 &&
            state.selectedSpriteIndex < static_cast<int>(sprites.size()))
        {
            sprites.erase(sprites.begin() + state.selectedSpriteIndex);
            state.spriteNames.erase(state.spriteNames.begin() +
                state.selectedSpriteIndex);
            state.selectedSpriteIndex = -1;
            state.renamingSpriteIndex = -1;
            spriteSheet->SetDirtyFlag(true);
        }

        ImGui::BeginChild("##sprite_sheet_sprite_list", ImVec2(0.0f, 130.0f),
            ImGuiChildFlags_Borders);
        for (size_t i = 0; i < sprites.size(); ++i)
        {
            const auto &sprite = sprites[i];
            const bool selected = state.selectedSpriteIndex == static_cast<int>(i);
            const std::string rowLabel =
                std::format("{}##sprite_row_{}", state.spriteNames[i], i);
            if (ImGui::Selectable(rowLabel.c_str(), selected))
            {
                state.selectedSpriteIndex = static_cast<int>(i);
                state.selectionStartUV = sprite.uv0;
                state.selectionEndUV = sprite.uv1;
                state.renamingSpriteIndex = state.selectedSpriteIndex;
                std::strncpy(state.renameBuffer, state.spriteNames[i].c_str(),
                    sizeof(state.renameBuffer) - 1);
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
            ImGui::TextDisabled("(%.3f, %.3f) -> (%.3f, %.3f)", sprite.uv0.x,
                sprite.uv0.y, sprite.uv1.x, sprite.uv1.y);
        }
        ImGui::EndChild();

        const bool canMoveUp =
            state.selectedSpriteIndex > 0 &&
            state.selectedSpriteIndex < static_cast<int>(sprites.size());
        if (!canMoveUp)
            ImGui::BeginDisabled();
        if (ImGui::Button("Move Up"))
        {
            const int i = state.selectedSpriteIndex;
            std::swap(sprites[static_cast<size_t>(i)],
                sprites[static_cast<size_t>(i - 1)]);
            std::swap(state.spriteNames[static_cast<size_t>(i)],
                state.spriteNames[static_cast<size_t>(i - 1)]);
            state.selectedSpriteIndex = i - 1;
            spriteSheet->SetDirtyFlag(true);
        }
        if (!canMoveUp)
            ImGui::EndDisabled();

        ImGui::SameLine();
        const bool canMoveDown =
            state.selectedSpriteIndex >= 0 &&
            state.selectedSpriteIndex < static_cast<int>(sprites.size()) - 1;
        if (!canMoveDown)
            ImGui::BeginDisabled();
        if (ImGui::Button("Move Down"))
        {
            const int i = state.selectedSpriteIndex;
            std::swap(sprites[static_cast<size_t>(i)],
                sprites[static_cast<size_t>(i + 1)]);
            std::swap(state.spriteNames[static_cast<size_t>(i)],
                state.spriteNames[static_cast<size_t>(i + 1)]);
            state.selectedSpriteIndex = i + 1;
            spriteSheet->SetDirtyFlag(true);
        }
        if (!canMoveDown)
            ImGui::EndDisabled();

        if (state.selectedSpriteIndex >= 0 &&
            state.selectedSpriteIndex < static_cast<int>(sprites.size()))
        {
            if (state.renamingSpriteIndex != state.selectedSpriteIndex)
            {
                state.renamingSpriteIndex = state.selectedSpriteIndex;
                std::strncpy(
                    state.renameBuffer,
                    state.spriteNames[static_cast<size_t>(state.selectedSpriteIndex)]
                    .c_str(),
                    sizeof(state.renameBuffer) - 1);
                state.renameBuffer[sizeof(state.renameBuffer) - 1] = '\0';
            }
            if (ImGui::InputText("Sprite Name", state.renameBuffer,
                sizeof(state.renameBuffer)))
            {
                state.spriteNames[static_cast<size_t>(state.selectedSpriteIndex)] =
                    state.renameBuffer;
            }

            auto &sprite = sprites[static_cast<size_t>(state.selectedSpriteIndex)];
            glm::vec2 selectedUv0 = sprite.uv0;
            glm::vec2 selectedUv1 = sprite.uv1;
            if (ImGui::DragFloat2("Sprite UV0", &selectedUv0.x, 0.001f, 0.0f, 1.0f,
                "%.3f") ||
                ImGui::DragFloat2("Sprite UV1", &selectedUv1.x, 0.001f, 0.0f, 1.0f,
                    "%.3f"))
            {
                sprite.uv0 = glm::clamp(glm::min(selectedUv0, selectedUv1),
                    glm::vec2(0.0f), glm::vec2(1.0f));
                sprite.uv1 = glm::clamp(glm::max(selectedUv0, selectedUv1),
                    glm::vec2(0.0f), glm::vec2(1.0f));
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
                ImGui::DragFloat("Snap Step U", &state.snapStepU, 0.001f, 0.001f, 1.0f,
                    "%.3f");
                ImGui::DragFloat("Snap Step V", &state.snapStepV, 0.001f, 0.001f, 1.0f,
                    "%.3f");
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
                    state.spriteNames.push_back(
                        std::format("Sprite {}", sprites.size() - 1));
                }
            }

            if (texture && texture->GetWidth() > 0 && texture->GetHeight() > 0)
            {
                spriteSheet->SetAtlasSize(
                    { static_cast<float>(texture->GetWidth()) / static_cast<float>(cols),
                     static_cast<float>(texture->GetHeight()) /
                         static_cast<float>(rows) });
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

    void AssetEditorPanel::RenderMaterial2DEditor(const Ref<Material2D> &material2D)
    {
        if (!material2D || !m_EditorLayer || !m_EditorLayer->GetActiveProject())
        {
            return;
        }

        auto &assetManager = m_EditorLayer->GetActiveProject()->GetAssetManager();

        const char *materialTypeLabel =
            material2D->data.type == MATERIAL_2D_TYPE_LIT ? "Lit" : "Unlit";
        if (ImGui::BeginCombo("Material Type", materialTypeLabel))
        {
            if (ImGui::Selectable("Unlit",
                material2D->data.type == MATERIAL_2D_TYPE_UNLIT))
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

        std::string textureLabel = material2D->textureHandle == AssetHandle(0) ? "Drop Texture Here" : "Texture Loaded";
        ImGui::Button(textureLabel.c_str(), ImVec2(220.0f, 0.0f));
        if (ImGui::BeginDragDropTarget())
        {
            if (const ImGuiPayload *payload =
                ImGui::AcceptDragDropPayload("content_browser_item"))
            {
                if (payload->Data && payload->DataSize == sizeof(AssetHandle))
                {
                    const AssetHandle droppedHandle = *static_cast<const AssetHandle *>(payload->Data);
                    const AssetMetaData &droppedMetadata = assetManager.GetMetaData(droppedHandle);
                    if (droppedMetadata.type == AssetType::Texture)
                    {
                        material2D->textureHandle = droppedHandle;
                        material2D->SetDirtyFlag(true);
                    }
                }
            }
            ImGui::EndDragDropTarget();
        }

        if (material2D->textureHandle != AssetHandle(0))
        {
            ImGui::SameLine();
            if (ImGui::Button("Clear Texture"))
            {
                material2D->textureHandle = AssetHandle(0);
                material2D->SetDirtyFlag(true);
            }
        }

        ImGui::Text("Texture Handle: %llu", static_cast<unsigned long long>(static_cast<uint64_t>(material2D->textureHandle)));
    }

    void AssetEditorPanel::RenderTextureEditor(AssetEditorData &assetData,
        const Ref<Texture> &texture) {
        if (!texture || !m_EditorLayer || !m_EditorLayer->GetActiveProject())
        {
            return;
        }

        Project *project = m_EditorLayer->GetActiveProject().get();
        auto &assetManager = project->GetAssetManager();

        const uint64_t stateKey = static_cast<uint64_t>(assetData.handle);
        TextureEditorState &state = s_TextureEditorState[stateKey];
        if (!state.initialized)
        {
            state.createInfo = texture->GetCreateInfo();
            state.initialized = true;
        }

        ImGui::Text("Path: %s", assetData.metadata.filepath.generic_string().c_str());
        ImGui::Text("Resolution: %d x %d", texture->GetWidth(), texture->GetHeight());
        ImGui::Text("Channels: %d", texture->GetChannels());
        ImGui::Text("Current Format: %s",
            TextureFormatToString(texture->GetFormat()));

        const float previewMaxWidth =
            std::min(320.0f, ImGui::GetContentRegionAvail().x);
        if (previewMaxWidth > 0.0f && texture->GetWidth() > 0 &&
            texture->GetHeight() > 0)
        {
            const float aspectRatio = static_cast<float>(texture->GetWidth()) /
                static_cast<float>(texture->GetHeight());
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
            ImTextureID textureId =
                reinterpret_cast<ImTextureID>(texture->GetHandle().Get());
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
            state.createInfo.sampleCount =
                static_cast<uint32_t>(std::max(sampleCount, 1));
        }

        int sampleQuality = static_cast<int>(state.createInfo.sampleQuality);
        if (ImGui::DragInt("Sample Quality", &sampleQuality, 1.0f, 0, 16))
        {
            state.createInfo.sampleQuality =
                static_cast<uint32_t>(std::max(sampleQuality, 0));
        }

        const nvrhi::Format formatOptions[] = { nvrhi::Format::RGBA8_UNORM,
                                               nvrhi::Format::RGBA32_FLOAT };
        int currentFormatIndex = 0;
        for (int i = 0; i < static_cast<int>(std::size(formatOptions)); ++i)
        {
            if (state.createInfo.format == formatOptions[i])
            {
                currentFormatIndex = i;
                break;
            }
        }

        if (ImGui::BeginCombo(
            "Import Format",
            TextureFormatToString(formatOptions[currentFormatIndex])))
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

        const nvrhi::SamplerAddressMode addressModeOptions[] = {
            nvrhi::SamplerAddressMode::Repeat, nvrhi::SamplerAddressMode::ClampToEdge,
            nvrhi::SamplerAddressMode::ClampToBorder };

        auto drawAddressModeCombo = [&addressModeOptions](
            const char *label,
            nvrhi::SamplerAddressMode &mode)
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
            assetManager.SetTextureCreateInfo(assetData.handle, state.createInfo);

            AssetMetaData importMetadata = assetData.metadata;
            importMetadata.filepath =
                project->GetAssetFilepath(assetData.metadata.filepath);

            Ref<Texture> reimportedTexture = AssetImporter::ImportTexture(
                assetData.handle, importMetadata, state.createInfo);
            if (reimportedTexture)
            {
                reimportedTexture->handle = assetData.handle;
                assetManager.AssignAsset(assetData.handle, reimportedTexture);
                assetManager.SetTextureCreateInfo(assetData.handle,
                    reimportedTexture->GetCreateInfo());
                assetData.asset = reimportedTexture;
                state.createInfo = reimportedTexture->GetCreateInfo();
                reimportedTexture->SetDirtyFlag(false);
            }
        }
    }

    bool AssetEditorPanel::DrawAssetEditorHeader(AssetEditorData &assetData) {
        ImGui::Text("Asset: %s", assetData.metadata.filepath.filename().string().c_str());
        ImGui::Text("Type: %s", AssetTypeToString(assetData.metadata.type).c_str());
        ImGui::Separator();

        const bool isDirty = assetData.asset && assetData.asset->IsDirty();
        ImGui::Text("Status: %s", isDirty ? "Modified" : "Saved");
        ImGui::SameLine();

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

    void AssetEditorPanel::RenderSkeletalAnimationEditor(const Ref<SkeletalAnimation> &animation)
    {
        ImGui::Text("Name: %s", animation->name.c_str());
        ImGui::Text("Duration: %.3f", animation->duration);
        ImGui::Text("Ticks Per Second: %.3f", animation->ticksPerSeconds);
        ImGui::Text("Channels: %zu", animation->channels.size());

        Project *project =
            m_EditorLayer ? m_EditorLayer->GetActiveProject().get() : nullptr;
        if (!project)
        {
            return;
        }

        auto &assetManager = project->GetAssetManager();

        const AssetHandle skeletonHandle =
            AssetHandle(animation->GetSkeletonHandle());
        if (skeletonHandle != AssetHandle(0))
        {
            const AssetMetaData &skeletonMetadata =
                assetManager.GetMetaData(skeletonHandle);
            if (skeletonMetadata.type == AssetType::Skeleton)
            {
                ImGui::Text("Skeleton: %s",
                    skeletonMetadata.filepath.generic_string().c_str());
            }
            else
            {
                ImGui::Text("Skeleton Handle: %llu",
                    static_cast<unsigned long long>(
                        static_cast<uint64_t>(skeletonHandle)));
            }
        }
        else
        {
            ImGui::Text("Skeleton: <none>");
        }

        ImGui::Button("Drop Skeleton Here", ImVec2(220.0f, 0.0f));
        if (ImGui::BeginDragDropTarget())
        {
            if (const ImGuiPayload *payload =
                ImGui::AcceptDragDropPayload("content_browser_item"))
            {
                if (payload->Data && payload->DataSize == sizeof(AssetHandle))
                {
                    const AssetHandle droppedHandle =
                        *static_cast<const AssetHandle *>(payload->Data);
                    const AssetMetaData &droppedMetadata =
                        assetManager.GetMetaData(droppedHandle);
                    if (droppedMetadata.type == AssetType::Skeleton)
                    {
                        animation->SetSkeletonHandle(
                            UUID(static_cast<uint64_t>(droppedHandle)));
                        animation->SetDirtyFlag(true);
                    }
                }
            }
            ImGui::EndDragDropTarget();
        }
    }

    bool AssetEditorPanel::SaveAsset(AssetEditorData &assetData)
    {
        if (!m_EditorLayer || !m_EditorLayer->GetActiveProject() || !assetData.asset)
        {
            return false;
        }

        Project *project = m_EditorLayer->GetActiveProject().get();
        const std::filesystem::path savePath =
            project->GetAssetFilepath(assetData.metadata.filepath);

        switch (assetData.metadata.type)
        {
            case AssetType::SpriteSheet:
            {
                Ref<SpriteSheet> spriteSheet =
                    std::dynamic_pointer_cast<SpriteSheet>(assetData.asset);
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
                Ref<Material2D> material2D =
                    std::dynamic_pointer_cast<Material2D>(assetData.asset);
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
                Ref<SkeletalAnimation> animation =
                    std::dynamic_pointer_cast<SkeletalAnimation>(assetData.asset);
                if (!animation)
                {
                    return false;
                }
                animation->Serialize(savePath);
                animation->SetDirtyFlag(false);
                return true;
            }

            case AssetType::Animation2D:
            {
                Ref<Animation2D> anim =
                    std::dynamic_pointer_cast<Animation2D>(assetData.asset);
                if (!anim)
                    return false;
                std::filesystem::path fullPath =
                    m_EditorLayer->GetActiveProject()->GetAssetDirectory() /
                    assetData.metadata.filepath;
                return anim->Serialize(fullPath);
            }
            case AssetType::AnimatorController2D:
            {
                Ref<AnimatorController2D> ctrl =
                    std::dynamic_pointer_cast<AnimatorController2D>(assetData.asset);
                if (!ctrl)
                    return false;
                std::filesystem::path fullPath =
                    m_EditorLayer->GetActiveProject()->GetAssetDirectory() /
                    assetData.metadata.filepath;
                return ctrl->Serialize(fullPath);
            }
            default:
            return false;
        }
    }

    void AssetEditorPanel::OnEvent(Event &event)
    {
        EventDispatcher dispatcher(event);
        dispatcher.Dispatch<AssetEditorOpenEvent>(BIND_CLASS_EVENT_FN(AssetEditorPanel::OnAssetEditorOpenEvent));
        dispatcher.Dispatch<AssetEditorCreateEvent>(BIND_CLASS_EVENT_FN(AssetEditorPanel::OnAssetEditorCreateEvent));
    }

    bool AssetEditorPanel::OnAssetEditorOpenEvent(AssetEditorOpenEvent &event)
    {
        auto handle = event.GetAssetHandle();
        auto &metadata = event.GetAssetMetaData();
        if (metadata.type == AssetType::Invalid || handle == AssetHandle(0) ||
            !m_EditorLayer || !m_EditorLayer->GetActiveProject())
            return false;

        // Check if the asset window is already open.
        auto it = std::ranges::find(m_Assets, handle, &AssetEditorData::handle);
        if (it != m_Assets.end())
        {
            it->isOpen = true;
            it->requestFocus = true;
            return true;
        }

        auto &assetManager = m_EditorLayer->GetActiveProject()->GetAssetManager();
        Ref<Asset> asset = assetManager.GetAsset(handle);
        if (!asset)
        {
            asset = assetManager.GetAssetImmediate(handle);
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

            m_Assets.push_back(std::move(data));
            return true;
        }

        return false;
    }

    bool AssetEditorPanel::OnAssetEditorCreateEvent(AssetEditorCreateEvent &event)
    {
        if (!m_EditorLayer || !m_EditorLayer->GetActiveProject())
            return false;

        if (event.GetAssetType() == AssetType::Invalid)
            return false;

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

        return true;
    }

    void AssetEditorPanel::OnGuiRender()
    {
        RenderCreateAssetPopup();

        for (auto &assetData : m_Assets)
        {
            if (!assetData.isOpen)
                continue;

            auto renderUnsavedClosePopup = [&](bool &isOpen)
            {
                const std::string popupId =
                    std::format("Unsaved Changes###asset_unsaved_close_{}",
                        static_cast<uint64_t>(assetData.handle));
                if (assetData.showUnsavedClosePopup)
                {
                    ImGui::OpenPopup(popupId.c_str());
                }

                if (ImGui::BeginPopupModal(popupId.c_str(), nullptr,
                    ImGuiWindowFlags_AlwaysAutoResize))
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
            };

            if (assetData.requestFocus)
            {
                ImGui::SetNextWindowFocus();
            }

            bool isOpen = assetData.isOpen;
            ImGui::SetNextWindowSize(ImVec2(1280.0f, 1080.0f), ImGuiCond_FirstUseEver);
            ImGui::SetNextWindowSizeConstraints(ImVec2(420.0f, 640.0f),
                ImVec2(FLT_MAX, FLT_MAX));
            if (!ImGui::Begin(assetData.windowTitle.c_str(), &isOpen,
                ImGuiWindowFlags_NoScrollWithMouse))
            {
                if (!isOpen && assetData.asset && assetData.asset->IsDirty())
                {
                    isOpen = true;
                    assetData.showUnsavedClosePopup = true;
                }
                renderUnsavedClosePopup(isOpen);
                assetData.isOpen = isOpen;
                assetData.requestFocus = false;
                ImGui::End();
                continue;
            }

            assetData.requestFocus = false;

            if (!DrawAssetEditorHeader(assetData))
            {
                ImGui::End();
                assetData.isOpen = isOpen;
                continue;
            }

            if (!assetData.asset || !assetData.asset->IsReady())
            {
                ImGui::Text("Loading asset...");
                ImGui::End();
                assetData.isOpen = isOpen;
                continue;
            }

            switch (assetData.metadata.type)
            {
                case AssetType::SpriteSheet:
                {
                    Ref<SpriteSheet> spriteSheet = std::dynamic_pointer_cast<SpriteSheet>(assetData.asset);
                    if (!spriteSheet)
                        break;

                    RenderSpriteSheetEditor(spriteSheet);
                } break;

                case AssetType::Texture:
                {
                    Ref<Texture> texture = std::dynamic_pointer_cast<Texture>(assetData.asset);
                    if (!texture)
                        break;

                    RenderTextureEditor(assetData, texture);
                } break;

                case AssetType::Material2D:
                {
                    Ref<Material2D> material2D = std::dynamic_pointer_cast<Material2D>(assetData.asset);
                    if (!material2D)
                        break;

                    RenderMaterial2DEditor(material2D);
                } break;

                case AssetType::SkeletalAnimation:
                {
                    Ref<SkeletalAnimation> anim = std::dynamic_pointer_cast<SkeletalAnimation>(assetData.asset);
                    if (!anim)
                        break;

                    RenderSkeletalAnimationEditor(anim);
                } break;

                case AssetType::Animation2D:
                {
                    Ref<Animation2D> anim2d = std::dynamic_pointer_cast<Animation2D>(assetData.asset);
                    if (!anim2d)
                        break;

                    RenderAnimation2DEditor(anim2d);
                } break;

                case AssetType::AnimatorController2D:
                {
                    Ref<AnimatorController2D> ctrl = std::dynamic_pointer_cast<AnimatorController2D>(assetData.asset);
                    if (!ctrl)
                        break;

                    RenderAnimatorController2DEditor(ctrl);
                } break;

                default:
                ImGui::Text("Asset type '%s' editor is not implemented yet.", AssetTypeToString(assetData.metadata.type).c_str());
                break;
            }

            if (!isOpen && assetData.asset && assetData.asset->IsDirty())
            {
                isOpen = true;
                assetData.showUnsavedClosePopup = true;
            }
            renderUnsavedClosePopup(isOpen);

            ImGui::End();
            assetData.isOpen = isOpen;
        }

        std::erase_if(m_Assets, [](const AssetEditorData &assetData)
        {
            return !assetData.isOpen;
        });
    }

    // =========================================================================
    // Animation2D Editor
    // =========================================================================
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
                    st.playbackTime = std::fmod(st.playbackTime, totalDur);
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
                    ImGui::InvisibleButton("##anim2d_view_ibt", vpSize);

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
                        for (int cx = 0; cx < cols; ++cx)
                        {
                            const ImVec2 tMin = { vpPos.x + cx * cs, vpPos.y + ry * cs };
                            const ImVec2 tMax = { tMin.x + cs, tMin.y + cs };
                            dl->AddRectFilled(tMin, tMax, ((cx + ry) % 2 == 0) ? cA : cB);
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

                    dl->AddImage(
                        reinterpret_cast<ImTextureID>(texture->GetHandle().Get()),
                        imgPos, ImVec2(imgPos.x + imgW, imgPos.y + imgH),
                        ImVec2(fr.uv0.x, fr.uv1.y), ImVec2(fr.uv1.x, fr.uv0.y));

                    // Frame label
                    const std::string lbl = std::format("Frame {} / {}", fi, frameCount - 1);
                    dl->AddText(ImVec2(vpPos.x + 6, vpPos.y + 6), IM_COL32(255, 255, 255, 220), lbl.c_str());
                }
                else
                {
                    const char *msg = texture ? "No frames" : "No texture";
                    const ImVec2 ts = ImGui::CalcTextSize(msg);
                    ImGui::GetWindowDrawList()->AddText(
                        ImVec2(vpPos.x + (vpSize.x - ts.x) * 0.5f, vpPos.y + (vpSize.y - ts.y) * 0.5f),
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
                    st.playing = false;
                else
                {
                    // If at end and not looping, restart
                    if (!anim->loop && st.playbackTime >= totalDur)
                        st.playbackTime = 0.0f;
                    st.playing = true;
                }
            }
            if (playDisabled) ImGui::EndDisabled();

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
                            dl->AddText(ImVec2(cx - tsz.x * 0.5f, tlPos.y + 1.0f),
                                IM_COL32(160, 160, 180, 200), ts.c_str());
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
                        dl->AddLine(ImVec2(cx - cellW * 0.5f, tlPos.y + rulerH),
                            ImVec2(cx - cellW * 0.5f, tlPos.y + tlH), IM_COL32(55, 55, 65, 255));

                        const bool isActive = (i == st.previewFrame);
                        const float dotR = isActive ? 7.0f : 5.0f;
                        const ImU32 dotCol = isActive
                            ? IM_COL32(80, 200, 120, 255)
                            : IM_COL32(120, 130, 160, 220);
                        dl->AddCircleFilled(ImVec2(cx, dotY), dotR, dotCol);
                        dl->AddCircle(ImVec2(cx, dotY), dotR, IM_COL32(200, 200, 220, 255), 0, 1.3f);
                    }

                    // Playhead line
                    if (totalDur > 0.0f)
                    {
                        const float phX = tlPos.x + (st.playbackTime / totalDur) * tlW;
                        dl->AddLine(ImVec2(phX, tlPos.y), ImVec2(phX, tlPos.y + tlH), IM_COL32(255, 100, 60, 230), 2.0f);
                        // Playhead handle triangle
                        dl->AddTriangleFilled(
                            ImVec2(phX - 5, tlPos.y),
                            ImVec2(phX + 5, tlPos.y),
                            ImVec2(phX, tlPos.y + 10),
                            IM_COL32(255, 100, 60, 230));
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
            ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeEW);
        if (ImGui::IsItemActive())
            st.toolsWidth = std::clamp(st.toolsWidth - ImGui::GetIO().MouseDelta.x, minRight, available.x - minLeft - splitter);
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
                    const AssetMetaData &md = project->GetAssetManager().GetMetaData(handle);
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
                        const auto &md = project->GetAssetManager().GetMetaData(handle);
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

    // =========================================================================
    // AnimatorController2D Editor
    // =========================================================================
    static const char *s_ParamTypeNames[]  = { "Float", "Int", "Bool", "String" };
    static const char *s_ConditionOpNames[] = { "==", "!=", ">", "<", ">=", "<=" };

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
                        const AssetMetaData &md = project->GetAssetManager().GetMetaData(handle);
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
                    p.type = static_cast<AnimParam2D::Type>(typeIdx);
                    ctrl->SetDirtyFlag(true);
                }
                ImGui::SameLine();

                // Value
                switch (p.type)
                {
                case AnimParam2D::Type::Float:
                    ImGui::SetNextItemWidth(80);
                    if (ImGui::DragFloat("##p_float", &p.floatVal, 0.01f)) ctrl->SetDirtyFlag(true);
                    break;
                case AnimParam2D::Type::Int:
                    ImGui::SetNextItemWidth(80);
                    if (ImGui::DragInt("##p_int", &p.intVal)) ctrl->SetDirtyFlag(true);
                    break;
                case AnimParam2D::Type::Bool:
                    if (ImGui::Checkbox("##p_bool", &p.boolVal)) ctrl->SetDirtyFlag(true);
                    break;
                case AnimParam2D::Type::String:
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
                ctrl->params.push_back({ AnimParam2D::Type::Float,  "NewFloat" }); ctrl->SetDirtyFlag(true);
            }
            ImGui::SameLine();
            if (ImGui::Button("+ Bool##p_addb"))
            {
                ctrl->params.push_back({ AnimParam2D::Type::Bool,   "NewBool" }); ctrl->SetDirtyFlag(true);
            }
            ImGui::SameLine();
            if (ImGui::Button("+ Int##p_addi"))
            {
                ctrl->params.push_back({ AnimParam2D::Type::Int,    "NewInt" }); ctrl->SetDirtyFlag(true);
            }
            ImGui::SameLine();
            if (ImGui::Button("+ String##p_adds"))
            {
                ctrl->params.push_back({ AnimParam2D::Type::String, "NewString" }); ctrl->SetDirtyFlag(true);
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
                std::strncpy(toBuf,   tr.toState.c_str(),   sizeof(toBuf));
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
                        cond.op = static_cast<AnimCondition2D::Op>(opIdx);
                        ctrl->SetDirtyFlag(true);
                    }
                    ImGui::SameLine();

                    const AnimParam2D *param = ctrl->GetParam(cond.paramName);
                    if (param)
                    {
                        switch (param->type)
                        {
                            case AnimParam2D::Type::Float:
                            ImGui::SetNextItemWidth(70);
                            if (ImGui::DragFloat("##cond_fval", &cond.floatThreshold, 0.01f)) ctrl->SetDirtyFlag(true);
                            break;
                            case AnimParam2D::Type::Int:
                            ImGui::SetNextItemWidth(70);
                            if (ImGui::DragInt("##cond_ival", &cond.intThreshold)) ctrl->SetDirtyFlag(true);
                            break;
                            case AnimParam2D::Type::Bool:
                            if (ImGui::Checkbox("##cond_bval", &cond.boolThreshold)) ctrl->SetDirtyFlag(true);
                            break;
                            case AnimParam2D::Type::String:
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
                AnimTransition2D tr;
                if (!ctrl->states.empty())
                {
                    tr.fromState = ctrl->states[0].name;
                    tr.toState   = ctrl->states.size() > 1 ? ctrl->states[1].name : "";
                }
                ctrl->transitions.push_back(tr);
                ctrl->SetDirtyFlag(true);
            }
        }
    }
}
