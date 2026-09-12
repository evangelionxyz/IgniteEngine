using System;
using Avalonia;
using Avalonia.Controls;
using Avalonia.Controls.Chrome;
using Avalonia.Input;
using Avalonia.VisualTree;
using IgniteEditor.ViewModels;

namespace IgniteEditor;

public partial class MainWindow : Window
{
    public MainWindow()
    {
        InitializeComponent();
        DataContext = new MainWindowViewModel();

        Loaded += (_, _) => HideBuiltInTitleBarElements();
    }

    protected override void OnOpened(EventArgs e)
    {
        base.OnOpened(e);
        HideBuiltInTitleBarElements();
    }

    private void HideBuiltInTitleBarElements()
    {
        foreach (var visual in this.GetVisualDescendants())
        {
            if (visual is TextBlock tb)
            {
                if (tb.Name == "PART_Title" || tb.Text == "Ignite Editor" || tb.Text == "IgniteEditor")
                {
                    tb.IsVisible = false;
                    tb.Opacity = 0;
                    tb.Width = 0;
                    tb.Height = 0;
                }
            }
            else if (visual is Button btn)
            {
                if (btn.Name == "PART_FullScreenButton" ||
                    WindowDecorationProperties.GetElementRole(btn) == WindowDecorationsElementRole.FullScreenButton)
                {
                    btn.IsVisible = false;
                    btn.IsEnabled = false;
                    btn.Width = 0;
                    btn.Height = 0;
                    btn.Opacity = 0;
                }
            }
        }
    }

    private void OnTitleBarPointerPressed(object? sender, PointerPressedEventArgs e)
    {
        if (e.ClickCount == 2)
        {
            WindowState = WindowState == WindowState.Maximized ? WindowState.Normal : WindowState.Maximized;
            return;
        }

        if (e.GetCurrentPoint(this).Properties.IsLeftButtonPressed)
        {
            BeginMoveDrag(e);
        }
    }
}
