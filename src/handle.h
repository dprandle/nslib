#pragma once
#include "memory.h"
#include "hashfuncs.h"
#include "basic_types.h"

namespace nslib
{

struct mem_arena;

struct ref_counter
{
    u32 cnt;
};

template<class T>
using handle_obj_terminate_func = void(T *item);

template<class T>
struct handle
{
    handle()
    {}

    handle(const handle<T> &copy)
        : ptr(copy.ptr),
          tfunc(copy.tfunc),
          owner(copy.owner),
          handle_ref(copy.handle_ref),
          item_arena(copy.item_arena),
          handle_arena(copy.handle_arena)
    {
        if (handle_ref) {
            ++handle_ref->cnt;
        }
    }

    handle<T> &operator=(handle<T> rhs)
    {
        std::swap(ptr, rhs.ptr);
        std::swap(tfunc, rhs.tfunc);
        std::swap(owner, rhs.owner);
        std::swap(handle_ref, rhs.handle_ref);
        std::swap(item_arena, rhs.item_arena);
        std::swap(handle_arena, rhs.handle_arena);
        return *this;
    }

    ~handle()
    {
        if (handle_ref) {
            --handle_ref->cnt;
            if (handle_ref->cnt == 0) {
                if (tfunc) {
                    tfunc(ptr);
                }
                mem_free(ptr, item_arena);
                mem_free(handle_ref, handle_arena);
            }
        }
    }

    T *ptr{};
    handle_obj_terminate_func<T> *tfunc;
    void *owner{};
    ref_counter *handle_ref{};
    mem_arena *item_arena{};
    mem_arena *handle_arena{};

    operator bool() const
    {
        return ptr;
    }
    T *operator->()
    {
        return ptr;
    }
    T *operator->() const
    {
        return ptr;
    }
    T &operator*()
    {
        return *ptr;
    }
    T &operator*() const
    {
        return *ptr;
    }
};

template<class T>
handle<T> make_handle(T *ptr, handle_obj_terminate_func<T> *tfunc, void *owner, mem_arena *item_arena, mem_arena *handle_arena)
{
    handle<T> ret;
    ret.ptr = ptr;
    ret.tfunc = tfunc;
    ret.owner = owner;
    ret.handle_ref = mem_calloc<ref_counter>(1, handle_arena);
    ret.item_arena = item_arena;
    ret.handle_arena = handle_arena;
    return ret;
}

op_eq_func_tt(handle)
{
    return (lhs.ptr == rhs.ptr);
}

op_neq_func_tt(handle);

template<class T>
inline bool operator==(const handle<T> &lhs, const T *rhs)
{
    return (lhs.ptr == rhs);
}

template<class T>
inline bool operator!=(const handle<T> &lhs, const T *rhs)
{
    return !(lhs == rhs);
}

template<class T>
inline bool operator==(const T *lhs, const handle<T> &rhs)
{
    return (rhs == lhs);
}

template<class T>
inline bool operator!=(const T *lhs, const handle<T> &rhs)
{
    return (rhs != lhs);
}

template<class T>
inline u64 hash_type(const handle<T> &key, u64 s0, u64 s1)
{
    return hash_type((sizet)key.open_ref, s0, s1);
}

} // namespace nslib
