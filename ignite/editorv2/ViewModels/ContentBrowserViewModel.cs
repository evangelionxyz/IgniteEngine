using System;
using System.Collections.ObjectModel;
using System.IO;
using System.Linq;
using CommunityToolkit.Mvvm.ComponentModel;
using CommunityToolkit.Mvvm.Input;

namespace IgniteEditor.ViewModels;

public partial class ContentBrowserViewModel : ViewModelBase
{
    [ObservableProperty]
    private string _currentPath = string.Empty;

    [ObservableProperty]
    private ObservableCollection<FileItemViewModel> _items = new();

    [ObservableProperty]
    private ObservableCollection<BreadcrumbSegment> _breadcrumbs = new();

    [ObservableProperty]
    private ObservableCollection<DirectoryNodeViewModel> _directoryTree = new();

    [ObservableProperty]
    private FileItemViewModel? _selectedItem;

    [ObservableProperty]
    private int _thumbnailSize = 72;

    [ObservableProperty]
    private string _searchFilter = string.Empty;

    private string _basePath = string.Empty;
    private readonly System.Collections.Generic.Stack<string> _backStack = new();
    private readonly System.Collections.Generic.Stack<string> _forwardStack = new();

    public bool CanGoBack => _backStack.Count > 0;
    public bool CanGoForward => _forwardStack.Count > 0;

    public void Initialize(string basePath)
    {
        _basePath = basePath;
        CurrentPath = basePath;
        RefreshDirectoryTree();
        NavigateTo(basePath);
    }

    [RelayCommand]
    private void NavigateBack()
    {
        if (!CanGoBack) return;
        _forwardStack.Push(CurrentPath);
        var path = _backStack.Pop();
        NavigateToInternal(path);
        OnPropertyChanged(nameof(CanGoBack));
        OnPropertyChanged(nameof(CanGoForward));
    }

    [RelayCommand]
    private void NavigateForward()
    {
        if (!CanGoForward) return;
        _backStack.Push(CurrentPath);
        var path = _forwardStack.Pop();
        NavigateToInternal(path);
        OnPropertyChanged(nameof(CanGoBack));
        OnPropertyChanged(nameof(CanGoForward));
    }

    [RelayCommand]
    private void NavigateUp()
    {
        if (string.IsNullOrEmpty(CurrentPath) || CurrentPath == _basePath) return;
        var parent = Path.GetDirectoryName(CurrentPath);
        if (!string.IsNullOrEmpty(parent) && parent.StartsWith(_basePath))
        {
            NavigateTo(parent);
        }
    }

    public void NavigateTo(string path)
    {
        if (path == CurrentPath) return;
        if (!string.IsNullOrEmpty(CurrentPath))
        {
            _backStack.Push(CurrentPath);
            _forwardStack.Clear();
        }
        NavigateToInternal(path);
        OnPropertyChanged(nameof(CanGoBack));
        OnPropertyChanged(nameof(CanGoForward));
    }

    private void NavigateToInternal(string path)
    {
        CurrentPath = path;
        RefreshItems();
        RefreshBreadcrumbs();
    }

    private void RefreshItems()
    {
        Items.Clear();

        if (!Directory.Exists(CurrentPath)) return;

        try
        {
            foreach (var dir in Directory.GetDirectories(CurrentPath).OrderBy(d => Path.GetFileName(d)))
            {
                Items.Add(new FileItemViewModel
                {
                    Name = Path.GetFileName(dir),
                    FullPath = dir,
                    IsDirectory = true,
                    Extension = "",
                });
            }

            foreach (var file in Directory.GetFiles(CurrentPath).OrderBy(f => Path.GetFileName(f)))
            {
                var info = new FileInfo(file);
                Items.Add(new FileItemViewModel
                {
                    Name = Path.GetFileName(file),
                    FullPath = file,
                    IsDirectory = false,
                    Extension = info.Extension.ToLowerInvariant(),
                    FileSize = info.Length,
                });
            }
        }
        catch (UnauthorizedAccessException) { }
        catch (IOException) { }
    }

