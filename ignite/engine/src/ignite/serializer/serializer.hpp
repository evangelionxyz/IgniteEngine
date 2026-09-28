// Copyright (c) 2026 Evangelion Manuhutu

#ifndef IGN_SERIALIZER_HPP
#define IGN_SERIALIZER_HPP

#include "ignite/core/uuid.hpp"
#include "ignite/math/math.hpp"
#include "ignite/core/base.hpp"
#include "ignite/core/logger.hpp"
#include "ignite/animation/skeletal_animation.hpp"

#include <nlohmann/json.hpp>
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <string>
#include <filesystem>
#include <vector>
#include <memory>

namespace nlohmann
{
    // char16_t
    template <>
    struct adl_serializer<char16_t>
    {
        template <typename BasicJsonType>
        static void to_json(BasicJsonType &j, const char16_t &c)
        {
            j = static_cast<uint16_t>(c);
        }
        template <typename BasicJsonType>
        static void from_json(const BasicJsonType &j, char16_t &c)
        {
            if (j.is_number())
                c = static_cast<char16_t>(j.template get<uint16_t>());
        }
    };

    // glm::vec2
    template <>
    struct adl_serializer<glm::vec2>
    {
        template <typename BasicJsonType>
        static void to_json(BasicJsonType &j, const glm::vec2 &v)
        {
            j = BasicJsonType::array({ v.x, v.y });
        }
        template <typename BasicJsonType>
        static void from_json(const BasicJsonType &j, glm::vec2 &v)
        {
            if (j.is_array() && j.size() >= 2)
            {
                v.x = j[0].template get<float>();
                v.y = j[1].template get<float>();
            }
        }
    };

    // glm::vec3
    template <>
    struct adl_serializer<glm::vec3>
    {
        template <typename BasicJsonType>
        static void to_json(BasicJsonType &j, const glm::vec3 &v)
        {
            j = BasicJsonType::array({ v.x, v.y, v.z });
        }
        template <typename BasicJsonType>
        static void from_json(const BasicJsonType &j, glm::vec3 &v)
        {
            if (j.is_array() && j.size() >= 3)
            {
                v.x = j[0].template get<float>();
                v.y = j[1].template get<float>();
                v.z = j[2].template get<float>();
            }
        }
    };

    // glm::vec4
    template <>
    struct adl_serializer<glm::vec4>
    {
        template <typename BasicJsonType>
        static void to_json(BasicJsonType &j, const glm::vec4 &v)
        {
            j = BasicJsonType::array({ v.x, v.y, v.z, v.w });
        }
        template <typename BasicJsonType>
        static void from_json(const BasicJsonType &j, glm::vec4 &v)
        {
            if (j.is_array() && j.size() >= 4)
            {
                v.x = j[0].template get<float>();
                v.y = j[1].template get<float>();
                v.z = j[2].template get<float>();
                v.w = j[3].template get<float>();
            }
        }
    };

    // glm::ivec2
    template <>
    struct adl_serializer<glm::ivec2>
    {
        template <typename BasicJsonType>
        static void to_json(BasicJsonType &j, const glm::ivec2 &v)
        {
            j = BasicJsonType::array({ v.x, v.y });
        }
        template <typename BasicJsonType>
        static void from_json(const BasicJsonType &j, glm::ivec2 &v)
        {
            if (j.is_array() && j.size() >= 2)
            {
                v.x = j[0].template get<int>();
                v.y = j[1].template get<int>();
            }
        }
    };

    // glm::ivec3
    template <>
    struct adl_serializer<glm::ivec3>
    {
        template <typename BasicJsonType>
        static void to_json(BasicJsonType &j, const glm::ivec3 &v)
        {
            j = BasicJsonType::array({ v.x, v.y, v.z });
        }
        template <typename BasicJsonType>
        static void from_json(const BasicJsonType &j, glm::ivec3 &v)
        {
            if (j.is_array() && j.size() >= 3)
            {
                v.x = j[0].template get<int>();
                v.y = j[1].template get<int>();
                v.z = j[2].template get<int>();
            }
        }
    };

