// Copyright (c) 2026 Evangelion Manuhutu

#include "ignite_pch.hpp"

#include "file_watcher.hpp"
#include "ignite/core/logger.hpp"
#include "ignite/core/types.hpp"
#include "FileWatch.hpp"

#include <algorithm>
#include <atomic>
#include <cctype>
#include <chrono>
#include <memory>
#include <mutex>
#include <unordered_map>

namespace ignite
{
    namespace
    {
        namespace fs = std::filesystem;
        using Clock = std::chrono::steady_clock;

        std::string ToLower(std::string text)
        {
            std::transform(text.begin(), text.end(), text.begin(),
                [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
            return text;
        }

        // Canonical string used to compare paths: absolute, normalized, '/' separated,
        // case-folded on Windows.
        std::string MakeKey(const fs::path &path)
        {
            std::error_code ec;
            fs::path absolute = fs::absolute(path, ec);
            if (ec)
                absolute = path;

            std::string key = absolute.lexically_normal().generic_string();
            while (key.size() > 1 && key.back() == '/')
                key.pop_back();

#ifdef PLATFORM_WINDOWS
            key = ToLower(std::move(key));
#endif
            return key;
        }

        struct Subscription
        {
            FileWatchToken token = kInvalidFileWatchToken;
            std::string key;          // file or directory key
            std::string directoryKey; // key of the directory that owns the OS watcher
            bool isDirectory = false;
            std::vector<std::string> extensions; // lowercase, with dot
            FileChangeCallback callback;
            std::atomic<bool> active{ true };
        };

        struct DirectoryWatch
        {
            Scope<filewatch::FileWatch<std::string>> watcher;
            uint32_t refCount = 0;
        };

        struct PendingEvent
        {
            fs::path path;
            FileChangeType type = FileChangeType::Modified;
            Clock::time_point lastEvent;
        };

        struct State
        {
            std::mutex mutex;
            uint32_t debounceMs = 150;
            FileWatchToken lastToken = kInvalidFileWatchToken;

            std::vector<std::shared_ptr<Subscription>> subscriptions;
            std::unordered_map<std::string, PendingEvent> pending;
            std::unordered_map<std::string, fs::file_time_type> lastWriteTimes;

            // Declared last: destroyed first, while the mutex is still alive for any
            // in-flight raw callback that is waiting on it.
            std::unordered_map<std::string, DirectoryWatch> directories;
        };

        State &GetState()
        {
            static State state;
            return state;
        }

        bool Matches(const Subscription &sub, const std::string &key)
        {
            if (!sub.active.load())
                return false;

            if (!sub.isDirectory)
                return sub.key == key;

            // Must be located under the watched directory
            if (key.size() <= sub.key.size() + 1 || key.compare(0, sub.key.size(), sub.key) != 0 || key[sub.key.size()] != '/')
                return false;

            if (sub.extensions.empty())
                return true;

            const std::string extension = ToLower(fs::path(key).extension().string());
            return std::find(sub.extensions.begin(), sub.extensions.end(), extension) != sub.extensions.end();
        }

        FileChangeType ToChangeType(filewatch::Event event)
        {
            switch (event)
            {
                case filewatch::Event::added:
                case filewatch::Event::renamed_new: return FileChangeType::Added;
                case filewatch::Event::removed:
                case filewatch::Event::renamed_old: return FileChangeType::Removed;
                case filewatch::Event::modified:
                default: return FileChangeType::Modified;
            }
        }

        // Runs on the OS watcher's callback thread.
        void OnRawEvent(const fs::path &directory, const std::string &file, filewatch::Event event)
        {
            const fs::path fullPath = (directory / fs::path(file)).lexically_normal();
            const std::string key = MakeKey(fullPath);

            State &state = GetState();
            std::lock_guard lock(state.mutex);

            // Drop everything nobody listens to (e.g. shader binaries written to the cache directory)
            const bool interesting = std::any_of(state.subscriptions.begin(), state.subscriptions.end(),
                [&key](const std::shared_ptr<Subscription> &sub) { return Matches(*sub, key); });
            if (!interesting)
                return;

            const FileChangeType type = ToChangeType(event);
            auto [it, inserted] = state.pending.try_emplace(key);
            PendingEvent &pending = it->second;
            pending.path = fullPath;
            pending.lastEvent = Clock::now();

            // Added followed by Modified is still an Added file
            if (inserted || !(pending.type == FileChangeType::Added && type == FileChangeType::Modified))
                pending.type = type;
        }

        FileWatchToken AddSubscription(const fs::path &path, bool isDirectory, std::vector<std::string> extensions, FileChangeCallback callback)
        {
            if (!callback || path.empty())
                return kInvalidFileWatchToken;

            std::error_code ec;
            const fs::path absolute = fs::absolute(path, ec).lexically_normal();
            const fs::path directory = isDirectory ? absolute : absolute.parent_path();
            if (!fs::is_directory(directory, ec))
            {
                LOG_WARN("[FileWatcher] Directory does not exist, cannot watch '{}'", absolute.generic_string());
                return kInvalidFileWatchToken;
            }

            for (std::string &extension : extensions)
                extension = ToLower(std::move(extension));

            auto sub = std::make_shared<Subscription>();
            sub->key = MakeKey(absolute);
            sub->directoryKey = MakeKey(directory);
            sub->isDirectory = isDirectory;
            sub->extensions = std::move(extensions);
            sub->callback = std::move(callback);

            State &state = GetState();
            std::lock_guard lock(state.mutex);

            auto dirIt = state.directories.find(sub->directoryKey);
            if (dirIt == state.directories.end())
            {
                DirectoryWatch directoryWatch;
                try
                {
                    directoryWatch.watcher = CreateScope<filewatch::FileWatch<std::string>>(directory.string(),
                        [directory](const std::string &file, const filewatch::Event event)
                        {
                            OnRawEvent(directory, file, event);
                        });
                }
                catch (const std::exception &e)
                {
                    LOG_ERROR("[FileWatcher] Failed to watch '{}': {}", directory.generic_string(), e.what());
                    return kInvalidFileWatchToken;
                }

                dirIt = state.directories.emplace(sub->directoryKey, std::move(directoryWatch)).first;
            }
            dirIt->second.refCount++;

            // Baseline for the "did the file really change" check
            if (!isDirectory)
            {
                const fs::file_time_type writeTime = fs::last_write_time(absolute, ec);
                if (!ec)
                    state.lastWriteTimes.try_emplace(sub->key, writeTime);
            }

            sub->token = ++state.lastToken;
            state.subscriptions.push_back(sub);
            return sub->token;
        }
    }

    FileWatchToken FileWatcher::WatchFile(const std::filesystem::path &file, FileChangeCallback callback)
    {
        return AddSubscription(file, false, {}, std::move(callback));
    }

    FileWatchToken FileWatcher::WatchDirectory(const std::filesystem::path &directory, FileChangeCallback callback, std::vector<std::string> extensions)
    {
        return AddSubscription(directory, true, std::move(extensions), std::move(callback));
    }

    void FileWatcher::Unwatch(FileWatchToken token)
    {
        if (token == kInvalidFileWatchToken)
            return;

        // The OS watcher joins its callback thread on destruction. That thread may be waiting
        // for the state mutex, so it has to be destroyed after the lock is released.
        Scope<filewatch::FileWatch<std::string>> watcherToDestroy;
        {
            State &state = GetState();
            std::lock_guard lock(state.mutex);

            auto subIt = std::find_if(state.subscriptions.begin(), state.subscriptions.end(),
                [token](const std::shared_ptr<Subscription> &sub) { return sub->token == token; });
            if (subIt == state.subscriptions.end())
                return;

            const std::shared_ptr<Subscription> sub = *subIt;
            sub->active.store(false);
            state.subscriptions.erase(subIt);

            auto dirIt = state.directories.find(sub->directoryKey);
            if (dirIt != state.directories.end() && --dirIt->second.refCount == 0)
            {
                watcherToDestroy = std::move(dirIt->second.watcher);
                state.directories.erase(dirIt);
            }

            if (!sub->isDirectory)
            {
                const bool stillWatched = std::any_of(state.subscriptions.begin(), state.subscriptions.end(),
                    [&sub](const std::shared_ptr<Subscription> &other) { return !other->isDirectory && other->key == sub->key; });
                if (!stillWatched)
                    state.lastWriteTimes.erase(sub->key);
            }
        }
    }

    void FileWatcher::Update()
    {
        struct Dispatch
        {
            FileChangeEvent event;
            std::vector<std::shared_ptr<Subscription>> targets;
        };
        std::vector<Dispatch> dispatches;

        State &state = GetState();
        {
            std::lock_guard lock(state.mutex);
            if (state.pending.empty())
                return;

            const Clock::time_point now = Clock::now();
            const std::chrono::milliseconds debounce(state.debounceMs);

            for (auto it = state.pending.begin(); it != state.pending.end();)
            {
                if (now - it->second.lastEvent < debounce)
                {
                    ++it;
                    continue;
                }

                const std::string key = it->first;
                const fs::path path = it->second.path;
                FileChangeType type = it->second.type;
                it = state.pending.erase(it);

                // Resolve the final on-disk state: atomic saves look like remove + add.
                std::error_code ec;
                if (!fs::exists(path, ec) || ec)
                {
                    type = FileChangeType::Removed;
                    state.lastWriteTimes.erase(key);
                }
                else
                {
                    if (type == FileChangeType::Removed)
                        type = FileChangeType::Modified;

                    const fs::file_time_type writeTime = fs::last_write_time(path, ec);
                    if (!ec)
                    {
                        auto [timeIt, inserted] = state.lastWriteTimes.try_emplace(key, writeTime);
                        if (!inserted)
                        {
                            if (timeIt->second == writeTime)
                                continue; // access/attribute notification, content did not change
                            timeIt->second = writeTime;
                        }
                    }
                }

                Dispatch dispatch;
                dispatch.event = { path, type };
                for (const std::shared_ptr<Subscription> &sub : state.subscriptions)
                {
                    if (Matches(*sub, key))
                        dispatch.targets.push_back(sub);
                }

                if (!dispatch.targets.empty())
                    dispatches.push_back(std::move(dispatch));
            }
        }

        // Callbacks may watch/unwatch freely, no lock is held here.
        for (const Dispatch &dispatch : dispatches)
        {
            for (const std::shared_ptr<Subscription> &sub : dispatch.targets)
            {
                if (sub->active.load())
                    sub->callback(dispatch.event);
            }
        }
    }

    void FileWatcher::Shutdown()
    {
        std::unordered_map<std::string, DirectoryWatch> directories;
        std::vector<std::shared_ptr<Subscription>> subscriptions;
        {
            State &state = GetState();
            std::lock_guard lock(state.mutex);
            directories.swap(state.directories);
            subscriptions.swap(state.subscriptions);
            state.pending.clear();
            state.lastWriteTimes.clear();
        }

        for (const std::shared_ptr<Subscription> &sub : subscriptions)
            sub->active.store(false);

        directories.clear(); // joins the OS watcher threads, outside of the lock
    }

    void FileWatcher::SetDebounceMs(uint32_t milliseconds)
    {
        State &state = GetState();
        std::lock_guard lock(state.mutex);
        state.debounceMs = milliseconds;
    }
}
