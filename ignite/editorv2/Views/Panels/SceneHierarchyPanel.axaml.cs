using System;
using System.Globalization;
using Avalonia;
using Avalonia.Controls;
using Avalonia.Input;
using Avalonia.Data.Converters;
using Avalonia.Media;
using Avalonia.VisualTree;
using IgniteEditor.ViewModels;

namespace IgniteEditor.Views.Panels;

public sealed class DropTargetBrushConverter : IValueConverter
{
    public object Convert(object? value, Type targetType, object? parameter, CultureInfo culture)
    {
        return value is true
            ? new SolidColorBrush(Color.FromArgb(80, 209, 254, 23))
            : Brushes.Transparent;
    }

    public object ConvertBack(object? value, Type targetType, object? parameter, CultureInfo culture)
    {
        throw new NotSupportedException();
    }
}

public partial class SceneHierarchyPanel : UserControl
{
    private static readonly DataFormat<EntityNodeViewModel> EntityDragFormat =
        DataFormat.CreateInProcessFormat<EntityNodeViewModel>("Ignite.EntityNode");

    private EntityNodeViewModel? _draggedNode;
    private PointerPressedEventArgs? _pointerPressedEvent;
    private Point _pointerDownPosition;
    private bool _dragStarted;
    private EntityNodeViewModel? _dropTargetNode;

    public SceneHierarchyPanel()
    {
        InitializeComponent();
    }

    private void OnItemPointerPressed(object? sender, PointerPressedEventArgs e)
    {
        if (!e.GetCurrentPoint(this).Properties.IsLeftButtonPressed)
            return;

        _draggedNode = (sender as Control)?.DataContext as EntityNodeViewModel;
        _pointerPressedEvent = e;
        _pointerDownPosition = e.GetPosition(this);
        _dragStarted = false;
    }

    private async void OnItemPointerMoved(object? sender, PointerEventArgs e)
    {
        if (_draggedNode == null || _pointerPressedEvent == null || _dragStarted || !e.GetCurrentPoint(this).Properties.IsLeftButtonPressed)
            return;

        var delta = e.GetPosition(this) - _pointerDownPosition;
        if (Math.Abs(delta.X) < 4 && Math.Abs(delta.Y) < 4)
            return;

        _dragStarted = true;
        var node = _draggedNode;
        var data = new DataTransfer();
        data.Add(DataTransferItem.Create(EntityDragFormat, node));

        try
        {
            await DragDrop.DoDragDropAsync(_pointerPressedEvent, data, DragDropEffects.Move);
        }
        finally
        {
            _draggedNode = null;
            _pointerPressedEvent = null;
            _dragStarted = false;
        }
    }

    private void OnItemPointerReleased(object? sender, PointerReleasedEventArgs e)
    {
        // A normal click must not be treated as a reparent operation. Actual
        // reparenting is handled by the TreeView Drop event below.
        if (!_dragStarted)
        {
            _draggedNode = null;
            _pointerPressedEvent = null;
        }
    }

    private void OnTreeDragOver(object? sender, DragEventArgs e)
    {
        var sourceNode = e.DataTransfer.TryGetValue(EntityDragFormat);
        var targetNode = GetDropTarget(e);
        var vm = DataContext as SceneHierarchyViewModel;
        var canDrop = sourceNode != null && targetNode != null && vm != null
            && vm.CanReparent(sourceNode.EntityId, targetNode.EntityId);

        SetDropTarget(canDrop ? targetNode : null);
        e.DragEffects = canDrop ? DragDropEffects.Move : DragDropEffects.None;
    }

    private void OnTreeDragLeave(object? sender, DragEventArgs e)
    {
        SetDropTarget(null);
    }

    private void OnTreeDrop(object? sender, DragEventArgs e)
    {
        var sourceNode = e.DataTransfer.TryGetValue(EntityDragFormat);
        var targetNode = GetDropTarget(e);
        var vm = DataContext as SceneHierarchyViewModel;

        if (sourceNode == null || targetNode == null || vm == null
            || !vm.CanReparent(sourceNode.EntityId, targetNode.EntityId))
        {
            e.DragEffects = DragDropEffects.None;
            SetDropTarget(null);
            return;
        }

        vm.Reparent(sourceNode.EntityId, targetNode.EntityId);
        e.DragEffects = DragDropEffects.Move;
        e.Handled = true;
        SetDropTarget(null);
    }

    private static EntityNodeViewModel? GetDropTarget(DragEventArgs e)
    {
        for (Visual? visual = e.Source as Visual; visual != null; visual = visual.GetVisualParent())
        {
            if (visual is Control control && control.DataContext is EntityNodeViewModel node)
                return node;
        }

        return null;
    }

    private void SetDropTarget(EntityNodeViewModel? node)
    {
        if (_dropTargetNode == node)
            return;

        if (_dropTargetNode != null)
            _dropTargetNode.IsDropTarget = false;

        _dropTargetNode = node;
        if (_dropTargetNode != null)
            _dropTargetNode.IsDropTarget = true;
    }
}
