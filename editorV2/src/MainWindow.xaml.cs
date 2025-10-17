using System;
using System.Windows;
using System.Windows.Controls;
using System.Windows.Media;
using Microsoft.Win32;

namespace IgniteEditorV2
{
    public partial class MainWindow : Window
    {
        public MainWindow()
        {
            InitializeComponent();
            StatusText.Text = "Ignite Editor V2 Ready";
        }

        private void NewFile_Click(object sender, RoutedEventArgs e)
        {
            MessageBox.Show("New File clicked!", "Menu Action", MessageBoxButton.OK, MessageBoxImage.Information);
            StatusText.Text = "New file created";
        }

        private void OpenFile_Click(object sender, RoutedEventArgs e)
        {
            OpenFileDialog openFileDialog = new OpenFileDialog();
            openFileDialog.Filter = "Ignite Project (*.ixproj*)|*.ixproj*";
            
            if (openFileDialog.ShowDialog() == true)
            {
                StatusText.Text = $"Opened: {openFileDialog.FileName}";
            }
        }

        private void SaveFile_Click(object sender, RoutedEventArgs e)
        {
            SaveFileDialog saveFileDialog = new SaveFileDialog();
            saveFileDialog.Filter = "All files (*.*)|*.*";
            
            if (saveFileDialog.ShowDialog() == true)
            {
                StatusText.Text = $"Saved: {saveFileDialog.FileName}";
            }
        }

        private void Exit_Click(object sender, RoutedEventArgs e)
        {
            Application.Current.Shutdown();
        }

        private void About_Click(object sender, RoutedEventArgs e)
        {
            MessageBox.Show("Ignite Editor V2\n\nA CMake-based WPF Game Editor\nBuilt with C# and WPF", 
                          "About Ignite Editor V2", 
                          MessageBoxButton.OK, 
                          MessageBoxImage.Information);
        }

        private void SceneView_Click(object sender, RoutedEventArgs e)
        {
            StatusText.Text = "Scene view activated";
            // Add visual feedback for active tab
            var button = sender as Button;
            if (button != null)
            {
                button.Background = new SolidColorBrush((Color)ColorConverter.ConvertFromString("#094771"));
            }
        }

        private void GameView_Click(object sender, RoutedEventArgs e)
        {
            StatusText.Text = "Game view activated";
            // Add visual feedback for active tab
            var button = sender as Button;
            if (button != null)
            {
                button.Background = new SolidColorBrush((Color)ColorConverter.ConvertFromString("#094771"));
            }
        }

        private void Play_Click(object sender, RoutedEventArgs e)
        {
            StatusText.Text = "Game started - Playing";
            MessageBox.Show("Play button clicked!\n\nThis would start the game simulation.", 
                          "Game Control", MessageBoxButton.OK, MessageBoxImage.Information);
        }

        private void Pause_Click(object sender, RoutedEventArgs e)
        {
            StatusText.Text = "Game paused";
            MessageBox.Show("Pause button clicked!\n\nThis would pause the game simulation.", 
                          "Game Control", MessageBoxButton.OK, MessageBoxImage.Information);
        }

        private void Stop_Click(object sender, RoutedEventArgs e)
        {
            StatusText.Text = "Game stopped - Ready";
            MessageBox.Show("Stop button clicked!\n\nThis would stop the game simulation.", 
                          "Game Control", MessageBoxButton.OK, MessageBoxImage.Information);
        }
    }
}
