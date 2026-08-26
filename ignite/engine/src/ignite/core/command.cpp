// Copyright (c) 2026 Evangelion Manuhutu

#include "ignite_pch.hpp"
#include "command.hpp"
#include <queue>

namespace ignite
{
    static CommandManager *s_CmdInstance = nullptr;
    
    CommandManager::CommandManager()
    {
        s_CmdInstance = this;
    }

    CommandManager *CommandManager::GetInstance()
    {
        return s_CmdInstance;
    }
}
