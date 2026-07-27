// Copyright (c) 2026 Evangelion Manuhutu

#include "pch.hpp"
#include "blend_space_editor.hpp"
#include "ext/editor_ui.hpp"
#include "states.hpp"
#include "ignite/scripting/script_engine.hpp"
#include "ignite/scene/scene.hpp"
#include "ignite/scene/entity.hpp"
#include "ignite/scene/component.hpp"
#include "ignite/asset/asset_manager.hpp"
#include "ignite/animation/skeletal_animation.hpp"
#include "ignite/animation/skeleton.hpp"
#include "ignite/animation/animator/animator_controller.hpp"
#include "editor_layer.hpp"
#include "ignite/core/logger.hpp"

#include <imgui.h>
#include <imgui_internal.h>

namespace ignite
{
    void BlendSpaceEditor::DrawBlendSpaceEditor(const Ref<BlendSpace> &blendSpace, AssetManager *assetManager, BlendSpaceEditorState &state)
    {
        if (!blendSpace || !assetManager)
            return;

        const float totalWidth = ImGui::GetContentRegionAvail().x;
        const float leftWidth = std::max(300.0f, totalWidth * 0.30f);

        // Left Panel - Settings & Samples List
        ImGui::BeginChild("##bs_left", ImVec2(leftWidth, 0.0f), ImGuiChildFlags_Borders | ImGuiChildFlags_ResizeX);

        ImGui::TextUnformatted("BlendSpace Settings");
        ImGui::Separator();

        ImGui::Checkbox("Preview in Scene", &state.previewInScene);
        ImGui::Spacing();

        // Skeleton Handle
        AssetHandle skeletonHandle = blendSpace->GetSkeletonAssetHandle();
        std::string skeletonLabel = assetManager->GetAssetDisplayName(skeletonHandle);
        if (UI::DrawAssetDropTarget("Skeleton", skeletonLabel.c_str(), { AssetType::Skeleton }, &skeletonHandle, assetManager))
        {
            blendSpace->SetSkeletonAssetHandle(skeletonHandle);
            blendSpace->SetDirtyFlag(true);
        }

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::TextUnformatted("Axes Setup");

        char axisXBuf[64];
        strncpy(axisXBuf, blendSpace->axisXName.c_str(), sizeof(axisXBuf) - 1);
        axisXBuf[sizeof(axisXBuf) - 1] = '\0';
        if (ImGui::InputText("Axis X Name", axisXBuf, sizeof(axisXBuf)))
        {
            blendSpace->axisXName = axisXBuf;
            blendSpace->SetDirtyFlag(true);
        }

        float axisXMin = blendSpace->axisMin.x;
        float axisXMax = blendSpace->axisMax.x;
        if (ImGui::DragFloatRange2("Axis X Range", &axisXMin, &axisXMax, 1.0f, -10000.0f, 10000.0f, "Min: %.1f", "Max: %.1f"))
        {
            blendSpace->axisMin.x = axisXMin;
            blendSpace->axisMax.x = axisXMax;
            blendSpace->SetDirtyFlag(true);
        }

        char axisYBuf[64];
        strncpy(axisYBuf, blendSpace->axisYName.c_str(), sizeof(axisYBuf) - 1);
        axisYBuf[sizeof(axisYBuf) - 1] = '\0';
        if (ImGui::InputText("Axis Y Name", axisYBuf, sizeof(axisYBuf)))
        {
            blendSpace->axisYName = axisYBuf;
            blendSpace->SetDirtyFlag(true);
        }

        float axisYMin = blendSpace->axisMin.y;
        float axisYMax = blendSpace->axisMax.y;
        if (ImGui::DragFloatRange2("Axis Y Range", &axisYMin, &axisYMax, 1.0f, -10000.0f, 10000.0f, "Min: %.1f", "Max: %.1f"))
        {
            blendSpace->axisMin.y = axisYMin;
            blendSpace->axisMax.y = axisYMax;
            blendSpace->SetDirtyFlag(true);
        }

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::TextUnformatted("Preview Parameters");
        ImGui::SliderFloat(blendSpace->axisXName.empty() ? "Axis X" : blendSpace->axisXName.c_str(), &state.previewInput.x, blendSpace->axisMin.x, blendSpace->axisMax.x, "%.2f");
        ImGui::SliderFloat(blendSpace->axisYName.empty() ? "Axis Y" : blendSpace->axisYName.c_str(), &state.previewInput.y, blendSpace->axisMin.y, blendSpace->axisMax.y, "%.2f");

        if (ImGui::Button("Reset Preview", ImVec2(-1.0f, 0.0f)))
        {
            state.previewInput = glm::vec2(0.0f, 0.0f);
        }

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::TextUnformatted("Samples List");

        if (ImGui::Button("+ Add Sample", ImVec2(-1.0f, 0.0f)))
        {
            BlendSpaceSample newSample;
            newSample.position = glm::vec2(0.0f, 0.0f);
            blendSpace->samples.push_back(newSample);
            state.selectedSample = static_cast<int>(blendSpace->samples.size()) - 1;
            blendSpace->SetDirtyFlag(true);
        }

        int sampleToDelete = -1;
        for (int i = 0; i < static_cast<int>(blendSpace->samples.size()); ++i)
        {
            BlendSpaceSample &sample = blendSpace->samples[i];
            ImGui::PushID(i);

            std::string label = sample.GetAnimationAssetHandle() == AssetHandle(0)
                ? std::format("Sample {}", i)
                : assetManager->GetAssetDisplayName(sample.GetAnimationAssetHandle());

            const bool isSelected = (state.selectedSample == i);
            if (ImGui::Selectable(label.c_str(), isSelected))
            {
                state.selectedSample = i;
            }

            if (ImGui::BeginPopupContextItem())
            {
                if (ImGui::MenuItem("Delete Sample"))
                {
                    sampleToDelete = i;
                }
                ImGui::EndPopup();
            }

            ImGui::PopID();
        }

        if (sampleToDelete >= 0 && sampleToDelete < static_cast<int>(blendSpace->samples.size()))
        {
            blendSpace->samples.erase(blendSpace->samples.begin() + sampleToDelete);
            if (state.selectedSample == sampleToDelete)
                state.selectedSample = -1;
            else if (state.selectedSample > sampleToDelete)
                state.selectedSample--;
            blendSpace->SetDirtyFlag(true);
        }

        ImGui::Spacing();
        ImGui::Separator();

        // Selected Sample Inspector
        if (state.selectedSample >= 0 && state.selectedSample < static_cast<int>(blendSpace->samples.size()))
        {
            BlendSpaceSample &selected = blendSpace->samples[state.selectedSample];
            ImGui::Text("Sample #%d Details", state.selectedSample);

            AssetHandle animHandle = selected.GetAnimationAssetHandle();
            std::string animLabel = assetManager->GetAssetDisplayName(animHandle);
            if (UI::DrawAssetDropTarget("Animation Clip", animLabel.c_str(), { AssetType::SkeletalAnimation }, &animHandle, assetManager))
            {
                selected.SetAnimationHandle(animHandle);
                blendSpace->SetDirtyFlag(true);
            }

            glm::vec2 pos = selected.position;
            if (ImGui::DragFloat2("Position", &pos.x, 0.5f, blendSpace->axisMin.x, blendSpace->axisMax.x))
            {
                selected.position = blendSpace->ClampInput(pos);
                blendSpace->SetDirtyFlag(true);
            }
        }
        else
        {
            ImGui::TextDisabled("Select a sample point to edit properties.");
        }

        ImGui::EndChild();

        // Right Panel - Interactive 2D Grid Canvas
        ImGui::SameLine();
        ImGui::BeginChild("##bs_canvas", ImVec2(0.0f, 0.0f), ImGuiChildFlags_Borders, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);

        ImDrawList *drawList = ImGui::GetWindowDrawList();
        const ImVec2 canvasPos = ImGui::GetCursorScreenPos();
        const ImVec2 canvasSize = ImGui::GetContentRegionAvail();

        if (canvasSize.x < 50.0f || canvasSize.y < 50.0f)
        {
            ImGui::EndChild();
            return;
        }

        // Draw Canvas Background
        drawList->AddRectFilled(canvasPos, ImVec2(canvasPos.x + canvasSize.x, canvasPos.y + canvasSize.y), IM_COL32(35, 35, 38, 255));
        drawList->AddRect(canvasPos, ImVec2(canvasPos.x + canvasSize.x, canvasPos.y + canvasSize.y), IM_COL32(60, 60, 65, 255));

        const float margin = 40.0f;
        const ImVec2 gridMin(canvasPos.x + margin, canvasPos.y + margin);
        const ImVec2 gridMax(canvasPos.x + canvasSize.x - margin, canvasPos.y + canvasSize.y - margin);
        const ImVec2 gridRectSize(gridMax.x - gridMin.x, gridMax.y - gridMin.y);

        // Draw inner grid frame
        drawList->AddRectFilled(gridMin, gridMax, IM_COL32(25, 25, 28, 255));
        drawList->AddRect(gridMin, gridMax, IM_COL32(80, 80, 90, 255));

        // Coordinate transforms
        const glm::vec2 minVal = glm::min(blendSpace->axisMin, blendSpace->axisMax);
        const glm::vec2 maxVal = glm::max(blendSpace->axisMin, blendSpace->axisMax);
        const glm::vec2 valRange = glm::max(maxVal - minVal, glm::vec2(0.0001f));

        auto gridToScreen = [&](const glm::vec2 &gridPos) -> ImVec2
        {
            float normX = (gridPos.x - minVal.x) / valRange.x;
            float normY = 1.0f - (gridPos.y - minVal.y) / valRange.y; 
            return ImVec2(gridMin.x + normX * gridRectSize.x, gridMin.y + normY * gridRectSize.y);
        };

        auto screenToGrid = [&](const ImVec2 &screenPos) -> glm::vec2
        {
            float normX = (screenPos.x - gridMin.x) / gridRectSize.x;
            float normY = 1.0f - (screenPos.y - gridMin.y) / gridRectSize.y;
            return glm::vec2(minVal.x + normX * valRange.x, minVal.y + normY * valRange.y);
        };

        // Draw grid subdivisions (10x10)
        for (int i = 0; i <= 10; ++i)
        {
            float t = static_cast<float>(i) / 10.0f;
            float px = gridMin.x + t * gridRectSize.x;
            float py = gridMin.y + t * gridRectSize.y;
            drawList->AddLine(ImVec2(px, gridMin.y), ImVec2(px, gridMax.y), IM_COL32(45, 45, 50, 255));
            drawList->AddLine(ImVec2(gridMin.x, py), ImVec2(gridMax.x, py), IM_COL32(45, 45, 50, 255));
        }

        // Draw Zero Axes if within range
        if (minVal.x <= 0.0f && maxVal.x >= 0.0f)
        {
            ImVec2 zeroX = gridToScreen(glm::vec2(0.0f, minVal.y));
            drawList->AddLine(ImVec2(zeroX.x, gridMin.y), ImVec2(zeroX.x, gridMax.y), IM_COL32(100, 100, 120, 255), 1.5f);
        }
        if (minVal.y <= 0.0f && maxVal.y >= 0.0f)
        {
            ImVec2 zeroY = gridToScreen(glm::vec2(minVal.x, 0.0f));
            drawList->AddLine(ImVec2(gridMin.x, zeroY.y), ImVec2(gridMax.x, zeroY.y), IM_COL32(100, 100, 120, 255), 1.5f);
        }

        // Draw Axis Labels
        const std::string xLabel = std::format("{} ({:.1f} .. {:.1f})", blendSpace->axisXName.empty() ? "Axis X" : blendSpace->axisXName, minVal.x, maxVal.x);
        const std::string yLabel = std::format("{} ({:.1f} .. {:.1f})", blendSpace->axisYName.empty() ? "Axis Y" : blendSpace->axisYName, minVal.y, maxVal.y);
        drawList->AddText(ImVec2(gridMin.x + gridRectSize.x * 0.5f - 40.0f, gridMax.y + 10.0f), IM_COL32(200, 200, 200, 255), xLabel.c_str());
        drawList->AddText(ImVec2(gridMin.x - 35.0f, gridMin.y - 25.0f), IM_COL32(200, 200, 200, 255), yLabel.c_str());

        const ImVec2 mousePos = ImGui::GetMousePos();
        const bool canvasHovered = ImGui::IsWindowHovered() && mousePos.x >= gridMin.x && mousePos.x <= gridMax.x && mousePos.y >= gridMin.y && mousePos.y <= gridMax.y;

        // Draw Sample Points
        for (int i = 0; i < static_cast<int>(blendSpace->samples.size()); ++i)
        {
            BlendSpaceSample &sample = blendSpace->samples[i];
            const ImVec2 p = gridToScreen(sample.position);
            const bool isSelected = (state.selectedSample == i);
            const float radius = isSelected ? 8.0f : 6.0f;
            const ImU32 color = isSelected ? IM_COL32(255, 200, 50, 255) : IM_COL32(60, 160, 240, 255);

            drawList->AddCircleFilled(p, radius, color);
            drawList->AddCircle(p, radius + 1.0f, IM_COL32(255, 255, 255, 200));

            std::string sampleText = sample.GetAnimationAssetHandle() == AssetHandle(0)
                ? std::format("#{}", i)
                : assetManager->GetAssetDisplayName(sample.GetAnimationAssetHandle());
            drawList->AddText(ImVec2(p.x + 10.0f, p.y - 7.0f), IM_COL32(220, 220, 220, 255), sampleText.c_str());

            // Handle Selection & Dragging of Sample Points
            if (canvasHovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left))
            {
                const float distSq = (mousePos.x - p.x) * (mousePos.x - p.x) + (mousePos.y - p.y) * (mousePos.y - p.y);
                if (distSq <= 100.0f)
                {
                    state.selectedSample = i;
                    state.isDraggingSample = true;
                    state.draggingSampleIndex = i;
                }
            }
        }

