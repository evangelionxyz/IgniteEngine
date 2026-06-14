// Copyright (c) 2026 Evangelion Manuhutu

#include "vfs.hpp"

#include "ignite/core/logger.hpp"
#include "ignite/core/string_utils.hpp"

#include <fstream>
#include <cassert>
#include <algorithm>
#include <utility>
#include <sstream>

#include <stdint.h>

#if PLATFORM_WINDOWS
#include <Shlwapi.h>
#elif __linux__ || __GNUG__
#include <glob.h>
#endif

namespace ignite::vfs
{
    bool NativeFileSystem::DirectoryExists(const ignite::Path &path)
    {
        return ignite::Path::exists(path) && ignite::Path::is_directory(path);
    }

    bool NativeFileSystem::FileExists(const ignite::Path &path)
    {
        return ignite::Path::exists(path) && ignite::Path::is_regular_file(path);
    }

    Buffer NativeFileSystem::ReadFile(const ignite::Path &path)
    {
        std::ifstream file(path, std::ios::binary);
        if (!file.is_open())
            return { };

        file.seekg(0, std::ios::end);
        std::streampos tellgSize = file.tellg();
        
        if (tellgSize < 0)
        {
			LOG_ASSERT(false, "[NativeFileSystem] Failed to determine file size!", tellgSize);
			return { };
        }
        
        const auto size = static_cast<uint64_t>(tellgSize);
        file.seekg(0, std::ios::beg);
        if (size > std::numeric_limits<size_t>::max())
        {
			LOG_ASSERT(false, "[NativeFileSystem] File is too large! {} bytes", size);
			return { };
        }

        if (size == 0 || !file.good())
        {
            LOG_ASSERT(false, "[NativeFileSystem] File might corrupted!");
            return { };
        }

        std::vector<uint8_t> dataVector(size);
        if (!file.read((char *)dataVector.data(), static_cast<std::streamsize>(size)))
        {
			LOG_ASSERT(false, "[NativeFileSystem] Failed to read expected file bytes!");
			return { };
        }

        return { dataVector };
    }

    bool NativeFileSystem::WriteFile(const ignite::Path &path, const void *data, size_t size)
    {
        std::ofstream file(path, std::ios::binary);
        if (!file.is_open())
        {
            return false;
        }

        if (size > 0)
        {
            file.write(static_cast<const char *>(data), static_cast<std::streamsize>(size));
        }

        if (!file.good())
        {
            LOG_ASSERT(false, "File writing error");
            return false;
        }
        return true;
    }

