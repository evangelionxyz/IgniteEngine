// Copyright (c) 2026 Evangelion Manuhutu

#include "ignite/core/threadpool.hpp"

#include <cppcoro/sync_wait.hpp>
#include <cppcoro/task.hpp>
#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <thread>

namespace
{
    cppcoro::task<std::thread::id> ScheduleOn(ignite::ThreadPool &pool)
    {
        co_await pool.Schedule();
        co_return std::this_thread::get_id();
    }
}

TEST(ThreadPool, ExecutesQueuedTasks)
{
    ignite::ThreadPool pool(2, "ThreadPoolTest");
    std::atomic<int> completed = 0;

    for (int i = 0; i < 32; ++i)
    {
        ASSERT_TRUE(pool.EnqueueTask([&completed]()
        {
            ++completed;
        }));
    }

    pool.WaitIdle();
    EXPECT_EQ(completed.load(), 32);
}

TEST(ThreadPool, ShutdownDrainsQueuedTasks)
{
    ignite::ThreadPool pool(1, "ThreadPoolDrainTest");
    std::atomic<int> completed = 0;

    ASSERT_TRUE(pool.EnqueueTask([&completed]()
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
        ++completed;
    }));

    for (int i = 1; i < 32; ++i)
    {
        ASSERT_TRUE(pool.EnqueueTask([&completed]()
        {
            ++completed;
        }));
    }

    pool.Shutdown();
    EXPECT_EQ(completed.load(), 32);
    EXPECT_FALSE(pool.IsRunning());
}

TEST(ThreadPool, ResumesCoroutineOnWorker)
{
    ignite::ThreadPool pool(1, "CoroutineTest");
    const std::thread::id callerThread = std::this_thread::get_id();
    const std::thread::id workerThread = cppcoro::sync_wait(ScheduleOn(pool));

    EXPECT_NE(workerThread, callerThread);
}