        // Dragging Sample logic
        if (state.isDraggingSample && ImGui::IsMouseDown(ImGuiMouseButton_Left))
        {
            if (state.draggingSampleIndex >= 0 && state.draggingSampleIndex < static_cast<int>(blendSpace->samples.size()))
            {
                glm::vec2 newGridPos = screenToGrid(mousePos);
                blendSpace->samples[state.draggingSampleIndex].position = blendSpace->ClampInput(newGridPos);
                blendSpace->SetDirtyFlag(true);
            }
        }
        else if (state.isDraggingSample && !ImGui::IsMouseDown(ImGuiMouseButton_Left))
        {
            state.isDraggingSample = false;
            state.draggingSampleIndex = -1;
        }

        // Preview Point (Interactive Crosshair)
        const ImVec2 previewScreen = gridToScreen(state.previewInput);
        drawList->AddLine(ImVec2(previewScreen.x - 12.0f, previewScreen.y), ImVec2(previewScreen.x + 12.0f, previewScreen.y), IM_COL32(255, 80, 80, 255), 2.0f);
        drawList->AddLine(ImVec2(previewScreen.x, previewScreen.y - 12.0f), ImVec2(previewScreen.x, previewScreen.y + 12.0f), IM_COL32(255, 80, 80, 255), 2.0f);
        drawList->AddCircle(previewScreen, 6.0f, IM_COL32(255, 80, 80, 255), 0, 2.0f);

