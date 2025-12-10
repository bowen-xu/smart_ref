#include <cstddef>
#include <gtest/gtest.h>
#include <iostream>
#include <smart_ref.hpp>
#include <stdexcept>
#include "TestHolderPolicy.hpp"

using namespace smart_ref;

struct Obj
{
    int value;
    Obj(int v) : value(v) {}
};

struct DerivedObj : Obj
{
    DerivedObj(int v) : Obj(v) {}
};

// struct SelfObj : enable_shared_ref_from_this<SelfObj, TestHolderPolicy>, enable_ref_holder
// {
//     int x;
//     SelfObj(int x) : x(x) {}
// };

// ----------------------
// 1. Basic Construction
// ----------------------

TEST(SmartRefBasic, DefaultConstruct)
{
    shared_ref<Obj, TestHolderPolicy> p;
    auto holder = TestHolderPolicy();
    EXPECT_EQ(p.get(), nullptr);
    EXPECT_EQ(p.handler, nullptr);
    EXPECT_EQ(p.ptr, nullptr);
    EXPECT_THROW(p.set_holder(&holder), std::runtime_error);
}

TEST(SmartRefBasic, ConstructFromRawPtr)
{
    auto holder = TestHolderPolicy();
    auto p = shared_ref<Obj, TestHolderPolicy>(new Obj(5));
    ASSERT_NE(p.get(), nullptr);
    EXPECT_EQ(p->value, 5);
    EXPECT_EQ(p.handler->strong, 1);
    EXPECT_EQ(p.handler->weak, 0);
    EXPECT_NO_THROW(p.set_holder(&holder));
}

// ----------------------
// 2. Cast
// ----------------------
TEST(SharedRefCast, StaticPointerCast)
{
    auto holder = TestHolderPolicy();
    shared_ref<Obj, TestHolderPolicy> base_ref;
    {
        shared_ref<DerivedObj, TestHolderPolicy> derived_ref(new DerivedObj(10));
        ASSERT_NO_THROW(derived_ref.set_holder(&holder));
        base_ref = std::static_pointer_cast<Obj>(derived_ref);
        EXPECT_EQ(base_ref.handler->strong, 2); // both derived_ref and base_ref share the same handler
        EXPECT_TRUE(holder.holds(base_ref.handler));
    }

    EXPECT_EQ(base_ref->value, 10);
    EXPECT_EQ(base_ref.handler->strong, 1); // both derived_ref and base_ref share the same handler
    EXPECT_TRUE(holder.holds(base_ref.handler));
    base_ref.reset();
    EXPECT_FALSE(holder.holds(base_ref.handler));
}

TEST(SharedRefCast, StaticPointerCastNullptr)
{
    auto holder = TestHolderPolicy();
    shared_ref<DerivedObj, TestHolderPolicy> derived_ref(nullptr);
    shared_ref<Obj, TestHolderPolicy> base_ref = std::static_pointer_cast<Obj>(derived_ref);
    ASSERT_THROW(base_ref.set_holder(&holder), std::runtime_error);
    EXPECT_EQ(base_ref.get(), nullptr);
    EXPECT_EQ(base_ref.handler, nullptr);
    EXPECT_FALSE(holder.holds(base_ref.handler));
}

// ----------------------
// 3. Assignment
// ----------------------

TEST(SmartRefBasic, CopyIncrementsStrong)
{
    auto holder = TestHolderPolicy();
    auto p1 = shared_ref<Obj, TestHolderPolicy>(new Obj(1));
    auto h = p1.handler;
    ASSERT_NO_THROW(p1.set_holder(&holder));
    EXPECT_TRUE(holder.holds(h));
    {
        auto p2 = p1;
        shared_ref<Obj, TestHolderPolicy> p3;
        EXPECT_EQ(p1.handler->strong, 2);
        p3 = p1;
        EXPECT_EQ(p1.handler->strong, 3);
        EXPECT_EQ(p2.handler->holder, &holder);
        EXPECT_EQ(p3.handler->holder, &holder);
    }
    EXPECT_EQ(p1.handler->strong, 1);
    p1 = nullptr;
    EXPECT_FALSE(holder.holds(h));
}

