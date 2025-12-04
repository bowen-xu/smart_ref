#pragma once
#include <unordered_set>
#include <cstddef>
#include <iostream>

struct TestHolderPolicy
{
    std::unordered_set<void*> held_handlers;

    static void hold_ref(void* self_, const auto& shared)
    {
        auto* self = static_cast<TestHolderPolicy*>(self_);
        self->held_handlers.insert(shared.handler);
    }

    static void unhold_ref(void* self_, void* h)
    {
        auto* self = static_cast<TestHolderPolicy*>(self_);
        self->held_handlers.erase(h);
    }

    bool holds(void* h) const
    {
        return held_handlers.count(h) > 0;
    }
};
