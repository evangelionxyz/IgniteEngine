#pragma once

#include <string>

namespace ignite
{
    class Path
    {
    public:
        Path();
        Path(const char *p);
        Path(const std::string &p);
        Path(const Path &other);
        Path(Path &&other) noexcept;
        ~Path();

        Path &operator=(const Path &other);
        Path &operator=(Path &&other) noexcept;
        Path &operator=(const std::string &p);
        Path &operator=(const char *p);

        Path &operator+=(const Path &other);
        Path &operator+=(Path &&other);
        Path &operator+=(const std::string &p);
        Path &operator+=(const char *p);

        Path operator/(const Path &other) const;
        Path operator/(const std::string &other) const;
        Path operator/(const char *other) const;

        Path &operator/=(const Path &other);
        Path &operator/=(const std::string &other);
        Path &operator/=(const char *other);

        bool operator==(const Path &other) const;
        bool operator!=(const Path &other) const;
        bool operator<(const Path &other) const;
        bool operator>(const Path &other) const;
        bool operator<=(const Path &other) const;
        bool operator>=(const Path &other) const;

        std::string string() const;
        std::string generic_string() const;
        const char *c_str() const;

        std::wstring wstring() const;
        std::wstring generic_wstring() const;

        Path begin() const;

        Path parent_path() const;
        Path extension() const;
        Path filename() const;
        Path stem() const;

        bool empty() const;
        bool is_absolute() const;
        bool is_relative() const;
        void clear();

        bool has_extension() const;
        bool has_parent_path() const;
        bool has_filename() const;
        static bool exists(const Path &path);
        static bool create_directory(const std::string &path);
        static bool create_directory(const char *path);
        static bool create_directory(const Path &path);
        static bool create_directories(const std::string &path);
        static bool create_directories(const char *path);
        static bool create_directories(const Path &path);

        static bool is_directory(const Path &path);
        static bool is_directory(const std::string &path);
        static bool is_directory(const char *path);

        static bool is_regular_file(const Path &path);
        static bool is_regular_file(const std::string &path);
        static bool is_regular_file(const char *path);

        Path lexically_normal() const;
        Path lexically_relative(const Path &other) const;
        Path relative_path() const;

        static Path relative(const Path &path, const Path &base);

        Path replace_extension(const std::string &ext);

        // implicit conversion to string for convenience
        operator std::string() const;

    private:
        struct Impl;
        Impl *m_Impl;
    };
}

namespace std
{
    template<>
    struct hash<ignite::Path>
    {
        size_t operator()(const ignite::Path& p) const
        {
            return hash<string>()(p.string());
        }
    };
}
