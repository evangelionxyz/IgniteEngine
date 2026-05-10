// Copyright (c) 2026 Evangelion Manuhutu

#include "ignite_pch.hpp"

#include "scriptable_object.hpp"
#include "script_engine.hpp"
#include "ignite/serializer/serializer.hpp"
#include "ignite/core/logger.hpp"

#include <glm/glm.hpp>

namespace ignite
{
    // -------------------------------------------------------------------------
    // Helpers to serialize a ScriptInstanceField value to/from YAML
    // -------------------------------------------------------------------------
    static void SerializeFieldValue(Serializer &sr, const std::string &name, const ScriptField &fieldDef, const ScriptInstanceField &field)
    {
        switch (fieldDef.Type)
        {
        case ScriptFieldType::Bool:
            sr.AddKeyValue(name.c_str(), field.GetValue<bool>());
            break;
        case ScriptFieldType::Char:
            sr.AddKeyValue(name.c_str(), static_cast<uint16_t>(field.GetValue<char16_t>()));
            break;
        case ScriptFieldType::Byte:
            sr.AddKeyValue(name.c_str(), static_cast<int>(field.GetValue<uint8_t>()));
            break;
        case ScriptFieldType::SByte:
            sr.AddKeyValue(name.c_str(), static_cast<int>(field.GetValue<int8_t>()));
            break;
        case ScriptFieldType::Short:
            sr.AddKeyValue(name.c_str(), static_cast<int>(field.GetValue<int16_t>()));
            break;
        case ScriptFieldType::UShort:
            sr.AddKeyValue(name.c_str(), static_cast<int>(field.GetValue<uint16_t>()));
            break;
        case ScriptFieldType::Int:
            sr.AddKeyValue(name.c_str(), field.GetValue<int32_t>());
            break;
        case ScriptFieldType::UInt:
            sr.AddKeyValue(name.c_str(), field.GetValue<uint32_t>());
            break;
        case ScriptFieldType::Long:
            sr.AddKeyValue(name.c_str(), field.GetValue<int64_t>());
            break;
        case ScriptFieldType::ULong:
            sr.AddKeyValue(name.c_str(), field.GetValue<uint64_t>());
            break;
        case ScriptFieldType::Float:
            sr.AddKeyValue(name.c_str(), field.GetValue<float>());
            break;
        case ScriptFieldType::Double:
            sr.AddKeyValue(name.c_str(), field.GetValue<double>());
            break;
        case ScriptFieldType::String:
            sr.AddKeyValue(name.c_str(), field.GetValue<std::string>());
            break;
        case ScriptFieldType::Vector2:
            sr.AddKeyValue(name.c_str(), field.GetValue<glm::vec2>());
            break;
        case ScriptFieldType::Vector3:
            sr.AddKeyValue(name.c_str(), field.GetValue<glm::vec3>());
            break;
        case ScriptFieldType::Vector4:
        case ScriptFieldType::Color:
            sr.AddKeyValue(name.c_str(), field.GetValue<glm::vec4>());
            break;
        case ScriptFieldType::Quat:
            sr.AddKeyValue(name.c_str(), field.GetValue<glm::quat>());
            break;
        case ScriptFieldType::Entity:
        case ScriptFieldType::Asset:
            sr.AddKeyValue(name.c_str(), field.GetValue<uint64_t>());
            break;
        case ScriptFieldType::Enum:
            sr.AddKeyValue(name.c_str(), field.GetValue<int32_t>());
            break;
        default:
            break;
        }
    }

    static void DeserializeFieldValue(const YAML::Node &node, const std::string &name, ScriptField &fieldDef, ScriptInstanceField &outField)
    {
        if (!node[name]) return;
        const auto &valueNode = node[name];

        outField.field = fieldDef;
        try
        {
            switch (fieldDef.Type)
            {
            case ScriptFieldType::Bool:
                outField.SetValue<bool>(valueNode.as<bool>());
                break;
            case ScriptFieldType::Char:
                outField.SetValue<char16_t>(static_cast<char16_t>(valueNode.as<uint16_t>()));
                break;
            case ScriptFieldType::Byte:
                outField.SetValue<uint8_t>(static_cast<uint8_t>(valueNode.as<int>()));
                break;
            case ScriptFieldType::SByte:
                outField.SetValue<int8_t>(static_cast<int8_t>(valueNode.as<int>()));
                break;
            case ScriptFieldType::Short:
                outField.SetValue<int16_t>(static_cast<int16_t>(valueNode.as<int>()));
                break;
            case ScriptFieldType::UShort:
                outField.SetValue<uint16_t>(static_cast<uint16_t>(valueNode.as<int>()));
                break;
            case ScriptFieldType::Int:
                outField.SetValue<int32_t>(valueNode.as<int32_t>());
                break;
            case ScriptFieldType::UInt:
                outField.SetValue<uint32_t>(valueNode.as<uint32_t>());
                break;
            case ScriptFieldType::Long:
                outField.SetValue<int64_t>(valueNode.as<int64_t>());
                break;
            case ScriptFieldType::ULong:
                outField.SetValue<uint64_t>(valueNode.as<uint64_t>());
                break;
            case ScriptFieldType::Float:
                outField.SetValue<float>(valueNode.as<float>());
                break;
            case ScriptFieldType::Double:
                outField.SetValue<double>(valueNode.as<double>());
                break;
            case ScriptFieldType::String:
                outField.SetValue<std::string>(valueNode.as<std::string>());
                break;
            case ScriptFieldType::Vector2:
                outField.SetValue<glm::vec2>(valueNode.as<glm::vec2>());
                break;
            case ScriptFieldType::Vector3:
                outField.SetValue<glm::vec3>(valueNode.as<glm::vec3>());
                break;
            case ScriptFieldType::Vector4:
            case ScriptFieldType::Color:
                outField.SetValue<glm::vec4>(valueNode.as<glm::vec4>());
                break;
            case ScriptFieldType::Quat:
                outField.SetValue<glm::quat>(valueNode.as<glm::quat>());
                break;
            case ScriptFieldType::Entity:
            case ScriptFieldType::Asset:
                outField.SetValue<uint64_t>(valueNode.as<uint64_t>());
                break;
            case ScriptFieldType::Enum:
                outField.SetValue<int32_t>(valueNode.as<int32_t>());
                break;
            default:
                break;
            }
        }
        catch (...)
        {
            LOG_WARN("[ScriptableObject] Failed to deserialize field '{}'", name);
        }
    }