    private void RefreshBreadcrumbs()
    {
        Breadcrumbs.Clear();
        if (string.IsNullOrEmpty(_basePath) || string.IsNullOrEmpty(CurrentPath)) return;

        var relativePath = Path.GetRelativePath(_basePath, CurrentPath);
        var baseName = Path.GetFileName(_basePath);

        Breadcrumbs.Add(new BreadcrumbSegment { Name = baseName, FullPath = _basePath });

        if (relativePath != ".")
        {
            var parts = relativePath.Split(Path.DirectorySeparatorChar, Path.AltDirectorySeparatorChar);
            var accumulated = _basePath;
            foreach (var part in parts)
            {
                accumulated = Path.Combine(accumulated, part);
                Breadcrumbs.Add(new BreadcrumbSegment { Name = part, FullPath = accumulated });
            }
        }
    }

    private void RefreshDirectoryTree()
    {
        DirectoryTree.Clear();
        if (!Directory.Exists(_basePath)) return;

        DirectoryTree.Add(CreateDirectoryNode(_basePath, maxDepth: 4));
    }

    private DirectoryNodeViewModel CreateDirectoryNode(string path, int depth = 0, int maxDepth = 4)
    {
        var node = new DirectoryNodeViewModel
        {
            Name = Path.GetFileName(path),
            FullPath = path,
            IsExpanded = depth < 2
        };

        if (depth >= maxDepth) return node;

        try
        {
            foreach (var dir in Directory.GetDirectories(path).OrderBy(d => Path.GetFileName(d)))
            {
                node.Children.Add(CreateDirectoryNode(dir, depth + 1, maxDepth));
            }
        }
        catch (UnauthorizedAccessException) { }
        catch (IOException) { }

        return node;
    }

    [RelayCommand]
    private void OpenItem(FileItemViewModel? item)
    {
        if (item == null) return;
        if (item.IsDirectory)
        {
            NavigateTo(item.FullPath);
        }
    }

    [RelayCommand]
    private void CreateFolder()
    {
        var newPath = Path.Combine(CurrentPath, "New Folder");
        var counter = 1;
        while (Directory.Exists(newPath))
        {
            newPath = Path.Combine(CurrentPath, $"New Folder ({counter++})");
        }
        Directory.CreateDirectory(newPath);
        RefreshItems();
    }

    [RelayCommand]
    private void Refresh()
    {
        RefreshItems();
        RefreshDirectoryTree();
    }
}

public partial class FileItemViewModel : ViewModelBase
{
    [ObservableProperty] private string _name = string.Empty;
    [ObservableProperty] private string _fullPath = string.Empty;
    [ObservableProperty] private bool _isDirectory;
    [ObservableProperty] private string _extension = string.Empty;
    [ObservableProperty] private long _fileSize;
    [ObservableProperty] private bool _isSelected;

    public string IconGlyph => IsDirectory ? "📁" : Extension switch
    {
        ".png" or ".jpg" or ".jpeg" or ".bmp" or ".tga" or ".hdr" => "🖼",
        ".fbx" or ".obj" or ".gltf" or ".glb" => "🧊",
        ".ixscene" => "🎬",
        ".ixproj" => "📦",
        ".cs" => "📝",
        ".hlsl" or ".hlsli" or ".glsl" => "🎨",
        ".wav" or ".mp3" or ".ogg" => "🔊",
        ".ttf" or ".otf" => "🔤",
        _ => "📄"
    };
}

public partial class BreadcrumbSegment : ViewModelBase
{
    [ObservableProperty] private string _name = string.Empty;
    [ObservableProperty] private string _fullPath = string.Empty;
}

public partial class DirectoryNodeViewModel : ViewModelBase
{
    [ObservableProperty] private string _name = string.Empty;
    [ObservableProperty] private string _fullPath = string.Empty;
    [ObservableProperty] private bool _isExpanded;

    public ObservableCollection<DirectoryNodeViewModel> Children { get; } = new();
}

