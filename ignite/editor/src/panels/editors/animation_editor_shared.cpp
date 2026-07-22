// Copyright (c) 2026 Evangelion Manuhutu

#include "pch.hpp"
#include "animation_editor_shared.hpp"
#include "ignite/graphics/render_target.hpp"
#include "ignite/asset/asset_manager.hpp"
#include "ignite/imgui/gizmo.hpp"
#include "ignite/math/math.hpp"
#include <imgui_internal.h>
#include <format>

namespace ignite::UI
{
	void UI::AnimPreviewViewport::Draw(EditorSceneData &sceneData, float deltaTime)
	{
		ImGui::TextUnformatted("Viewport");
		ImGui::Separator();

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

	void UI::AnimPreviewViewport::DrawOverlay(
		EditorSceneData &sceneData,
		const Ref<Skeleton> &skeleton,
		const std::vector<glm::mat4> *previewGlobalTransforms,
		int32_t &selectedJoint,
		int32_t &selectedSocket,
		int gizmoTarget,
		Gizmo &gizmo,
		bool &isDirty)
	{
		if (!skeleton)
			return;

		const ImVec2 viewportPos = ImGui::GetItemRectMin();
		const ImVec2 viewportSize = ImGui::GetItemRectSize();

		if (viewportSize.x <= 0.0f || viewportSize.y <= 0.0f)
			return;

		const glm::mat4 viewProjection = sceneData.camera.GetProjection() * sceneData.camera.GetView();
		const Rect viewportRect{ viewportPos.x, viewportPos.y, viewportPos.x + viewportSize.x, viewportPos.y + viewportSize.y };

		const bool hasSelectedJoint = selectedJoint >= 0 && selectedJoint < static_cast<int32_t>(skeleton->joints.size());
		const bool hasSelectedSocket = selectedSocket >= 0 && selectedSocket < static_cast<int32_t>(skeleton->sockets.size());
		const bool useSocketGizmo = (gizmoTarget == 1) && hasSelectedSocket;
		const bool previewViewportFocused = sceneData.viewportHovered && ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows);
		const bool hasPreviewPose = previewGlobalTransforms && previewGlobalTransforms->size() == skeleton->joints.size();

		auto getJointGlobal = [&](int32_t jointId) -> const glm::mat4 &
		{
			if (hasPreviewPose && jointId >= 0 && jointId < static_cast<int32_t>(previewGlobalTransforms->size()))
			{
				return (*previewGlobalTransforms)[static_cast<size_t>(jointId)];
			}
			if (jointId >= 0 && jointId < static_cast<int32_t>(skeleton->joints.size()))
			{
				return skeleton->joints[static_cast<size_t>(jointId)].globalTransform;
			}
			static const glm::mat4 identity(1.0f);
			return identity;
		};

		bool isPreviewGizmoManipulating = false;
		bool isPreviewGizmoHovered = false;

		if (previewViewportFocused && (useSocketGizmo || hasSelectedJoint))
		{
			GizmoInfo gizmoInfo;
			gizmoInfo.cameraView = sceneData.camera.GetView();
			gizmoInfo.cameraProjection = sceneData.camera.GetProjection();
			gizmoInfo.cameraType = sceneData.camera.projectionType;
			gizmoInfo.snapValue = 0.05f;
			gizmoInfo.isSnapping = false;
			gizmoInfo.viewRect = viewportRect;

			gizmo.SetInfo(gizmoInfo);
			gizmo.SetOperation(ImGuizmo::OPERATION::TRANSLATE);
			gizmo.SetMode(ImGuizmo::MODE::LOCAL);

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

			gizmo.Manipulate(gizmoTransform);
			isPreviewGizmoManipulating = gizmo.IsManipulating();
			isPreviewGizmoHovered = gizmo.IsHovered();

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

				isDirty = true;
				skeleton->SetDirtyFlag(true);
			}
		}

