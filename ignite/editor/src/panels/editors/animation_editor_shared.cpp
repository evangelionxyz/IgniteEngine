// Copyright (c) 2026 Evangelion Manuhutu

#include "pch.hpp"
#include "animation_editor_shared.hpp"
#include "ignite/graphics/render_target.hpp"
#include "ignite/asset/asset_manager.hpp"
#include <imgui_internal.h>
#include <format>

namespace ignite::UI
{
	void UI::AnimPreviewViewport::Draw(EditorSceneData &sceneData, float deltaTime)
	{
		const ImVec2 viewportSize = ImGui::GetContentRegionAvail();
		sceneData.viewportWidth = std::max(1u, static_cast<uint32_t>(viewportSize.x));
		sceneData.viewportHeight = std::max(1u, static_cast<uint32_t>(viewportSize.y));

		// Draw the preview texture if available, otherwise draw a dummy rectangle
		Ref<Texture> previewTexture = sceneData.compositeRT ? sceneData.compositeRT->GetColorAttachment(0) : nullptr;
		if (previewTexture && previewTexture->GetHandle())
		{
			ImGui::Image((ImTextureID)(previewTexture->GetHandle().Get()), viewportSize, ImVec2(0.0f, 0.0f), ImVec2(1.0f, 1.0f));
			sceneData.viewportHovered = ImGui::IsItemHovered();
		}
		else
		{
			ImGui::Dummy(viewportSize);
			sceneData.viewportHovered = ImGui::IsItemHovered();
		}
	}


