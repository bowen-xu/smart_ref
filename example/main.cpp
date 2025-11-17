
#include <fmt/core.h>
#include <cstdint>
#include <set>
#include <unordered_map>
#include <vector>
#include <chrono>
#include <iostream>
#include <functional>
#include "smart_ref.hpp"

using namespace smart_ref;

struct Node;

struct Graph
{
    std::set<weak_ref<Node, Graph>> nodes;

    static void hold_ref(void *self, const weak_ref<Node, Graph> &x) { static_cast<Graph *>(self)->nodes.insert(x); }
    static void unhold_ref(void *self, const weak_ref<Node, Graph> &w) { static_cast<Graph *>(self)->nodes.erase(w); }
};

struct Node : public enable_shared_ref_from_this<Node, Graph>, enable_ref_holder
{
    inline static int _id_counter = 0;
    int id;
    int value;
    Node(int v) : id(_id_counter++), value(v) {}
};

struct ConceptNetwork;
struct Concept;
struct ConceptHolder;
using pConcept = shared_ref<Concept, ConceptHolder>;
using wpConcept = weak_ref<Concept, ConceptHolder>;

struct Concept : enable_ref_holder
{
    int id;
    Concept(int id) : id(id) {}
    Concept(std::vector<pConcept> comps)
    {
        std::vector<int> comp_ids;
        for (auto &c : comps)
        {
            components.push_back(c);
            comp_ids.push_back(c->id);
        }
        this->id = (int)(hashInts(comp_ids) % std::numeric_limits<int>::max());
    }
    std::vector<wpConcept> components;

public:
    static uint64_t hashInts(const std::vector<int> &arr)
    {
        uint64_t h = 1469598103934665603ULL; // offset basis
        for (int v : arr)
        {
            h ^= (uint64_t)v;
            h *= 1099511628211ULL; // prime
        }
        return h;
    }
};

struct ConceptHolder
{
    std::unordered_map<int, wpConcept> holder_map;  // concept id -> concept weak pointer
    std::unordered_map<size_t, int> holder_map_rev; // handler -> concept id

    static void hold_ref(void *self_, const pConcept &x)
    {
        auto &self = *static_cast<ConceptHolder *>(self_);
        if (self.holder_map.find(x->id) != self.holder_map.end())
            return; // already held
        self.holder_map[x->id] = x;
        self.holder_map_rev[(size_t)(x.handler)] = x->id;
    }

    static void unhold_ref(void *self_, const wpConcept &w)
    {
        auto &self = *static_cast<ConceptHolder *>(self_);
        auto it = self.holder_map_rev.find((size_t)(w.handler));
        if (it == self.holder_map_rev.end())
            throw std::runtime_error("ConceptHolder::unhold_ref: concept not found in holder");
        auto id_it = self.holder_map.find(it->second);
        if (id_it != self.holder_map.end())
            self.holder_map.erase(id_it);
        self.holder_map_rev.erase(it);
    }

    ~ConceptHolder()
    {
        for (auto &[_, wp] : holder_map)
            wp.handler->holder = nullptr;
    }
};
struct ConceptNetwork
{
    std::unordered_map<int, pConcept> concepts;
    ConceptHolder holder;
    auto new_concept(int id)
    {
        if (concepts.find(id) != concepts.end())
            return concepts[id];
        auto c = pConcept(new Concept(id));
        c.set_holder(&holder);
        concepts[id] = c;
        return c;
    }
    auto new_concept(const std::vector<pConcept> &comps)
    {
        std::vector<int> comp_ids;
        for (auto &c : comps)
            comp_ids.push_back(c->id);
        int id = (int)(Concept::hashInts(comp_ids) % std::numeric_limits<int>::max());
        if (concepts.find(id) != concepts.end())
            return concepts[id];
        if (holder.holder_map.find(id) != holder.holder_map.end())
        {
            auto c = new Concept(comps);
            auto wp = holder.holder_map[id];
            return shared_ref<Concept, ConceptHolder>::revive(c, wp);
        }
        auto c = pConcept(new Concept(comps));
        c.set_holder(&holder);
        concepts[c->id] = c;
        return c;
    }
    void del_concept(int id) { concepts.erase(id); }
};

void performance()
{
    const auto N = 5000000;

    {
        uint64_t sum = 0;
        std::vector<uint64_t *> data;
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

void concept_network_example()
{

    auto net = ConceptNetwork();
    auto c1 = net.new_concept(1);
    auto c2 = net.new_concept(2);
    auto c3 = net.new_concept({c1, c2});
    auto c4 = net.new_concept(3);
    auto c5 = net.new_concept({c3, c4});
    auto id_c3 = c3->id;
    // c1.reset();
    // c2.reset();
    c3.reset();
    c4.reset();
    // c5.reset();
    net.del_concept(id_c3);
    fmt::print("Component 0 of c5: {}\n", (void *)c5->components[0].lock().get());
    c3 = net.new_concept({c1, c2});
    fmt::print("Recreated c3\n");
    fmt::print("Component 0 of c5: {}\n", (void *)c5->components[0].lock().get());
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
    if (bool(n1w.lock()))
        fmt::print("Locked node value: {}\n", n1w.lock()->value);

    concept_network_example();
    performance();

    std::string s = "hello";
    std::hash<std::string> hasher;

    size_t h = hasher(s);
    std::cout << h << std::endl;
    return 0;
}