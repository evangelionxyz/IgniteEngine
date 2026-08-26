// Copyright (c) 2026 Evangelion Manuhutu

#pragma once
#ifndef IGN_VFS_HPP
#define IGN_VFS_HPP

#include "ignite/core/buffer.hpp"
#include "ignite/core/types.hpp"
#include "FileWatch.hpp"
#include <functional>

namespace ignite::vfs
{
    namespace status
    {
        constexpr int OK = 0;
        constexpr int Failed = -1;
        constexpr int PathNotFound = -2;
        constexpr int NotImplemented = -3;
    }

    using enumerate_callback_t = const std::function<void(std::string_view)> &;
    using FileWatchCallback = std::function<void(const std::string &, const filewatch::Event)>;


    inline Scope<filewatch::FileWatch<std::string>> WatchFile(const std::filesystem::path &path, FileWatchCallback callback)
    {
        if (path.empty())
        {
            return {};
        }
        return CreateScope<filewatch::FileWatch<std::string>>(path.string(), std::move(callback));
    }

    inline bool TryGetFileWriteTime(const std::filesystem::path &filepath, std::chrono::time_point<std::chrono::file_clock> &outTime)
    {
        std::error_code ec;
        if (!std::filesystem::exists(filepath.string(), ec) || ec)
        {
            return false;
        }
        outTime = std::filesystem::last_write_time(filepath.string(), ec);
        return !ec;
    }

    inline bool WaitForFileReady(const std::filesystem::path &filepath)
    {
        using namespace std::chrono_literals;

        uintmax_t lastSize = 0;
        bool hasLastSize = false;
        std::chrono::time_point<std::chrono::file_clock> lastWriteTime{};
        bool hasLastWrite = false;
        int stableCount = 0;

        // Require more consecutive stable reads to handle MSBuild's two-phase write
        // (write partial file -> flush -> rename/finalize). Each poll is 25ms apart,
        // so stableRequired=5 means ~125ms of confirmed stability before loading.
        constexpr int stableRequired = 5;

        for (int i = 0; i < 80; i++)
        {
            std::error_code ec;
            if (!std::filesystem::exists(filepath.string(), ec) || ec)
            {
                std::this_thread::sleep_for(25ms);
                continue;
            }

            const auto writeTime = std::filesystem::last_write_time(filepath.string(), ec);
            if (ec)
            {
                std::this_thread::sleep_for(25ms);
                continue;
            }

            const auto fileSize = std::filesystem::file_size(filepath.string(), ec);
            if (ec)
            {
                std::this_thread::sleep_for(25ms);
                continue;
            }

            std::ifstream stream(filepath, std::ios::binary);
            if (!stream.good())
            {
                std::this_thread::sleep_for(25ms);
                continue;
            }

            const bool sameWrite = hasLastWrite && writeTime == lastWriteTime;
            const bool sameSize = hasLastSize && fileSize == lastSize;

            if (sameWrite && sameSize)
            {
                stableCount++;
                if (stableCount >= stableRequired)
                {
                    return true;
                }
            }
            else
            {
                stableCount = 0;
            }

            hasLastWrite = true;
            hasLastSize = true;
            lastWriteTime = writeTime;
            lastSize = fileSize;

            std::this_thread::sleep_for(25ms);
        }

        return false;
    }

    inline bool WaitForFileNewerThan(const std::filesystem::path &filepath, const std::chrono::time_point<std::chrono::file_clock> &previousWriteTime)
    {
        using namespace std::chrono_literals;

        for (int i = 0; i < 120; i++)
        {
            std::chrono::time_point<std::chrono::file_clock> currentWriteTime{};
            if (TryGetFileWriteTime(filepath, currentWriteTime) && currentWriteTime > previousWriteTime)
            {
                return true;
            }

            std::this_thread::sleep_for(25ms);
        }

        return false;
    }

    class IGN_API IFileSystem
    {
    public:
        virtual ~IFileSystem() = default;

