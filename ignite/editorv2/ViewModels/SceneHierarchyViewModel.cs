using System;
using System.Collections.Generic;
using System.Collections.ObjectModel;
using CommunityToolkit.Mvvm.ComponentModel;
using CommunityToolkit.Mvvm.Input;
using Ignite.Managed.Models;
using Ignite.Managed.Services;

namespace IgniteEditor.ViewModels;

public partial class SceneHierarchyViewModel : ViewModelBase
{
    private readonly ISceneService _sceneService;

    [ObservableProperty]
    private ObservableCollection<EntityNodeViewModel> _rootEntities = new();

    [ObservableProperty]
    private EntityNodeViewModel? _selectedEntity;

    [ObservableProperty]
    private string _searchFilter = string.Empty;

    public SceneHierarchyViewModel(ISceneService sceneService)
    {
        _sceneService = sceneService;
        RefreshHierarchy();
    }

    public void RefreshHierarchy()
    {
        RootEntities.Clear();
        foreach (var entity in _sceneService.GetRootEntities())
        {
            RootEntities.Add(CreateNodeFromModel(entity));
        }
    }

    private EntityNodeViewModel CreateNodeFromModel(EntityModel model)
    {
        var node = new EntityNodeViewModel(
            onActiveChanged: (id, active) =>
            {
                _sceneService.SetEntityActive(id, active);
            },
            onSelectedChanged: (n, selected) =>
            {
                OnNodeSelectionChanged(n, selected);
            })
        {
            EntityId = model.Id,
            Name = model.Name,
            IsActive = model.IsActive
        };

        foreach (var child in model.Children)
        {
            node.Children.Add(CreateNodeFromModel(child));
        }

        return node;
    }

    [RelayCommand]
    private void CreateEntity()
    {
        var entity = _sceneService.CreateEntity("New Entity");
        var node = CreateNodeFromModel(entity);
        RootEntities.Add(node);
        SelectedEntity = node;
    }

    [RelayCommand]
    private void CreateChildEntity()
    {
        if (SelectedEntity == null)
        {
            CreateEntity();
            return;
        }

        var child = _sceneService.CreateEntity("Child Entity", SelectedEntity.EntityId);
        RefreshHierarchy();

        // Select the newly created child
        var found = FindNodeRecursive(RootEntities, child.Id);
        if (found != null)
        {
            SelectedEntity = found;
            ExpandParents(RootEntities, child.Id);
        }
    }

    [RelayCommand]
    private void UnparentEntity()
    {
        if (SelectedEntity == null) return;
        Guid entityId = SelectedEntity.EntityId;
        _sceneService.ReparentEntity(entityId, null);
        RefreshHierarchy();
        SelectedEntity = FindNodeRecursive(RootEntities, entityId);
    }

    public void Reparent(Guid entityId, Guid? newParentId)
    {
        _sceneService.ReparentEntity(entityId, newParentId);
        RefreshHierarchy();
        SelectedEntity = FindNodeRecursive(RootEntities, entityId);
    }

    private bool _suppressSelectionEvents;

    public event EventHandler? MultiSelectionChanged;

    private void OnNodeSelectionChanged(EntityNodeViewModel node, bool selected)
    {
        if (_suppressSelectionEvents) return;

        if (selected)
        {
            if (SelectedEntity == null)
            {
                SelectedEntity = node;
            }
        }
        else if (SelectedEntity == node)
        {
            SelectedEntity = FindFirstSelectedNode(RootEntities);
        }

        MultiSelectionChanged?.Invoke(this, EventArgs.Empty);
    }

    public void SelectEntityById(Guid entityId, bool multiSelect = false)
    {
        if (entityId == Guid.Empty)
        {
            if (!multiSelect)
            {
                ClearSelection();
            }
            return;
        }

        var found = FindNodeRecursive(RootEntities, entityId);
        if (found != null)
        {
            ExpandParents(RootEntities, entityId);

            _suppressSelectionEvents = true;
            try
            {
                if (!multiSelect)
                {
                    SetAllNodesSelected(RootEntities, false);
                    found.IsSelected = true;
                    SelectedEntity = found;
                }
                else
                {
                    found.IsSelected = !found.IsSelected;
                    if (found.IsSelected)
                    {
                        SelectedEntity = found;
                    }
                    else if (SelectedEntity == found)
                    {
                        SelectedEntity = FindFirstSelectedNode(RootEntities);
                    }
                }
            }
            finally
            {
                _suppressSelectionEvents = false;
            }

            SelectionChanged?.Invoke(this, SelectedEntity);
            MultiSelectionChanged?.Invoke(this, EventArgs.Empty);
        }
    }

