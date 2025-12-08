#include <cstddef>
#include <gtest/gtest.h>
#include <iostream>
#include <smart_ref.hpp>
#include "TestHolderPolicy.hpp"

using namespace smart_ref;

struct Obj : enable_ref_holder
{
    int value;
    Obj(int v) : value(v) {}
};

// ----------------------
// 1. Basic Construction
// ----------------------

TEST(SmartRefBasic, DefaultConstruct)
{
    shared_ref<Obj, TestHolderPolicy> p;
    EXPECT_EQ(p.get(), nullptr);
    EXPECT_EQ(p.handler, nullptr);
    EXPECT_EQ(p.ptr, nullptr);
}

TEST(SmartRefBasic, ConstructFromRawPtr)
{
    auto p = shared_ref<Obj, TestHolderPolicy>(new Obj(5));
    ASSERT_NE(p.get(), nullptr);
    EXPECT_EQ(p->value, 5);
    EXPECT_EQ(p.handler->strong, 1);
    EXPECT_EQ(p.handler->weak, 0);
}

// ----------------------
// 2. Reference Counting Behavior
// ----------------------

TEST(SmartRefBasic, CopyIncrementsStrong)
{
    auto p1 = shared_ref<Obj, TestHolderPolicy>(new Obj(1));
    {
        auto p2 = p1;
        EXPECT_EQ(p1.handler->strong, 2);
    }
    EXPECT_EQ(p1.handler->strong, 1);
}

TEST(SmartRefBasic, SelfAssignmentDoesNotChangeCount)
{
    auto p1 = shared_ref<Obj, TestHolderPolicy>(new Obj(1));
    {
        weak_ref<Obj, TestHolderPolicy> w1 = p1;
        p1 = p1;
        ASSERT_EQ(p1.handler->strong, 1);
        EXPECT_NE(p1.handler->ptr, nullptr);
    }

    p1 = p1;
    ASSERT_EQ(p1.handler->strong, 1);
    EXPECT_NE(p1.handler->ptr, nullptr);
}

TEST(SmartRefBasic, CopyNullptrRef)
{
    auto p1 = shared_ref<Obj, TestHolderPolicy>(nullptr);
    auto p2 = shared_ref<Obj, TestHolderPolicy>(new Obj(2));
    auto wp2 = weak_ref<Obj, TestHolderPolicy>(p2);
    p2 = p1;
    EXPECT_EQ(p2.get(), nullptr);
    EXPECT_EQ(p2.handler, nullptr);
    ASSERT_NE(wp2.handler, nullptr);
    EXPECT_EQ(wp2.handler->strong, 0);
    EXPECT_EQ(wp2.handler->weak, 1);
}

// ----------------------
// 3. weak_ref behavior
// ----------------------

TEST(SmartRefWeak, WeakConstruct)
{
    auto s = shared_ref<Obj, TestHolderPolicy>(new Obj(10));
    weak_ref<Obj, TestHolderPolicy> w = s;

    EXPECT_EQ(s.handler->weak, 1);
}

TEST(SmartRefWeak, LockAfterSharedDestroyed)
{
    auto w = weak_ref<Obj, TestHolderPolicy>();
    {
        auto s = shared_ref<Obj, TestHolderPolicy>(new Obj(7));
        w = s;
    }
    EXPECT_TRUE(w.expired());
    auto s2 = w.lock();
    EXPECT_FALSE((bool)s2);
    ASSERT_NE(w.handler, nullptr);
    EXPECT_EQ(w.handler->ptr, nullptr);
    EXPECT_EQ(w.handler->strong, 0);
    EXPECT_EQ(w.handler->weak, 1);
}

TEST(SmartRefWeak, LockSuccess)
{
    auto s = shared_ref<Obj, TestHolderPolicy>(new Obj(3));
    weak_ref<Obj, TestHolderPolicy> w = s;

    auto s2 = w.lock();
    EXPECT_TRUE(s2);
    EXPECT_EQ(s2->value, 3);
}

// ----------------------
// 4. revive
// ----------------------

