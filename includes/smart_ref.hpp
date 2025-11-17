#pragma once

#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <type_traits>

namespace smart_ref
{
    template <typename T, typename HolderPolicy = nullptr_t>
    struct shared_ref;

    template <typename T, typename HolderPolicy = nullptr_t>
    struct weak_ref;

    template <typename T, typename HolderPolicy = nullptr_t>
    struct enable_shared_ref_from_this;
    struct enable_ref_holder;

    namespace _
    {
        struct holder_base
        {
            void *holder = nullptr;
        };
        struct empty_base
        {
        };

        template <typename T>
        struct ref_block : std::conditional_t<std::is_base_of_v<enable_ref_holder, T>, holder_base, empty_base>
        {
            T *ptr = nullptr;
            uint32_t strong = 0;
            uint32_t weak = 0;

            ~ref_block()
            {
                if (this->ptr)
                    delete this->ptr;
            }

            void destroy_ptr()
            {
                delete this->ptr;
                this->ptr = nullptr;
            }
        };
    } // namespace _

    template <typename T, typename HolderPolicy>
    struct shared_ref
    {
    public:
        T *ptr;
        _::ref_block<T> *handler;

        shared_ref() : handler(nullptr), ptr(nullptr) {}
        shared_ref(nullptr_t) : handler(nullptr), ptr(nullptr) {}

        shared_ref(T *p)
        {
            handler = new _::ref_block<T>();
            handler->strong = 1;
            handler->ptr = p;
            ptr = p;

            if constexpr (std::is_base_of_v<enable_shared_ref_from_this<T>, T>)
            {
                p->_weak_self = *this;
            }
        }

    private:
        shared_ref(T *p, _::ref_block<T> *h) : ptr(p), handler(h) {}
        shared_ref(_::ref_block<T> *h) : handler(h)
        {
            if (h)
            {
                ptr = h->ptr;
                h->strong++;
            }
            else
                ptr = nullptr;
        }

    public:
        shared_ref(const shared_ref &other) : shared_ref() { this->_move_shared(other); }
        shared_ref(shared_ref &&other) : shared_ref() { this->_move_shared(other); }

        ~shared_ref()
        {
            if (handler)
            {
                handler->strong--;
                if (handler->strong == 0)
                {
                    if (handler->weak == 0)
                        delete handler;
                    else
                    {
                        delete handler->ptr;
                        handler->ptr = nullptr;
                    }
                }
            }
        }

    private:
        void _move_shared(const shared_ref<T, HolderPolicy> &other)
        {
            if (this->handler)
            {
                this->handler->strong--;
                if (this->handler->strong == 0)
                {
                    if (this->handler->weak == 0)
                        delete this->handler;
                    else
                    {
                        delete this->handler->ptr;
                        this->handler->ptr = nullptr;
                    }
                }
            }

            handler = other.handler;
            ptr = other.ptr;
            if (handler)
                handler->strong++;
        }

    public:
        shared_ref &operator=(const shared_ref &other)
        {
            this->_move_shared(other);
            return *this;
        }

        shared_ref &operator=(shared_ref &&other)
        {
            this->_move_shared(other);
            return *this;
        }

        static shared_ref revive(T *p, const weak_ref<T, HolderPolicy> &other)
        {
            if (other.handler == nullptr || other.handler->strong > 0)
                throw std::runtime_error("bad weak reference");
            shared_ref self{p, other.handler};
            self.handler->strong++;
            self.handler->ptr = p;
            return self;
        }

        void reset()
        {
            if (handler)
            {
                handler->strong--;
                if (handler->strong == 0 && handler->weak == 0)
                    delete handler;
                handler = nullptr;
            }
            ptr = nullptr;
        }

        void set_holder(void *holder)
        {
            if constexpr (std::is_base_of_v<enable_ref_holder, T> && !std::is_same_v<HolderPolicy, nullptr_t>)
            {
                handler->holder = holder;
                if (holder)
                    HolderPolicy::hold_ref(holder, *this);
            }
            else
            {
                static_assert(false, "T must inherit from enable_ref_holder to use set_holder");
            }
        }

