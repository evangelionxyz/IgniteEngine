// Copyright (c) 2026 Evangelion Manuhutu

#pragma once
#ifndef IGN_CORE_THREADPOOL_HPP
#define IGN_CORE_THREADPOOL_HPP

#include "ignite/core/base.hpp"

#include <cppcoro/coroutine.hpp>
#include <cppcoro/task.hpp>

#include <atomic>
#include <condition_variable>
#include <functional>
#include <future>
#include <memory>
#include <mutex>
#include <queue>
#include <string>
#include <thread>
#include <type_traits>
#include <vector>

namespace ignite
{    
    class ThreadPool;

    struct IGN_CORE_API SchedulerAwaiter
    {
        ThreadPool *m_Pool = nullptr;

        explicit SchedulerAwaiter(ThreadPool *pool) noexcept : m_Pool(pool) {}

        [[nodiscard]] bool await_ready() const noexcept { return false; }
        bool await_suspend(std::coroutine_handle<> handle) const noexcept;
        void await_resume() const noexcept {}
    };

    class IGN_CORE_API ThreadPool
    {
    public:
        explicit ThreadPool(std::size_t threadCount = 0, std::string poolName = "Worker");
        ~ThreadPool();

        ThreadPool(const ThreadPool &) = delete;
        ThreadPool &operator=(const ThreadPool &) = delete;
        ThreadPool(ThreadPool &&) = delete;
        ThreadPool &operator=(ThreadPool &&) = delete;

        template <typename F, typename... Args>
        auto Enqueue(F &&f, Args &&... args) -> std::future<std::invoke_result_t<std::decay_t<F>, std::decay_t<Args>...>>
        {
            using return_type = std::invoke_result_t<std::decay_t<F>, std::decay_t<Args>...>;
            auto task = std::make_shared<std::packaged_task<return_type()>>(
                [func = std::forward<F>(f), ...capturedArgs = std::forward<Args>(args)]() mutable
                {
                    return std::invoke(std::forward<F>(func), std::forward<Args>(capturedArgs)...);
                }
            );

            std::future<return_type> res = task->get_future();
            (void)EnqueueTask([task]() { (*task)(); });

            return res;
        }

        [[nodiscard]] bool EnqueueTask(std::function<void()> task);

        [[nodiscard]] SchedulerAwaiter Schedule() noexcept;

        void Shutdown();
        void WaitIdle();

        [[nodiscard]] std::size_t GetThreadCount() const noexcept;
        [[nodiscard]] std::size_t GetPendingTaskCount() const noexcept;
        [[nodiscard]] bool IsRunning() const noexcept;
        [[nodiscard]] const std::string &GetName() const noexcept;

    private:
        void WorkerLoop(std::size_t workerIndex);

    private:
        std::string m_PoolName;
        std::vector<std::thread> m_Workers;
        std::queue<std::function<void()>> m_Tasks;

        mutable std::mutex m_QueueMutex;
        std::condition_variable m_Cv;
        std::condition_variable m_IdleCv;

        std::atomic<bool> m_Stop;
        std::atomic<std::size_t> m_ActiveTasks;
    };
}

#endif
