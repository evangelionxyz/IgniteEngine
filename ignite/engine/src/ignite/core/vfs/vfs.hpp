// Copyright (c) 2026 Evangelion Manuhutu

#pragma once
#ifndef IGN_VFS_HPP
#define IGN_VFS_HPP

#include "ignite/core/buffer.hpp"
#include "ignite/core/types.hpp"
#include "ignite/core/path.hpp"
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

    class IGN_API IFileSystem
    {
    public:
        virtual ~IFileSystem() = default;

        virtual bool DirectoryExists(const ignite::Path &path) = 0;
        virtual bool FileExists(const ignite::Path &path) = 0;

        virtual Buffer ReadFile(const ignite::Path &path) = 0;
        virtual bool WriteFile(const ignite::Path &path, const void *data, const size_t size) = 0;

        virtual int EnumerateFiles(const ignite::Path &path, const std::vector<std::string> &extensions, enumerate_callback_t callback, bool allowDuplicates = false) = 0;
        virtual int EnumerateDirectories(const ignite::Path &path, enumerate_callback_t callback, bool allowDuplicates) = 0;
    };

    class IGN_API NativeFileSystem : public IFileSystem
    {
    public:
        virtual bool DirectoryExists(const ignite::Path &path) override;
        virtual bool FileExists(const ignite::Path &path) override;

        virtual Buffer ReadFile(const ignite::Path &path) override;
        virtual bool WriteFile(const ignite::Path &path, const void *data, const size_t size) override;

        virtual int EnumerateFiles(const ignite::Path &path, const std::vector<std::string> &extensions, enumerate_callback_t callback, bool allowDuplicates = false) override;
        virtual int EnumerateDirectories(const ignite::Path &path, enumerate_callback_t callback, bool allowDuplicates) override;
    };

    class IGN_API RelativeFileSystem : public IFileSystem
    {
    public:
        RelativeFileSystem(Ref<IFileSystem> fs, const ignite::Path &basePath);
        [[nodiscard]] ignite::Path const &GetBasePath() const { return m_BasePath; }

        virtual bool DirectoryExists(const ignite::Path &path) override;
        virtual bool FileExists(const ignite::Path &path) override;

        virtual Buffer ReadFile(const ignite::Path &path) override;
        virtual bool WriteFile(const ignite::Path &path, const void *data, const size_t size) override;

        virtual int EnumerateFiles(const ignite::Path &path, const std::vector<std::string> &extensions, enumerate_callback_t callback, bool allowDuplicates = false) override;
        virtual int EnumerateDirectories(const ignite::Path &path, enumerate_callback_t callback, bool allowDuplicates) override;

		ignite::Path ResolveAndSanitize(const ignite::Path &name) const;

    private:
        Ref<IFileSystem> m_UnderlyingFS;
        ignite::Path m_BasePath;
    };

    class IGN_API RootFileSystem : public IFileSystem
    {
    public:
        void Mount(const ignite::Path &path, Ref<IFileSystem> fs);
        void Mount(const ignite::Path &path, const ignite::Path &nativePath);
        bool Unmount(const ignite::Path &path);

        virtual bool DirectoryExists(const ignite::Path &path) override;
        virtual bool FileExists(const ignite::Path &path) override;

        virtual Buffer ReadFile(const ignite::Path &path) override;
        virtual bool WriteFile(const ignite::Path &path, const void *data, const size_t size) override;

        virtual int EnumerateFiles(const ignite::Path &path, const std::vector<std::string> &extensions, enumerate_callback_t callback, bool allowDuplicates = false) override;
        virtual int EnumerateDirectories(const ignite::Path &path, enumerate_callback_t callback, bool allowDuplicates) override;
    private:
        std::vector<std::pair<std::string, Ref<IFileSystem>>> m_MountPoints;
        bool FindMountPoint(const ignite::Path& path, ignite::Path* pRelativePath, IFileSystem** ppFS);
    };

    IGN_API std::string GetFileSearchRegex(const ignite::Path &path, const std::vector<std::string> &extensions);

	// Returns the full path to the running executable, e.g. C:\Dev\Ignite\Build\MyExe.exe
	IGN_API ignite::Path GetExecutablePath();

	// Returns the directory that contains the running executable, e.g. C:\Dev\Ignite\Build
	IGN_API ignite::Path GetExecutableDirectory();
}

#endif
