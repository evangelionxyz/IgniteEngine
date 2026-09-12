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
        var node = new EntityNodeViewModel(onActiveChanged: (id, active) =>
        {
            _sceneService.SetEntityActive(id, active);
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
        if (SelectedEntity == null) return;
        _sceneService.DeleteEntity(SelectedEntity.EntityId);
        RefreshHierarchy();
        SelectedEntity = null;
    }

    [RelayCommand]
    private void DuplicateEntity()
    {
        if (SelectedEntity == null) return;
        var dup = _sceneService.DuplicateEntity(SelectedEntity.EntityId);
        RefreshHierarchy();
        SelectedEntity = FindNodeRecursive(RootEntities, dup.Id);
    }

    partial void OnSelectedEntityChanged(EntityNodeViewModel? value)
    {
        SelectionChanged?.Invoke(this, value);
    }

    public event EventHandler<EntityNodeViewModel?>? SelectionChanged;
}

public partial class EntityNodeViewModel : ViewModelBase
{
    private readonly Action<Guid, bool>? _onActiveChanged;

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

    public EntityNodeViewModel(Action<Guid, bool>? onActiveChanged = null)
    {
        _onActiveChanged = onActiveChanged;
    }

    public EntityNodeViewModel() { }

    partial void OnIsActiveChanged(bool value)
    {
        _onActiveChanged?.Invoke(EntityId, value);
    }
}

