using System;
using System.Collections.ObjectModel;
using CommunityToolkit.Mvvm.ComponentModel;
using CommunityToolkit.Mvvm.Input;
using IgniteEditor.Models;
using IgniteEditor.Services;

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
        var node = new EntityNodeViewModel
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
        RootEntities.Add(CreateNodeFromModel(entity));
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
        RootEntities.Add(CreateNodeFromModel(dup));
    }

    partial void OnSelectedEntityChanged(EntityNodeViewModel? value)
    {
        SelectionChanged?.Invoke(this, value);
    }

    public event EventHandler<EntityNodeViewModel?>? SelectionChanged;
}

public partial class EntityNodeViewModel : ViewModelBase
{
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
}

