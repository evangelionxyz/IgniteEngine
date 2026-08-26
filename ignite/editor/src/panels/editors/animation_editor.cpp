// Copyright (c) 2026 Evangelion Manuhutu

#include "pch.hpp"
#include "animation_editor.hpp"
#include "animation_editor_shared.hpp"
#include "ext/editor_ui.hpp"
#include "ignite/animation/skeletal_animation.hpp"
#include "ignite/animation/skeleton.hpp"
#include "ignite/asset/asset_manager.hpp"
#include "ignite/core/logger.hpp"

#include "states.hpp"
#include "ignite/math/math.hpp"
#include "ignite/imgui/gizmo.hpp"
#include <imgui.h>
#include <imgui_internal.h>
#include <algorithm>
#include <cstring>
#include <format>

namespace ignite
{
    void AnimationEditor::Draw(const Ref<SkeletalAnimation> &animation, AssetManager *assetManager,
        UI::EditorSceneData &sceneData, AnimationEditorState &state, float deltaTime)
    {
        if (!animation || !assetManager)
            return;

        // Fetch skeleton
        Ref<Skeleton> skeleton = nullptr;
        AssetHandle skeletonHandle = animation->GetSkeletonHandle();
		const bool hasSkeleton = assetManager->IsAssetHandleValid(skeletonHandle);

        if (hasSkeleton)
            skeleton = assetManager->GetAsset<Skeleton>(skeletonHandle);

        const float totalDuration = (animation->duration > 0.0f)
            ? animation->duration / std::max(animation->ticksPerSeconds, 0.0001f)
            : 0.0f;

        // Layout
        const ImVec2 contentSize = ImGui::GetContentRegionAvail();
        const float splitterWidth = 6.0f;
        const float minLeftWidth = 200.0f;
        const float minCenterWidth = 300.0f;
        const float minRightWidth = 260.0f;

        if (state.previewColumnWidth <= 0.0f)
        {
            state.previewColumnWidth = std::max(minCenterWidth, contentSize.x * 0.50f);
        }

        const float maxCenterWidth = std::max(minCenterWidth, contentSize.x - minLeftWidth - minRightWidth - splitterWidth * 2.0f);
        state.previewColumnWidth = std::clamp(state.previewColumnWidth, minCenterWidth, maxCenterWidth);
        const float leftWidth = std::max(minLeftWidth, (contentSize.x - state.previewColumnWidth - splitterWidth * 2.0f) * 0.4f);

        // ==== Left Column: Joint Tree ====
        ImGui::BeginChild("##anim_left", ImVec2(leftWidth, 0.0f), ImGuiChildFlags_ResizeX);
        {
            ImGui::TextUnformatted("Skeleton Joints");

            if (ImGui::TreeNodeEx("Properties", ImGuiTreeNodeFlags_DefaultOpen))
            {
                if (UI::DrawCheckbox("Root Motion", &animation->rootMotion))
                    animation->SetDirtyFlag(true);
                if (UI::DrawCheckbox("In Place", &animation->inPlace))
                    animation->SetDirtyFlag(true);

                ImGui::TreePop();
            }

            ImGui::Spacing();

            // Skeleton drag and drop target button
            const std::string skeletonLabel = assetManager->GetAssetDisplayName(animation->GetSkeletonHandle());
			if (UI::DrawAssetDropTarget("Skeleton Handle", skeletonLabel, { AssetType::Skeleton }, &skeletonHandle, assetManager))
			{
                animation->SetSkeletonHandle(skeletonHandle);
                animation->SetDirtyFlag(true);
			}

            ImGui::Spacing();

            if (skeleton && !skeleton->joints.empty())
            {
                // Build children map
                std::vector<std::vector<int32_t>> children(skeleton->joints.size());
                for (const auto &joint : skeleton->joints)
                {
                    if (joint.parentJointId >= 0 && joint.parentJointId < static_cast<int32_t>(skeleton->joints.size()))
                    {
                        children[static_cast<size_t>(joint.parentJointId)].push_back(joint.id);
                    }
                }

                std::function<void(int32_t)> drawJointNode = [&](int32_t jointId)
                {
                    if (jointId < 0 || jointId >= static_cast<int32_t>(skeleton->joints.size()))
                        return;

                    const Joint &joint = skeleton->joints[static_cast<size_t>(jointId)];
                    const bool isSelected = state.selectedJoint == jointId;
                    const bool hasChildren = !children[static_cast<size_t>(jointId)].empty();
                    const bool hasChannel = animation->channels.contains(jointId);

                    const ImGuiTreeNodeFlags jointTreeNodeFlags = ImGuiTreeNodeFlags_OpenOnArrow
                        | ImGuiTreeNodeFlags_SpanAvailWidth | ImGuiTreeNodeFlags_DefaultOpen
                        | (hasChildren ? ImGuiTreeNodeFlags_None : ImGuiTreeNodeFlags_Leaf)
                        | (isSelected ? ImGuiTreeNodeFlags_Selected : ImGuiTreeNodeFlags_None);

                    // Color joints with channels differently
                    if (hasChannel)
                    {
                        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.4f, 0.9f, 0.5f, 1.0f));
                    }

                    bool opened = ImGui::TreeNodeEx((void *)static_cast<intptr_t>(jointId + 1), jointTreeNodeFlags, "%s", joint.name.c_str());

                    if (hasChannel)
                    {
                        ImGui::PopStyleColor();
                    }

                    if (ImGui::IsItemClicked())
                    {
                        state.selectedJoint = jointId;
                        state.selectedKeyframeIndex = -1;
                    }

                    if (opened)
                    {
                        for (int32_t childId : children[static_cast<size_t>(jointId)])
                        {
                            drawJointNode(childId);
                        }
                        ImGui::TreePop();
                    }
                };

                for (const auto &joint : skeleton->joints)
                {
                    if (joint.parentJointId == -1)
                    {
                        drawJointNode(joint.id);
                    }
                }
            }
        }
        ImGui::EndChild();

        ImGui::SameLine();

        // ==== Center Column: Viewport + Timeline ====
        ImGui::BeginChild("##anim_center", ImVec2(state.previewColumnWidth, 0.0f), ImGuiChildFlags_ResizeX, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
        {
            constexpr float minViewportH = 120.0f;
            constexpr float minTimelineH = 100.0f;
            constexpr float splitterH = 4.0f;

            const ImVec2 centerRegion = ImGui::GetContentRegionAvail();

            float maxTimelineH = std::max(minTimelineH, centerRegion.y - minViewportH - splitterH);
            state.timelineHeight = std::clamp(state.timelineHeight, minTimelineH, maxTimelineH);
            float viewportH = std::max(minViewportH, centerRegion.y - state.timelineHeight - splitterH - ImGui::GetFrameHeight());

            // Scene preview viewport
            if (ImGui::BeginChild("##anim_viewport", { 0.0f, viewportH }, ImGuiChildFlags_Borders))
            {
                UI::AnimPreviewViewport::Draw(sceneData, deltaTime);

                std::vector<glm::mat4> animGlobalTransforms;
                const std::vector<glm::mat4> *poseTransforms = nullptr;
                if (skeleton && !skeleton->joints.empty())
                {
                    const size_t jointCount = skeleton->joints.size();
                    animGlobalTransforms.resize(jointCount, glm::mat4(1.0f));

                    const float ticksPerSec = std::max(animation->ticksPerSeconds, 0.0001f);
                    const float timeInTicks = state.timeSeconds * ticksPerSec;

                    for (size_t jointIndex = 0; jointIndex < jointCount; ++jointIndex)
                    {
                        const Joint &joint = skeleton->joints[jointIndex];
                        glm::mat4 local = joint.defaultTransform.GetMatrix();
                        if (animation->channels.contains(static_cast<int>(jointIndex)))
                        {
                            local = animation->channels[static_cast<int>(jointIndex)].Calculate(timeInTicks, joint.defaultTransform).GetMatrix();
                        }

                        if (joint.parentJointId < 0 || joint.parentJointId >= static_cast<int32_t>(jointCount))
                        {
                            animGlobalTransforms[jointIndex] = local;
                        }
                        else
                        {
                            animGlobalTransforms[jointIndex] = animGlobalTransforms[static_cast<size_t>(joint.parentJointId)] * local;
                        }
                    }
                    poseTransforms = &animGlobalTransforms;
                }

                static Gizmo s_AnimEditorGizmo;
                int32_t dummySocket = -1;
                bool isDirty = false;

                UI::AnimPreviewViewport::DrawOverlay(sceneData, skeleton, poseTransforms, state.selectedJoint, dummySocket, 0, s_AnimEditorGizmo, isDirty);
            }
            ImGui::EndChild();

            // Vertical splitter
            {
                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.18f, 0.18f, 0.20f, 1.0f));
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.26f, 0.26f, 0.30f, 1.0f));
                ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.30f, 0.30f, 0.36f, 1.0f));

                ImGui::BeginChild("##anim_tl_splitter", { 0.0f, splitterH });
                ImGui::Button("##anim_tl_splitter_btn", ImVec2(-1.0f, -1.0f));

                if (ImGui::IsItemHovered() || ImGui::IsItemActive())
                    ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeNS);

                if (ImGui::IsItemActive())
                {
                    state.timelineHeight -= ImGui::GetIO().MouseDelta.y;
                    state.timelineHeight = std::clamp(state.timelineHeight, minTimelineH, maxTimelineH);
                }
                ImGui::PopStyleColor(3);
                ImGui::EndChild();
            }

            // Timeline area
            if (ImGui::BeginChild("##anim_timeline_area", { 0.0f, state.timelineHeight }, ImGuiChildFlags_None, ImGuiWindowFlags_NoScrollbar))
            {
                // Playback controls
                if (ImGui::BeginChild("##anim_playback", { 0.0f, 30.0f }))
                {
                    UI::AnimPlaybackControls::Draw(state.playing, state.loop, state.timeSeconds, totalDuration, true);
                }
                ImGui::EndChild();

                ImGui::Spacing();

                // Timeline with source animation events
                if (ImGui::BeginChild("##anim_timeline", { 0.0f, 0.0f }))
                {
                    UI::AnimTimelineTrack::Draw(ImGui::GetWindowDrawList(),
                        std::max(54.0f, ImGui::GetContentRegionAvail().y), totalDuration, &state.timeSeconds,
                        &state.playing, &animation->timelineEvents, nullptr, nullptr, &state.selectedEventIndex, "Select an animation");

                    if (ImGui::BeginDragDropTarget())
                    {
                        if (const ImGuiPayload *payload = ImGui::AcceptDragDropPayload(DND_PAYLOAD_CONTENT_BROWSER_ITEM))
                        {
                            if (payload->Data && payload->DataSize == sizeof(AssetHandle))
                            {
                                AssetHandle droppedHandle = *static_cast<const AssetHandle *>(payload->Data);
                                AssetMetaData meta = assetManager->GetMetaData(droppedHandle);
                                if (meta.type == AssetType::Audio)
                                {
                                    const float mouseX = ImGui::GetMousePos().x;
                                    const ImVec2 tlPos = ImGui::GetItemRectMin();
                                    const float tlWidth = ImGui::GetItemRectSize().x;
                                    const float normTime = (tlWidth > 0.0f) ? std::clamp((mouseX - tlPos.x) / tlWidth, 0.0f, 1.0f) : 0.0f;

                                    AnimationTimelineEvent evt;
                                    evt.action = AnimationTimelineEvent::Action::Audio;
                                    evt.SetAudioHandle(droppedHandle);
                                    evt.normalizedTime = normTime;
                                    evt.name = assetManager->GetAssetDisplayName(droppedHandle);
                                    animation->timelineEvents.push_back(evt);
                                    state.selectedEventIndex = static_cast<int>(animation->timelineEvents.size()) - 1;
                                    animation->SetDirtyFlag(true);
                                }
                            }
                        }
                        ImGui::EndDragDropTarget();
                    }
                }
                ImGui::EndChild();
            }
            ImGui::EndChild();
        }
        ImGui::EndChild();

        ImGui::SameLine();

        // ==== Right Column: Keyframe & Event Inspector ====
        ImGui::BeginChild("##anim_right", ImVec2(0.0f, 0.0f), ImGuiChildFlags_Borders);
        {
            ImGui::TextUnformatted("Inspector");
            ImGui::Separator();

			// Preview Mesh drag and drop target button
			AssetHandle previewMeshHandle = state.previewMeshHandle;
			const std::string previewMeshLabel = assetManager->GetAssetDisplayName(previewMeshHandle);
			if (UI::DrawAssetDropTarget("Mesh", previewMeshLabel.c_str(), { AssetType::Mesh, AssetType::SkeletalMesh }, &previewMeshHandle, assetManager))
			{
				state.previewMeshHandle = previewMeshHandle;
			}

			ImGui::Spacing();

            if (state.selectedJoint >= 0 && animation->channels.contains(state.selectedJoint))
            {
                // Safety: grab non-const reference for inspection (read-only but needs non-const iterator)
                auto channelIt = animation->channels.find(state.selectedJoint);
                if (channelIt != animation->channels.end())
                {
                    const AnimationChannel &channel = channelIt->second;

                    std::string jointName = std::format("Joint {}", state.selectedJoint);
                    if (skeleton && state.selectedJoint >= 0 && state.selectedJoint < static_cast<int>(skeleton->joints.size()))
                    {
                        jointName = skeleton->joints[static_cast<size_t>(state.selectedJoint)].name;
                    }
                    ImGui::Text("Joint: %s", jointName.c_str());

                    const char *keyTypes[] = { "Translation", "Rotation", "Scale" };
                    ImGui::Combo("Channel", &state.selectedKeyframeType, keyTypes, IM_ARRAYSIZE(keyTypes));

                    ImGui::Spacing();

                    // Compute current time in ticks
                    float timeInTicks = state.timeSeconds * std::max(animation->ticksPerSeconds, 0.0001f);

                    // Display keyframe list for selected channel type
                    auto drawKeyframeList = [&](const auto &keys, const char *valueLabel)
                    {
                        ImGui::Text("Keyframes: %zu", keys.frames.size());
                        ImGui::Separator();

                        // Find closest keyframe to current time
                        int closestIdx = -1;
                        float closestDist = FLT_MAX;
                        for (int i = 0; i < static_cast<int>(keys.frames.size()); ++i)
                        {
                            float d = std::abs(keys.frames[i].Timestamp - timeInTicks);
                            if (d < closestDist)
                            {
                                closestDist = d;
                                closestIdx = i;
                            }
                        }

                        if (ImGui::BeginChild("##kf_list", ImVec2(0.0f, 150.0f), ImGuiChildFlags_Borders))
                        {
                            for (int i = 0; i < static_cast<int>(keys.frames.size()); ++i)
                            {
                                const auto &frame = keys.frames[i];
                                bool isClosest = (i == closestIdx);
                                bool isSelected = (i == state.selectedKeyframeIndex);

                                std::string label = std::format("#{} t={:.3f}", i, frame.Timestamp);
                                if (isClosest)
                                {
                                    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.8f, 0.2f, 1.0f));
                                }

                                if (ImGui::Selectable(label.c_str(), isSelected))
                                {
                                    state.selectedKeyframeIndex = i;
                                }

                                if (isClosest)
                                {
                                    ImGui::PopStyleColor();
                                }
                            }
                        }
                        ImGui::EndChild();

                        // Show selected keyframe details (read-only inspect)
                        if (state.selectedKeyframeIndex >= 0 && state.selectedKeyframeIndex < static_cast<int>(keys.frames.size()))
                        {
                            const auto &frame = keys.frames[state.selectedKeyframeIndex];
                            ImGui::SeparatorText("Selected Keyframe");
                            ImGui::Text("Timestamp: %.4f ticks", frame.Timestamp);

                            if constexpr (std::is_same_v<std::decay_t<decltype(frame.Value)>, glm::vec3>)
                            {
                                ImGui::Text("X: %.4f", frame.Value.x);
                                ImGui::Text("Y: %.4f", frame.Value.y);
                                ImGui::Text("Z: %.4f", frame.Value.z);
                            }
                            else if constexpr (std::is_same_v<std::decay_t<decltype(frame.Value)>, glm::quat>)
                            {
                                ImGui::Text("X: %.4f", frame.Value.x);
                                ImGui::Text("Y: %.4f", frame.Value.y);
                                ImGui::Text("Z: %.4f", frame.Value.z);
                                ImGui::Text("W: %.4f", frame.Value.w);
                            }
                        }
                    };

                    switch (state.selectedKeyframeType)
                    {
                        case 0: drawKeyframeList(channel.translationKeys, "Position"); break;
                        case 1: drawKeyframeList(channel.rotationKeys, "Rotation"); break;
                        case 2: drawKeyframeList(channel.scaleKeys, "Scale"); break;
                    }

                    // Show interpolated value at current time
                    ImGui::SeparatorText("Current Value");
                    if (skeleton && state.selectedJoint >= 0 && state.selectedJoint < static_cast<int>(skeleton->joints.size()))
                    {
                        const Joint &joint = skeleton->joints[static_cast<size_t>(state.selectedJoint)];
                        Transform currentTransform = const_cast<AnimationChannel &>(channel).Calculate(timeInTicks, joint.defaultTransform);
                        ImGui::Text("Pos: (%.3f, %.3f, %.3f)", currentTransform.translation.x, currentTransform.translation.y, currentTransform.translation.z);
                        ImGui::Text("Rot: (%.3f, %.3f, %.3f, %.3f)", currentTransform.rotation.x, currentTransform.rotation.y, currentTransform.rotation.z, currentTransform.rotation.w);
                        ImGui::Text("Scale: (%.3f, %.3f, %.3f)", currentTransform.scale.x, currentTransform.scale.y, currentTransform.scale.z);
                    }
                }
            }
            else
            {
                ImGui::TextDisabled("Select a joint with animation data to inspect keyframes.");
            }

            ImGui::Spacing();
            ImGui::SeparatorText("Timeline Events");
            if (!animation->timelineEvents.empty())
            {
                if (state.selectedEventIndex >= static_cast<int>(animation->timelineEvents.size()))
                {
                    state.selectedEventIndex = -1;
                }

                if (ImGui::BeginChild("##event_list", ImVec2(0.0f, 100.0f), ImGuiChildFlags_Borders))
                {
                    for (int i = 0; i < static_cast<int>(animation->timelineEvents.size()); ++i)
                    {
                        auto &evt = animation->timelineEvents[i];
                        bool isSelected = (state.selectedEventIndex == i);
                        std::string label = std::format("{} ({:.2f})##evt_{}", evt.name, evt.normalizedTime, i);
                        if (ImGui::Selectable(label.c_str(), isSelected))
                        {
                            state.selectedEventIndex = i;
                        }
                    }
                }
                ImGui::EndChild();

                if (state.selectedEventIndex >= 0 && state.selectedEventIndex < static_cast<int>(animation->timelineEvents.size()))
                {
                    auto &evt = animation->timelineEvents[static_cast<size_t>(state.selectedEventIndex)];
                    ImGui::SeparatorText("Selected Event");

                    char nameBuf[256] {};
                    std::strncpy(nameBuf, evt.name.c_str(), sizeof(nameBuf) - 1);
                    if (ImGui::InputText("Event Name", nameBuf, sizeof(nameBuf)))
                    {
                        evt.name = nameBuf;
                        animation->SetDirtyFlag(true);
                    }

                    if (ImGui::DragFloat("Normalized Time", &evt.normalizedTime, 0.005f, 0.0f, 1.0f))
                    {
                        animation->SetDirtyFlag(true);
                    }

                    AssetHandle currentAudioH = evt.GetAudioHandle();
                    const std::string audioLabel = assetManager->GetAssetDisplayName(currentAudioH);
                    if (UI::DrawAssetDropTarget("Audio Asset", audioLabel.c_str(), { AssetType::Audio }, &currentAudioH, assetManager))
                    {
                        evt.SetAudioHandle(currentAudioH);
                        animation->SetDirtyFlag(true);
                    }

                    if (ImGui::Button("Delete Event"))
                    {
                        animation->timelineEvents.erase(animation->timelineEvents.begin() + state.selectedEventIndex);
                        state.selectedEventIndex = -1;
                        animation->SetDirtyFlag(true);
                    }
                }
            }
            else
            {
                ImGui::TextDisabled("No events on timeline. Drag sound here.");
            }

            ImGui::Spacing();
            ImGui::SeparatorText("Animation Info");
            ImGui::Text("Name: %s", animation->name.c_str());
            ImGui::Text("Duration: %.2f ticks (%.2fs)", animation->duration, totalDuration);
            ImGui::Text("Ticks/s: %.2f", animation->ticksPerSeconds);
            ImGui::Text("Channels: %zu", animation->channels.size());
            ImGui::Text("Events: %zu", animation->timelineEvents.size());
        }
        ImGui::EndChild();
    }
}
