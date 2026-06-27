// Copyright (c) 2026 Evangelion Manuhutu

#include "ignite_pch.hpp"
#include "signal_bus.hpp"
#include <map>
#include <mutex>
#include <atomic>
#include <algorithm>

namespace ignite
{
    static std::atomic<SignalToken> s_NextToken{ 0 };

    struct SignalBusState
    {
        std::mutex mutex;
        std::map<std::type_index, std::vector<std::shared_ptr<SignalBus::SubscriberWrapper>>> subscribers;
    };

    static SignalBusState& GetBusState()
    {
        static SignalBusState state;
        return state;
    }

    SignalToken SignalBus::GenerateNextToken()
    {
        return ++s_NextToken;
    }

    SignalToken SignalBus::AddSubscriber(std::type_index type, std::shared_ptr<SubscriberWrapper> subscriber)
    {
        auto& state = GetBusState();
        std::lock_guard lock(state.mutex);
        state.subscribers[type].push_back(subscriber);
        return subscriber->token;
    }

    void SignalBus::RemoveSubscriber(std::type_index type, SignalToken token)
    {
        if (token == kInvalidSignalToken)
            return;

        auto& state = GetBusState();
        std::lock_guard lock(state.mutex);
        auto it = state.subscribers.find(type);
        if (it != state.subscribers.end())
        {
            auto& v = it->second;
            v.erase(std::remove_if(v.begin(), v.end(),
                [token](const std::shared_ptr<SubscriberWrapper>& e) { return e->token == token; }),
                v.end());
        }
    }

    std::vector<std::shared_ptr<SignalBus::SubscriberWrapper>> SignalBus::GetSubscribers(std::type_index type)
    {
        auto& state = GetBusState();
        std::lock_guard lock(state.mutex);
        auto it = state.subscribers.find(type);
        if (it != state.subscribers.end())
        {
            return it->second; // return copy
        }
        return {};
    }
}
