// Copyright (c) 2026 Evangelion Manuhutu

#pragma once
#ifndef IGN_SIGNAL_BUS_HPP
#define IGN_SIGNAL_BUS_HPP

#include <cstdint>
#include <functional>
#include <mutex>
#include <vector>
#include <algorithm>

namespace ignite
{
    // -------------------------------------------------------------------------
    // SignalBus — lightweight, type-safe, in-process pub/sub for internal
    //             engine notifications (asset events, system state changes, etc.)
    //
    // Usage:
    //   // Subscribe (typically in a constructor)
    //   m_Token = SignalBus::Subscribe<AssetChangeSignal>(
    //       [this](const AssetChangeSignal& s) { OnAssetChanged(s); });
    //
    //   // Emit (from any system, any thread*)
    //   SignalBus::Emit(AssetChangeSignal{ handle, type });
    //
    //   // Unsubscribe (typically in destructor)
    //   SignalBus::Unsubscribe<AssetChangeSignal>(m_Token);
    //
    // * Thread safety: Subscribe/Unsubscribe/Emit are each individually mutex-
    //   guarded. However, listeners themselves may not be thread-safe — if you
    //   Emit from a background or render thread and the listener touches
    //   main-thread-only state, wrap the Emit in Application::SubmitToMainThread.
    // -------------------------------------------------------------------------

    using SignalToken = uint32_t;
    static constexpr SignalToken kInvalidSignalToken = 0;

    class SignalBus
    {
    public:
        SignalBus() = delete;

        template<typename T>
        static SignalToken Subscribe(std::function<void(const T&)> callback)
        {
            auto& state = GetState<T>();
            std::lock_guard lock(state.mutex);
            SignalToken token = ++s_NextToken;
            state.entries.push_back({ token, std::move(callback) });
            return token;
        }

        template<typename T>
        static void Unsubscribe(SignalToken token)
        {
            if (token == kInvalidSignalToken)
                return;

            auto& state = GetState<T>();
            std::lock_guard lock(state.mutex);
            auto& v = state.entries;
            v.erase(std::remove_if(v.begin(), v.end(),
                [token](const Entry<T>& e) { return e.token == token; }),
                v.end());
        }

        // Emit the signal to all currently-registered subscribers.
        // Copies the subscriber list under the lock, then calls each callback
        // without holding the lock (avoids deadlock if a callback re-enters).
        template<typename T>
        static void Emit(const T& signal)
        {
            std::vector<Entry<T>> snapshot;
            {
                auto& state = GetState<T>();
                std::lock_guard lock(state.mutex);
                snapshot = state.entries; // copy
            }
            for (const auto& entry : snapshot)
                entry.callback(signal);
        }

    private:
        template<typename T>
        struct Entry
        {
            SignalToken token;
            std::function<void(const T&)> callback;
        };

        template<typename T>
        struct State
        {
            std::mutex mutex;
            std::vector<Entry<T>> entries;
        };

        template<typename T>
        static State<T>& GetState()
        {
            static State<T> s_State;
            return s_State;
        }

        static inline std::atomic<SignalToken> s_NextToken{ 0 };
    };
}

#endif