	void AnimTimelineTrack::Draw(ImDrawList *dl, float tlHeight, float totalDuration, float *playbackTime, bool *isPlaying, const std::vector<AnimationTimelineEvent> *sourceEvents /*= nullptr*/, const std::vector<AnimNotifyCallback> *notifyCallbacks /*= nullptr*/, int *selectedCallbackIndex /*= nullptr*/, const char *emptyMessage /*= "No animation assigned"*/)
	{
		const float rulerHeight = 16.0f;
		const float laneHeight = 18.0f;
		const ImVec2 tlPos = ImGui::GetCursorScreenPos();
		const float tlWidth = ImGui::GetContentRegionAvail().x;

		// Count active lanes
		int laneCount = 0;
		if (sourceEvents && !sourceEvents->empty())
			laneCount++;

		if (notifyCallbacks && !notifyCallbacks->empty())
			laneCount++;

		const float actualHeight = std::max(tlHeight, rulerHeight + laneCount * laneHeight + 20.0f);

		ImGui::InvisibleButton("##anim_tl", { tlWidth, actualHeight });
		const bool isHovered = ImGui::IsItemHovered();
		const bool isActive = ImGui::IsItemActive();

		// Background
		dl->AddRectFilled(tlPos, ImVec2(tlPos.x + tlWidth, tlPos.y + actualHeight), IM_COL32(30, 30, 35, 255));
		dl->AddRect(tlPos, ImVec2(tlPos.x + tlWidth, tlPos.y + actualHeight), IM_COL32(70, 70, 80, 255));

		if (totalDuration <= 0.0f)
		{
			const ImVec2 ns = ImGui::CalcTextSize(emptyMessage);
			dl->AddText(ImVec2(tlPos.x + (tlWidth - ns.x) * 0.5f, tlPos.y + (actualHeight - ns.y) * 0.5f),
				IM_COL32(110, 110, 120, 200), emptyMessage);
			return;
		}

		// Ruler: time labels at intervals
		const int tickCount = std::max(1, static_cast<int>(totalDuration * 10.0f));
		const float tickInterval = totalDuration / static_cast<float>(tickCount);
		for (int i = 0; i <= tickCount; ++i)
		{
			float t = static_cast<float>(i) * tickInterval;
			float px = tlPos.x + (t / totalDuration) * tlWidth;
			dl->AddLine(ImVec2(px, tlPos.y), ImVec2(px, tlPos.y + rulerHeight), IM_COL32(55, 55, 65, 255));

			if (i % std::max(1, tickCount / 10) == 0)
			{
				std::string ts = std::format("{:.2f}s", t);
				dl->AddText(ImVec2(px + 2.0f, tlPos.y + 1.0f), IM_COL32(160, 160, 180, 200), ts.c_str());
			}
		}

		// Horizontal divider under ruler
		dl->AddLine({ tlPos.x, tlPos.y + rulerHeight }, { tlPos.x + tlWidth, tlPos.y + rulerHeight }, IM_COL32(70, 70, 80, 255));

		float currentLaneY = tlPos.y + rulerHeight + 2.0f;

		// Source animation events lane (read-only)
		if (sourceEvents && !sourceEvents->empty())
		{
			dl->AddText(ImVec2(tlPos.x + 4.0f, currentLaneY), IM_COL32(180, 180, 200, 200), "Src Events");
			for (const auto &event : *sourceEvents)
			{
				float eventX = tlPos.x + event.normalizedTime * tlWidth;
				ImU32 color = (event.action == AnimationTimelineEvent::Action::Audio)
					? IM_COL32(100, 200, 255, 255) : IM_COL32(255, 200, 80, 255);

				// Diamond marker
				dl->AddQuadFilled(
					ImVec2(eventX, currentLaneY + 4.0f),
					ImVec2(eventX + 5.0f, currentLaneY + laneHeight * 0.5f + 4.0f),
					ImVec2(eventX, currentLaneY + laneHeight + 4.0f),
					ImVec2(eventX - 5.0f, currentLaneY + laneHeight * 0.5f + 4.0f),
					color);

				// Tooltip
				ImVec2 markerMin(eventX - 6.0f, currentLaneY + 2.0f);
				ImVec2 markerMax(eventX + 6.0f, currentLaneY + laneHeight + 6.0f);
				if (ImGui::IsMouseHoveringRect(markerMin, markerMax))
				{
					ImGui::SetTooltip("%s (%.2f)", event.name.c_str(), event.normalizedTime);
				}
			}
			currentLaneY += laneHeight + 4.0f;
		}

		// Notify callbacks lane (editable)
		if (notifyCallbacks && !notifyCallbacks->empty())
		{
			dl->AddText(ImVec2(tlPos.x + 4.0f, currentLaneY), IM_COL32(180, 180, 200, 200), "Callbacks");
			for (int i = 0; i < static_cast<int>(notifyCallbacks->size()); ++i)
			{
				const auto &cb = (*notifyCallbacks)[i];
				float cbX = tlPos.x + cb.timestep * tlWidth;
				bool isSelected = selectedCallbackIndex && *selectedCallbackIndex == i;
				ImU32 color = isSelected ? IM_COL32(255, 100, 60, 255) : IM_COL32(60, 200, 120, 255);

				// Circle marker
				float cy = currentLaneY + laneHeight * 0.5f + 4.0f;
				dl->AddCircleFilled(ImVec2(cbX, cy), isSelected ? 6.0f : 4.0f, color);
				dl->AddCircle(ImVec2(cbX, cy), (isSelected ? 6.0f : 4.0f) + 1.0f, IM_COL32(255, 255, 255, 200));

				// Click to select
				ImVec2 markerMin(cbX - 7.0f, currentLaneY + 2.0f);
				ImVec2 markerMax(cbX + 7.0f, currentLaneY + laneHeight + 6.0f);
				if (ImGui::IsMouseHoveringRect(markerMin, markerMax))
				{
					ImGui::SetTooltip("%s (%.3f)", cb.callbackName.c_str(), cb.timestep);
					if (ImGui::IsMouseClicked(ImGuiMouseButton_Left) && selectedCallbackIndex)
					{
						*selectedCallbackIndex = i;
					}
				}
			}
			currentLaneY += laneHeight + 4.0f;
		}

		// Playhead line
		const float phX = tlPos.x + (*playbackTime / totalDuration) * tlWidth;
		dl->AddLine(ImVec2(phX, tlPos.y), ImVec2(phX, tlPos.y + actualHeight), IM_COL32(255, 100, 60, 230), 2.0f);
		dl->AddTriangleFilled(ImVec2(phX - 5, tlPos.y), ImVec2(phX + 5, tlPos.y), ImVec2(phX, tlPos.y + 10), IM_COL32(255, 100, 60, 230));

		// Scrubbing
		if ((isHovered || isActive) && ImGui::IsMouseDown(ImGuiMouseButton_Left))
		{
			const float mx = std::clamp(ImGui::GetMousePos().x - tlPos.x, 0.0f, tlWidth);
			*playbackTime = (mx / tlWidth) * totalDuration;
			*isPlaying = false;
		}
	}


