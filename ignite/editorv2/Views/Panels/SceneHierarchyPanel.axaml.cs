using System;
using Avalonia;
using Avalonia.Controls;
using Avalonia.Input;
using Avalonia.VisualTree;
using IgniteEditor.ViewModels;

namespace IgniteEditor.Views.Panels;

public partial class SceneHierarchyPanel : UserControl
{
    private EntityNodeViewModel? _draggedNode;

    public SceneHierarchyPanel()
    {
        InitializeComponent();
    }

    private void OnItemPointerPressed(object? sender, PointerPressedEventArgs e)
    {
        var point = e.GetCurrentPoint(this);
        if (point.Properties.IsLeftButtonPressed)
        {
            _draggedNode = (sender as Control)?.DataContext as EntityNodeViewModel;
        }
    }

    private void OnItemPointerReleased(object? sender, PointerReleasedEventArgs e)
    {
        if (_draggedNode != null)
        {
            var hit = (e.Source as Visual)?.FindAncestorOfType<Control>();
            var targetNode = hit?.DataContext as EntityNodeViewModel;

            if (targetNode != null && targetNode.EntityId != _draggedNode.EntityId)
            {
                if (DataContext is SceneHierarchyViewModel vm)
                {
                    vm.Reparent(_draggedNode.EntityId, targetNode.EntityId);
                }
            }
            _draggedNode = null;
        }
    }
}
