// Copyright (c) 2026 Evangelion Manuhutu

#pragma once
#ifndef IGN_COMMAND_HPP
#define IGN_COMMAND_HPP

#include <cstdint>
#include <deque>
#include <functional>
#include <string>
#include <unordered_map>
#include <vector>

#include "ignite/core/base.hpp"
#include "ignite/core/buffer.hpp"
#include "ignite/core/types.hpp"

namespace ignite
{
    using CommandFunc = std::function<void()>;

    enum CommandState
    {
        CommandState_Create,
        CommandState_Destroy,
        CommandState_Renaming
    };

    class ICommand
    {
    public:
        virtual ~ICommand() = default;
        virtual void Execute() = 0;
        virtual void Undo() = 0;
        virtual std::string GetName() const { return "Command"; }
        virtual size_t GetMemoryUsage() const { return sizeof(*this); }
    };

    class LambdaCommand final : public ICommand
    {
    public:
        LambdaCommand(std::string name, CommandFunc executeFn, CommandFunc undoFn, size_t extraMemoryBytes = 0)
            : m_Name(std::move(name))
            , m_ExecuteFn(std::move(executeFn))
            , m_UndoFn(std::move(undoFn))
            , m_ExtraMemoryBytes(extraMemoryBytes)
        {
        }

        void Execute() override
        {
            if (m_ExecuteFn)
                m_ExecuteFn();
        }

        void Undo() override
        {
            if (m_UndoFn)
                m_UndoFn();
        }

        std::string GetName() const override
        {
            return m_Name.empty() ? "Command" : m_Name;
        }

        size_t GetMemoryUsage() const override
        {
            return sizeof(*this) + m_Name.capacity() + m_ExtraMemoryBytes;
        }

    private:
        std::string m_Name;
        CommandFunc m_ExecuteFn;
        CommandFunc m_UndoFn;
        size_t m_ExtraMemoryBytes = 0;
    };

    class CompoundCommand final : public ICommand
    {
    public:
        explicit CompoundCommand(std::string name = "Action")
            : m_Name(std::move(name))
        {
        }

        void Add(Scope<ICommand> cmd)
        {
            if (cmd)
                m_Commands.push_back(std::move(cmd));
        }

        bool Empty() const { return m_Commands.empty(); }
        size_t Size() const { return m_Commands.size(); }

        void Execute() override
        {
            for (const auto &cmd : m_Commands)
            {
                if (cmd)
                    cmd->Execute();
            }
        }

        void Undo() override
        {
            for (auto it = m_Commands.rbegin(); it != m_Commands.rend(); ++it)
            {
                if (*it)
                    (*it)->Undo();
            }
        }

        std::string GetName() const override
        {
            return m_Name.empty() ? "Action" : m_Name;
        }

        size_t GetMemoryUsage() const override
        {
            size_t total = sizeof(*this) + m_Name.capacity() + m_Commands.capacity() * sizeof(Scope<ICommand>);
            for (const auto &cmd : m_Commands)
            {
                if (cmd)
                    total += cmd->GetMemoryUsage();
            }
            return total;
        }

    private:
        std::string m_Name;
        std::vector<Scope<ICommand>> m_Commands;
    };

    class IGN_API CommandHistory
    {
    public:
        explicit CommandHistory(size_t maxHistory = 200);
        ~CommandHistory() = default;

        CommandHistory(const CommandHistory &) = delete;
        CommandHistory &operator=(const CommandHistory &) = delete;
        CommandHistory(CommandHistory &&) noexcept = default;
        CommandHistory &operator=(CommandHistory &&) noexcept = default;

        // Record a pre-executed command
        void AddCommand(Scope<ICommand> command);

        // Execute immediately, then record
        void ExecuteCommand(Scope<ICommand> command);

        void BeginTransaction(std::string name);
        void EndTransaction();
        void CancelTransaction();

        bool Undo();
        bool Redo();
        void Clear();

        bool CanUndo() const { return !m_UndoStack.empty(); }
        bool CanRedo() const { return !m_RedoStack.empty(); }
        bool IsExecutingUndoRedo() const { return m_IsExecutingUndoRedo; }

        size_t GetUndoCount() const { return m_UndoStack.size(); }
        size_t GetRedoCount() const { return m_RedoStack.size(); }

