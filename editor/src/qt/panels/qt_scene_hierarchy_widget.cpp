// Copyright (c) 2026 Evangelion Manuhutu

#include "qt_scene_hierarchy_widget.hpp"

#include "editor_layer.hpp"
#include "ignite/core/command.hpp"
#include "ignite/scene/component.hpp"
#include "ignite/scene/entity.hpp"
#include "ignite/scene/entity_command_manager.hpp"
#include "ignite/scene/entity_destroy_command.hpp"
#include "ignite/scene/entity_reparent_command.hpp"
#include "ignite/scene/entity_rename_command.hpp"
#include "ignite/scene/scene.hpp"
#include "ignite/scene/scene_manager.hpp"

#include <QAbstractItemView>
#include <QContextMenuEvent>
#include <QInputDialog>
#include <QLabel>
#include <QMenu>
#include <QApplication>
#include <QSignalBlocker>
#include <QTimer>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QVBoxLayout>
#include <memory>
#include <functional>
#include <unordered_map>
#include <vector>

namespace ignite
{
    constexpr int kEntityUuidRole = Qt::UserRole + 1;

    enum class CreateKind
    {
        Empty,
        Camera,
        Widget,
        Sprite,
        Circle,
        PointLight,
        Mesh,
        DirectionalLight,
        WorldEnvironment
    };

    UUID GetItemUuid(const QTreeWidgetItem *item)
    {
        if (!item)
        {
            return UUID(0);
        }

        return UUID(item->data(0, kEntityUuidRole).toULongLong());
    }

    bool ReparentEntity(Scene *scene, UUID sourceUuid, UUID newParentUuid)
    {
        if (!scene || sourceUuid == UUID(0))
        {
            return false;
        }

        Entity sourceEntity = SceneManager::GetEntity(scene, sourceUuid);
        if (!sourceEntity.IsValid())
        {
            return false;
        }

        const UUID oldParentUuid = sourceEntity.GetParentUUID();
        if (oldParentUuid == newParentUuid)
        {
            return false;
        }

        if (newParentUuid == UUID(0))
        {
            if (oldParentUuid == UUID(0))
            {
                return false;
            }

            Entity oldParentEntity = SceneManager::GetEntity(scene, oldParentUuid);
            if (oldParentEntity.IsValid())
            {
                oldParentEntity.GetComponent<IDComponent>().RemoveChild(sourceUuid);
            }
            sourceEntity.GetComponent<IDComponent>().parent = UUID(0);
        }
        else
        {
            Entity newParentEntity = SceneManager::GetEntity(scene, newParentUuid);
            if (!newParentEntity.IsValid())
            {
                return false;
            }

            SceneManager::AddChild(scene, newParentEntity, sourceEntity);
            if (sourceEntity.GetParentUUID() != newParentUuid)
            {
                return false;
            }
        }

        CommandManager::AddCommand(CreateScope<EntityReparentCommand>(scene, sourceUuid, oldParentUuid, newParentUuid));
        return true;
    }


    Entity CreateEntityByKind(Scene *scene, CreateKind kind, const std::string &name, UUID uuid)
    {
        using namespace ignite;

        switch (kind)
        {
            case CreateKind::Empty:
            return SceneManager::CreateEmptyEntity(scene, name, uuid);
            case CreateKind::Camera:
            return SceneManager::CreateCamera(scene, name, uuid);
            case CreateKind::Widget:
            {
                Entity entity = SceneManager::CreateEmptyEntity(scene, name, uuid);
                if (entity.IsValid() && !entity.HasComponent<WidgetComponent>())
                {
                    entity.AddComponent<WidgetComponent>();
                }
                return entity;
            }
            case CreateKind::Sprite:
            return SceneManager::CreateSprite(scene, name, uuid);
            case CreateKind::Circle:
            return SceneManager::CreateCircle(scene, name, uuid);
            case CreateKind::PointLight:
            return SceneManager::CreatePointLight2D(scene, name, uuid);
            case CreateKind::Mesh:
            {
                Entity entity = SceneManager::CreateEmptyEntity(scene, name, uuid);
                if (entity.IsValid() && !entity.HasComponent<MeshComponent>())
                {
                    entity.AddComponent<MeshComponent>();
                }
                return entity;
            }
            case CreateKind::DirectionalLight:
            {
                Entity entity = SceneManager::CreateEmptyEntity(scene, name, uuid);
                if (entity.IsValid() && !entity.HasComponent<DirectionalLightComponent>())
                {
                    entity.AddComponent<DirectionalLightComponent>();
                }
                return entity;
            }
            case CreateKind::WorldEnvironment:
            return SceneManager::CreateWorldEnvironment(scene, name, uuid);
            default:
            return {};
        }
    }

