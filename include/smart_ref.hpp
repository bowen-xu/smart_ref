/*
 * Author: Bowen Xu
 * E-mail: bowenxu.agi@gmail.com
 *
 * MIT License
 *
 * Copyright (c) 2025 Bowen Xu
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#pragma once

#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <type_traits>
#include <cassert>
#include <format>
#include <memory>

// using std::enable_shared_from_this;
using std::shared_ptr;

namespace smart_ref
{
    template <typename T, typename HolderPolicy = nullptr_t>
    struct shared_ref;

    // struct enable_ref_holder
    // {
    // };

    template <typename T, typename HolderPolicy = nullptr_t>
    struct weak_ref;

    // template <typename T, typename HolderPolicy = nullptr_t>
    // struct enable_shared_ref_from_this;

    namespace _
    {
        // struct holder_base
        // {
        //     void *holder = nullptr;
        // };
        // struct empty_base
        // {
        // };

        // template <typename T>
        struct ref_block //: std::conditional_t<std::is_base_of_v<enable_ref_holder, T>, holder_base, empty_base>
        {
            // T *ptr = nullptr;
            void *ptr = nullptr;
            uint32_t strong = 0;
            uint32_t weak = 0;

            void *holder = nullptr;

            ref_block() = default;

            ~ref_block()
            {
                //  LOGD << std::format("Destory: {}", (void *)this);
            }

            void reset_holder()
            {

                this->holder = nullptr;
                // if constexpr (std::is_base_of_v<enable_ref_holder, T>)
                // {
                //     this->holder = nullptr;
                // }
                // else
                //     static_assert(false, "T must inherit from enable_ref_holder to use reset_holder");
            }

            bool empty() const { return ptr == nullptr; }
        };
    } // namespace _

    template <typename T, typename HolderPolicy>
    struct shared_ref
    {
    public:
        using element_type = T;
        // using handler_type = _::ref_block<T>;
        using handler_type = _::ref_block;
        using holder_type = HolderPolicy;

        T *ptr;
        handler_type *handler;

        shared_ref() : handler(nullptr), ptr(nullptr) {}
        shared_ref(nullptr_t) : handler(nullptr), ptr(nullptr) {}

        shared_ref(T *p)
        {
            if (p == nullptr)
            {
                handler = nullptr;
                ptr = nullptr;
                return;
            }
            handler = new handler_type();
            handler->strong = 1;
            handler->ptr = p;
            ptr = p;

            // if constexpr (std::is_base_of_v<enable_shared_ref_from_this<T, HolderPolicy>, T>)
            // {
            //     p->_weak_self = *this;
            // }
        }

    private:
        template <typename U>
        // requires((std::is_base_of_v<enable_ref_holder, T> && std::is_base_of_v<enable_ref_holder, U>) ||
        //          (!std::is_base_of_v<enable_ref_holder, T> && !std::is_base_of_v<enable_ref_holder, U>))
        shared_ref(const shared_ref<U, HolderPolicy> &other, T *p) noexcept
            // : ptr(p), handler(reinterpret_cast<handler_type *>(other.handler))
            : ptr(p), handler(other.handler)

        {
            // assume ptr==nullptr && handler==nullptr, or handler!=nullptr && ptr==handler->ptr
            if (handler && ptr)
            {
                handler->strong++;
            }
        }

        shared_ref(handler_type *h) /* only called by weak_ref::lock() */
        {
            if (h && h->ptr)
            {
                this->handler = h;
                this->ptr = static_cast<T *>(h->ptr);
                h->strong++;
            }
            else
            {
                this->ptr = nullptr;
                this->handler = nullptr;
            }
        }

        shared_ref(T *p, handler_type *h) : ptr(p), handler(h) {} /* only called by shared_ref::revive */

    public:
        shared_ref(const shared_ref &other) : shared_ref() { this->_copy_shared(other); }

    public:
        ~shared_ref() { this->_destroy_ref(); }

    public:
        shared_ref &operator=(const shared_ref &other)
        {
            this->_copy_shared(other);
            return *this;
        }

        shared_ref &operator=(nullptr_t)
        {
            this->reset();
            return *this;
        }

    private:
        static void _release_handler(handler_type *&handler)
        {
            if (handler)
            {
                handler->strong--;
                if (handler->strong == 0)
                {
                    if (handler->weak == 0)
                    {
                        // if constexpr (std::is_base_of_v<enable_ref_holder, T> && !std::is_same_v<HolderPolicy,
                        // nullptr_t>)
                        if constexpr (!std::is_same_v<HolderPolicy, nullptr_t>)
                        {
                            if (handler->holder)
                            {
                                auto holder = handler->holder;
                                handler->holder = nullptr;
                                HolderPolicy::unhold_ref(holder, static_cast<void *>(handler));
                            }
                        }
                        delete static_cast<T *>(handler->ptr);
                        delete handler;
                        handler = nullptr;
                    }
                    else
                    {
                        delete static_cast<T *>(handler->ptr);
                        handler->ptr = nullptr;
                    }
                }
            }
        }

        void _destroy_ref() { _release_handler(this->handler); }

    private:
        void _copy_shared(const shared_ref<T, HolderPolicy> &other)
        {
            if (this->handler == other.handler)
                return;
            auto old_handler = this->handler;

            this->handler = other.handler;
            this->ptr = other.ptr;
            if (this->handler)
                this->handler->strong++;

            _release_handler(old_handler);
        }

    public:
        // static shared_ref revive(T *p, const weak_ref<T, HolderPolicy> &other)
        // {
        //     if (other.handler == nullptr)
        //         throw std::runtime_error("bad weak reference");
        //     if (other.handler->strong > 0)
        //         throw std::runtime_error("revive a living reference");
        //     return revive(p, other.handler);
        // }

        static shared_ref revive(T *p, handler_type *other)
        {
            /* Make sure
             * 1. p != nullptr
             * 2. other != nullptr
             * 3. other->ptr == nullptr
             */
            if (p == nullptr || other == nullptr || other->ptr != nullptr)
                throw std::runtime_error("Cannot revive due to invalid parameters");
            shared_ref self{p, other};
            self.handler->strong++;
            self.handler->ptr = p;
            return self;
        }

        void reset()
        {
            this->_destroy_ref();
            this->handler = nullptr;
            this->ptr = nullptr;
        }

        auto set_holder(void *holder)
        {
            // if constexpr (std::is_base_of_v<enable_ref_holder, T> && !std::is_same_v<HolderPolicy, nullptr_t>)
            if constexpr (!std::is_same_v<HolderPolicy, nullptr_t>)
            {
                if (!handler)
                    throw std::runtime_error("Cannot set holder on empty shared_ref");
                handler->holder = holder;
                if (holder)
                    HolderPolicy::hold_ref(holder, *this);
            }
            else
            {
                static_assert(false, "T must inherit from enable_ref_holder to use set_holder");
            }
        }
        T *get() const { return static_cast<T *>(ptr); }
        T *operator->() const { return static_cast<T *>(ptr); }

        T &operator*() const { return *static_cast<T *>(ptr); }

        operator bool() const { return ptr != nullptr; }

        friend class weak_ref<T, HolderPolicy>;
        template <typename _T, typename _U, typename _HolderPolicy>
        friend smart_ref::shared_ref<_T, _HolderPolicy>
        std::static_pointer_cast(const smart_ref::shared_ref<_U, _HolderPolicy> &r) noexcept;
        template <typename _T, typename _U, typename _HolderPolicy>
        friend smart_ref::shared_ref<_T, _HolderPolicy>
        std::dynamic_pointer_cast(const smart_ref::shared_ref<_U, _HolderPolicy> &r) noexcept;
    };

    template <typename T, typename HolderPolicy>
    struct weak_ref
    {
        using element_type = T;
        using handler_type = _::ref_block;

        handler_type *handler;

        weak_ref() : handler(nullptr) {}
        weak_ref(nullptr_t) : weak_ref() {}
        weak_ref(const shared_ref<T, HolderPolicy> &other) { this->_copy_ref(other); }
        weak_ref(const weak_ref<T, HolderPolicy> &other) { this->_copy_ref(other); }

        weak_ref &operator=(nullptr_t)
        {
            _destroy_ref();
            handler = nullptr;
            return *this;
        }
        weak_ref &operator=(const shared_ref<T, HolderPolicy> &other)
        {
            this->_copy_ref(other);
            return *this;
        }
        weak_ref &operator=(const weak_ref<T, HolderPolicy> &other)
        {
            this->_copy_ref(other);
            return *this;
        }

        ~weak_ref() { _destroy_ref(); }

        void _destroy_ref()
        {
            if (this->handler)
            {
                this->handler->weak--;
                if (this->handler->weak == 0 && this->handler->strong == 0)
                {
                    if constexpr (!std::is_same_v<HolderPolicy, nullptr_t>)
                    {
                        if (this->handler->holder)
                        {
                            auto holder = handler->holder;
                            handler->holder = nullptr;
                            HolderPolicy::unhold_ref(holder, static_cast<void *>(this->handler));
                        }
                    }
                    delete this->handler;
                    this->handler = nullptr;
                }
            }
        }

        //     void set_holder(HolderPolicy *holder)
        //     {
        //         if constexpr (std::is_base_of_v<enable_ref_holder, T> && !std::is_same_v<HolderPolicy, nullptr_t>)
        //         {
        //             if (handler)
        //             {
        //                 handler->holder = holder;
        //                 if (holder)
        //                 {
        //                     auto s = this->lock();
        //                     if (s)
        //                         HolderPolicy::hold_ref(holder, s);
        //                 }
        //             }
        //         }
        //         else
        //         {
        //             static_assert(false, "T must inherit from enable_ref_holder to use set_holder");
        //         }
        //     }

        shared_ref<T, HolderPolicy> lock() const
        {
            if (this->expired())
                return nullptr;
            return shared_ref<T, HolderPolicy>(handler);
        }

        bool expired() const { return handler == nullptr || handler->ptr == nullptr; }

    private:
        void _copy_ref(const weak_ref<T, HolderPolicy> &other)
        {
            if (this->handler == other.handler)
                return;
            handler = other.handler;
            if (handler)
                handler->weak++;
        }
        void _copy_ref(const shared_ref<T, HolderPolicy> &other)
        {
            if (this->handler == other.handler)
                return;
            handler = other.handler;
            if (handler)
                handler->weak++;
        }
    };

    // template <typename T, typename HolderPolicy>
    // struct enable_shared_ref_from_this
    // {
    // private:
    //     mutable weak_ref<T, HolderPolicy> _weak_self;
    //     friend struct shared_ref<T, HolderPolicy>;

    // public:
    //     shared_ref<T, HolderPolicy> shared_from_this()
    //     {
    //         if (auto p = _weak_self.lock())
    //         {
    //             if (this->_weak_self.handler == nullptr)
    //                 throw std::runtime_error("enable_shared_ref_from_this: object is no longer owned by a
    //                 shared_ref");
    //             return p;
    //         }
    //         throw std::runtime_error("enable_shared_ref_from_this: object is no longer owned by a shared_ref");
    //     }

    //     weak_ref<T, HolderPolicy> weak_from_this() const noexcept { return _weak_self; }

    //     ~enable_shared_ref_from_this()
    //     {
    //         // if (this->_weak_self.handler)
    //         // {
    //         //     this->_weak_self.handler->ptr = nullptr;
    //         // }
    //     }
    // };

} // namespace smart_ref

