// Copyright (c) 2026 Evangelion Manuhutu

#include "ignite_pch.hpp"

#include "serializer.hpp"

#include "ignite/asset/asset_importer.hpp"
#include "ignite/scene/scene.hpp"

namespace ignite
{    
    Serializer::Serializer(const ignite::Path &filepath)
        : m_Filepath(filepath)
    {
    }

    void Serializer::Serialize() const
    {
        std::ofstream outFile(m_Filepath);

		LOG_INFO("[Serializer] Serialized to {}", m_Filepath);

        outFile << m_Emitter.c_str();
        outFile.close();
    }

    void Serializer::Serialize(const ignite::Path &filepath)
    {
        m_Filepath = filepath;

        LOG_INFO("[Serializer] Serialized to {}", filepath);

        std::ofstream outFile(m_Filepath);
        outFile << m_Emitter.c_str();
        outFile.close();
    }

    void Serializer::BeginMap(const std::string &mapName)
    {
        m_Emitter << YAML::Key << mapName;
        m_Emitter << YAML::BeginMap;
    }

    void Serializer::BeginMap()
    {
        m_Emitter << YAML::BeginMap;
    }

    void Serializer::EndMap()
    {
        m_Emitter << YAML::EndMap;
    }

    void Serializer::BeginSequence()
    {
        m_Emitter << YAML::BeginSeq;
    }

    void Serializer::BeginSequence(const std::string &sequenceName)
    {
        m_Emitter << YAML::Key << sequenceName << YAML::Value << YAML::BeginSeq;
    }

    void Serializer::EndSequence()
    {
        m_Emitter << YAML::EndSeq;
    }

    YAML::Node Serializer::Deserialize(const ignite::Path &filepath)
    {
        std::ifstream inFile(filepath);
        std::stringstream buffer;
        buffer << inFile.rdbuf();
        inFile.close();
        return YAML::Load(buffer.str());
    }
}