    Entity ExecuteCreateEntityCommand(Scene *scene, CreateKind kind, const std::string &name, UUID parentUuid = UUID(0))
    {
        auto createdUuid = UUID();

        auto createFunc = [scene, kind, name, parentUuid, createdUuid]() mutable
        {
            Entity entity = CreateEntityByKind(scene, kind, name, createdUuid);
            if (!entity.IsValid())
            {
                return;
            }

            if (parentUuid != UUID(0))
            {
                if (Entity parent = SceneManager::GetEntity(scene, parentUuid); parent.IsValid())
                {
                    SceneManager::AddChild(scene, parent, entity);
                }
            }
        };

        auto destroyFunc = [scene, createdUuid]()
        {
            if (createdUuid == UUID(0))
            {
                return;
            }

            if (Entity entity = SceneManager::GetEntity(scene, createdUuid); entity.IsValid())
            {
                SceneManager::DestroyEntity(scene, entity);
            }
        };

        CommandManager::ExecuteCommand(CreateScope<EntityManagerCommand>(createFunc, destroyFunc, CommandState_Create));
        return SceneManager::GetEntity(scene,createdUuid);
    }

    void AddCreateActions(QMenu &menu, Scene *scene, UUID parentUuid, QtSceneHierarchyWidget *owner)
    {
        using namespace ignite;

        auto addAction = [&menu, scene, parentUuid, owner](const char *label, CreateKind kind, const char *defaultName)
        {
            QAction *action = menu.addAction(label);
            QObject::connect(action, &QAction::triggered, &menu, [scene, parentUuid, kind, defaultName]()
            {
                ExecuteCreateEntityCommand(scene, kind, defaultName, parentUuid);
            });
            QObject::connect(action, &QAction::triggered, &menu, [owner]()
            {
                if (owner)
                {
                    owner->RefreshHierarchy();
                }
            });
        };

        addAction("Empty", CreateKind::Empty, "Empty");
        addAction("Camera", CreateKind::Camera, "Camera");
        addAction("Widget", CreateKind::Widget, "Widget");

        QMenu *menu2D = menu.addMenu("2D");
        QObject::connect(menu2D->addAction("Sprite"), &QAction::triggered, menu2D, [scene, parentUuid, owner]()
        {
            ExecuteCreateEntityCommand(scene, CreateKind::Sprite, "Sprite", parentUuid);
            if (owner) owner->RefreshHierarchy();
        });
        QObject::connect(menu2D->addAction("Circle"), &QAction::triggered, menu2D, [scene, parentUuid, owner]()
        {
            ExecuteCreateEntityCommand(scene, CreateKind::Circle, "Circle", parentUuid);
            if (owner) owner->RefreshHierarchy();
        });
        QObject::connect(menu2D->addAction("Point Light"), &QAction::triggered, menu2D, [scene, parentUuid, owner]()
        {
            ExecuteCreateEntityCommand(scene, CreateKind::PointLight, "Point Light 2D", parentUuid);
            if (owner) owner->RefreshHierarchy();
        });

        QMenu *menu3D = menu.addMenu("3D");
        QObject::connect(menu3D->addAction("Mesh"), &QAction::triggered, menu3D, [scene, parentUuid, owner]()
        {
            ExecuteCreateEntityCommand(scene, CreateKind::Mesh, "Mesh", parentUuid);
            if (owner) owner->RefreshHierarchy();
        });
        QObject::connect(menu3D->addAction("Directional Light"), &QAction::triggered, menu3D, [scene, parentUuid, owner]()
        {
            ExecuteCreateEntityCommand(scene, CreateKind::DirectionalLight, "Directional Light", parentUuid);
            if (owner) owner->RefreshHierarchy();
        });
        QObject::connect(menu3D->addAction("World Environment"), &QAction::triggered, menu3D, [scene, parentUuid, owner]()
        {
            ExecuteCreateEntityCommand(scene, CreateKind::WorldEnvironment, "World Environment", parentUuid);
            if (owner) owner->RefreshHierarchy();
        });
    }