namespace smart_ref
{
    template <class _Tp, class H>
    inline bool operator==(const shared_ref<_Tp, H> &__x, nullptr_t) noexcept
    {
        return !__x;
    }

    template <class _Tp, class H>
    inline bool operator==(nullptr_t, const shared_ref<_Tp, H> &__x) noexcept
    {
        return !__x;
    }

    template <class _Tp, class H>
    inline bool operator!=(const shared_ref<_Tp, H> &__x, nullptr_t) noexcept
    {
        return static_cast<bool>(__x);
    }

    template <class _Tp, class H>
    inline bool operator!=(nullptr_t, const shared_ref<_Tp, H> &__x) noexcept
    {
        return static_cast<bool>(__x);
    }

    template <class _Tp, class H>
    inline bool operator<(const shared_ref<_Tp, H> &__x, nullptr_t) noexcept
    {
        return less<typename shared_ref<_Tp, H>::element_type *>()(__x.get(), nullptr);
    }

    template <class _Tp, class H>
    inline bool operator<(nullptr_t, const shared_ref<_Tp, H> &__x) noexcept
    {
        return less<typename shared_ref<_Tp, H>::element_type *>()(nullptr, __x.get());
    }

    template <class _Tp, class H>
    inline bool operator>(const shared_ref<_Tp, H> &__x, nullptr_t) noexcept
    {
        return nullptr < __x;
    }

