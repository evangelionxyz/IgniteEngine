// Copyright (c) 2026 Evangelion Manuhutu

#include "ignite_pch.hpp"
#include "command.hpp"

#include <algorithm>

namespace ignite
{
    namespace
    {
        struct ScopedBoolGuard
        {
            bool &flag;
            explicit ScopedBoolGuard(bool &f) : flag(f) { flag = true; }
            ~ScopedBoolGuard() { flag = false; }
        };
    }

    static CommandManager *s_CmdInstance = nullptr;

    CommandHistory::CommandHistory(size_t maxHistory)
        : m_MaxHistory(std::max<size_t>(1, maxHistory))
    {
    }

    void CommandHistory::AddCommand(Scope<ICommand> command)
    {
        if (!command || m_IsExecutingUndoRedo)
            return;

        if (!m_TransactionStack.empty())
        {
            m_TransactionStack.back()->Add(std::move(command));
            return;
        }

        m_UndoStack.push_back(std::move(command));
        if (!m_RedoStack.empty())
        {
            if (m_SavedRevision > m_CurrentRevision)
            {
                m_SavedRevision = -1;
            }
            m_RedoStack.clear();
        }

        ++m_CurrentRevision;
        TrimUndoStack();
    }

    void CommandHistory::ExecuteCommand(Scope<ICommand> command)
    {
        if (!command || m_IsExecutingUndoRedo)
            return;

        command->Execute();
        AddCommand(std::move(command));
    }

    void CommandHistory::BeginTransaction(std::string name)
    {
        if (m_IsExecutingUndoRedo)
            return;

        m_TransactionStack.push_back(CreateScope<CompoundCommand>(std::move(name)));
    }

    void CommandHistory::EndTransaction()
    {
        if (m_IsExecutingUndoRedo || m_TransactionStack.empty())
            return;

        Scope<CompoundCommand> completed = std::move(m_TransactionStack.back());
        m_TransactionStack.pop_back();

        if (!completed || completed->Empty())
            return;

        AddCommand(std::move(completed));
    }

    void CommandHistory::CancelTransaction()
    {
        if (!m_TransactionStack.empty())
        {
            m_TransactionStack.pop_back();
        }
    }

    bool CommandHistory::Undo()
    {
        if (m_UndoStack.empty() || m_IsExecutingUndoRedo)
            return false;

        ScopedBoolGuard guard(m_IsExecutingUndoRedo);
        auto command = std::move(m_UndoStack.back());
        m_UndoStack.pop_back();

        if (command)
        {
            command->Undo();
            m_RedoStack.push_back(std::move(command));
            --m_CurrentRevision;
            return true;
        }

        return false;
    }

    bool CommandHistory::Redo()
    {
        if (m_RedoStack.empty() || m_IsExecutingUndoRedo)
            return false;

        ScopedBoolGuard guard(m_IsExecutingUndoRedo);
        auto command = std::move(m_RedoStack.back());
        m_RedoStack.pop_back();

        if (command)
        {
            command->Execute();
            m_UndoStack.push_back(std::move(command));
            ++m_CurrentRevision;
            TrimUndoStack();
            return true;
        }

        return false;
    }

    void CommandHistory::Clear()
    {
        m_UndoStack.clear();
        m_RedoStack.clear();
        m_TransactionStack.clear();
        m_CurrentRevision = 0;
        m_SavedRevision = 0;
    }

    std::string CommandHistory::GetUndoName() const
    {
        if (m_UndoStack.empty() || !m_UndoStack.back())
            return {};
        return m_UndoStack.back()->GetName();
    }

    std::string CommandHistory::GetRedoName() const
    {
        if (m_RedoStack.empty() || !m_RedoStack.back())
            return {};
        return m_RedoStack.back()->GetName();
    }

    void CommandHistory::SetMaxHistory(size_t maxHistory)
    {
        m_MaxHistory = std::max<size_t>(1, maxHistory);
        TrimUndoStack();
    }

    size_t CommandHistory::GetEstimatedMemoryUsage() const
    {
        size_t total = sizeof(*this);
        total += (m_UndoStack.size() + m_RedoStack.size()) * sizeof(Scope<ICommand>);
        total += m_TransactionStack.capacity() * sizeof(Scope<CompoundCommand>);

        for (const auto &cmd : m_UndoStack)
        {
            if (cmd)
                total += cmd->GetMemoryUsage();
        }
        for (const auto &cmd : m_RedoStack)
        {
            if (cmd)
                total += cmd->GetMemoryUsage();
        }
        for (const auto &tx : m_TransactionStack)
        {
            if (tx)
                total += tx->GetMemoryUsage();
        }

        return total;
    }