    class SceneHierarchyTreeWidget final : public QTreeWidget
    {
    public:
        explicit SceneHierarchyTreeWidget(QWidget *parent = nullptr)
            : QTreeWidget(parent)
        {
            setHeaderHidden(true);
            setIndentation(10);
            setDragEnabled(true);
            setAcceptDrops(true);
            setDropIndicatorShown(true);
            setDefaultDropAction(Qt::MoveAction);
            setDragDropMode(QAbstractItemView::DragDrop);
            setSelectionMode(QAbstractItemView::ExtendedSelection);
        }

    protected:
        void startDrag(Qt::DropActions supportedActions) override
        {
            m_DraggedSourceUuid = UUID(0);

            if (QTreeWidgetItem *item = currentItem())
            {
                m_DraggedSourceUuid = GetItemUuid(item);
            }

            if (m_DraggedSourceUuid == UUID(0))
            {
                const QList<QTreeWidgetItem *> selected = selectedItems();
                if (!selected.isEmpty())
                {
                    m_DraggedSourceUuid = GetItemUuid(selected.front());
                }
            }

            QTreeWidget::startDrag(supportedActions);
        }

        void contextMenuEvent(QContextMenuEvent *event) override
        {
            Scene *scene = EditorLayer::GetInstance() ? EditorLayer::GetInstance()->GetActiveScene().get() : nullptr;
            if (!scene)
            {
                return;
            }

            QTreeWidgetItem *item = itemAt(event->pos());
            const UUID targetUuid = GetItemUuid(item);

            QMenu menu(this);
            AddCreateActions(menu, scene, targetUuid, static_cast<QtSceneHierarchyWidget *>(parentWidget()));
            menu.addSeparator();

            if (item)
            {
                QAction *renameAction = menu.addAction("Rename");
                QAction *duplicateAction = menu.addAction("Duplicate");
                QAction *deleteAction = menu.addAction("Delete");

                QAction *selectedAction = menu.exec(event->globalPos());
                if (!selectedAction)
                {
                    return;
                }

                if (selectedAction == renameAction)
                {
                    if (Entity entity = SceneManager::GetEntity(scene, targetUuid); entity.IsValid())
                    {
                        bool ok = false;
                        const QString currentName = QString::fromStdString(entity.GetName());
                        const QString newName = QInputDialog::getText(this, "Rename Entity", "Name", QLineEdit::Normal, currentName, &ok);
                        if (ok)
                        {
                            const std::string renamed = newName.toStdString();
                            if (!renamed.empty() && renamed != entity.GetName())
                            {
                                CommandManager::ExecuteCommand(CreateScope<EntityRenameCommand>(scene, entity.GetUUID(), entity.GetName(), renamed));
                                if (auto *owner = static_cast<QtSceneHierarchyWidget *>(parentWidget()))
                                {
                                    owner->RefreshHierarchy();
                                }
                            }
                        }
                    }
                }
                else if (selectedAction == duplicateAction)
                {
                    if (Entity entity = SceneManager::GetEntity(scene, targetUuid); entity.IsValid())
                    {
                        SceneManager::DuplicateEntity(scene, entity);
                        if (auto *owner = static_cast<QtSceneHierarchyWidget *>(parentWidget()))
                        {
                            owner->RefreshHierarchy();
                        }
                    }
                }
                else if (selectedAction == deleteAction)
                {
                    if (Entity entity = SceneManager::GetEntity(scene, targetUuid); entity.IsValid())
                    {
                        if (EditorLayer *editor = EditorLayer::GetInstance(); editor && editor->GetSelectedEntities().contains(targetUuid))
                        {
                            editor->ClearSelection(false);
                        }

                        CommandManager::ExecuteCommand(CreateScope<EntityDestroyCommand>(scene, entity));
                        if (auto *owner = static_cast<QtSceneHierarchyWidget *>(parentWidget()))
                        {
                            owner->RefreshHierarchy();
                        }
                    }
                }
            }
            else
            {
                menu.exec(event->globalPos());
            }
        }

