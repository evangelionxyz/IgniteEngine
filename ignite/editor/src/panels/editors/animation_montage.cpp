// Copyright (c) 2026 Evangelion Manuhutu

#include "pch.hpp"
#include "animation_montage.hpp"
#include "animation_editor_shared.hpp"
#include "ext/editor_ui.hpp"
#include "states.hpp"
#include "ignite/animation/skeletal_animation.hpp"
#include "ignite/animation/skeleton.hpp"
#include "ignite/asset/asset_manager.hpp"
#include "ignite/core/logger.hpp"

#include <imgui.h>
#include <imgui_internal.h>
#include <algorithm>
#include <format>

namespace ignite
{
    void AnimationMontageEditor::Draw(const Ref<AnimationMontage> &montage, AssetManager *assetManager,
        UI::EditorSceneData &sceneData, AnimationMontageEditorState &state, float deltaTime)
    {
        if (!montage || !assetManager)
            return;

        // Fetch linked animation and skeleton
        Ref<SkeletalAnimation> linkedAnim = nullptr;
        if (montage->GetAnimationHandle() != AssetHandle(0))
        {
            linkedAnim = assetManager->GetAsset<SkeletalAnimation>(montage->GetAnimationHandle());
        }

        Ref<Skeleton> skeleton = nullptr;
        if (montage->GetSkeletonHandle() != AssetHandle(0))
        {
            skeleton = assetManager->GetAsset<Skeleton>(montage->GetSkeletonHandle());
        }

        const float totalDuration = (linkedAnim && linkedAnim->duration > 0.0f)
            ? linkedAnim->duration / std::max(linkedAnim->ticksPerSeconds, 0.0001f)
            : 0.0f;

        // Advance playback
        if (state.playing && totalDuration > 0.0f)
        {
            state.timeSeconds += deltaTime;
            if (state.loop)
            {
                state.timeSeconds = std::fmod(state.timeSeconds, std::max(totalDuration, 0.0001f));
            }
            else
            {
                state.timeSeconds = std::min(state.timeSeconds, totalDuration);
            }
        }

        // Layout
        const ImVec2 contentSize = ImGui::GetContentRegionAvail();
        const float splitterWidth = 6.0f;
        const float minLeftWidth = 280.0f;
        const float minRightWidth = 240.0f;
        const float maxLeftWidth = std::max(minLeftWidth, contentSize.x - minRightWidth - splitterWidth);

        if (state.previewColumnWidth <= 0.0f)
        {
            state.previewColumnWidth = std::max(minLeftWidth, contentSize.x * 0.65f);
        }
        state.previewColumnWidth = std::clamp(state.previewColumnWidth, minLeftWidth, maxLeftWidth);

        // ==== Left Column: Viewport + Timeline ====
        ImGui::BeginChild("##montage_left", ImVec2(state.previewColumnWidth, 0.0f), ImGuiChildFlags_None);
        {
            const float minViewportH = 120.0f;
            const float minTimelineH = 100.0f;
            const float splitterH = 6.0f;
            const ImVec2 leftRegion = ImGui::GetContentRegionAvail();
            float maxTimelineH = std::max(minTimelineH, leftRegion.y - minViewportH - splitterH);
            state.timelineHeight = std::clamp(state.timelineHeight, minTimelineH, maxTimelineH);
            float viewportH = std::max(minViewportH, leftRegion.y - state.timelineHeight - splitterH);

            // Scene preview viewport
            if (ImGui::BeginChild("##montage_viewport", { 0.0f, viewportH }, ImGuiChildFlags_Borders))
            {
                UI::AnimPreviewViewport::Draw(sceneData, deltaTime);
            }
            ImGui::EndChild();

            // Vertical splitter between viewport and timeline
            {
                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.18f, 0.18f, 0.20f, 1.0f));
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.26f, 0.26f, 0.30f, 1.0f));
                ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.30f, 0.30f, 0.36f, 1.0f));
                ImGui::BeginChild("##montage_tl_splitter", { 0.0f, splitterH });
                ImGui::Button("##montage_tl_splitter_btn", ImVec2(-1.0f, -1.0f));
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
            if (ImGui::BeginChild("##montage_timeline_area", { 0.0f, state.timelineHeight }, ImGuiChildFlags_Borders, ImGuiWindowFlags_NoScrollbar))
            {
                // Playback controls
                if (ImGui::BeginChild("##montage_playback", { 0.0f, 30.0f }))
                {
                    UI::AnimPlaybackControls::Draw(
                        state.playing, state.loop, state.timeSeconds, totalDuration,
                        linkedAnim != nullptr);
                }
                ImGui::EndChild();

                ImGui::Spacing();

                // Extended timeline with source events and notify callbacks
                if (ImGui::BeginChild("##montage_timeline", { 0.0f, 0.0f }))
                {
                    const std::vector<AnimationTimelineEvent> *srcEvents =
                        (linkedAnim && !linkedAnim->timelineEvents.empty()) ? &linkedAnim->timelineEvents : nullptr;
                    const std::vector<AnimNotifyCallback> *callbacks =
                        (!montage->GetNotifyCallbacks().empty()) ? &montage->GetNotifyCallbacks() : nullptr;

                    UI::AnimTimelineTrack::Draw(
                        ImGui::GetWindowDrawList(),
                        std::max(54.0f, ImGui::GetContentRegionAvail().y),
                        totalDuration,
                        &state.timeSeconds,
                        &state.playing,
                        srcEvents,
                        callbacks,
                        &state.selectedCallbackIndex,
                        linkedAnim ? nullptr : "Drop animation clip to assign");
                }
                ImGui::EndChild();
            }
            ImGui::EndChild();
        }
        ImGui::EndChild();

        // Horizontal splitter
        ImGui::SameLine(0.0f, 0.0f);
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.20f, 0.20f, 0.20f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.28f, 0.28f, 0.28f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.32f, 0.32f, 0.32f, 1.0f));
        ImGui::BeginChild("##montage_h_splitter", ImVec2(splitterWidth, 0.0f), ImGuiChildFlags_None);
        ImGui::Button("##montage_h_splitter_btn", ImVec2(-1.0f, -1.0f));
        if (ImGui::IsItemHovered() || ImGui::IsItemActive())
            ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeEW);
        if (ImGui::IsItemActive())
        {
            state.previewColumnWidth += ImGui::GetIO().MouseDelta.x;
            state.previewColumnWidth = std::clamp(state.previewColumnWidth, minLeftWidth, maxLeftWidth);
        }
        ImGui::EndChild();
        ImGui::PopStyleColor(3);

        ImGui::SameLine(0.0f, 0.0f);

        // ==== Right Column: Settings, Notify Callbacks, Body Part Selector ====
        ImGui::BeginChild("##montage_right", ImVec2(0.0f, 0.0f), ImGuiChildFlags_Borders);
        {
            if (ImGui::BeginTabBar("##montage_tabs"))
            {
                // Settings Tab
                if (ImGui::BeginTabItem("Settings"))
                {
                    // Animation Handle
                    {
                        AssetHandle animHandle = montage->GetAnimationHandle();
                        std::string animLabel = animHandle == AssetHandle(0)
                            ? "None" : assetManager->GetAssetDisplayName(animHandle);
                        if (UI::DrawAssetDropTarget("Animation Clip", animLabel.c_str(), { AssetType::SkeletalAnimation }, &animHandle, assetManager))
                        {
                            montage->SetAnimationHandle(animHandle);
                            montage->SetDirtyFlag(true);
                        }
                    }

                    ImGui::Spacing();

                    // Skeleton Handle
                    {
                        AssetHandle skelHandle = montage->GetSkeletonHandle();
                        std::string skelLabel = skelHandle == AssetHandle(0)
                            ? "None" : assetManager->GetAssetDisplayName(skelHandle);
                        if (UI::DrawAssetDropTarget("Skeleton", skelLabel.c_str(), { AssetType::Skeleton }, &skelHandle, assetManager))
                        {
                            montage->SetSkeletonHandle(skelHandle);
                            montage->SetDirtyFlag(true);
                        }
                    }

                    ImGui::Spacing();
                    ImGui::Separator();

                    // Range-based notifies
                    ImGui::TextUnformatted("Range Notifies");
                    auto &notifies = montage->GetAnimNotifies();
                    static char newNotifyName[128] = "NewNotify";
                    ImGui::InputText("##new_notif_name", newNotifyName, sizeof(newNotifyName));
                    ImGui::SameLine();
                    if (ImGui::Button("+ Add Notif"))
                    {
                        montage->AddNotif(newNotifyName, 0.0f, totalDuration > 0.0f ? totalDuration : 1.0f);
                        montage->SetDirtyFlag(true);
                    }

                    std::string notifToRemove;
                    for (auto &[nname, nnotif] : notifies)
                    {
                        ImGui::PushID(nname.c_str());
                        ImGui::Text("%s", nname.c_str());
                        bool changed = false;
                        changed |= ImGui::DragFloat("Start", &nnotif.startTime, 0.01f, 0.0f, nnotif.endTime);
                        changed |= ImGui::DragFloat("End", &nnotif.endTime, 0.01f, nnotif.startTime, totalDuration > 0.0f ? totalDuration : 100.0f);
                        if (changed) montage->SetDirtyFlag(true);
                        if (ImGui::Button("Remove"))
                        {
                            notifToRemove = nname;
                        }
                        ImGui::Separator();
                        ImGui::PopID();
                    }
                    if (!notifToRemove.empty())
                    {
                        montage->RemoveNotif(notifToRemove);
                        montage->SetDirtyFlag(true);
                    }

                    ImGui::EndTabItem();
                }

                // Callbacks Tab
                if (ImGui::BeginTabItem("Callbacks"))
                {
                    ImGui::TextUnformatted("Notify Callbacks");
                    ImGui::Separator();

                    if (ImGui::Button("+ Add Callback", ImVec2(-1.0f, 0.0f)))
                    {
                        montage->AddNotifyCallback(0.0f, AnimationTimelineEvent::Action::ScriptCallback, "NewCallback");
                    }

                    auto &callbacks = montage->GetNotifyCallbacks();
                    int cbToRemove = -1;
                    for (int i = 0; i < static_cast<int>(callbacks.size()); ++i)
                    {
                        auto &cb = callbacks[i];
                        ImGui::PushID(i);
                        bool isSelected = state.selectedCallbackIndex == i;
                        std::string label = std::format("{} ({:.3f})", cb.callbackName, cb.timestep);
                        if (ImGui::Selectable(label.c_str(), isSelected))
                        {
                            state.selectedCallbackIndex = i;
                        }

                        if (isSelected)
                        {
                            char nameBuf[128];
                            strncpy(nameBuf, cb.callbackName.c_str(), sizeof(nameBuf) - 1);
                            nameBuf[sizeof(nameBuf) - 1] = '\0';
                            if (ImGui::InputText("Name", nameBuf, sizeof(nameBuf)))
                            {
                                cb.callbackName = nameBuf;
                                montage->SetDirtyFlag(true);
                            }

                            if (ImGui::SliderFloat("Timestep", &cb.timestep, 0.0f, 1.0f, "%.3f"))
                            {
                                montage->SetDirtyFlag(true);
                            }

                            int actionInt = static_cast<int>(cb.actionType);
                            const char *actionNames[] = { "Audio", "ScriptCallback" };
                            if (ImGui::Combo("Action", &actionInt, actionNames, IM_ARRAYSIZE(actionNames)))
                            {
                                cb.actionType = static_cast<AnimationTimelineEvent::Action>(actionInt);
                                montage->SetDirtyFlag(true);
                            }

                            if (cb.actionType == AnimationTimelineEvent::Action::Audio)
                            {
                                AssetHandle audioH = cb.audioHandle;
                                std::string audioLabel = assetManager->GetAssetDisplayName(audioH);
                                if (UI::DrawAssetDropTarget("Audio Asset", audioLabel.c_str(), { AssetType::Audio }, &audioH, assetManager))
                                {
                                    cb.audioHandle = audioH;
                                    montage->SetDirtyFlag(true);
                                }
                            }

                            if (ImGui::Button("Remove Callback"))
                            {
                                cbToRemove = i;
                            }
                        }

                        ImGui::PopID();
                    }

                    if (cbToRemove >= 0)
                    {
                        montage->RemoveNotifyCallback(static_cast<size_t>(cbToRemove));
                        if (state.selectedCallbackIndex >= static_cast<int>(callbacks.size()))
                            state.selectedCallbackIndex = -1;
                    }

                    ImGui::EndTabItem();
                }

                // Body Parts Tab
                if (ImGui::BeginTabItem("Body Parts"))
                {
                    if (skeleton)
                    {
                        auto maskedJoints = montage->GetMaskedJoints();
                        bool dirty = false;
                        std::vector<int32_t> jointsCopy(maskedJoints.begin(), maskedJoints.end());
                        UI::SkeletonBodyPartSelector::Draw(skeleton, jointsCopy, dirty);
                        if (dirty)
                        {
                            montage->SetMaskedJoints(jointsCopy);
                        }
                    }
                    else
                    {
                        ImGui::TextDisabled("Assign a skeleton to configure body parts.");
                    }
                    ImGui::EndTabItem();
                }

                ImGui::EndTabBar();
            }
        }
        ImGui::EndChild();
    }
}