    void CommandHistory::TrimUndoStack()
    {
        while (m_UndoStack.size() > m_MaxHistory)
        {
            m_UndoStack.pop_front();
        }
    }

    CommandManager::CommandManager()
        : m_MaxHistory(200)
        , m_SceneHistory(200)
    {
        s_CmdInstance = this;
    }

    CommandManager::~CommandManager()
    {
        if (s_CmdInstance == this)
        {
            s_CmdInstance = nullptr;
        }
    }

    CommandManager *CommandManager::GetInstance()
    {
        return s_CmdInstance;
    }

    void CommandManager::AddCommand(Scope<ICommand> command)
    {
        if (auto *inst = GetInstance())
        {
            inst->m_SceneHistory.AddCommand(std::move(command));
        }
    }

    void CommandManager::ExecuteCommand(Scope<ICommand> command)
    {
        if (auto *inst = GetInstance())
        {
            inst->m_SceneHistory.ExecuteCommand(std::move(command));
        }
        else if (command)
        {
            command->Execute();
        }
    }

    void CommandManager::BeginTransaction(const std::string &name)
    {
        if (auto *inst = GetInstance())
        {
            inst->m_SceneHistory.BeginTransaction(name);
        }
    }

    void CommandManager::EndTransaction()
    {
        if (auto *inst = GetInstance())
        {
            inst->m_SceneHistory.EndTransaction();
        }
    }

    bool CommandManager::IsExecutingUndoRedo()
    {
        if (auto *inst = GetInstance())
        {
            if (inst->m_SceneHistory.IsExecutingUndoRedo())
                return true;

            for (const auto &[_, history] : inst->m_AssetHistories)
            {
                if (history && history->IsExecutingUndoRedo())
                    return true;
            }
        }
        return false;
    }

    CommandHistory *CommandManager::GetOrCreateAssetHistory(uint64_t assetKey)
    {
        if (assetKey == 0)
            return nullptr;

        auto &entry = m_AssetHistories[assetKey];
        if (!entry)
        {
            entry = CreateScope<CommandHistory>(m_MaxHistory);
        }
        return entry.get();
    }

    CommandHistory *CommandManager::FindAssetHistory(uint64_t assetKey)
    {
        auto it = m_AssetHistories.find(assetKey);
        return (it != m_AssetHistories.end()) ? it->second.get() : nullptr;
    }

    const CommandHistory *CommandManager::FindAssetHistory(uint64_t assetKey) const
    {
        auto it = m_AssetHistories.find(assetKey);
        return (it != m_AssetHistories.end()) ? it->second.get() : nullptr;
    }

    void CommandManager::RemoveAssetHistory(uint64_t assetKey)
    {
        m_AssetHistories.erase(assetKey);
    }

    void CommandManager::ClearSceneHistory()
    {
        m_SceneHistory.Clear();
    }

    void CommandManager::ClearAllAssetHistories()
    {
        m_AssetHistories.clear();
    }

    void CommandManager::ClearAll()
    {
        m_SceneHistory.Clear();
        m_AssetHistories.clear();
    }

    void CommandManager::SetMaxHistory(size_t maxHistory)
    {
        m_MaxHistory = std::max<size_t>(1, maxHistory);
        m_SceneHistory.SetMaxHistory(m_MaxHistory);
        for (auto &[_, history] : m_AssetHistories)
        {
            if (history)
            {
                history->SetMaxHistory(m_MaxHistory);
            }
        }
    }

    size_t CommandManager::GetSceneEstimatedMemoryUsage() const
    {
        return m_SceneHistory.GetEstimatedMemoryUsage();
    }

    size_t CommandManager::GetAssetHistoriesEstimatedMemoryUsage() const
    {
        size_t total = 0;
        for (const auto &[_, history] : m_AssetHistories)
        {
            if (history)
            {
                total += history->GetEstimatedMemoryUsage();
            }
        }
        return total;
    }

    size_t CommandManager::GetTotalEstimatedMemoryUsage() const
    {
        return sizeof(*this) + GetSceneEstimatedMemoryUsage() + GetAssetHistoriesEstimatedMemoryUsage();
    }

    size_t CommandManager::GetTotalAssetCommandCount() const
    {
        size_t total = 0;
        for (const auto &[_, history] : m_AssetHistories)
        {
            if (history)
            {
                total += history->GetUndoCount() + history->GetRedoCount();
            }
        }
        return total;
    }
}
