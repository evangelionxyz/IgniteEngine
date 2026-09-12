using Avalonia.Controls;
using IgniteEditor.ViewModels;

namespace IgniteEditor.Views.Panels;

public partial class ViewportPanel : UserControl
{
    public ViewportPanel()
    {
        InitializeComponent();

        if (ViewportHost != null)
        {
            ViewportHost.EngineConnectionChanged += connected =>
            {
                if (DataContext is ViewportViewModel vm)
                {
                    vm.IsEngineConnected = connected;
                    vm.StatusMessage = connected ? "Engine Connected (Vulkan)" : "Failed to Connect Engine";
                }
            };

            ViewportHost.FpsUpdated += fps =>
            {
                if (DataContext is ViewportViewModel vm)
                {
                    vm.Fps = fps;
                }
            };
        }
    }
}