    // glm::ivec4
    template <>
    struct adl_serializer<glm::ivec4>
    {
        template <typename BasicJsonType>
        static void to_json(BasicJsonType &j, const glm::ivec4 &v)
        {
            j = BasicJsonType::array({ v.x, v.y, v.z, v.w });
        }
        template <typename BasicJsonType>
        static void from_json(const BasicJsonType &j, glm::ivec4 &v)
        {
            if (j.is_array() && j.size() >= 4)
            {
                v.x = j[0].template get<int>();
                v.y = j[1].template get<int>();
                v.z = j[2].template get<int>();
                v.w = j[3].template get<int>();
            }
        }
    };

    // glm::quat
    template <>
    struct adl_serializer<glm::quat>
    {
        template <typename BasicJsonType>
        static void to_json(BasicJsonType &j, const glm::quat &q)
        {
            j = BasicJsonType::array({ q.x, q.y, q.z, q.w });
        }
        template <typename BasicJsonType>
        static void from_json(const BasicJsonType &j, glm::quat &q)
        {
            if (j.is_array() && j.size() >= 4)
            {
                q.x = j[0].template get<float>();
                q.y = j[1].template get<float>();
                q.z = j[2].template get<float>();
                q.w = j[3].template get<float>();
            }
        }
    };

    // ignite::UUID
    template <>
    struct adl_serializer<ignite::UUID>
    {
        template <typename BasicJsonType>
        static void to_json(BasicJsonType &j, const ignite::UUID &uuid)
        {
            j = static_cast<uint64_t>(uuid);
        }
        template <typename BasicJsonType>
        static void from_json(const BasicJsonType &j, ignite::UUID &uuid)
        {
            if (j.is_number())
                uuid = ignite::UUID(j.template get<uint64_t>());
        }
    };

    // ignite::Rect
    template <>
    struct adl_serializer<ignite::Rect>
    {
        template <typename BasicJsonType>
        static void to_json(BasicJsonType &j, const ignite::Rect &rect)
        {
            j = BasicJsonType::array({ rect.min.x, rect.min.y, rect.max.x, rect.max.y });
        }
        template <typename BasicJsonType>
        static void from_json(const BasicJsonType &j, ignite::Rect &rect)
        {
            if (j.is_array() && j.size() >= 4)
            {
                rect.SetMin({ j[0].template get<float>(), j[1].template get<float>() });
                rect.SetMax({ j[2].template get<float>(), j[3].template get<float>() });
            }
        }
    };
}

namespace glm
{
    inline void to_json(nlohmann::ordered_json &j, const vec2 &v) { j = nlohmann::ordered_json::array({ v.x, v.y }); }
    inline void from_json(const nlohmann::ordered_json &j, vec2 &v) { if (j.is_array() && j.size() >= 2) { v.x = j[0].get<float>(); v.y = j[1].get<float>(); } }

    inline void to_json(nlohmann::ordered_json &j, const vec3 &v) { j = nlohmann::ordered_json::array({ v.x, v.y, v.z }); }
    inline void from_json(const nlohmann::ordered_json &j, vec3 &v) { if (j.is_array() && j.size() >= 3) { v.x = j[0].get<float>(); v.y = j[1].get<float>(); v.z = j[2].get<float>(); } }

    inline void to_json(nlohmann::ordered_json &j, const vec4 &v) { j = nlohmann::ordered_json::array({ v.x, v.y, v.z, v.w }); }
    inline void from_json(const nlohmann::ordered_json &j, vec4 &v) { if (j.is_array() && j.size() >= 4) { v.x = j[0].get<float>(); v.y = j[1].get<float>(); v.z = j[2].get<float>(); v.w = j[3].get<float>(); } }

    inline void to_json(nlohmann::ordered_json &j, const ivec2 &v) { j = nlohmann::ordered_json::array({ v.x, v.y }); }
    inline void from_json(const nlohmann::ordered_json &j, ivec2 &v) { if (j.is_array() && j.size() >= 2) { v.x = j[0].get<int>(); v.y = j[1].get<int>(); } }

    inline void to_json(nlohmann::ordered_json &j, const ivec3 &v) { j = nlohmann::ordered_json::array({ v.x, v.y, v.z }); }
    inline void from_json(const nlohmann::ordered_json &j, ivec3 &v) { if (j.is_array() && j.size() >= 3) { v.x = j[0].get<int>(); v.y = j[1].get<int>(); v.z = j[2].get<int>(); } }