TEST(SmartRefBasic, SelfAssignmentDoesNotChangeCount)
{
    auto holder = TestHolderPolicy();
    auto p1 = shared_ref<Obj, TestHolderPolicy>(new Obj(1));
    ASSERT_NO_THROW(p1.set_holder(&holder));
    {
        auto p2 = p1;
        ASSERT_EQ(p1.handler->strong, 2);
        p2 = p1;
        ASSERT_EQ(p1.handler->strong, 2);
        EXPECT_NE(p1.handler->ptr, nullptr);
    }

    p1 = p1;
    ASSERT_EQ(p1.handler->strong, 1);
    EXPECT_NE(p1.handler->ptr, nullptr);
}

TEST(SmartRefBasic, CopyNullptrRef)
{
    auto holder = TestHolderPolicy();
    auto p1 = shared_ref<Obj, TestHolderPolicy>(nullptr);
    auto p2 = shared_ref<Obj, TestHolderPolicy>(new Obj(2));
    p2.set_holder(&holder);
    auto h = p2.handler;
    EXPECT_TRUE(holder.holds(h));
    p2 = p1;
    EXPECT_EQ(p2.get(), nullptr);
    EXPECT_EQ(p2.handler, nullptr);
    EXPECT_FALSE(holder.holds(h));
}

// ----------------------
// 3. weak_ref behavior
// ----------------------

TEST(SmartRefWeak, WeakConstruct)
{
    auto holder = TestHolderPolicy();

    auto s = shared_ref<Obj, TestHolderPolicy>(new Obj(10));
    s.set_holder(&holder);
    auto h = s.handler;
    weak_ref<Obj, TestHolderPolicy> w = s;
    EXPECT_FALSE(w.expired());
    s = nullptr;
    ASSERT_TRUE(holder.holds(h));
    EXPECT_EQ(w.handler->weak, 1);
    EXPECT_TRUE(holder.holds(h));
    EXPECT_TRUE(w.expired());
    w = nullptr;
    EXPECT_TRUE(w.expired());
    EXPECT_FALSE(holder.holds(h));
}

TEST(SmartRefWeak, LockSuccess)
{
    auto holder = TestHolderPolicy();
    auto s = shared_ref<Obj, TestHolderPolicy>(new Obj(3));
    ASSERT_NO_THROW(s.set_holder(&holder));
    weak_ref<Obj, TestHolderPolicy> w;
    w = s;

    s = w.lock();
    auto s2 = w.lock();
    ASSERT_NE(s2.handler, nullptr);
    holder.holds(s2.handler);
    EXPECT_TRUE(s);
    EXPECT_EQ(s->value, 3);
}