    template <class _Tp, class H>
    inline bool operator>(nullptr_t, const shared_ref<_Tp, H> &__x) noexcept
    {
        return __x < nullptr;
    }

    template <class _Tp, class H>
    inline bool operator<=(const shared_ref<_Tp, H> &__x, nullptr_t) noexcept
    {
        return !(nullptr < __x);
    }

    template <class _Tp, class H>
    inline bool operator<=(nullptr_t, const shared_ref<_Tp, H> &__x) noexcept
    {
        return !(__x < nullptr);
    }

    template <class _Tp, class H>
    inline bool operator>=(const shared_ref<_Tp, H> &__x, nullptr_t) noexcept
    {
        return !(__x < nullptr);
    }

    template <class _Tp, class H>
    inline bool operator>=(nullptr_t, const shared_ref<_Tp, H> &__x) noexcept
    {
        return !(nullptr < __x);
    }

    template <class _Tp, class _Up, class TH, class UH>
    inline bool operator==(const shared_ref<_Tp, TH> &__x, const shared_ref<_Up, UH> &__y) noexcept
    {
        return __x.get() == __y.get();
    }

    template <class _Tp, class _Up, class TH, class UH>
    inline bool operator!=(const shared_ref<_Tp, TH> &__x, const shared_ref<_Up, UH> &__y) noexcept
    {
        return !(__x == __y);
    }

