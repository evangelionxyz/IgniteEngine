// Copyright (c) 2026 Evangelion Manuhutu
#include "game_ui_system.hpp"
#include "ignite/scene/scene.hpp"
#include "ignite/core/subsystem.hpp"
#include "ignite/core/application.hpp"
#include "ignite/graphics/ui/widget_canvas.hpp"
#include "ignite/graphics/ui/widget_container.hpp"
#include "ignite/project/project.hpp"

namespace ignite
{
	struct GameUISystemData
	{
		Scene* scene = nullptr;
		Project* project = nullptr;
	};

	GameUISystemData s_GameUIData;

	namespace
	{
		static bool HitTestRecursive(IWidgetItem* item, float mx, float my)
		{
			if (!item || !item->IsVisible())
				return false;

			if (item->GetWidgetType() == WidgetType::Button ||
				item->GetWidgetType() == WidgetType::Label ||
				item->GetWidgetType() == WidgetType::Image)
			{
				if (item->HitTest(static_cast<int>(mx), static_cast<int>(my)))
				{
					return true;
				}
			}

			for (const Ref<IWidgetItem>& child : item->children)
			{
				if (HitTestRecursive(child.get(), mx, my))
				{
					return true;
				}
			}

			return false;
		}

		static bool CheckCanvasRecursive(const Ref<WidgetCanvas>& canvas, float mx, float my, Project* project)
		{
			if (!canvas || !canvas->IsEnabled())
				return false;

			WidgetContainer* root = canvas->GetRoot();
			if (root)
			{
				if (HitTestRecursive(root, mx, my))
				{
					return true;
				}
			}

			if (project)
			{
				for (const WidgetChildEntry& child : canvas->GetChildWidgets())
				{
					if (!child.enabled || child.handle == AssetHandle(0))
						continue;

					Ref<WidgetCanvas> childCanvas = project->GetAsset<WidgetCanvas>(child.handle);
					if (childCanvas)
					{
						if (CheckCanvasRecursive(childCanvas, mx, my, project))
						{
							return true;
						}
					}
				}
			}

			return false;
		}
	}

	void GameUISystem::Init() {}
	void GameUISystem::Shutdown()
	{
		s_GameUIData.scene = nullptr;
		s_GameUIData.project = nullptr;
	}

	void GameUISystem::SetSceneContext(Scene* scene)
	{
		s_GameUIData.scene = scene;
	}

	bool GameUISystem::IsMouseOverUI(float mouseX, float mouseY)
	{
		if (!s_GameUIData.scene)
			return false;

		Ref<WidgetCanvas> rootWidget = s_GameUIData.scene->GetRootWidget();
		if (!rootWidget)
			return false;

		Project* project = s_GameUIData.scene->GetProject();
		return CheckCanvasRecursive(rootWidget, mouseX, mouseY, project);
	}

	void GameUISystem::HandleMouseMotion(float mouseX, float mouseY)
	{

	}

}
