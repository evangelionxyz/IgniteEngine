// Copyright (c) 2026 Evangelion Manuhutu

#ifndef IGN_SCRIPTABLE_OBJECT_HPP
#define IGN_SCRIPTABLE_OBJECT_HPP

#include "ignite/asset/asset.hpp"
#include "script_field.hpp"

#include "ignite/core/base.hpp"
#include <string>
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

    class IGN_API ScriptableObject : public Asset
    {
    public:
        ScriptableObject() = default;
        explicit ScriptableObject(const std::string &className);

        virtual AssetType GetAssetType() override { return AssetType::ScriptableObject; }
        virtual bool Serialize(const std::filesystem::path &filepath) override;

        static Ref<ScriptableObject> Deserialize(const std::filesystem::path &filepath);
        static Ref<ScriptableObject> Create(const std::string &className);

        // The fully-qualified C# class name (e.g. "MyProject.MyData")
        const std::string &GetClassName() const { return m_ClassName; }
        void SetClassName(const std::string &name) { m_ClassName = name; }

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
