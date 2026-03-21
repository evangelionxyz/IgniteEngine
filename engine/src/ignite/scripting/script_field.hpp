// Copyright (c) 2026 Evangelion Manuhutu

#ifndef SCRIPT_FIELD_HPP
#define SCRIPT_FIELD_HPP

#include <string>

namespace ignite
{
    enum class ScriptFieldType
    {
        Invalid = 0,
        Entity,
        Float, 
        Double,
        Bool, 
        Char,
        Byte, 
        Short, 
        Int, 
        Long,
        UByte, 
        UShort, 
        UInt, 
        ULong,
        Vector2, 
        Vector3, 
        Vector4
    };

    struct ScriptField
    {
        ScriptFieldType Type = ScriptFieldType::Invalid;
        std::string Name;
    };
}

#endif
