// Copyright (c) 2026 Evangelion Manuhutu

#pragma once
#ifndef IGN_CORE_SHARED_TASK_HPP
#define IGN_CORE_SHARED_TASK_HPP

#include <coroutine>
#include <exception>
#include <memory>
#include <mutex>
#include <optional>
#include <condition_variable>
#include <type_traits>
#include <utility>
#include <vector>

namespace ignite
{
    template<typename T>
    class shared_task
    {
    private:
        struct state
        {
            ~state()
            {
                if (coroutine)
                {
                    coroutine.destroy();
                }
            }

            void Complete() noexcept
            {
                std::vector<std::coroutine_handle<>> continuations;
                {
                    std::lock_guard lock(mutex);
                    completed = true;
                    continuations.swap(waiters);
                }

                for (const auto continuation : continuations)
                {
                    continuation.resume();
                }

                completion.notify_all();
            }

            std::mutex mutex;
            std::vector<std::coroutine_handle<>> waiters;
            std::coroutine_handle<> coroutine;
            std::optional<T> value;
            std::exception_ptr exception;
            std::condition_variable completion;
            bool started = false;
            bool completed = false;
        };

    public:
        struct promise_type
        {
            promise_type() : taskState(new state) {}

            shared_task get_return_object() noexcept
            {
                taskState->coroutine = std::coroutine_handle<promise_type>::from_promise(*this);
                return shared_task(std::shared_ptr<state>(taskState));
            }

        T Get()
        {
            bool startTask = false;
            {
                std::lock_guard lock(m_State->mutex);
                if (!m_State->completed && !m_State->started)
                {
                    m_State->started = true;
                    startTask = true;
                }
            }

            if (startTask)
            {
                m_State->coroutine.resume();
            }

            std::unique_lock lock(m_State->mutex);
            m_State->completion.wait(lock, [this] { return m_State->completed; });
            if (m_State->exception)
            {
                std::rethrow_exception(m_State->exception);
            }

            return *m_State->value;
        }

            std::suspend_always initial_suspend() const noexcept { return {}; }

            auto final_suspend() const noexcept
            {
                struct final_awaiter
                {
                    bool await_ready() const noexcept { return false; }

                    void await_suspend(std::coroutine_handle<promise_type> handle) const noexcept
                    {
                        handle.promise().taskState->Complete();
                    }

                    void await_resume() const noexcept {}
                };

                return final_awaiter{};
            }

            void unhandled_exception() noexcept
            {
                taskState->exception = std::current_exception();
            }

            template<typename Value>
            void return_value(Value &&value)
            {
                taskState->value.emplace(std::forward<Value>(value));
            }

        private:
            state *taskState;
        };

        shared_task() noexcept = default;
        shared_task(const shared_task &) noexcept = default;
        shared_task(shared_task &&) noexcept = default;
        shared_task &operator=(const shared_task &) noexcept = default;
        shared_task &operator=(shared_task &&) noexcept = default;

        [[nodiscard]] explicit operator bool() const noexcept
        {
            return static_cast<bool>(m_State);
        }

        struct awaiter
        {
            std::shared_ptr<state> taskState;

            bool await_ready() const noexcept
            {
                std::lock_guard lock(taskState->mutex);
                return taskState->completed;
            }

            bool await_suspend(std::coroutine_handle<> continuation)
            {
                bool startTask = false;
                {
                    std::lock_guard lock(taskState->mutex);
                    if (taskState->completed)
                    {
                        return false;
                    }

                    if (!taskState->started)
                    {
                        taskState->started = true;
                        startTask = true;
                    }
                }

                if (startTask)
                {
                    taskState->coroutine.resume();
                }

                std::lock_guard lock(taskState->mutex);
                if (taskState->completed)
                {
                    return false;
                }

                taskState->waiters.push_back(continuation);
                return true;
            }

            T await_resume()
            {
                std::lock_guard lock(taskState->mutex);
                if (taskState->exception)
                {
                    std::rethrow_exception(taskState->exception);
                }

                return *taskState->value;
            }
        };

        [[nodiscard]] awaiter operator co_await() const noexcept
        {
            return awaiter{ m_State };
        }

    private:
        explicit shared_task(std::shared_ptr<state> taskState) noexcept
            : m_State(std::move(taskState))
        {
        }

        std::shared_ptr<state> m_State;
    };

    template<typename T>
    T sync_wait(shared_task<T> task)
    {
        return task.Get();
    }

    class task_scope
    {
    private:
        struct detached_task
        {
            struct promise_type
            {
                detached_task get_return_object() noexcept { return {}; }
                std::suspend_never initial_suspend() const noexcept { return {}; }
                std::suspend_never final_suspend() const noexcept { return {}; }
                void return_void() const noexcept {}
                void unhandled_exception() const noexcept { std::terminate(); }
            };
        };

        template<typename Awaitable>
        detached_task Run(Awaitable awaitable)
        {
            try
            {
                co_await std::move(awaitable);
            }
            catch (...)
            {
                std::terminate();
            }

            {
                std::lock_guard lock(m_Mutex);
                --m_ActiveTasks;
            }
            m_Completion.notify_all();
        }

    public:
        template<typename Awaitable>
        void Spawn(Awaitable awaitable)
        {
            {
                std::lock_guard lock(m_Mutex);
                ++m_ActiveTasks;
            }
            Run(std::move(awaitable));
        }

        void Wait()
        {
            std::unique_lock lock(m_Mutex);
            m_Completion.wait(lock, [this] { return m_ActiveTasks == 0; });
        }

    private:
        std::mutex m_Mutex;
        std::condition_variable m_Completion;
        std::size_t m_ActiveTasks = 0;
    };
}

#endif