        T *get() const { return ptr; }
        T *operator->() const { return ptr; }

        T &operator*() const { return *ptr; }

        operator bool() const { return ptr != nullptr; }

        friend class weak_ref<T, HolderPolicy>;
    };

    template <typename T, typename HolderPolicy>
    struct weak_ref
    {
        _::ref_block<T> *handler = nullptr;

        weak_ref() : handler(nullptr) {}
        weak_ref(nullptr_t) : handler(nullptr) {}
        weak_ref(const shared_ref<T, HolderPolicy> &other) { this->_move_ref(other); }
        weak_ref(const weak_ref<T, HolderPolicy> &other) { this->_move_ref(other); }
        weak_ref(weak_ref<T, HolderPolicy> &&other) { this->_move_ref(other); }

        weak_ref &operator=(const shared_ref<T, HolderPolicy> &other)
        {
            this->_move_ref(other);
            return *this;
        }
        weak_ref &operator=(const weak_ref<T, HolderPolicy> &other)
        {
            this->_move_ref(other);
            return *this;
        }
        weak_ref &operator=(weak_ref<T, HolderPolicy> &&other)
        {
            this->_move_ref(other);
            return *this;
        }

        weak_ref &operator=(nullptr_t)
        {
            _destroy_ref();
            handler = nullptr;
            return *this;
        }

        ~weak_ref() { _destroy_ref(); }

        void _destroy_ref()
        {
            if (handler)
            {
                handler->weak--;
                if (handler->weak == 0 && handler->strong == 0)
                {
                    delete handler;
                    return;
                }

                if constexpr (std::is_base_of_v<enable_ref_holder, T> && !std::is_same_v<HolderPolicy, nullptr_t>)
                {
                    if (handler->holder)
                    {
                        if (this->handler->weak == 1 && this->handler->strong == 0)
                        {
                            auto holder = handler->holder;
                            handler->holder = nullptr;
                            HolderPolicy::unhold_ref(holder, *this);
                        }
                    }
                }
            }
        }

        void set_holder(HolderPolicy *holder)
        {
            if constexpr (std::is_base_of_v<enable_ref_holder, T> && !std::is_same_v<HolderPolicy, nullptr_t>)
            {
                if (handler)
                {
                    handler->holder = holder;
                    if (holder)
                        HolderPolicy::hold_ref(holder, *this);
                }
            }
            else
            {
                static_assert(false, "T must inherit from enable_ref_holder to use set_holder");
            }
        }

        shared_ref<T, HolderPolicy> lock() const
        {
            if (handler == nullptr)
                return nullptr;
            return shared_ref<T, HolderPolicy>(handler);
        }

        bool operator<(const weak_ref<T, HolderPolicy> &other) const { return handler < other.handler; }

    private:
        void _move_ref(const weak_ref<T, HolderPolicy> &other)
        {
            handler = other.handler;
            if (handler)
                handler->weak++;
        }
        void _move_ref(const shared_ref<T, HolderPolicy> &other)
        {
            handler = other.handler;
            if (handler)
                handler->weak++;
        }
    };

    template <typename T, typename HolderPolicy>
    struct enable_shared_ref_from_this
    {
    private:
        weak_ref<T, HolderPolicy> _weak_self;
        friend struct shared_ref<T, HolderPolicy>;

    public:
        shared_ref<T, HolderPolicy> shared_from_this()
        {
            if (auto p = _weak_self.lock())
            {
                if (this->_weak_self.handler == nullptr)
                    throw std::runtime_error("enable_shared_ref_from_this: object is no longer owned by a shared_ref");
                shared_ref<T> s;
                s.ptr = p;
                s.handler = _weak_self.handler;
                if (s.handler)
                    s.handler->strong++;
                return s;
            }
            throw std::runtime_error("enable_shared_ref_from_this: object is no longer owned by a shared_ref");
        }

        weak_ref<T> weak_from_this() const noexcept { return _weak_self; }

        ~enable_shared_ref_from_this()
        {
            if (this->_weak_self.handler)
                this->_weak_self.handler->ptr = nullptr;
        }
    };

    struct enable_ref_holder
    {
    };

} // namespace smart_ref
