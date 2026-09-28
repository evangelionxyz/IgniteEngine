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
            EntityId = model.Guid,
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
        var found = FindNodeRecursive(RootEntities, child.Guid);
        if (found != null)
        {
            SelectedEntity = found;
            ExpandParents(RootEntities, child.Guid);
        }
    }

    [RelayCommand]
    private void UnparentEntity()
    {
        var selected = GetAllSelectedNodes();
        if (selected.Count == 0 && SelectedEntity != null)
            selected.Add(SelectedEntity);

        if (selected.Count == 0)
            return;

        foreach (var node in selected)
            _sceneService.ReparentEntity(node.EntityId, null);

        RefreshHierarchy();
        RestoreSelection(selected);
    }

    public bool CanReparent(Guid entityId, Guid? newParentId)
    {
        if (!newParentId.HasValue)
            return true;

        if (entityId == newParentId.Value)
            return false;

        var source = FindNodeRecursive(RootEntities, entityId);
        return source != null && !ContainsNode(source.Children, newParentId.Value);
    }

    public void Reparent(Guid entityId, Guid? newParentId)
    {
        var selected = GetAllSelectedNodes();
        if (selected.Count == 0 || !IsDirectlySelected(selected, entityId))
        {
            selected.Clear();
            var source = FindNodeRecursive(RootEntities, entityId);
            if (source != null)
                selected.Add(source);
        }

        var moved = new List<EntityNodeViewModel>();
        foreach (var node in selected)
        {
            // Moving an ancestor also moves its descendants. Do not reparent
            // those descendants a second time as separate siblings.
            if (HasSelectedAncestor(node, selected) || !CanReparent(node.EntityId, newParentId))
                continue;

            _sceneService.ReparentEntity(node.EntityId, newParentId);
            moved.Add(node);
        }

        if (moved.Count == 0)
            return;

        RefreshHierarchy();
        RestoreSelection(selected);
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
                    // Multi-selection is append-only for viewport and hierarchy
                    // notifications. Explicit deselection is handled by clearing
                    // the selection or changing the selection mode.
                    if (!found.IsSelected)
                        found.IsSelected = true;

                    SelectedEntity = found;
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

    private static bool ContainsNode(IEnumerable<EntityNodeViewModel> nodes, Guid id)
    {
        foreach (var node in nodes)
        {
            if (node.EntityId == id || ContainsNode(node.Children, id))
                return true;
        }

        return false;
    }

    private static bool IsDirectlySelected(IEnumerable<EntityNodeViewModel> selected, Guid id)
    {
        foreach (var node in selected)
        {
            if (node.EntityId == id)
                return true;
        }

        return false;
    }

    private static bool HasSelectedAncestor(EntityNodeViewModel node, IEnumerable<EntityNodeViewModel> selected)
    {
        foreach (var candidate in selected)
        {
            if (candidate.EntityId != node.EntityId && ContainsNode(candidate.Children, node.EntityId))
                return true;
        }

        return false;
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
        var selected = GetAllSelectedNodes();
        if (selected.Count == 0 && SelectedEntity != null)
            selected.Add(SelectedEntity);

        if (selected.Count == 0)
            return;

        var duplicatedIds = new List<Guid>();
        foreach (var node in selected)
        {
            var duplicate = _sceneService.DuplicateEntity(node.EntityId);
            if (duplicate.Guid != Guid.Empty)
                duplicatedIds.Add(duplicate.Guid);
        }

        RefreshHierarchy();
        ClearSelection();
        foreach (var id in duplicatedIds)
            SelectEntityById(id, multiSelect: true);
    }

    private void RestoreSelection(IEnumerable<EntityNodeViewModel> selected)
    {
        ClearSelection();
        foreach (var node in selected)
        {
            if (FindNodeRecursive(RootEntities, node.EntityId) != null)
                SelectEntityById(node.EntityId, multiSelect: true);
        }
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

    [ObservableProperty]
    private bool _isDropTarget;

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

