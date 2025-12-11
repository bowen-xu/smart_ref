#include <cstddef>
#include <gtest/gtest.h>
#include <iostream>
#include <smart_ref.hpp>
#include <stdexcept>
#include "TestHolderPolicy.hpp"

using namespace smart_ref;

struct Obj : enable_ref_holder
{
    int value;
    Obj(int v) : value(v) {}
    virtual ~Obj() {}
};

struct DerivedObj : Obj
{
    DerivedObj(int v) : Obj(v) {}
};

struct SelfObj : enable_shared_ref_from_this<SelfObj, TestHolderPolicy>, enable_ref_holder
{
    int x;
    SelfObj(int x) : x(x) {}
};

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

    EXPECT_FALSE(holder.holds(handler_addr))
        << "Handler must be unheld when last shared_ref is released via assignment";
}

// ----------------------
// 6. enable_shared_ref_from_this
// ----------------------

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

#include <functional>

// ----------------------
// 7. pointer_cast
// ----------------------

TEST(SharedRefCast, DynamicPointerCastSuccess)
{
    auto holder = TestHolderPolicy();
    shared_ref<DerivedObj, TestHolderPolicy> derived(new DerivedObj(123));
    ASSERT_NO_THROW(derived.set_holder(&holder));
    auto h = derived.handler;

    shared_ref<Obj, TestHolderPolicy> base = std::static_pointer_cast<Obj>(derived);
    EXPECT_EQ(base.handler, h);

    auto cast_back = std::dynamic_pointer_cast<DerivedObj>(base);
    ASSERT_TRUE(cast_back);
    EXPECT_EQ(cast_back->value, 123);
    EXPECT_EQ(cast_back.handler->strong, 3); // derived, base, cast_back

    EXPECT_TRUE(holder.holds(h));
}

TEST(SharedRefCast, DynamicPointerCastFailureReturnsEmpty)
{
    shared_ref<Obj, TestHolderPolicy> base(new Obj(10));
    auto holder = TestHolderPolicy();
    base.set_holder(&holder);

    auto d = std::dynamic_pointer_cast<DerivedObj>(base);
    EXPECT_FALSE(d);
    EXPECT_EQ(d.get(), nullptr);
    EXPECT_EQ(d.handler, nullptr);
}

TEST(SharedRefCast, ConstPointerCast)
{
    shared_ref<Obj, TestHolderPolicy> p(new Obj(55));
    const shared_ref<Obj, TestHolderPolicy> cp = p;

    auto non_const = std::const_pointer_cast<Obj>(cp);
    ASSERT_TRUE(non_const);
    non_const->value = 99;
    EXPECT_EQ(p->value, 99);
}

TEST(SharedRefCast, ReinterpretPointerCastDoesNotChangeHandler)
{
    shared_ref<Obj, TestHolderPolicy> p(new Obj(1));
    auto h = p.handler;

    auto rp = std::reinterpret_pointer_cast<Obj>(p);
    EXPECT_EQ(rp.handler, h);
    EXPECT_EQ(rp.get(), p.get());
    EXPECT_EQ(h->strong, 2);

    rp = nullptr;
    EXPECT_EQ(h->strong, 1);
}

// ----------------------
// 8. weak_ref copy and assignment
// ----------------------

TEST(SmartRefWeak, CopyAndAssignWeakRef)
{
    using W = weak_ref<Obj, TestHolderPolicy>;
    shared_ref<Obj, TestHolderPolicy> s(new Obj(5));

    W w1 = s;
    EXPECT_EQ(w1.handler->weak, 1);

    W w2(w1);
    EXPECT_EQ(w1.handler->weak, 2);
    EXPECT_EQ(w1.handler, w2.handler);

    W w3;
    w3 = w2;
    EXPECT_EQ(w1.handler->weak, 3);
    EXPECT_EQ(w3.handler, w1.handler);

    // 自赋值不应改变 weak 计数
    w3 = w3;
    EXPECT_EQ(w1.handler->weak, 3);
}

TEST(SmartRefWeak, ConstructFromNullSharedRefGivesExpiredWeak)
{
    using Ref = shared_ref<Obj, TestHolderPolicy>;
    using WRef = weak_ref<Obj, TestHolderPolicy>;

    Ref r(nullptr);
    WRef w = r;

    EXPECT_TRUE(w.expired());
    EXPECT_EQ(w.handler, nullptr);

    auto locked = w.lock();
    EXPECT_FALSE(locked);
    EXPECT_EQ(locked.handler, nullptr);
}

// ----------------------
// 9. revive edge cases
// ----------------------

TEST(SmartRefRevive, ReviveThrowsOnNullHandler)
{
    using S = shared_ref<Obj, TestHolderPolicy>;
    S s(new Obj(1));
    auto handler = s.handler;
    ASSERT_NE(handler, nullptr);

    // other == nullptr
    EXPECT_THROW(S::revive(new Obj(2), nullptr), std::runtime_error);
}

