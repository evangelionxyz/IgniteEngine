// Copyright (c) 2026 Evangelion Manuhutu

#ifndef WIN32_LEAN_AND_MEAN
    #define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
    #define NOMINMAX
#endif

#ifdef _WIN32
    #include <windows.h>
    #include <processthreadsapi.h>
#endif

#include "threadpool.hpp"
#include "ignite/core/logger.hpp"

namespace
{
#ifdef _WIN32
    void SetCurrentThreadName(const std::string &name)
    {
        std::wstring wname(name.begin(), name.end());
        SetThreadDescription(GetCurrentThread(), wname.c_str());
    }
#else
    void SetCurrentThreadName(const std::string &name) {}
#endif

}

namespace ignite
{

    bool SchedulerAwaiter::await_suspend(std::coroutine_handle<> handle) const noexcept
    {
        return m_Pool && m_Pool->EnqueueTask([handle]()
        {
            handle.resume();
        });
    }

    SchedulerAwaiter ThreadPool::Schedule() noexcept
    {
        return SchedulerAwaiter(this);
    }

    ThreadPool::ThreadPool(std::size_t threadCount, std::string poolName)
        : m_PoolName(std::move(poolName)), m_Stop(false), m_ActiveTasks(0)
    {
        if (threadCount == 0)
        {
            threadCount = std::max(std::thread::hardware_concurrency(), 1u);
        }

        m_Workers.reserve(threadCount);
        for (std::size_t i = 0; i < threadCount; ++i)
        {
            m_Workers.emplace_back(&ThreadPool::WorkerLoop, this, i);
        }
    }

    ThreadPool::~ThreadPool()
    {
        Shutdown();
    }

    bool ThreadPool::EnqueueTask(std::function<void()> task)
    {
        {
            std::unique_lock<std::mutex> lock(m_QueueMutex);
            if (m_Stop)
            {
                LOG_WARN("[ThreadPool:{}] Cannot enqueue task on stopped pool.", m_PoolName);
                return false;
            }

            m_Tasks.push(std::move(task));
        }

        m_Cv.notify_one();
        return true;
    }

    void ThreadPool::Shutdown()
    {
        {
            std::unique_lock<std::mutex> lock(m_QueueMutex);
            if (m_Stop)
            {
                return;
            }

            m_Stop = true;
        }

        m_Cv.notify_all();
        for (auto &worker : m_Workers)
        {
            if (worker.joinable())
            {
                worker.join();
            }
        }
        m_Workers.clear();
    }

    void ThreadPool::WaitIdle()
    {
        std::unique_lock<std::mutex> lock(m_QueueMutex);
        m_IdleCv.wait(lock, [this]() {
            return m_Tasks.empty() && m_ActiveTasks == 0;
        });
    }

    std::size_t ThreadPool::GetThreadCount() const noexcept
    {
        return m_Workers.size();
    }

    std::size_t ThreadPool::GetPendingTaskCount() const noexcept
    {
        std::unique_lock<std::mutex> lock(m_QueueMutex);
        return m_Tasks.size();
    }

    bool ThreadPool::IsRunning() const noexcept
    {
        return !m_Stop;
    }

    const std::string &ThreadPool::GetName() const noexcept
    {
        return m_PoolName;
    }

    void ThreadPool::WorkerLoop(std::size_t workerIndex)
    {
        SetCurrentThreadName(std::format("{}_{}", m_PoolName, workerIndex));

        while (true)
        {
            std::function<void()> task;
            {
                std::unique_lock<std::mutex> lock(m_QueueMutex);
                m_Cv.wait(lock, [this]() {
                    return m_Stop || !m_Tasks.empty();
                });

                if (m_Stop && m_Tasks.empty())
                {
                    return;
                }

                task = std::move(m_Tasks.front());
                m_Tasks.pop();
                m_ActiveTasks++;
            }

            try
            {
                task();
            }
            catch (const std::exception &e)
            {
                LOG_ERROR("[ThreadPool:{}] Unhandled exception in task: {}", m_PoolName, e.what());
            }
            catch (...)
            {
                LOG_ERROR("[ThreadPool:{}] Unhandled non-standard exception in task.", m_PoolName);
            }

            {
                std::unique_lock<std::mutex> lock(m_QueueMutex);
                m_ActiveTasks--;
                if (m_Tasks.empty() && m_ActiveTasks == 0)
                {
                    m_IdleCv.notify_all();
                }
            }
        }
    }
}