        void dragEnterEvent(QDragEnterEvent *event) override
        {
            if (event->source() == this)
            {
                event->acceptProposedAction();
                return;
            }

            QTreeWidget::dragEnterEvent(event);
        }

        void dragMoveEvent(QDragMoveEvent *event) override
        {
            if (event->source() == this)
            {
                event->acceptProposedAction();
                return;
            }

            QTreeWidget::dragMoveEvent(event);
        }

        void dropEvent(QDropEvent *event) override
        {
            Scene *scene = EditorLayer::GetInstance() ? EditorLayer::GetInstance()->GetActiveScene().get() : nullptr;
            if (!scene)
            {
                return;
            }

            const UUID sourceUuid = m_DraggedSourceUuid;
            if (sourceUuid == UUID(0))
            {
                return;
            }

            QTreeWidgetItem *targetItem = itemAt(event->position().toPoint());
            const UUID targetUuid = (targetItem == topLevelItem(0)) ? UUID(0) : GetItemUuid(targetItem);
            if (targetUuid == sourceUuid)
            {
                m_DraggedSourceUuid = UUID(0);
                return;
            }

            if (ReparentEntity(scene, sourceUuid, targetUuid))
            {
                if (auto *owner = static_cast<QtSceneHierarchyWidget *>(parentWidget()))
                {
                    QTimer::singleShot(0, owner, [owner]()
                    {
                        owner->RefreshHierarchy();
                    });
                }

                event->acceptProposedAction();
            }

            m_DraggedSourceUuid = UUID(0);
        }

    private:
        UUID m_DraggedSourceUuid = UUID(0);
    };

    QtSceneHierarchyWidget::QtSceneHierarchyWidget(QWidget *parent)
        : QWidget(parent)
    {
        auto *layout = new QVBoxLayout(this);
        layout->setContentsMargins(6, 6, 6, 6);
        layout->setSpacing(6);

        auto *title = new QLabel("Scene Hierarchy", this);
        layout->addWidget(title);

        auto *tree = new SceneHierarchyTreeWidget(this);
        tree->setObjectName("QtSceneHierarchyTree");
        layout->addWidget(tree);

        QObject::connect(tree, &QTreeWidget::itemSelectionChanged, this, [tree]()
        {
            EditorLayer *editor = EditorLayer::GetInstance();
            Scene *scene = editor && editor->GetActiveScene() ? editor->GetActiveScene().get() : nullptr;
            if (!editor || !scene)
            {
                return;
            }

            std::vector<Entity> selectedEntities;
            selectedEntities.reserve(static_cast<size_t>(tree->selectedItems().size()));

            for (QTreeWidgetItem *item : tree->selectedItems())
            {
                const UUID uuid = GetItemUuid(item);
                if (uuid == UUID(0))
                {
                    continue;
                }

                Entity entity = SceneManager::GetEntity(scene, uuid);
                if (entity.IsValid())
                {
                    selectedEntities.push_back(entity);
                }
            }

            const UUID trackingUuid = GetItemUuid(tree->currentItem());
            if (selectedEntities.empty())
            {
                editor->ClearSelection(false);
            }
            else
            {
                editor->SetSelectedEntities(selectedEntities, trackingUuid, false);
            }
        });
    }