    inline void to_json(nlohmann::ordered_json &j, const ivec4 &v) { j = nlohmann::ordered_json::array({ v.x, v.y, v.z, v.w }); }
    inline void from_json(const nlohmann::ordered_json &j, ivec4 &v) { if (j.is_array() && j.size() >= 4) { v.x = j[0].get<int>(); v.y = j[1].get<int>(); v.z = j[2].get<int>(); v.w = j[3].get<int>(); } }

    inline void to_json(nlohmann::ordered_json &j, const quat &q) { j = nlohmann::ordered_json::array({ q.x, q.y, q.z, q.w }); }
    inline void from_json(const nlohmann::ordered_json &j, quat &q) { if (j.is_array() && j.size() >= 4) { q.x = j[0].get<float>(); q.y = j[1].get<float>(); q.z = j[2].get<float>(); q.w = j[3].get<float>(); } }
}

namespace ignite
{
    inline void to_json(nlohmann::ordered_json &j, const UUID &uuid) { j = static_cast<uint64_t>(uuid); }
    inline void from_json(const nlohmann::ordered_json &j, UUID &uuid) { if (j.is_number()) uuid = UUID(j.get<uint64_t>()); }

    inline void to_json(nlohmann::ordered_json &j, const Rect &rect) { j = nlohmann::ordered_json::array({ rect.min.x, rect.min.y, rect.max.x, rect.max.y }); }
    inline void from_json(const nlohmann::ordered_json &j, Rect &rect) { if (j.is_array() && j.size() >= 4) { rect.SetMin({ j[0].get<float>(), j[1].get<float>() }); rect.SetMax({ j[2].get<float>(), j[3].get<float>() }); } }

    class IGN_API JsonNode
    {
    public:
        JsonNode() : m_Storage(nullptr), m_Json(nullptr) {}

        JsonNode(std::shared_ptr<nlohmann::ordered_json> storage)
            : m_Storage(std::move(storage)), m_Json(m_Storage ? m_Storage.get() : nullptr) {}

        JsonNode(nlohmann::ordered_json &&j)
            : m_Storage(std::make_shared<nlohmann::ordered_json>(std::move(j)))
        {
            m_Json = m_Storage.get();
        }

        JsonNode(const nlohmann::ordered_json &j)
            : m_Storage(nullptr), m_Json(&j) {}

        JsonNode(const nlohmann::ordered_json *j, std::shared_ptr<nlohmann::ordered_json> storage)
            : m_Storage(std::move(storage)), m_Json(j) {}

        static JsonNode Parse(const std::string &jsonString)
        {
            try
            {
                auto storage = std::make_shared<nlohmann::ordered_json>(nlohmann::ordered_json::parse(jsonString));
                return JsonNode(std::move(storage));
            }
            catch (const std::exception &e)
            {
                LOG_ERROR("[JsonNode] Failed to parse JSON string: {}", e.what());
                return JsonNode();
            }
        }

        bool IsValid() const { return m_Json != nullptr && !m_Json->is_null(); }
        explicit operator bool() const { return IsValid(); }

        bool operator!() const { return !IsValid(); }

        JsonNode operator[](const std::string &key) const
        {
            if (m_Json && m_Json->is_object())
            {
                auto it = m_Json->find(key);
                if (it != m_Json->end() && !it->is_null())
                {
                    return JsonNode(&(*it), m_Storage);
                }
            }
            return JsonNode();
        }

        JsonNode operator[](const char *key) const
        {
            if (!key) return JsonNode();
            return operator[](std::string(key));
        }

        JsonNode operator[](size_t index) const
        {
            if (m_Json && m_Json->is_array() && index < m_Json->size())
            {
                return JsonNode(&(*m_Json)[index], m_Storage);
            }
            return JsonNode();
        }

        JsonNode operator[](int index) const
        {
            if (index >= 0)
                return operator[](static_cast<size_t>(index));
            return JsonNode();
        }

        template <typename T>
        T as() const
        {
            if (m_Json && !m_Json->is_null())
            {
                try
                {
                    return m_Json->get<T>();
                }
                catch (const std::exception &e)
                {
                    LOG_WARN("[JsonNode] Failed to get value: {}", e.what());
                }
            }
            return T();
        }