TEST(SmartRefRevive, ReviveThrowsOnNullPointer)
{
    using S = shared_ref<Obj, TestHolderPolicy>;
    S s(new Obj(1));
    auto handler = s.handler;
    ASSERT_NE(handler, nullptr);

    EXPECT_THROW(S::revive(nullptr, handler), std::runtime_error);
}

TEST(SmartRefRevive, ReviveThrowsIfHandlerStillOwnsObject)
{
    using S = shared_ref<Obj, TestHolderPolicy>;
    S s(new Obj(1));
    auto handler = s.handler;
    ASSERT_NE(handler, nullptr);
    ASSERT_NE(handler->ptr, nullptr);
    ASSERT_GT(handler->strong, 0u);

    Obj *raw = new Obj(2);
    try
    {
        S::revive(raw, handler);
        FAIL() << "Expected std::runtime_error";
    }
    catch (const std::runtime_error &)
    {
        delete raw;
    }
}

// ----------------------
// 10. Comparison operators
// ----------------------

TEST(SharedRefCompare, EqualityAndOrdering)
{
    shared_ref<Obj, TestHolderPolicy> a(new Obj(1));
    shared_ref<Obj, TestHolderPolicy> b = a;
    shared_ref<Obj, TestHolderPolicy> c(new Obj(1));
    shared_ref<Obj, TestHolderPolicy> n(nullptr);

    EXPECT_TRUE(a == b);
    EXPECT_FALSE(a != b);

    EXPECT_TRUE(a != c);
    EXPECT_FALSE(a == c);

    EXPECT_TRUE(n == nullptr);
    EXPECT_TRUE(nullptr == n);
    EXPECT_FALSE(a == nullptr);
    EXPECT_FALSE(nullptr == a);

    // As long as it doesn't crash, the order is determined by pointer addresses
    (void)(a < c);
    (void)(c < a);
    (void)(a <= b);
    (void)(a >= b);
}

TEST(SharedRefCompare, CompareWithNullptr)
{
    shared_ref<Obj, TestHolderPolicy> p;
    EXPECT_TRUE(p == nullptr);
    EXPECT_TRUE(nullptr == p);
    EXPECT_FALSE(p != nullptr);
    EXPECT_FALSE(nullptr != p);

    shared_ref<Obj, TestHolderPolicy> q(new Obj(1));
    EXPECT_TRUE(q != nullptr);
    EXPECT_TRUE(nullptr != q);
}

// ----------------------
// 11. std::hash support
// ----------------------

TEST(SharedRefHash, HashUsesUnderlyingPointer)
{
    shared_ref<Obj, TestHolderPolicy> p1(new Obj(1));
    shared_ref<Obj, TestHolderPolicy> p2 = p1;
    shared_ref<Obj, TestHolderPolicy> p3(new Obj(1));

    std::hash<shared_ref<Obj, TestHolderPolicy>> h;

    auto h1 = h(p1);
    auto h2 = h(p2);
    auto h3 = h(p3);

    EXPECT_EQ(h1, h2);
}

// ----------------------
// 12. set_holder edge cases
// ----------------------

TEST(HolderPolicy, SetHolderOnResetRefThrows)
{
    TestHolderPolicy holder;
    shared_ref<Obj, TestHolderPolicy> p(new Obj(1));
    p.set_holder(&holder);

    p.reset();
    EXPECT_EQ(p.get(), nullptr);
    EXPECT_EQ(p.handler, nullptr);

    EXPECT_THROW(p.set_holder(&holder), std::runtime_error);
}

TEST(HolderPolicy, SetHolderTwiceKeepsTracked)
{
    TestHolderPolicy holder;

    shared_ref<Obj, TestHolderPolicy> p(new Obj(1));
    p.set_holder(&holder);
    auto h = p.handler;
    ASSERT_TRUE(holder.holds(h));

    // Calling set_holder again with the same holder should not crash and should still be tracked
    EXPECT_NO_THROW(p.set_holder(&holder));
    EXPECT_TRUE(holder.holds(h));
}

// ----------------------
// 13. enable_shared_ref_from_this and weak_ref
// ----------------------

TEST(SharedFromThis, WeakFromThisTracksSameObject)
{
    auto p = shared_ref<SelfObj, TestHolderPolicy>(new SelfObj(42));
    auto w1 = p->weak_from_this();
    auto w2 = p->weak_from_this();

    EXPECT_FALSE(w1.expired());
    EXPECT_FALSE(w2.expired());

    auto s1 = w1.lock();
    auto s2 = w2.lock();
    ASSERT_TRUE(s1);
    ASSERT_TRUE(s2);

    EXPECT_EQ(s1.handler, s2.handler);
    EXPECT_EQ(s1->x, 42);
    EXPECT_EQ(s2->x, 42);
}

TEST(SharedFromThis, SharedFromThisThrowsAfterReset)
{
    auto p = shared_ref<SelfObj, TestHolderPolicy>(new SelfObj(5));
    SelfObj *raw = p.get();
    auto w = p->weak_from_this();

    p.reset();
    EXPECT_TRUE(w.expired());

    EXPECT_THROW(raw->shared_from_this(), std::runtime_error);
}