	void SkeletonBodyPartSelector::Draw(const Ref<Skeleton> &skeleton, std::vector<int32_t> &maskedJoints, bool &dirty)
	{
		if (!skeleton || skeleton->joints.empty())
		{
			ImGui::TextDisabled("No skeleton assigned.");
			return;
		}

		ImGui::TextUnformatted("Body Part Mask");
		ImGui::Separator();

		if (ImGui::Button("Select All", ImVec2(-1.0f, 0.0f)))
		{
			maskedJoints.clear();
			maskedJoints.reserve(skeleton->joints.size());
			for (const auto &joint : skeleton->joints)
			{
				maskedJoints.push_back(joint.id);
			}
			dirty = true;
		}

		if (ImGui::Button("Deselect All", ImVec2(-1.0f, 0.0f)))
		{
			maskedJoints.clear();
			dirty = true;
		}

		ImGui::Spacing();

		// Build children map
		std::vector<std::vector<int32_t>> children(skeleton->joints.size());
		for (const auto &joint : skeleton->joints)
		{
			if (joint.parentJointId >= 0 && joint.parentJointId < static_cast<int32_t>(skeleton->joints.size()))
			{
				children[static_cast<size_t>(joint.parentJointId)].push_back(joint.id);
			}
		}

		auto isJointMasked = [&](int32_t jointId) -> bool
			{
				return std::find(maskedJoints.begin(), maskedJoints.end(), jointId) != maskedJoints.end();
			};

		auto toggleJoint = [&](int32_t jointId)
			{
				auto it = std::find(maskedJoints.begin(), maskedJoints.end(), jointId);
				if (it != maskedJoints.end())
				{
					maskedJoints.erase(it);
				}
				else
				{
					maskedJoints.push_back(jointId);
				}
				dirty = true;
			};

		std::function<void(int32_t)> drawJointCheckbox = [&](int32_t jointId)
			{
				if (jointId < 0 || jointId >= static_cast<int32_t>(skeleton->joints.size()))
					return;

				const Joint &joint = skeleton->joints[static_cast<size_t>(jointId)];
				bool checked = isJointMasked(jointId);
				const bool hasChildren = !children[static_cast<size_t>(jointId)].empty();

				ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_SpanAvailWidth;
				if (!hasChildren) flags |= ImGuiTreeNodeFlags_Leaf;

				bool opened = ImGui::TreeNodeEx(
					reinterpret_cast<void *>(static_cast<intptr_t>(jointId + 1)),
					flags, "");
				ImGui::SameLine();
				if (ImGui::Checkbox(std::format("##{}", jointId).c_str(), &checked))
				{
					toggleJoint(jointId);
				}
				ImGui::SameLine();
				ImGui::TextUnformatted(joint.name.c_str());

				if (opened)
				{
					for (int32_t childId : children[static_cast<size_t>(jointId)])
					{
						drawJointCheckbox(childId);
					}
					ImGui::TreePop();
				}
			};

		for (const auto &joint : skeleton->joints)
		{
			if (joint.parentJointId == -1)
			{
				drawJointCheckbox(joint.id);
			}
		}
	}


	void AnimPlaybackControls::Draw(bool &playing, bool &loop, float &timeSeconds, float totalDuration, bool enabled /*= true*/)
	{
		ImGui::BeginDisabled(!enabled);
		if (ImGui::Button(playing ? "Pause" : "Play"))
		{
			playing = !playing;
		}
		ImGui::SameLine();
		if (ImGui::Button("Stop"))
		{
			playing = false;
			timeSeconds = 0.0f;
		}
		ImGui::SameLine();
		ImGui::Checkbox("Loop", &loop);
		ImGui::SameLine();
		ImGui::SetNextItemWidth(120.0f);
		if (ImGui::SliderFloat("##time_scrub", &timeSeconds, 0.0f, totalDuration, "%.3fs"))
		{
			playing = false;
		}
		ImGui::EndDisabled();
	}

}
