#include "path.hpp"
#include <filesystem>

namespace ignite
{
    struct Path::Impl
    {
        std::filesystem::path internalPath;
    };

    Path::Path() : m_Impl(new Impl()) {}

    Path::Path(const char* p) : m_Impl(new Impl{std::filesystem::path(p)}) {}

    Path::Path(const std::string& p) : m_Impl(new Impl{std::filesystem::path(p)}) {}

    Path::Path(const Path& other) : m_Impl(new Impl{other.m_Impl->internalPath}) {}

    Path::Path(Path&& other) noexcept : m_Impl(other.m_Impl)
    {
        other.m_Impl = nullptr;
    }

    Path::~Path()
    {
        delete m_Impl;
    }

    Path& Path::operator=(const Path& other)
    {
        if (this != &other)
        {
            if (!m_Impl) m_Impl = new Impl();
            m_Impl->internalPath = other.m_Impl->internalPath;
        }
        return *this;
    }

    Path& Path::operator=(Path&& other) noexcept
    {
        if (this != &other)
        {
            delete m_Impl;
            m_Impl = other.m_Impl;
            other.m_Impl = nullptr;
        }
        return *this;
    }

    Path& Path::operator=(const std::string& p)
    {
        if (!m_Impl) m_Impl = new Impl();
        m_Impl->internalPath = p;
        return *this;
    }

    Path& Path::operator=(const char* p)
    {
        if (!m_Impl) m_Impl = new Impl();
        m_Impl->internalPath = p;
        return *this;
    }


    Path &Path::operator+=(const Path &other)
    {
        if (!m_Impl) m_Impl = new Impl();
        m_Impl->internalPath += other.m_Impl->internalPath;
        return *this;
    }


    Path &Path::operator+=(Path &&other)
    {
        if (!m_Impl) m_Impl = new Impl();
        m_Impl->internalPath += other.m_Impl->internalPath;
        return *this;
    }


    Path &Path::operator+=(const std::string &p)
    {
        if (!m_Impl) m_Impl = new Impl();
        m_Impl->internalPath += p;
        return *this;
    }


    Path &Path::operator+=(const char *p)
    {
        if (!m_Impl) m_Impl = new Impl();
        m_Impl->internalPath += p;
        return *this;
    }

    Path Path::operator/(const Path &other) const
    {
        Path result;
        result.m_Impl->internalPath = m_Impl->internalPath / other.m_Impl->internalPath;
        return result;
    }

    Path Path::operator/(const std::string& other) const
    {
        Path result;
        result.m_Impl->internalPath = m_Impl->internalPath / other;
        return result;
    }

    Path Path::operator/(const char* other) const
    {
        Path result;
        result.m_Impl->internalPath = m_Impl->internalPath / other;
        return result;
    }

    Path& Path::operator/=(const Path& other)
    {
        m_Impl->internalPath /= other.m_Impl->internalPath;
        return *this;
    }

    Path& Path::operator/=(const std::string& other)
    {
        m_Impl->internalPath /= other;
        return *this;
    }

    Path& Path::operator/=(const char* other)
    {
        m_Impl->internalPath /= other;
        return *this;
    }

    bool Path::operator==(const Path& other) const
    {
        return m_Impl->internalPath == other.m_Impl->internalPath;
    }

    bool Path::operator!=(const Path& other) const
    {
        return m_Impl->internalPath != other.m_Impl->internalPath;
    }

    bool Path::operator<(const Path& other) const
    {
        return m_Impl->internalPath < other.m_Impl->internalPath;
    }

    bool Path::operator>(const Path& other) const
    {
        return m_Impl->internalPath > other.m_Impl->internalPath;
    }

    bool Path::operator<=(const Path& other) const
    {
        return m_Impl->internalPath <= other.m_Impl->internalPath;
    }

    bool Path::operator>=(const Path& other) const
    {
        return m_Impl->internalPath >= other.m_Impl->internalPath;
    }

    std::string Path::string() const
    {
        return m_Impl->internalPath.string();
    }

    std::string Path::generic_string() const
    {
        return m_Impl->internalPath.generic_string();
    }

