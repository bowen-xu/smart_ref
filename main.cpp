#include <fmt/core.h>
#include <memory>
#include "smart_ref.hpp"
#include <chrono>
#include <set>

using namespace smart_ref;

struct Node;

struct Graph
{
    std::set<weak_ref<Node, Graph>> nodes;

    void hold_ref(const weak_ref<Node, Graph> &x) { this->nodes.insert(x); }
    void unhold_ref(const weak_ref<Node, Graph> &w) { this->nodes.erase(w); }

};


struct Node : public enable_shared_ref_from_this<Node, Graph>, enable_ref_holder<Graph>
{
    inline static int _id_counter = 0;
    int id;
    int value;
    Node(int v) : id(_id_counter++), value(v) {}
};

void performance()
{
    const auto N = 5000000;

    {
        uint64_t sum = 0;
        std::vector<uint64_t*> data;
        data.reserve(N);
        auto start = std::chrono::high_resolution_clock::now();
        for (uint64_t i = 0; i < N; ++i)
        {
            data.push_back(new uint64_t(i));
        }
        for (uint64_t i = 0; i < N; ++i)
        {
            if (i % 100 != 0)
                continue;
            sum += *data[i];
        }
        auto end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count() / 1000.0;
        fmt::print("Raw-pointer time: {} ms, sum: {}\n", duration, sum);
        for (auto ptr : data)
            delete ptr;
    }
    {
        uint64_t sum = 0;
        std::vector<shared_ref<int>> data;
        data.reserve(N);
        auto start = std::chrono::high_resolution_clock::now();
        for (auto i = 0; i < N; ++i)
        {
            data.emplace_back(new int(i));
        }

        for (auto i = 0; i < N; ++i)
        {
            if (i % 100 != 0)
                continue;
            sum += *data[i];
        }
        auto end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count() / 1000.0;
        fmt::print("smart_ref::shared_ref time: {} ms, sum: {}\n", duration, sum);
    }
    {
        uint64_t sum = 0;
        std::vector<std::shared_ptr<int>> data;
        data.reserve(N);
        auto start = std::chrono::high_resolution_clock::now();
        for (auto i = 0; i < N; ++i)
        {
            data.emplace_back(new int(i));
        }
        for (auto i = 0; i < N; ++i)
        {
            if (i % 100 != 0)
                continue;
            sum += *data[i];
        }

        auto end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count() / 1000.0;
        fmt::print("std::shared_ptr time: {} ms, sum: {}\n", duration, sum);
    }
}

int main()
{
    auto g = Graph();
    auto x = shared_ref<Node, Graph>(new Node(13));
    x.set_holder(&g);
    auto x2 = weak_ref(x);
    x = nullptr;
    x2 = nullptr;

    fmt::print("Size of SharedRef<int>: {}; size of std::shared_ptr<int>: {}\n", sizeof(shared_ref<int>),
               sizeof(std::shared_ptr<int>));
    auto a = std::shared_ptr<int>(new int(42));
    fmt::print("strong count: {}\n", a.use_count());
    fmt::print("Hello, C++ Concept Graph!\n");

    auto n1 = shared_ref(new Node(10));
    auto n2 = n1;

    fmt::print("reset n1\n");
    n1.reset();
    fmt::print("reset n2\n");
    n2.reset();

    n1 = shared_ref(new Node(20));
    auto n1w = weak_ref<Node>(n1);
    n1 = n2;
    auto n1r = shared_ref<Node>::revive(new Node(30), n1w);
    if (n1w.lock())
        fmt::print("Locked node value: {}\n", n1w.lock()->value);

    performance();
    return 0;
}