// Copyright (c) 2026 Evangelion Manuhutu

#include "ignite_pch.hpp"

#include "serializer.hpp"

#include "ignite/asset/asset_importer.hpp"
#include "ignite/scene/scene.hpp"

namespace ignite
{    
    Serializer::Serializer(const std::filesystem::path &filepath)
        : m_Filepath(filepath)
    {
    }

    void Serializer::Serialize() const
    {
        std::ofstream outFile(m_Filepath);
        LOG_INFO("[Serializer] Serialized to {}", m_Filepath.string());
        outFile << m_Root.dump(4);
        outFile.close();
    }

    void Serializer::Serialize(const std::filesystem::path &filepath)
    {
        m_Filepath = filepath;
        LOG_INFO("[Serializer] Serialized to {}", filepath.string());
        std::ofstream outFile(m_Filepath);
        outFile << m_Root.dump(4);
        outFile.close();
    }

    void Serializer::BeginMap()
    {
        if (m_Stack.empty())
        {
            m_Root = nlohmann::ordered_json::object();
            m_Stack.push_back(&m_Root);
        }
        else if (m_Stack.back()->is_array())
        {
            m_Stack.back()->push_back(nlohmann::ordered_json::object());
            m_Stack.push_back(&m_Stack.back()->back());
        }
        else if (m_Stack.back()->is_object())
        {
            m_Stack.push_back(m_Stack.back());
        }
    }

    void Serializer::BeginMap(const std::string &mapName)
    {
        if (m_Stack.empty())
        {
            m_Root = nlohmann::ordered_json::object();
            m_Root[mapName] = nlohmann::ordered_json::object();
            m_Stack.push_back(&m_Root[mapName]);
        }
        else if (m_Stack.back()->is_object())
        {
            (*m_Stack.back())[mapName] = nlohmann::ordered_json::object();
            m_Stack.push_back(&(*m_Stack.back())[mapName]);
        }
        else if (m_Stack.back()->is_array())
        {
            nlohmann::ordered_json obj = nlohmann::ordered_json::object();
            obj[mapName] = nlohmann::ordered_json::object();
            m_Stack.back()->push_back(std::move(obj));
            m_Stack.push_back(&m_Stack.back()->back()[mapName]);
        }
    }

    void Serializer::EndMap()
    {
        if (!m_Stack.empty())
        {
            m_Stack.pop_back();
        }
    }

    void Serializer::BeginSequence()
    {
        if (m_Stack.empty())
        {
            m_Root = nlohmann::ordered_json::array();
            m_Stack.push_back(&m_Root);
        }
        else if (m_Stack.back()->is_array())
        {
            m_Stack.back()->push_back(nlohmann::ordered_json::array());
            m_Stack.push_back(&m_Stack.back()->back());
        }
    }

    void Serializer::BeginSequence(const std::string &sequenceName)
    {
        if (m_Stack.empty())
        {
            m_Root = nlohmann::ordered_json::object();
            m_Root[sequenceName] = nlohmann::ordered_json::array();
            m_Stack.push_back(&m_Root[sequenceName]);
        }
        else if (m_Stack.back()->is_object())
        {
            (*m_Stack.back())[sequenceName] = nlohmann::ordered_json::array();
            m_Stack.push_back(&(*m_Stack.back())[sequenceName]);
        }
    }

    void Serializer::EndSequence()
    {
        if (!m_Stack.empty())
        {
            m_Stack.pop_back();
        }
    }

    JsonNode Serializer::Deserialize(const std::filesystem::path &filepath)
    {
        if (!std::filesystem::exists(filepath))
        {
            LOG_ERROR("[Serializer] File does not exist: {}", filepath.string());
            return JsonNode();
        }

        std::ifstream inFile(filepath);
        if (!inFile.is_open())
        {
            LOG_ERROR("[Serializer] Could not open file: {}", filepath.string());
            return JsonNode();
        }

        try
        {
            auto storage = std::make_shared<nlohmann::ordered_json>(nlohmann::ordered_json::parse(inFile));
            return JsonNode(std::move(storage));
        }
        catch (const std::exception &e)
        {
            LOG_ERROR("[Serializer] Failed to parse JSON file '{}': {}", filepath.string(), e.what());
            return JsonNode();
        }
    }
}