TEST(SmartRefRevive, ReviveWorks)
{
    using S = shared_ref<Obj, TestHolderPolicy>;
    using W = weak_ref<Obj, TestHolderPolicy>;

    S s(new Obj(100));
    W w = s;
    auto *handler = s.handler;

    // 销毁强引用
    s.reset();
    EXPECT_EQ(handler->strong, 0);
    EXPECT_NE(handler->weak, 0);
    EXPECT_EQ(handler->ptr, nullptr);

    // revive
    S revived = S::revive(new Obj(200), handler);
    EXPECT_EQ(revived->value, 200);
    EXPECT_EQ(handler->strong, 1);
}

// TEST(SmartRef, WeakRevive)
// {
//     using Ref = smart_ref::shared_ref<Obj, TestHolderPolicy>;
//     using WRef = smart_ref::weak_ref<Obj, TestHolderPolicy>;

//     TestHolderPolicy holder;
//     Ref r(new Obj(1));
//     r.set_holder(&holder);

//     WRef w = r;
//     EXPECT_FALSE(w.expired());

//     r = nullptr; // strong -> 0, weak > 0, ptr 被 delete 并设为 nullptr
//     EXPECT_TRUE(w.expired());

//     // revives
//     Ref r2 = Ref::revive(new Obj(2), w.handler);
//     EXPECT_EQ(r2->value, 2);
// }

// ----------------------
// 5. holder (TestHolderPolicy)
// ----------------------

TEST(HolderPolicy, HoldCalled)
{
    TestHolderPolicy holder;

    shared_ref<Obj, TestHolderPolicy> p(new Obj(1));
    p.set_holder(&holder);

    EXPECT_TRUE(holder.holds(p.handler));
}

TEST(HolderPolicy, UnholdCalledWhenAllRefsReleased)
{
    TestHolderPolicy holder;

    {
        shared_ref<Obj, TestHolderPolicy> p(new Obj(1));
        p.set_holder(&holder);

        EXPECT_TRUE(holder.holds(p.handler));
    }

    EXPECT_TRUE(holder.held_handlers.empty());
}

TEST(SharedRefHolderPolicy, MoveAssignmentMustUnholdHandler)
{
    using Ref = shared_ref<Obj, TestHolderPolicy>;

    TestHolderPolicy holder;

    void *handler_addr = nullptr;

    Ref a(new Obj(42));
    a.set_holder(&holder);

    handler_addr = a.handler;
    ASSERT_NE(handler_addr, nullptr);
    EXPECT_TRUE(holder.holds(handler_addr)) << "Holder should track handler after set_holder";

    Ref b = nullptr;
    a = b;

    // 到这里，handler 应该已经被 unhold
    EXPECT_FALSE(holder.holds(handler_addr))
        << "Handler must be unheld when last shared_ref is released via assignment";
}

// ----------------------
// 6. enable_shared_ref_from_this
// ----------------------

struct SelfObj : enable_shared_ref_from_this<SelfObj, TestHolderPolicy>, enable_ref_holder
{
    int x;
    SelfObj(int x) : x(x) {}
};

TEST(SharedFromThis, SharedFromThisWorks)
{
    auto p = shared_ref<SelfObj, TestHolderPolicy>(new SelfObj(77));

    auto p2 = p->shared_from_this();

    EXPECT_EQ(p.handler, p2.handler);
    EXPECT_EQ(p2.handler->strong, 2);
    EXPECT_EQ(p2->x, 77);
}

TEST(SharedFromThis, SharedFromThisFailsWhenNotOwned)
{
    SelfObj o(1);

    EXPECT_THROW(o.shared_from_this(), std::runtime_error);
}

TEST(SharedFromThis, SharedFromThisExpiredAfterDeath)
{
    weak_ref<SelfObj, TestHolderPolicy> w;

    {
        auto p = shared_ref<SelfObj, TestHolderPolicy>(new SelfObj(99));
        auto p2 = p->shared_from_this();
        w = p2;
        EXPECT_FALSE(w.expired());
    }

    EXPECT_TRUE(w.expired());
    EXPECT_FALSE(w.lock());
}

TEST(SharedFromThis, WeakFromThisExpiredAfterDeath)
{
    weak_ref<SelfObj, TestHolderPolicy> w;

    {
        auto p = shared_ref<SelfObj, TestHolderPolicy>(new SelfObj(99));
        w = p->weak_from_this();
        EXPECT_FALSE(w.expired());
    }

    EXPECT_TRUE(w.expired());
    EXPECT_FALSE(w.lock());
}
