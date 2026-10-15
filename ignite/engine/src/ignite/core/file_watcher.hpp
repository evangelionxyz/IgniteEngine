// Copyright (c) 2026 Evangelion Manuhutu

#pragma once
#ifndef IGN_FILE_WATCHER_HPP
#define IGN_FILE_WATCHER_HPP

#include "ignite/core/base.hpp"

#include <cstdint>
#include <filesystem>
#include <functional>
#include <string>
#include <vector>

namespace ignite
{
    // -------------------------------------------------------------------------
    // FileWatcher - global, reusable file/directory change notification service.
    //
    // Usage:
    //   // Watch a single file
    //   m_Token = FileWatcher::WatchFile("resources/shaders/mesh.pixel.hlsl",
    //       [](const FileChangeEvent &e) { ... });
    //
    //   // Watch a directory, only for some extensions
    //   m_Token = FileWatcher::WatchDirectory("Assets/Textures",
    //       [](const FileChangeEvent &e) { ... }, { ".png", ".jpg" });
    //
    //   // Stop watching (safe with an invalid token, inside a callback, or after Shutdown)
    //   FileWatcher::Unwatch(m_Token);
    //
    // Guarantees:
    //   * Callbacks are always invoked on the main thread from FileWatcher::Update(),
    //     so they can touch engine/GPU state without extra synchronization.
    //   * Bursts of OS notifications for the same file (editors typically emit several
    //     per save) are coalesced into one callback after a short quiet period.
    //   * Notifications that did not change the file's last-write-time (e.g. a read
    //     bumping the access time) are dropped, so reacting to a change by reading
    //     the file cannot create a feedback loop.
    //   * One OS watcher is shared per directory, regardless of how many watches
    //     are registered in it.
    //
    // Directory watches include sub-directories on platforms where the OS watcher is
    // recursive (Windows).
    // -------------------------------------------------------------------------

    enum class FileChangeType : uint8_t
    {
        Added,
        Modified,
        Removed
    };

    struct FileChangeEvent
    {
        std::filesystem::path path; // absolute, normalized
        FileChangeType type = FileChangeType::Modified;
    };

    using FileWatchToken = uint32_t;
    static constexpr FileWatchToken kInvalidFileWatchToken = 0;

    using FileChangeCallback = std::function<void(const FileChangeEvent &)>;

    class IGN_API FileWatcher
    {
    public:
        FileWatcher() = delete;

        // Watch one file. The file's parent directory must exist, the file itself may not (yet).
        [[nodiscard]] static FileWatchToken WatchFile(const std::filesystem::path &file, FileChangeCallback callback);

        // Watch every file in a directory. Extensions include the dot (".hlsl"), case-insensitive.
        // An empty list means all files.
        [[nodiscard]] static FileWatchToken WatchDirectory(const std::filesystem::path &directory, FileChangeCallback callback, std::vector<std::string> extensions = {});

        static void Unwatch(FileWatchToken token);

        // Dispatch pending notifications. Call once per frame on the main thread.
        static void Update();

        // Stop all OS watchers and drop every registration.
        static void Shutdown();

        // Quiet period (ms) after the last OS notification before a callback is fired.
        static void SetDebounceMs(uint32_t milliseconds);
    };
}

#endif