        std::string GetUndoName() const;
        std::string GetRedoName() const;

        void SetMaxHistory(size_t maxHistory);
        size_t GetMaxHistory() const { return m_MaxHistory; }

        void MarkSaved() { m_SavedRevision = m_CurrentRevision; }
        bool IsAtSavedRevision() const { return m_CurrentRevision == m_SavedRevision; }

        size_t GetEstimatedMemoryUsage() const;

        const std::deque<Scope<ICommand>> &GetUndoStack() const { return m_UndoStack; }
        const std::deque<Scope<ICommand>> &GetRedoStack() const { return m_RedoStack; }

    private:
        void TrimUndoStack();

        std::deque<Scope<ICommand>> m_UndoStack;
        std::deque<Scope<ICommand>> m_RedoStack;
        std::vector<Scope<CompoundCommand>> m_TransactionStack;

        size_t m_MaxHistory = 200;
        int64_t m_CurrentRevision = 0;
        int64_t m_SavedRevision = 0;
        bool m_IsExecutingUndoRedo = false;
    };

    class IGN_API CommandManager
    {
    public:
        CommandManager();
        ~CommandManager();

        static CommandManager *GetInstance();

        // Backwards-compatible static helpers targeting Scene history (null-safe)
        static void AddCommand(Scope<ICommand> command);
        static void ExecuteCommand(Scope<ICommand> command);
        static void BeginTransaction(const std::string &name);
        static void EndTransaction();
        static bool IsExecutingUndoRedo();

        CommandHistory &GetSceneHistory() { return m_SceneHistory; }
        const CommandHistory &GetSceneHistory() const { return m_SceneHistory; }

        CommandHistory *GetOrCreateAssetHistory(uint64_t assetKey);
        CommandHistory *FindAssetHistory(uint64_t assetKey);
        const CommandHistory *FindAssetHistory(uint64_t assetKey) const;
        void RemoveAssetHistory(uint64_t assetKey);

        void ClearSceneHistory();
        void ClearAllAssetHistories();
        void ClearAll();

        bool Undo() { return m_SceneHistory.Undo(); }
        bool Redo() { return m_SceneHistory.Redo(); }
        void Clear() { m_SceneHistory.Clear(); }

        bool CanUndo() const { return m_SceneHistory.CanUndo(); }
        bool CanRedo() const { return m_SceneHistory.CanRedo(); }

        void SetMaxStackSize(int size)
        {
            SetMaxHistory(size > 0 ? static_cast<size_t>(size) : 1);
        }

        void SetMaxHistory(size_t maxHistory);
        size_t GetMaxHistory() const { return m_MaxHistory; }

        size_t GetSceneEstimatedMemoryUsage() const;
        size_t GetAssetHistoriesEstimatedMemoryUsage() const;
        size_t GetTotalEstimatedMemoryUsage() const;
        size_t GetAssetHistoryCount() const { return m_AssetHistories.size(); }
        size_t GetTotalAssetCommandCount() const;

    private:
        size_t m_MaxHistory = 200;
        CommandHistory m_SceneHistory;
        std::unordered_map<uint64_t, Scope<CommandHistory>> m_AssetHistories;
    };

    class ScopedCommandTransaction
    {
    public:
        explicit ScopedCommandTransaction(std::string name, CommandHistory *history = nullptr)
            : m_History(history)
        {
            if (!m_History)
            {
                if (auto *mgr = CommandManager::GetInstance())
                {
                    m_History = &mgr->GetSceneHistory();
                }
            }
            if (m_History && !m_History->IsExecutingUndoRedo())
            {
                m_History->BeginTransaction(std::move(name));
                m_Active = true;
            }
        }

        ~ScopedCommandTransaction()
        {
            Commit();
        }

        void Commit()
        {
            if (m_Active && m_History)
            {
                m_History->EndTransaction();
                m_Active = false;
            }
        }

        void Cancel()
        {
            if (m_Active && m_History)
            {
                m_History->CancelTransaction();
                m_Active = false;
            }
        }

        ScopedCommandTransaction(const ScopedCommandTransaction &) = delete;
        ScopedCommandTransaction &operator=(const ScopedCommandTransaction &) = delete;

    private:
        CommandHistory *m_History = nullptr;
        bool m_Active = false;
    };
}

#endif