        // Preview Point Dragging / Mouse Control
        if (canvasHovered && ImGui::IsMouseClicked(ImGuiMouseButton_Right))
        {
            state.isDraggingPreviewPoint = true;
        }
        if (state.isDraggingPreviewPoint && ImGui::IsMouseDown(ImGuiMouseButton_Right))
        {
            state.previewInput = blendSpace->ClampInput(screenToGrid(mousePos));
        }
        else if (state.isDraggingPreviewPoint && !ImGui::IsMouseDown(ImGuiMouseButton_Right))
        {
            state.isDraggingPreviewPoint = false;
        }

        // Canvas Drag & Drop Target for Animation Clips
        ImGui::SetCursorScreenPos(gridMin);
        ImGui::Dummy(gridRectSize);
        if (ImGui::BeginDragDropTarget())
        {
            if (const ImGuiPayload *payload = ImGui::AcceptDragDropPayload(DND_PAYLOAD_CONTENT_BROWSER_ITEM))
            {
                if (payload->DataSize == sizeof(AssetHandle))
                {
                    AssetHandle handle = *static_cast<const AssetHandle *>(payload->Data);
                    if (assetManager->GetMetaData(handle).type == AssetType::SkeletalAnimation)
                    {
                        BlendSpaceSample newSample;
                        newSample.SetAnimationHandle(handle);
                        newSample.position = blendSpace->ClampInput(screenToGrid(mousePos));
                        blendSpace->samples.push_back(newSample);
                        state.selectedSample = static_cast<int>(blendSpace->samples.size()) - 1;
                        blendSpace->SetDirtyFlag(true);
                    }
                }
            }
            ImGui::EndDragDropTarget();
        }