		if (sceneData.viewportHovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left) && !isPreviewGizmoManipulating && !isPreviewGizmoHovered)
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


	void AnimTimelineTrack::Draw(ImDrawList *dl, float tlHeight, float totalDuration, float *playbackTime, bool *isPlaying, const std::vector<AnimationTimelineEvent> *sourceEvents /*= nullptr*/, const std::vector<AnimNotifyCallback> *notifyCallbacks /*= nullptr*/, int *selectedCallbackIndex /*= nullptr*/, int *selectedEventIndex /*= nullptr*/, const char *emptyMessage /*= "No animation assigned"*/)
	{
		static int s_DraggingEventIndex = -1;
		static int s_DraggingCallbackIndex = -1;

		if (!ImGui::IsMouseDown(ImGuiMouseButton_Left))
		{
			s_DraggingEventIndex = -1;
			s_DraggingCallbackIndex = -1;
		}

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
		bool clickedOnAnyMarker = false;

		// Source animation events lane (editable)
		if (sourceEvents && !sourceEvents->empty())
		{
			dl->AddText(ImVec2(tlPos.x + 4.0f, currentLaneY), IM_COL32(180, 180, 200, 200), "Src Events");
			for (int i = 0; i < static_cast<int>(sourceEvents->size()); ++i)
			{
				const auto &event = (*sourceEvents)[i];
				float eventX = tlPos.x + event.normalizedTime * tlWidth;
				bool isSelected = (selectedEventIndex && *selectedEventIndex == i);

				ImU32 color = (event.action == AnimationTimelineEvent::Action::Audio)
					? (isSelected ? IM_COL32(140, 230, 255, 255) : IM_COL32(100, 200, 255, 255))
					: (isSelected ? IM_COL32(255, 230, 120, 255) : IM_COL32(255, 200, 80, 255));

				const float size = isSelected ? 7.0f : 5.0f;
				// Diamond marker
				dl->AddQuadFilled(
					ImVec2(eventX, currentLaneY + 4.0f - (size - 5.0f)),
					ImVec2(eventX + size, currentLaneY + laneHeight * 0.5f + 4.0f),
					ImVec2(eventX, currentLaneY + laneHeight + 4.0f + (size - 5.0f)),
					ImVec2(eventX - size, currentLaneY + laneHeight * 0.5f + 4.0f),
					color);

				if (isSelected)
				{
					dl->AddQuad(
						ImVec2(eventX, currentLaneY + 4.0f - (size - 5.0f)),
						ImVec2(eventX + size, currentLaneY + laneHeight * 0.5f + 4.0f),
						ImVec2(eventX, currentLaneY + laneHeight + 4.0f + (size - 5.0f)),
						ImVec2(eventX - size, currentLaneY + laneHeight * 0.5f + 4.0f),
						IM_COL32(255, 255, 255, 255), 1.5f);
				}

				// Click to select & drag to move
				ImVec2 markerMin(eventX - 7.0f, currentLaneY + 2.0f);
				ImVec2 markerMax(eventX + 7.0f, currentLaneY + laneHeight + 6.0f);
				if (ImGui::IsMouseHoveringRect(markerMin, markerMax))
				{
					ImGui::SetTooltip("%s (%.2f)", event.name.c_str(), event.normalizedTime);
					if (ImGui::IsMouseClicked(ImGuiMouseButton_Left))
					{
						clickedOnAnyMarker = true;
						s_DraggingEventIndex = i;
						s_DraggingCallbackIndex = -1;
						if (selectedEventIndex) *selectedEventIndex = i;
						if (selectedCallbackIndex) *selectedCallbackIndex = -1;
					}
				}

				if (s_DraggingEventIndex == i && ImGui::IsMouseDown(ImGuiMouseButton_Left))
				{
					const float mouseX = ImGui::GetMousePos().x;
					const_cast<AnimationTimelineEvent &>(event).normalizedTime = std::clamp((mouseX - tlPos.x) / tlWidth, 0.0f, 1.0f);
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
					if (ImGui::IsMouseClicked(ImGuiMouseButton_Left))
					{
						clickedOnAnyMarker = true;
						s_DraggingCallbackIndex = i;
						s_DraggingEventIndex = -1;
						if (selectedCallbackIndex) *selectedCallbackIndex = i;
						if (selectedEventIndex) *selectedEventIndex = -1;
					}
				}

				if (s_DraggingCallbackIndex == i && ImGui::IsMouseDown(ImGuiMouseButton_Left))
				{
					const float mouseX = ImGui::GetMousePos().x;
					const_cast<AnimNotifyCallback &>(cb).timestep = std::clamp((mouseX - tlPos.x) / tlWidth, 0.0f, 1.0f);
				}
			}
			currentLaneY += laneHeight + 4.0f;
		}

		// Deselect markers if clicking empty timeline space
		if (ImGui::IsMouseClicked(ImGuiMouseButton_Left) && isHovered && !clickedOnAnyMarker)
		{
			s_DraggingEventIndex = -1;
			s_DraggingCallbackIndex = -1;
			if (selectedEventIndex) *selectedEventIndex = -1;
			if (selectedCallbackIndex) *selectedCallbackIndex = -1;
		}

		// Playhead line
		const float phX = tlPos.x + (*playbackTime / totalDuration) * tlWidth;
		dl->AddLine(ImVec2(phX, tlPos.y), ImVec2(phX, tlPos.y + actualHeight), IM_COL32(255, 100, 60, 230), 2.0f);
		dl->AddTriangleFilled(ImVec2(phX - 5, tlPos.y), ImVec2(phX + 5, tlPos.y), ImVec2(phX, tlPos.y + 10), IM_COL32(255, 100, 60, 230));

		// Scrubbing (only if not dragging a marker)
		if ((isHovered || isActive) && ImGui::IsMouseDown(ImGuiMouseButton_Left) && s_DraggingEventIndex < 0 && s_DraggingCallbackIndex < 0)
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