    const char* Path::c_str() const
    {
        return (const char *)m_Impl->internalPath.c_str();
    }


    std::wstring Path::wstring() const
    {
        return m_Impl->internalPath.wstring();
    }


    std::wstring Path::generic_wstring() const
    {
        return m_Impl->internalPath.generic_wstring();
    }


    Path Path::begin() const
    {
        Path result;
        result.m_Impl->internalPath = (*m_Impl->internalPath.begin());
        return result;
    }

    Path Path::parent_path() const
    {
        Path result;
        result.m_Impl->internalPath = m_Impl->internalPath.parent_path();
        return result;
    }

    Path Path::extension() const
    {
        Path result;
        result.m_Impl->internalPath = m_Impl->internalPath.extension();
        return result;
    }

    Path Path::filename() const
    {
        Path result;
        result.m_Impl->internalPath = m_Impl->internalPath.filename();
        return result;
    }

    Path Path::stem() const
    {
        Path result;
        result.m_Impl->internalPath = m_Impl->internalPath.stem();
        return result;
    }

    bool Path::empty() const
    {
        return m_Impl->internalPath.empty();
    }

    bool Path::is_absolute() const
    {
        return m_Impl->internalPath.is_absolute();
    }

    bool Path::is_relative() const
    {
        return m_Impl->internalPath.is_relative();
    }

    void Path::clear()
    {
        m_Impl->internalPath.clear();
    }

    bool Path::has_extension() const
    {
        return m_Impl->internalPath.has_extension();
    }

    bool Path::has_parent_path() const
    {
        return m_Impl->internalPath.has_parent_path();
    }

    bool Path::has_filename() const
    {
        return m_Impl->internalPath.has_filename();
    }

    bool Path::exists(const Path &path)
    {
        return std::filesystem::exists(path.m_Impl->internalPath);
    }

    bool Path::create_directory(const std::string &path)
    {
        return std::filesystem::create_directory(path);
    }

    bool Path::create_directory(const char *path)
    {
        return std::filesystem::create_directory(path);
    }

    bool Path::create_directory(const Path &path)
    {
        return std::filesystem::create_directory(path.m_Impl->internalPath);
    }

    bool Path::create_directories(const std::string &path)
    {
        return std::filesystem::create_directories(path);
    }

    bool Path::create_directories(const char *path)
    {
        return std::filesystem::create_directories(path);
    }

    bool Path::create_directories(const Path &path)
    {
        return std::filesystem::create_directories(path.m_Impl->internalPath);
    }


    bool Path::is_directory(const Path &path)
    {
        return std::filesystem::is_directory(path.m_Impl->internalPath);
    }


    bool Path::is_directory(const std::string &path)
    {
        return std::filesystem::is_directory(path);
    }


    bool Path::is_directory(const char *path)
    {
        return std::filesystem::is_directory(path);
    }


    bool Path::is_regular_file(const Path &path)
    {
        return std::filesystem::is_regular_file(path.m_Impl->internalPath);
    }


    bool Path::is_regular_file(const std::string &path)
    {
        return std::filesystem::is_regular_file(path);
    }


    bool Path::is_regular_file(const char *path)
    {
        return std::filesystem::is_regular_file(path);
    }

    Path Path::lexically_normal() const
    {
        Path result;
        result.m_Impl->internalPath = m_Impl->internalPath.lexically_normal();
        return result;
    }

    Path Path::lexically_relative(const Path &other) const
    {
        Path result;
        result.m_Impl->internalPath = m_Impl->internalPath.lexically_relative(other.m_Impl->internalPath);
        return result;
    }

    Path Path::relative_path() const
    {
        Path result;
        result.m_Impl->internalPath = m_Impl->internalPath.relative_path();
        return result;
    }

    Path Path::relative(const Path &path, const Path &base)
    {
        Path result;
        result.m_Impl->internalPath = std::filesystem::relative(path.m_Impl->internalPath, base.m_Impl->internalPath);
        return result;
    }

    Path Path::replace_extension(const std::string &ext)
    {
        Path result;
        result.m_Impl->internalPath = m_Impl->internalPath.replace_extension(ext);
        return result;
    }

    Path::operator std::string() const
    {
        return string();
    }
}
