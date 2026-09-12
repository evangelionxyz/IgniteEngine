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

        void AttachViewModel(ViewportViewModel vm)
        {
            vm.PropertyChanged += (_, e) =>
            {
                if (e.PropertyName == nameof(ViewportViewModel.NavigationMode) && ViewportHost != null)
                    ViewportHost.SetNavigationMode(vm.NavigationMode);
            };
            if (ViewportHost != null)
            {
                ViewportHost.SetNavigationMode(vm.NavigationMode);
            }
        }

        if (DataContext is ViewportViewModel initialVm)
        {
            AttachViewModel(initialVm);
        }

        DataContextChanged += (_, _) =>
        {
            if (DataContext is ViewportViewModel vm)
            {
                AttachViewModel(vm);
            }
        };
    }
}
