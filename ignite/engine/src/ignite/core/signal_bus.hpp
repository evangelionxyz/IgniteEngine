// Copyright (c) 2026 Evangelion Manuhutu

#pragma once
#ifndef IGN_SIGNAL_BUS_HPP
#define IGN_SIGNAL_BUS_HPP

#include "ignite/core/base.hpp"
#include <cstdint>
#include <functional>
#include <vector>
#include <memory>
#include <typeindex>

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

    class IGN_API SignalBus
    {
    public:
        struct SubscriberWrapper
        {
            SignalToken token;
            virtual ~SubscriberWrapper() = default;
            virtual void Invoke(const void* signalPtr) = 0;
        };

    private:
        template<typename T>
        struct SubscriberWrapperImpl : public SubscriberWrapper
        {
            std::function<void(const T&)> callback;

            SubscriberWrapperImpl(SignalToken t, std::function<void(const T&)> cb)
                : callback(std::move(cb)) { token = t; }

            void Invoke(const void* signalPtr) override
            {
                callback(*static_cast<const T*>(signalPtr));
            }
        };

        static SignalToken AddSubscriber(std::type_index type, std::shared_ptr<SubscriberWrapper> subscriber);
        static void RemoveSubscriber(std::type_index type, SignalToken token);
        static std::vector<std::shared_ptr<SubscriberWrapper>> GetSubscribers(std::type_index type);
        static SignalToken GenerateNextToken();

    public:
        SignalBus() = delete;

        template<typename T>
        [[nodiscard]] static SignalToken Subscribe(std::function<void(const T&)> callback)
        {
            SignalToken token = GenerateNextToken();
            auto wrapper = std::make_shared<SubscriberWrapperImpl<T>>(token, std::move(callback));
            AddSubscriber(typeid(T), wrapper);
            return token;
        }

        template<typename T>
        static void Unsubscribe(SignalToken token)
        {
            RemoveSubscriber(typeid(T), token);
        }

        // Emit the signal to all currently-registered subscribers.
        // Copies the subscriber list under the lock, then calls each callback
        // without holding the lock (avoids deadlock if a callback re-enters).
        template<typename T>
        static void Emit(const T& signal)
        {
            auto snapshot = GetSubscribers(typeid(T));
            for (const auto& entry : snapshot)
                entry->Invoke(&signal);
        }
    };
}

#endif
