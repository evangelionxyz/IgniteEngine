using System.Windows;

namespace IgniteEditorV2
{
    public class App : Application
    {
        protected override void OnStartup(StartupEventArgs e)
        {
            base.OnStartup(e);
            
            // Create and show the main window
            MainWindow mainWindow = new MainWindow();
            mainWindow.Show();
        }
    }
}
