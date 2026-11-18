// Copyright (c) 2026 Evangelion Manuhutu

#include "ignite_pch.hpp"

#include "game_ui_system.hpp"
#include "ignite/scene/scene.hpp"
#include "ignite/core/subsystem.hpp"
#include "ignite/core/application.hpp"
#include "ignite/core/input/input_system.hpp"
#include "ignite/graphics/ui/widget_canvas.hpp"
#include "ignite/graphics/ui/widget_container.hpp"
#include "ignite/project/project.hpp"

namespace ignite
{
	struct GameUISystemData
	{
		Scene* scene = nullptr;
		Project* project = nullptr;
		std::string hoveredItemName;
	};

	GameUISystemData s_GameUIData;

	namespace
	{
		static bool HitTestRecursive(IWidgetItem* item, float mx, float my, std::string* outHitName)
		{
			if (!item || !item->IsVisible())
				return false;

			for (auto it = item->children.rbegin(); it != item->children.rend(); ++it)
			{
				if (HitTestRecursive(it->get(), mx, my, outHitName))
				{
					return true;
				}
			}

			if (item->GetWidgetType() == WidgetType::Button ||
				item->GetWidgetType() == WidgetType::Label ||
				item->GetWidgetType() == WidgetType::Image)
			{
				if (item->HitTest(static_cast<int>(mx), static_cast<int>(my)))
				{
					if (outHitName)
						*outHitName = item->name;
					return true;
				}
			}

			return false;
		}

		static bool CheckCanvasRecursive(const Ref<WidgetCanvas>& canvas, float mx, float my, Project* project, std::string* outHitName)
		{
			if (!canvas || !canvas->IsEnabled())
				return false;

			if (project)
			{
				const auto& children = canvas->GetChildWidgets();
				for (auto it = children.rbegin(); it != children.rend(); ++it)
				{
					const WidgetChildEntry& child = *it;
					if (!child.enabled || child.handle == AssetHandle(0))
						continue;

					Ref<WidgetCanvas> childCanvas = AssetManager::GetInstance()->GetAsset<WidgetCanvas>(child.handle);
					if (childCanvas && CheckCanvasRecursive(childCanvas, mx, my, project, outHitName))
					{
						return true;
					}
					if (child.blockWidgetsBelow)
						return false;
				}
			}

			WidgetContainer* root = canvas->GetRoot();
			if (root && HitTestRecursive(root, mx, my, outHitName))
			{
				return true;
			}

			return false;
		}
	}

	void GameUISystem::Init() {}
	void GameUISystem::Shutdown()
	{
		s_GameUIData.scene = nullptr;
		s_GameUIData.project = nullptr;
		s_GameUIData.hoveredItemName.clear();
	}

	void GameUISystem::SetSceneContext(Scene* scene)
	{
		s_GameUIData.scene = scene;
		s_GameUIData.hoveredItemName.clear();
	}

	bool GameUISystem::IsMouseOverUI(float mouseX, float mouseY)
	{
		if (InputSystem::GetCursorMode() == CursorMode::Disabled)
			return false;

		if (!s_GameUIData.scene)
			return false;

		Ref<WidgetCanvas> rootWidget = s_GameUIData.scene->GetRootWidget();
		if (!rootWidget)
			return false;

		Project* project = s_GameUIData.scene->GetProject();
		return CheckCanvasRecursive(rootWidget, mouseX, mouseY, project, nullptr);
	}

	void GameUISystem::HandleMouseMotion(float mouseX, float mouseY)
	{
		s_GameUIData.hoveredItemName.clear();
		if (InputSystem::GetCursorMode() == CursorMode::Disabled || !s_GameUIData.scene)
			return;

		Ref<WidgetCanvas> rootWidget = s_GameUIData.scene->GetRootWidget();
		if (!rootWidget)
			return;

		Project* project = s_GameUIData.scene->GetProject();
		CheckCanvasRecursive(rootWidget, mouseX, mouseY, project, &s_GameUIData.hoveredItemName);
	}

	const std::string &GameUISystem::GetHoveredItemName()
	{
		return s_GameUIData.hoveredItemName;
	}
}