        virtual bool DirectoryExists(const std::filesystem::path &path) = 0;
        virtual bool FileExists(const std::filesystem::path &path) = 0;

        virtual Buffer ReadFile(const std::filesystem::path &path) = 0;
        virtual bool WriteFile(const std::filesystem::path &path, const void *data, const size_t size) = 0;

        virtual int EnumerateFiles(const std::filesystem::path &path, const std::vector<std::string> &extensions, enumerate_callback_t callback, bool allowDuplicates = false) = 0;
        virtual int EnumerateDirectories(const std::filesystem::path &path, enumerate_callback_t callback, bool allowDuplicates) = 0;
    };

    class IGN_API NativeFileSystem : public IFileSystem
    {
    public:
        virtual bool DirectoryExists(const std::filesystem::path &path) override;
        virtual bool FileExists(const std::filesystem::path &path) override;

        virtual Buffer ReadFile(const std::filesystem::path &path) override;
        virtual bool WriteFile(const std::filesystem::path &path, const void *data, const size_t size) override;

        virtual int EnumerateFiles(const std::filesystem::path &path, const std::vector<std::string> &extensions, enumerate_callback_t callback, bool allowDuplicates = false) override;
        virtual int EnumerateDirectories(const std::filesystem::path &path, enumerate_callback_t callback, bool allowDuplicates) override;
    };

    class IGN_API RelativeFileSystem : public IFileSystem
    {
    public:
        RelativeFileSystem(Ref<IFileSystem> fs, const std::filesystem::path &basePath);
        [[nodiscard]] std::filesystem::path const &GetBasePath() const { return m_BasePath; }

        virtual bool DirectoryExists(const std::filesystem::path &path) override;
        virtual bool FileExists(const std::filesystem::path &path) override;

        virtual Buffer ReadFile(const std::filesystem::path &path) override;
        virtual bool WriteFile(const std::filesystem::path &path, const void *data, const size_t size) override;

        virtual int EnumerateFiles(const std::filesystem::path &path, const std::vector<std::string> &extensions, enumerate_callback_t callback, bool allowDuplicates = false) override;
        virtual int EnumerateDirectories(const std::filesystem::path &path, enumerate_callback_t callback, bool allowDuplicates) override;

		std::filesystem::path ResolveAndSanitize(const std::filesystem::path &name) const;

    private:
        Ref<IFileSystem> m_UnderlyingFS;
        std::filesystem::path m_BasePath;
    };

    class IGN_API RootFileSystem : public IFileSystem
    {
    public:
        void Mount(const std::filesystem::path &path, Ref<IFileSystem> fs);
        void Mount(const std::filesystem::path &path, const std::filesystem::path &nativePath);
        bool Unmount(const std::filesystem::path &path);

        virtual bool DirectoryExists(const std::filesystem::path &path) override;
        virtual bool FileExists(const std::filesystem::path &path) override;

        virtual Buffer ReadFile(const std::filesystem::path &path) override;
        virtual bool WriteFile(const std::filesystem::path &path, const void *data, const size_t size) override;

        virtual int EnumerateFiles(const std::filesystem::path &path, const std::vector<std::string> &extensions, enumerate_callback_t callback, bool allowDuplicates = false) override;
        virtual int EnumerateDirectories(const std::filesystem::path &path, enumerate_callback_t callback, bool allowDuplicates) override;
    private:
        std::vector<std::pair<std::string, Ref<IFileSystem>>> m_MountPoints;
        bool FindMountPoint(const std::filesystem::path& path, std::filesystem::path* pRelativePath, IFileSystem** ppFS);
    };

    IGN_API std::string GetFileSearchRegex(const std::filesystem::path &path, const std::vector<std::string> &extensions);

	// Returns the full path to the running executable, e.g. C:\Dev\Ignite\Build\MyExe.exe
	IGN_API std::filesystem::path GetExecutablePath();

	// Returns the directory that contains the running executable, e.g. C:\Dev\Ignite\Build
	IGN_API std::filesystem::path GetExecutableDirectory();
}

#endif
