/* MIT License
* 
* Copyright (c) 2026 Evangelion Manuhutu
* 
* Permission is hereby granted, free of charge, to any person obtaining a copy
* of this software and associated documentation files (the "Software"), to deal
* in the Software without restriction, including without limitation the rights
* to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
* copies of the Software, and to permit persons to whom the Software is
* furnished to do so, subject to the following conditions:
* 
* The above copyright notice and this permission notice shall be included in all
* copies or substantial portions of the Software.
* 
* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
* IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
* FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
* AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
* LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
* OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
* SOFTWARE.
*/

#pragma once

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

    class IBlob
    {
    public:
        virtual ~IBlob() = default;
        [[nodiscard]] virtual const void *Data() const = 0;
        [[nodiscard]] virtual size_t Size() const = 0;

        static bool IsEmpty(IBlob const *blob)
        {
            return blob == nullptr || blob->Data() == nullptr || blob->Size() == 0;
        }
    };

    class Blob : public IBlob
    {
    public:
        Blob() = default;
        Blob(void *data, size_t size);
        ~Blob() override;
        [[nodiscard]] const void *Data() const override;
        [[nodiscard]] size_t Size() const override;
    private:
        void *m_Data = nullptr;
        size_t m_Size = 0;
    };

    class IFileSystem
    {
    public:
        virtual ~IFileSystem() = default;

        virtual bool DirectoryExists(const ignite::Path &path) = 0;
        virtual bool FileExists(const ignite::Path &path) = 0;

        virtual Ref<IBlob> ReadFile(const ignite::Path &path) = 0;
        virtual bool WriteFile(const ignite::Path &path, const void *data, const size_t size) = 0;

        virtual int EnumerateFiles(const ignite::Path &path, const std::vector<std::string> &extensions, enumerate_callback_t callback, bool allowDuplicates = false) = 0;
        virtual int EnumerateDirectories(const ignite::Path &path, enumerate_callback_t callback, bool allowDuplicates) = 0;
    };

    class NativeFileSystem : public IFileSystem
    {
    public:
        virtual bool DirectoryExists(const ignite::Path &path) override;
        virtual bool FileExists(const ignite::Path &path) override;

        virtual Ref<IBlob> ReadFile(const ignite::Path &path) override;
        virtual bool WriteFile(const ignite::Path &path, const void *data, const size_t size) override;

        virtual int EnumerateFiles(const ignite::Path &path, const std::vector<std::string> &extensions, enumerate_callback_t callback, bool allowDuplicates = false) override;
        virtual int EnumerateDirectories(const ignite::Path &path, enumerate_callback_t callback, bool allowDuplicates) override;
    };

    class RelativeFileSystem : public IFileSystem
    {
    public:
        RelativeFileSystem(Ref<IFileSystem> fs, const ignite::Path &basePath);
        [[nodiscard]] ignite::Path const &GetBasePath() const { return m_BasePath; }

        virtual bool DirectoryExists(const ignite::Path &path) override;
        virtual bool FileExists(const ignite::Path &path) override;

        virtual Ref<IBlob> ReadFile(const ignite::Path &path) override;
        virtual bool WriteFile(const ignite::Path &path, const void *data, const size_t size) override;

        virtual int EnumerateFiles(const ignite::Path &path, const std::vector<std::string> &extensions, enumerate_callback_t callback, bool allowDuplicates = false) override;
        virtual int EnumerateDirectories(const ignite::Path &path, enumerate_callback_t callback, bool allowDuplicates) override;

    private:
        Ref<IFileSystem> m_UnderlyingFS;
        ignite::Path m_BasePath;
    };

    class RootFileSystem : public IFileSystem
    {
    public:
        void Mount(const ignite::Path &path, Ref<IFileSystem> fs);
        void Mount(const ignite::Path &path, const ignite::Path &nativePath);
        bool Unmount(const ignite::Path &path);

        virtual bool DirectoryExists(const ignite::Path &path) override;
        virtual bool FileExists(const ignite::Path &path) override;

        virtual Ref<IBlob> ReadFile(const ignite::Path &path) override;
        virtual bool WriteFile(const ignite::Path &path, const void *data, const size_t size) override;

        virtual int EnumerateFiles(const ignite::Path &path, const std::vector<std::string> &extensions, enumerate_callback_t callback, bool allowDuplicates = false) override;
        virtual int EnumerateDirectories(const ignite::Path &path, enumerate_callback_t callback, bool allowDuplicates) override;
    private:
        std::vector<std::pair<std::string, Ref<IFileSystem>>> m_MountPoints;
        bool FindMountPoint(const ignite::Path& path, ignite::Path* pRelativePath, IFileSystem** ppFS);
    };

    std::string GetFileSearchRegex(const ignite::Path &path, const std::vector<std::string> &extensions);
}
