// Copyright (c) 2026 Evangelion Manuhutu

#ifndef SCRIPTABLE_OBJECT_HPP
#define SCRIPTABLE_OBJECT_HPP

#include "ignite/asset/asset.hpp"
#include "script_field.hpp"

#include <string>
#include "ignite/core/path.hpp"
#include <unordered_map>
#include <vector>

namespace ignite
{
    // Describes a CreateAssetMenu attribute found on a C# class
    struct ScriptableObjectMenuEntry
    {
        std::string className;   // e.g. "MyProject.MyData"
        std::string fileName;    // default file name, e.g. "MyData"
        std::string menuName;    // menu path, e.g. "Game/My Data"
    };

    // C++ side representation of a .ixso asset.
    // Stores: which C# class to instantiate + serialized field values.
    class ScriptableObject : public Asset
    {
    public:
        ScriptableObject() = default;
        explicit ScriptableObject(const std::string &className);

        // ---- Asset interface ----
        virtual AssetType GetAssetType() override { return AssetType::ScriptableObject; }
        virtual bool Serialize(const ignite::Path &filepath) override;

        static Ref<ScriptableObject> Deserialize(const ignite::Path &filepath);
        static Ref<ScriptableObject> Create(const std::string &className);

        // The fully-qualified C# class name (e.g. "MyProject.MyData")
        const std::string &GetClassName() const { return m_ClassName; }
        void SetClassName(const std::string &name) { m_ClassName = name; }

        // Serialized field values keyed by field name
        std::unordered_map<std::string, ScriptInstanceField> &GetFields() { return m_Fields; }
        const std::unordered_map<std::string, ScriptInstanceField> &GetFields() const { return m_Fields; }

        void SetField(const std::string &name, const ScriptInstanceField &field)
        {
            m_Fields[name] = field;
        }

    private:
        std::string m_ClassName;
        std::unordered_map<std::string, ScriptInstanceField> m_Fields; // fieldName -> value
    };
}

#endif