    void QtSceneHierarchyWidget::RefreshHierarchy()
    {
        auto *tree = findChild<QTreeWidget *>("QtSceneHierarchyTree");
        if (!tree)
        {
            return;
        }

        const QSignalBlocker blocker(tree);
        tree->setUpdatesEnabled(false);
        tree->clear();

        if (EditorLayer *editor = EditorLayer::GetInstance(); editor && editor->GetActiveScene())
        {
            Ref<Scene> scene = editor->GetActiveScene();
            auto *root = new QTreeWidgetItem(tree, QStringList(scene->name.c_str()));

            root->setData(0, kEntityUuidRole, (qulonglong)UUID(0));

            root->setFlags(root->flags() | Qt::ItemIsDropEnabled | Qt::ItemIsSelectable | Qt::ItemIsEnabled);

            if (scene->registry)
            {
                std::unordered_map<UUID, QTreeWidgetItem *> items;
                items.reserve(scene->entities.size());

                scene->registry->view<IDComponent>().each([&](const entt::entity e, const IDComponent &id)
                {
                    Q_UNUSED(e);

                    auto *item = new QTreeWidgetItem(QStringList(id.name.c_str()));
                    item->setData(0, kEntityUuidRole, static_cast<qulonglong>(id.uuid));
                    item->setFlags(item->flags() | Qt::ItemIsDragEnabled | Qt::ItemIsDropEnabled | Qt::ItemIsSelectable | Qt::ItemIsEnabled);
                    items.emplace(id.uuid, item);
                });

                std::function<void(QTreeWidgetItem *, Entity)> addChildrenRecursively;
                addChildrenRecursively = [&](QTreeWidgetItem *parentItem, Entity parentEntity)
                {
                    if (!parentItem || !parentEntity.IsValid())
                    {
                        return;
                    }

                    const IDComponent &parentId = parentEntity.GetComponent<IDComponent>();
                    for (const UUID childUuid : parentId.children)
                    {
                        Entity childEntity = SceneManager::GetEntity(scene.get(), childUuid);
                        if (!childEntity.IsValid())
                        {
                            continue;
                        }

                        auto childIt = items.find(childUuid);
                        if (childIt == items.end())
                        {
                            continue;
                        }

                        QTreeWidgetItem *childItem = childIt->second;
                        if (childItem->parent() != nullptr)
                        {
                            continue;
                        }

                        parentItem->addChild(childItem);
                        childItem->setExpanded(true);
                        addChildrenRecursively(childItem, childEntity);
                    }
                };

                scene->registry->view<IDComponent>().each([&](const entt::entity e, const IDComponent &id)
                {
                    if (id.parent != UUID(0))
                    {
                        return;
                    }

                    auto itemIt = items.find(id.uuid);
                    if (itemIt == items.end())
                    {
                        return;
                    }

                    QTreeWidgetItem *item = itemIt->second;
                    root->addChild(item);
                    item->setExpanded(true);
                    addChildrenRecursively(item, Entity { e, scene.get() });
                });

                for (const auto &[uuid, entity] : editor->GetSelectedEntities())
                {
                    Q_UNUSED(entity);

                    auto itemIt = items.find(uuid);
                    if (itemIt != items.end())
                    {
                        itemIt->second->setSelected(true);
                    }
                }

                if (const UUID trackingUuid = editor->GetTrackingSelectedEntity(); trackingUuid != UUID(0))
                {
                    auto itemIt = items.find(trackingUuid);
                    if (itemIt != items.end())
                    {
                        tree->setCurrentItem(itemIt->second);
                    }
                }
            }

            root->setExpanded(true);
        }
        else
        {
            new QTreeWidgetItem(tree, QStringList("Waiting for active scene"));
        }

        tree->setUpdatesEnabled(true);
    }
}