        ImGui::EndChild();

        // Update active scene preview if Preview in Scene is enabled
        if (state.previewInScene && blendSpace)
        {
            EditorLayer *editorLayer = EditorLayer::GetInstance();
            Ref<Scene> activeScene = editorLayer ? editorLayer->GetActiveScene() : nullptr;

            if (activeScene && activeScene->registry)
            {
                auto view = activeScene->registry->view<SkeletalMeshComponent>();
                for (entt::entity e : view)
                {
                    Entity entity{ e, activeScene.get() };
                    if (!entity.IsValid())
                        continue;

                    auto &smc = entity.GetComponent<SkeletalMeshComponent>();

                    // Only update preview params for skeletal meshes using an AnimatorController that references this BlendSpace
					AssetHandle controllerHandle = smc.runtimeAnimatorHandle;
					if (controllerHandle == AssetHandle(0) && smc.runtimeAnimatorInstance)
						controllerHandle = smc.runtimeAnimatorInstance->handle;

                    if (controllerHandle == AssetHandle(0))
                        continue;

                    // Get controller
					Ref<AnimatorController> controller = assetManager->GetAsset<AnimatorController>(controllerHandle);
                    if (!controller)
                        continue;

                    // Get motion
                    bool foundBlendSpace = false;
					for (const auto &[name, animState] : controller->states)
					{
                        if (animState.GetMotionHandle() == blendSpace->handle)
                        {
                            foundBlendSpace = true;
                            break;
                        }
					}
                    if (!foundBlendSpace)
                        continue;

                    if (smc.runtimeAnimatorInstance)
                    {
                        if (!blendSpace->axisXName.empty())
                            smc.runtimeAnimatorInstance->SetParamFloat(blendSpace->axisXName, state.previewInput.x);
                        if (!blendSpace->axisYName.empty())
                            smc.runtimeAnimatorInstance->SetParamFloat(blendSpace->axisYName, state.previewInput.y);
                    }

                    auto updateParam = [&](const std::string &name, float val)
                    {
                        if (name.empty()) return;
                        auto it = smc.runtimeParams.find(name);
                        if (it != smc.runtimeParams.end())
                        {
                            it->second.floatVal = val;
                        }
                        else
                        {
                            smc.runtimeParams[name] = AnimParam{ .name = name, .strVal = "", .floatVal = val, .intVal = 0, .boolVal = false, .type = AnimParam::Type::Float };
                        }
                    };

                    updateParam(blendSpace->axisXName, state.previewInput.x);
                    updateParam(blendSpace->axisYName, state.previewInput.y);
                }
            }
        }
    }
}