    public void SelectEntityByUuid(ulong uuid, bool multiSelect = false)
    {
        if (uuid == 0)
        {
            if (!multiSelect)
            {
                ClearSelection();
            }
            return;
        }

        SelectEntityById(EntityModel.GuidFromUInt64(uuid), multiSelect);
    }

    public void ClearSelection()
    {
        _suppressSelectionEvents = true;
        try
        {
            SetAllNodesSelected(RootEntities, false);
            SelectedEntity = null;
        }
        finally
        {
            _suppressSelectionEvents = false;
        }

        SelectionChanged?.Invoke(this, null);
        MultiSelectionChanged?.Invoke(this, EventArgs.Empty);
    }

    public List<EntityNodeViewModel> GetAllSelectedNodes()
    {
        var list = new List<EntityNodeViewModel>();
        CollectSelectedNodes(RootEntities, list);
        return list;
    }

    private static void CollectSelectedNodes(IEnumerable<EntityNodeViewModel> nodes, List<EntityNodeViewModel> list)
    {
        foreach (var node in nodes)
        {
            if (node.IsSelected) list.Add(node);
            CollectSelectedNodes(node.Children, list);
        }
    }

    private static void SetAllNodesSelected(IEnumerable<EntityNodeViewModel> nodes, bool isSelected)
    {
        foreach (var node in nodes)
        {
            node.IsSelected = isSelected;
            SetAllNodesSelected(node.Children, isSelected);
        }
    }

    private static EntityNodeViewModel? FindFirstSelectedNode(IEnumerable<EntityNodeViewModel> nodes)
    {
        foreach (var node in nodes)
        {
            if (node.IsSelected) return node;
            var found = FindFirstSelectedNode(node.Children);
            if (found != null) return found;
        }
        return null;
    }

    private static EntityNodeViewModel? FindNodeRecursive(IEnumerable<EntityNodeViewModel> nodes, Guid id)
    {
        foreach (var node in nodes)
        {
            if (node.EntityId == id) return node;
            var found = FindNodeRecursive(node.Children, id);
            if (found != null) return found;
        }
        return null;
    }

    private static bool ExpandParents(IEnumerable<EntityNodeViewModel> nodes, Guid targetId)
    {
        foreach (var node in nodes)
        {
            if (node.EntityId == targetId) return true;
            if (ExpandParents(node.Children, targetId))
            {
                node.IsExpanded = true;
                return true;
            }
        }
        return false;
    }

    [RelayCommand]
    private void DeleteEntity()
    {
        var selected = GetAllSelectedNodes();
        if (selected.Count == 0 && SelectedEntity != null)
            selected.Add(SelectedEntity);

        if (selected.Count == 0) return;

        foreach (var node in selected)
        {
            _sceneService.DeleteEntity(node.EntityId);
        }
        RefreshHierarchy();
        ClearSelection();
    }

    [RelayCommand]
    private void DuplicateEntity()
    {
        if (SelectedEntity == null) return;
        var dup = _sceneService.DuplicateEntity(SelectedEntity.EntityId);
        RefreshHierarchy();
        SelectEntityById(dup.Id);
    }

    partial void OnSelectedEntityChanged(EntityNodeViewModel? value)
    {
        if (value != null && !value.IsSelected)
        {
            value.IsSelected = true;
        }
        SelectionChanged?.Invoke(this, value);
    }

    public event EventHandler<EntityNodeViewModel?>? SelectionChanged;
}

public partial class EntityNodeViewModel : ViewModelBase
{
    private readonly Action<Guid, bool>? _onActiveChanged;
    private readonly Action<EntityNodeViewModel, bool>? _onSelectedChanged;

    [ObservableProperty]
    private Guid _entityId;

    [ObservableProperty]
    private string _name = "Entity";

    [ObservableProperty]
    private bool _isActive = true;

    [ObservableProperty]
    private bool _isExpanded = true;

    [ObservableProperty]
    private bool _isSelected;

    public ObservableCollection<EntityNodeViewModel> Children { get; } = new();

    public EntityNodeViewModel(Action<Guid, bool>? onActiveChanged = null, Action<EntityNodeViewModel, bool>? onSelectedChanged = null)
    {
        _onActiveChanged = onActiveChanged;
        _onSelectedChanged = onSelectedChanged;
    }

    public EntityNodeViewModel() { }

    partial void OnIsActiveChanged(bool value)
    {
        _onActiveChanged?.Invoke(EntityId, value);
    }

    partial void OnIsSelectedChanged(bool value)
    {
        _onSelectedChanged?.Invoke(this, value);
    }
}