    // -------------------------------------------------------------------------

    ScriptableObject::ScriptableObject(const std::string &className)
        : m_ClassName(className)
    {
    }

    Ref<ScriptableObject> ScriptableObject::Create(const std::string &className)
    {
        return CreateRef<ScriptableObject>(className);
    }

    bool ScriptableObject::Serialize(const ignite::Path &filepath)
    {
        Serializer sr(filepath);

        sr.BeginMap(); // Root

        sr.BeginMap("ScriptableObject");
        sr.AddKeyValue("ClassName", m_ClassName);

        // Retrieve field definitions from the script engine to know each field's type
        ScriptEngine *engine = ScriptEngine::GetInstance();
        if (engine)
        {
            Ref<ScriptClass> scriptClass = engine->GetScriptableObjectClassByName(m_ClassName);
            if (scriptClass)
            {
                sr.BeginMap("Fields");
                for (const auto &fieldName : scriptClass->GetOrderedFieldNames())
                {
                    auto &fieldDefs = scriptClass->GetFields();
                    auto it = fieldDefs.find(fieldName);
                    if (it == fieldDefs.end()) continue;

                    const ScriptField &fieldDef = it->second;
                    auto fieldIt = m_Fields.find(fieldName);
                    if (fieldIt != m_Fields.end())
                    {
                        SerializeFieldValue(sr, fieldName, fieldDef, fieldIt->second);
                    }
                    else
                    {
                        // Write a default value
                        ScriptInstanceField defaultField;
                        defaultField.field = fieldDef;
                        SerializeFieldValue(sr, fieldName, fieldDef, defaultField);
                    }
                }
                sr.EndMap(); // Fields
            }
            else
            {
                // Class not found - still serialize whatever we have with raw types
                sr.BeginMap("Fields");
                for (auto &[fieldName, field] : m_Fields)
                {
                    SerializeFieldValue(sr, fieldName, field.field, field);
                }
                sr.EndMap(); // Fields
            }
        }
        else
        {
            sr.BeginMap("Fields");
            for (auto &[fieldName, field] : m_Fields)
            {
                SerializeFieldValue(sr, fieldName, field.field, field);
            }
            sr.EndMap(); // Fields
        }

        sr.EndMap(); // ScriptableObject

        sr.EndMap(); // Root
        sr.Serialize();

        SetDirtyFlag(false);
        return true;
    }

    Ref<ScriptableObject> ScriptableObject::Deserialize(const ignite::Path &filepath)
    {
        if (!ignite::Path::exists(filepath))
        {
            LOG_ERROR("[ScriptableObject] File does not exist: {}", filepath.generic_string());
            return nullptr;
        }

        YAML::Node root = Serializer::Deserialize(filepath);
        YAML::Node soNode = root["ScriptableObject"];
        if (!soNode)
        {
            LOG_ERROR("[ScriptableObject] Invalid .ixso file: {}", filepath.generic_string());
            return nullptr;
        }

        const std::string className = soNode["ClassName"].as<std::string>();
        auto so = CreateRef<ScriptableObject>(className);

        // Retrieve field definitions from script engine
        ScriptEngine *engine = ScriptEngine::GetInstance();
        Ref<ScriptClass> scriptClass = engine ? engine->GetScriptableObjectClassByName(className) : nullptr;

        const YAML::Node fieldsNode = soNode["Fields"];
        if (fieldsNode && fieldsNode.IsMap() && scriptClass)
        {
            for (auto &[fieldName, fieldDef] : scriptClass->GetFields())
            {
                if (!fieldsNode[fieldName])
                    continue;

                ScriptField fieldDefCopy = fieldDef;
                ScriptInstanceField instanceField;
                DeserializeFieldValue(fieldsNode, fieldName, fieldDefCopy, instanceField);
                so->m_Fields[fieldName] = instanceField;
            }
        }
        else if (fieldsNode && fieldsNode.IsMap())
        {
            // No script class found yet - store raw values as-is (will be resolved later)
            LOG_WARN("[ScriptableObject] Class '{}' not found in ScriptEngine. Fields will be empty.", className);
        }

        so->SetReadyFlag(true);
        so->SetDirtyFlag(false);
        return so;
    }
}