        template <typename T>
        T as(const T &defaultValue) const
        {
            if (m_Json && !m_Json->is_null())
            {
                try
                {
                    return m_Json->get<T>();
                }
                catch (...)
                {
                    return defaultValue;
                }
            }
            return defaultValue;
        }

        bool IsSequence() const { return m_Json && m_Json->is_array(); }
        bool IsMap() const { return m_Json && m_Json->is_object(); }
        bool IsScalar() const { return m_Json && !m_Json->is_structured() && !m_Json->is_null(); }
        bool IsNull() const { return m_Json == nullptr || m_Json->is_null(); }
        size_t size() const { return m_Json ? m_Json->size() : 0; }

        struct Iterator
        {
            using BaseIter = nlohmann::ordered_json::const_iterator;
            BaseIter iter;
            std::shared_ptr<nlohmann::ordered_json> storage;

            Iterator(BaseIter it, std::shared_ptr<nlohmann::ordered_json> s)
                : iter(it), storage(std::move(s)) {}

            JsonNode operator*() const { return JsonNode(&(*iter), storage); }
            Iterator &operator++() { ++iter; return *this; }
            bool operator!=(const Iterator &other) const { return iter != other.iter; }
            bool operator==(const Iterator &other) const { return iter == other.iter; }
        };

        Iterator begin() const
        {
            if (m_Json && (m_Json->is_array() || m_Json->is_object()))
                return Iterator(m_Json->begin(), m_Storage);
            return Iterator(nlohmann::ordered_json::const_iterator(), nullptr);
        }

        Iterator end() const
        {
            if (m_Json && (m_Json->is_array() || m_Json->is_object()))
                return Iterator(m_Json->end(), m_Storage);
            return Iterator(nlohmann::ordered_json::const_iterator(), nullptr);
        }

        const nlohmann::ordered_json &GetJson() const
        {
            static const nlohmann::ordered_json kNullJson = nullptr;
            return m_Json ? *m_Json : kNullJson;
        }

        operator const nlohmann::ordered_json &() const { return GetJson(); }

    private:
        std::shared_ptr<nlohmann::ordered_json> m_Storage;
        const nlohmann::ordered_json *m_Json = nullptr;
    };

    class IGN_API Serializer
    {
    public:
        explicit Serializer(const std::filesystem::path &filepath);

        void Serialize() const;
        void Serialize(const std::filesystem::path &filepath);

        void BeginMap();
        void BeginMap(const std::string &mapName);
        void EndMap();

        void BeginSequence();
        void BeginSequence(const std::string &sequenceName);
        void EndSequence();

        template<typename T>
        void AddKeyValue(const char *keyName, const T &value)
        {
            if (!m_Stack.empty() && m_Stack.back()->is_object())
            {
                (*m_Stack.back())[keyName] = value;
            }
        }

        template<typename T>
        void AddValue(const T &value)
        {
            if (!m_Stack.empty() && m_Stack.back()->is_array())
            {
                m_Stack.back()->push_back(value);
            }
        }

        static JsonNode Deserialize(const std::filesystem::path &filepath);
        static JsonNode DeserializeFromString(const std::string &jsonString) { return JsonNode::Parse(jsonString); }

        const std::filesystem::path &GetFilepath() const { return m_Filepath; }
        const nlohmann::ordered_json &GetRoot() const { return m_Root; }
        nlohmann::ordered_json &GetRoot() { return m_Root; }

        static void SerializeMat4(Serializer &sr, const char *key, const glm::mat4 &mat)
        {
            sr.BeginSequence(key);
            for (int col = 0; col < 4; ++col)
            {
                sr.AddValue(glm::vec4(mat[col]));
            }
            sr.EndSequence();
        }

        static bool DeserializeMat4(const JsonNode &node, const char *key, glm::mat4 &outMat)
        {
            const JsonNode matNode = node[key];
            if (!matNode || !matNode.IsSequence() || matNode.size() != 4)
            {
                return false;
            }

            for (size_t col = 0; col < 4; ++col)
            {
                const glm::vec4 v = matNode[col].as<glm::vec4>();
                outMat[static_cast<int>(col)] = v;
            }

            return true;
        }

    private:
        nlohmann::ordered_json m_Root;
        std::vector<nlohmann::ordered_json *> m_Stack;
        std::filesystem::path m_Filepath;
    };
}

#endif