TEST(SmartRefWeak, LockAfterSharedDestroyed)
{
    auto holder = TestHolderPolicy();
    auto w = weak_ref<Obj, TestHolderPolicy>();
    {
        auto s = shared_ref<Obj, TestHolderPolicy>(new Obj(7));
        ASSERT_NO_THROW(s.set_holder(&holder));
        w = s;
    }
    EXPECT_TRUE(w.expired());
    auto s2 = w.lock();
    EXPECT_FALSE((bool)s2);
    ASSERT_NE(w.handler, nullptr);
    EXPECT_EQ(w.handler->ptr, nullptr);
    EXPECT_EQ(w.handler->strong, 0);
    EXPECT_EQ(w.handler->weak, 1);
    holder.holds(w.handler);
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

TEST(SmartRef, WeakRevive)
{
    using Ref = smart_ref::shared_ref<Obj, TestHolderPolicy>;
    using WRef = smart_ref::weak_ref<Obj, TestHolderPolicy>;

    TestHolderPolicy holder;
    Ref r(new Obj(1));
    ASSERT_NO_THROW(r.set_holder(&holder));

    WRef w = r;
    EXPECT_FALSE(w.expired());

    r = nullptr; // strong -> 0, weak > 0, ptr 被 delete 并设为 nullptr
    EXPECT_TRUE(w.expired());
    ASSERT_TRUE(holder.holds(w.handler));

    // revives
    Ref r2 = Ref::revive(new Obj(2), w.handler);
    EXPECT_EQ(r2->value, 2);
    auto h = r2.handler;
    r2 = nullptr;
    EXPECT_TRUE(holder.holds(w.handler));
    w = nullptr;
    EXPECT_FALSE(holder.holds(h));
}

// // ----------------------
// // 5. holder (TestHolderPolicy)
// // ----------------------

// TEST(HolderPolicy, HoldCalled)
// {
//     TestHolderPolicy holder;

//     shared_ref<Obj, TestHolderPolicy> p(new Obj(1));
//     p.set_holder(&holder);

//     EXPECT_TRUE(holder.holds(p.handler));
// }

// TEST(HolderPolicy, UnholdCalledWhenAllRefsReleased)
// {
//     TestHolderPolicy holder;

//     {
//         shared_ref<Obj, TestHolderPolicy> p(new Obj(1));
//         p.set_holder(&holder);

//         EXPECT_TRUE(holder.holds(p.handler));
//     }

//     EXPECT_TRUE(holder.held_handlers.empty());
// }

// TEST(SharedRefHolderPolicy, MoveAssignmentMustUnholdHandler)
// {
//     using Ref = shared_ref<Obj, TestHolderPolicy>;

//     TestHolderPolicy holder;

//     void *handler_addr = nullptr;

//     Ref a(new Obj(42));
//     a.set_holder(&holder);

//     handler_addr = a.handler;
//     ASSERT_NE(handler_addr, nullptr);
//     EXPECT_TRUE(holder.holds(handler_addr)) << "Holder should track handler after set_holder";

//     Ref b = nullptr;
//     a = b;

//     // 到这里，handler 应该已经被 unhold
//     EXPECT_FALSE(holder.holds(handler_addr))
//         << "Handler must be unheld when last shared_ref is released via assignment";
// }

// // ----------------------
// // 6. enable_shared_ref_from_this
// // ----------------------

// TEST(SharedFromThis, SharedFromThisWorks)
// {
//     auto p = shared_ref<SelfObj, TestHolderPolicy>(new SelfObj(77));

//     auto p2 = p->shared_from_this();

//     EXPECT_EQ(p.handler, p2.handler);
//     EXPECT_EQ(p2.handler->strong, 2);
//     EXPECT_EQ(p2->x, 77);
// }

// TEST(SharedFromThis, SharedFromThisFailsWhenNotOwned)
// {
//     SelfObj o(1);

//     EXPECT_THROW(o.shared_from_this(), std::runtime_error);
// }

// TEST(SharedFromThis, SharedFromThisExpiredAfterDeath)
// {
//     weak_ref<SelfObj, TestHolderPolicy> w;
//     {
//         auto p = shared_ref<SelfObj, TestHolderPolicy>(new SelfObj(99));
//         auto p2 = p->shared_from_this();
//         w = p2;
//         EXPECT_FALSE(w.expired());
//     }

//     EXPECT_TRUE(w.expired());
//     EXPECT_FALSE(w.lock());
// }

// TEST(SharedFromThis, WeakFromThisExpiredAfterDeath)
// {
//     weak_ref<SelfObj, TestHolderPolicy> w;

//     {
//         auto p = shared_ref<SelfObj, TestHolderPolicy>(new SelfObj(99));
//         w = p->weak_from_this();
//         EXPECT_FALSE(w.expired());
//     }

//     EXPECT_TRUE(w.expired());
//     EXPECT_FALSE(w.lock());
// }