    template <class _Tp, class _Up, class TH, class UH>
    inline bool operator<(const shared_ref<_Tp, TH> &__x, const shared_ref<_Up, UH> &__y) noexcept
    {
        return __x.get() < __y.get();
    }
    template <class _Tp, class _Up, class TH, class UH>
    inline bool operator>(const shared_ref<_Tp, TH> &__x, const shared_ref<_Up, UH> &__y) noexcept
    {
        return __y < __x;
    }

    template <class _Tp, class _Up, class TH, class UH>
    inline bool operator<=(const shared_ref<_Tp, TH> &__x, const shared_ref<_Up, UH> &__y) noexcept
    {
        return !(__y < __x);
    }

    template <class _Tp, class _Up, class TH, class UH>
    inline bool operator>=(const shared_ref<_Tp, TH> &__x, const shared_ref<_Up, UH> &__y) noexcept
    {
        return !(__x < __y);
    }

} // namespace smart_ref

namespace std
{
    // reference: https://en.cppreference.com/w/cpp/memory/shared_ptr/pointer_cast.html
    template <typename T, typename U, typename HolderPolicy>
    smart_ref::shared_ref<T, HolderPolicy> static_pointer_cast(const smart_ref::shared_ref<U, HolderPolicy> &r) noexcept
    {
        auto *p = static_cast<typename smart_ref::shared_ref<T, HolderPolicy>::element_type *>(r.get());
        return smart_ref::shared_ref<T, HolderPolicy>(r, p);
    }

    template <class T, class U, typename HolderPolicy>
    smart_ref::shared_ref<T, HolderPolicy>
    dynamic_pointer_cast(const smart_ref::shared_ref<U, HolderPolicy> &r) noexcept
    {
        if (auto p = dynamic_cast<typename smart_ref::shared_ref<T, HolderPolicy>::element_type *>(r.get()))
            return smart_ref::shared_ref<T, HolderPolicy>{r, p};
        else
            return smart_ref::shared_ref<T, HolderPolicy>{};
    }

    //     template <class T, class U, typename HolderPolicy>
    //     smart_ref::shared_ref<T, HolderPolicy> const_pointer_cast(const smart_ref::shared_ref<U, HolderPolicy>
    //     &r) noexcept
    //     {
    //         auto p = const_cast<typename smart_ref::shared_ref<T, HolderPolicy>::element_type *>(r.get());
    //         return smart_ref::shared_ref<T, HolderPolicy>{r, p};
    //     }

    //     template <class T, class U, typename HolderPolicy>
    //     smart_ref::shared_ref<T, HolderPolicy>
    //     reinterpret_pointer_cast(const smart_ref::shared_ref<U, HolderPolicy> &r) noexcept
    //     {
    //         auto p = reinterpret_cast<typename smart_ref::shared_ref<T, HolderPolicy>::element_type *>(r.get());
    //         return smart_ref::shared_ref<T, HolderPolicy>{r, p};
    //     }
} // namespace std

namespace std
{
    template <typename T, typename H>
    struct hash<smart_ref::shared_ref<T, H>>
    {
        size_t operator()(const smart_ref::shared_ref<T, H> &sha) const { return std::hash<T *>()(sha.get()); }
    };
} // namespace std