    static int EnumerateNativeFiles(const char *pattern, bool directories, enumerate_callback_t callback)
    {
#ifdef _WIN32
        WIN32_FIND_DATAA findData;
        HANDLE hFind = FindFirstFileA(pattern, &findData);
        if (hFind == INVALID_HANDLE_VALUE)
        {
            if (GetLastError() == ERROR_FILE_NOT_FOUND)
                return 0;
            return status::Failed;
        }
        int numEntries = 0;
        do
        {
            bool isDirectory = (findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
            bool isDot = strcmp(findData.cFileName, ".") == 0;
            bool isDotDot = strcmp(findData.cFileName, "..") == 0;

            if ((isDirectory == directories) && !isDot && !isDotDot)
            {
                callback(findData.cFileName);
                ++numEntries;
            }
        } while (FindNextFileA(hFind, &findData) != 0);

        FindClose(hFind);

        return numEntries;

#else // !WIN32

        glob64_t glob_matches;
        int globResult = glob64(pattern, 0 /*flags*/, nullptr /*errfunc*/, &glob_matches);

        if (globResult == 0)
        {
            int numEntries = 0;

            for (int i=0; i<glob_matches.gl_pathc; ++i)
            {
                const char* globentry = (glob_matches.gl_pathv)[i];
                std::error_code ec, ec2;
                std::filesystem::directory_entry entry(globentry, ec);
                if (!ec)
                {
                    if (directories == entry.is_directory(ec2) && !ec2)
                    {
                        callback(entry.path().filename().native());
                        ++numEntries;
                    }
                }
            }
            globfree64(&glob_matches);

            return numEntries;
        }

        if (globResult == GLOB_NOMATCH)
            return 0;

        return status::Failed;
#endif
    }

    int NativeFileSystem::EnumerateFiles(const ignite::Path &path, const std::vector<std::string> &extensions, enumerate_callback_t callback, bool allowDuplicates)
    {
        (void)allowDuplicates;

        if (extensions.empty())
        {
            std::string pattern = (path / "*").generic_string();
            return EnumerateNativeFiles(pattern.c_str(), false, callback);
        }

        int numEntries = 0;
        for (const auto &ext : extensions)
        {
            std::string pattern = (path / ("*" + ext)).generic_string();
            int result = EnumerateNativeFiles(pattern.c_str(), false, callback);

            if (result < 0)
                return result;

            numEntries += result;
        }

        return numEntries;
    }

    int NativeFileSystem::EnumerateDirectories(const ignite::Path &path, enumerate_callback_t callback, bool allowDuplicates)
    {
        (void)allowDuplicates;

        std::string pattern = (path / "*").generic_string();
        return EnumerateNativeFiles(pattern.c_str(), true, callback);
    }

    RelativeFileSystem::RelativeFileSystem(Ref<IFileSystem> fs, const ignite::Path& basePath)
        : m_UnderlyingFS(std::move(fs))
        , m_BasePath(basePath.lexically_normal())
    {
    }

    bool RelativeFileSystem::DirectoryExists(const ignite::Path& name)
    {
        auto relative = ResolveAndSanitize(name);
        return m_UnderlyingFS->DirectoryExists(relative);
    }

    bool RelativeFileSystem::FileExists(const ignite::Path& name)
    {
		auto relative = ResolveAndSanitize(name);
        return m_UnderlyingFS->FileExists(relative);
    }

    Buffer RelativeFileSystem::ReadFile(const ignite::Path& name)
    {
		auto relative = ResolveAndSanitize(name);
        return m_UnderlyingFS->ReadFile(relative);
    }

    bool RelativeFileSystem::WriteFile(const ignite::Path& name, const void* data, size_t size)
    {
		auto relative = ResolveAndSanitize(name);
        return m_UnderlyingFS->WriteFile(relative, data, size);
    }

    int RelativeFileSystem::EnumerateFiles(const ignite::Path& path, const std::vector<std::string>& extensions, enumerate_callback_t callback, bool allowDuplicates)
    {
		auto relative = ResolveAndSanitize(path);
        return m_UnderlyingFS->EnumerateFiles(relative, extensions, callback, allowDuplicates);
    }

    int RelativeFileSystem::EnumerateDirectories(const ignite::Path& path, enumerate_callback_t callback, bool allowDuplicates)
    {
		auto relative = ResolveAndSanitize(path);
        return m_UnderlyingFS->EnumerateDirectories(relative, callback, allowDuplicates);
    }

	ignite::Path RelativeFileSystem::ResolveAndSanitize(const ignite::Path &name) const
	{
		ignite::Path combined = (m_BasePath / name).lexically_normal();
        ignite::Path relative = combined.lexically_relative(m_BasePath);
        std::string relativeStr = relative.generic_string();
        if (relativeStr.find("..") != std::string::npos)
        {
            return { };
        }
        return combined;
	}

    void RootFileSystem::Mount(const ignite::Path& path, std::shared_ptr<IFileSystem> fs)
    {
        if (FindMountPoint(path, nullptr, nullptr))
        {
            LOG_ERROR("Cannot mount a filesystem at {}: there is another FS that includes this path", path.generic_string());
            return;
        }

        m_MountPoints.push_back(std::make_pair(path.lexically_normal().generic_string(), fs));
    }

    void RootFileSystem::Mount(const ignite::Path& path, const ignite::Path& nativePath)
    {
        Mount(path, std::make_shared<RelativeFileSystem>(std::make_shared<NativeFileSystem>(), nativePath));
    }

    bool RootFileSystem::Unmount(const ignite::Path &path)
    {
        std::string spath = path.lexically_normal().generic_string();

        for (size_t index = 0; index < m_MountPoints.size(); index++)
        {
            if (m_MountPoints[index].first == spath)
            {
                m_MountPoints.erase(m_MountPoints.begin() + index);
                return true;
            }
        }

        return false;
    }

    bool RootFileSystem::FindMountPoint(const ignite::Path& path, ignite::Path* pRelativePath, IFileSystem** ppFS)
    {
        std::string spath = path.lexically_normal().generic_string();

        for (auto it : m_MountPoints)
        {
            if (spath.find(it.first, 0) == 0 && ((spath.length() == it.first.length()) || (spath[it.first.length()] == '/')))
            {
                if (pRelativePath)
                {
                    std::string relative = (spath.length() == it.first.length()) ? "" : spath.substr(it.first.size() + 1);
                    *pRelativePath = relative;
                }

                if (ppFS)
                {
                    *ppFS = it.second.get();
                }

                return true;
            }
        }

        return false;
    }

    bool RootFileSystem::DirectoryExists(const ignite::Path& name)
    {
        ignite::Path relativePath;
        IFileSystem* fs = nullptr;

        if (FindMountPoint(name, &relativePath, &fs))
        {
            return fs->DirectoryExists(relativePath);
        }

        return false;
    }

    bool RootFileSystem::FileExists(const ignite::Path& name)
    {
        ignite::Path relativePath;
        IFileSystem* fs = nullptr;

        if (FindMountPoint(name, &relativePath, &fs))
        {
            return fs->FileExists(relativePath);
        }

        return false;
    }

    Buffer RootFileSystem::ReadFile(const ignite::Path& name)
    {
        ignite::Path relativePath;
        IFileSystem* fs = nullptr;

        if (FindMountPoint(name, &relativePath, &fs))
        {
            return fs->ReadFile(relativePath);
        }

        return { };
    }

    bool RootFileSystem::WriteFile(const ignite::Path& name, const void* data, size_t size)
    {
        ignite::Path relativePath;
        IFileSystem* fs = nullptr;

        if (FindMountPoint(name, &relativePath, &fs))
        {
            return fs->WriteFile(relativePath, data, size);
        }

        return false;
    }

    int RootFileSystem::EnumerateFiles(const ignite::Path& path, const std::vector<std::string>& extensions, enumerate_callback_t callback, bool allowDuplicates)
    {
        ignite::Path relativePath;
        IFileSystem* fs = nullptr;

        if (FindMountPoint(path, &relativePath, &fs))
        {
            return fs->EnumerateFiles(relativePath, extensions, callback, allowDuplicates);
        }

        return status::PathNotFound;
    }

    int RootFileSystem::EnumerateDirectories(const ignite::Path& path, enumerate_callback_t callback, bool allowDuplicates)
    {
        ignite::Path relativePath;
        IFileSystem* fs = nullptr;

        if (FindMountPoint(path, &relativePath, &fs))
        {
            return fs->EnumerateDirectories(relativePath, callback, allowDuplicates);
        }

        return status::PathNotFound;
    }

    static void AppendPatternToReges(const std::string &pattern, std::stringstream &regex)
    {
        for (char c : pattern)
        {
            switch (c)
            {
                case '?': regex << "[^/]?"; break;
                case '*': regex << "[^/]+"; break;
                case '.': regex << "\\."; break;
                default: regex << c;
            }
        }
    }

    std::string GetFileSearchRegex(const ignite::Path &path, const std::vector<std::string> &extensions)
    {
        ignite::Path normalizedPath = path.lexically_normal();
        std::string normalizedPathStr = normalizedPath.generic_string();

        std::stringstream regex;
        AppendPatternToReges(normalizedPathStr, regex);

        if (stringutils::EndsWith(normalizedPathStr, "/") && !normalizedPath.empty())
            regex << '/';
        regex << "[^/]+";

        if (!extensions.empty())
        {
            regex << '(';
            bool first = true;
            for (const auto &ext : extensions)
            {
                if (!first)
                    regex << '|';
                AppendPatternToReges(ext, regex);
                first = false;
            }
            regex << ')';
        }

        return regex.str();
    }
}
