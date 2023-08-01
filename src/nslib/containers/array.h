#pragma once

#include "../basic_types.h"
#include "../mem.h"

namespace nslib
{

template<class T, sizet N>
struct static_array
{
    using iterator = T *;
    using const_iterator = const T *;
    using value_type = T;
    static inline constexpr sizet capacity = N;

    T data[N];
    sizet size{0};

    inline const T &operator[](sizet ind) const
    {
        return data[ind];
    }
    inline T &operator[](sizet ind)
    {
        return data[ind];
    }
};

template<class T>
struct array
{
    using iterator = T *;
    using const_iterator = const T *;
    using value_type = T;

    sizet size{};
    sizet capacity{};
    T* data{};
    mem_arena *arena{};

    inline const T &operator[](sizet ind) const
    {
        return data[ind];
    }
    inline T &operator[](sizet ind)
    {
        return data[ind];
    }
};

template<class T>
void arr_set_capacity(array<T> *arr, sizet new_cap)
{
    // New cap can't be any smaller than mem_nod since we are using free list allocator
    while (new_cap * sizeof(T) < sizeof(mem_node))
        ++new_cap;
    arr->data = mem_realloc(arr->data, new_cap, arr->arena);
    arr->capacity = new_cap;

    // Shrink the old size if its greater than the new capacity (so we only copy those items)
    if (arr->size > arr->capacity)
        arr->size = arr->capacity;

#if !defined(NDEBUG)
    for (int i = arr->size; i < arr->capacity; ++i)
        arr->data[i] = {};
#endif
}

template<class T>
void arr_reserve(array<T> *arr, sizet capacity)
{
    if (arr->capacity < capacity) {
        arr_set_capacity(arr, capacity);
    }
}

template<class T>
void arr_shrink_to_fit(array<T> *arr)
{
    assert(arr->size <= arr->capacity);
    if (arr->size < arr->capacity) {
        arr_set_capacity(arr, arr->size);
    }
}

template<class T>
T *arr_push_back(array<T> *arr, const T &item)
{
    nsassert(arr->size() <= arr->capacity());
    if (arr->size() == arr->capacity())
    {
        reallocate_and_copy(arr, arr->buf.cap * 2);
    }

    T *ret = &arr->buf.data[arr->size()];
    *ret = item;
    ++arr->buf.size;
    return ret;
}

template<class T, sizet N>
T *push_back(array<T, N> *arr, const T &item)
{
    if (arr->buf.size == arr->buf.cap)
        return nullptr;
    T *ret = &arr->buf.data[arr->buf.size];
    *ret = item;
    ++arr->buf.size;
    return ret;
}

template<class T, class... Args>
T *emplace_back(array<T> *arr, Args &&...args)
{
    nsassert(arr->size() <= arr->capacity());
    if (arr->size() == arr->capacity())
    {
        reallocate_and_copy(arr, arr->buf.cap * 2);
    }

    T *ret = &arr->buf.data[arr->size()];
    new (ret) T(std::forward<Args>(args)...);
    ++arr->buf.size;
    return ret;
}

template<class T, sizet N, class... Args>
T *emplace_back(array<T, N> *arr, Args &&...args)
{
    if (arr->buf.size == arr->buf.cap)
        return nullptr;
    T *ret = &arr->buf.data[arr->buf.size];
    new (ret) T(std::forward<Args>(args)...);
    ++arr->buf.size;
    return ret;
}

template<class T>
void clear_to(T *bufobj, const typename T::value_type &item)
{
    for (int i = 0; i < bufobj->buf.cap; ++i)
        bufobj->buf.data[i] = item;
}
 
template<class T>
void pop_back(T *bufobj)
{
    if (bufobj->buf.size == 0)
        return;
#ifdef DEBUG_BUILD
    bufobj->buf.data[bufobj->buf.size-1] = {};
#endif
    --bufobj->buf.size;
}

template<class T>
void clear(T *bufobj)
{
    bufobj->buf.size = 0;
#ifdef DEBUG_BUILD
    for (int i = 0; i < bufobj->buf.cap; ++i)
        bufobj->buf.data[i] = {};
#endif
}

template<class T>
typename T::value_type *back(T *bufobj)
{
    if (bufobj->buf.size > 0)
        return &bufobj->buf.data[bufobj->buf.size - 1];
    return {};
}

template<class T>
typename T::value_type *front(T *bufobj)
{
    if (bufobj->buf.size > 0)
        return &bufobj->buf.data[0];
    return {};
}

template<class T>
bool remove(T *bufobj, sizet index)
{
    if (index >= bufobj->buf.size)
        return false;

    // Copy the items after the item to delete
    for (int i = index; i < (bufobj->buf.size - 1); ++i)
    {
        bufobj->buf.data[i] = bufobj->buf.data[i + 1];
    }
    pop_back(bufobj);
    return true;
}

template<class T>
typename T::iterator find(T *bufobj, const typename T::value_type &item)
{
    auto iter = bufobj->begin();
    while (iter != bufobj->end())
    {
        if (*iter == item)
            return iter;
        ++iter;
    }
    return bufobj->end();
}


template<class T>
void resize(array<T> *arr, sizet new_size)
{
    if (arr->size() == new_size)
        return;
    
    nsassert(arr->size() <= arr->capacity());
    sizet cap = arr->buf.cap;
    if (new_size > cap)
    {
        cap = std::max(cap, (sizet)1);
        while (cap < new_size)
            cap *= 2;
        reallocate_and_copy(arr, cap);
    }

    // If in debug - clear the removed data (if any) to 0
    #ifdef DEBUG_BUILD
    for (int i = new_size; i < arr->buf.size; ++i)
        arr->buf.data[i] = {};
    #endif
    arr->buf.size = new_size;
}

template<class T>
void resize(array<T> *arr, sizet new_size, const T &copy)
{
    nsassert(arr->size() <= arr->capacity());
    sizet cap = arr->buf.cap;
    if (new_size > cap)
    {
        while (cap < new_size)
            cap *= 2;
        reallocate_and_copy(arr, cap);
    }
    for (int i = arr->buf.size; i < new_size; ++i)
        arr->buf.data[i] = copy;

    // If in debug - clear the removed data (if any) to 0
    #ifdef DEBUG_BUILD
    for (int i = new_size; i < arr->buf.size; ++i)
        arr->buf.data[i] = {};
    #endif
    arr->buf.size = new_size;
}

template<class T, class... Args>
void resize(array<T> *arr, sizet new_size, Args &&...args)
{
    nsassert(arr->size() <= arr->capacity());
    sizet cap = arr->buf.cap;
    if (new_size > cap)
    {
        while (cap < new_size)
            cap *= 2;
        reallocate_and_copy(arr, cap);
    }
    for (int i = arr->buf.size; i < new_size; ++i)
    {
        T *item = &arr->buf[i];
        new (item) T(std::forward<Args>(args)...);
    }
    // If in debug - clear the removed data (if any) to 0
    #ifdef DEBUG_BUILD
    for (int i = new_size; i < arr->buf.size; ++i)
        arr->buf.data[i] = {};
    #endif
    arr->buf.size = new_size;
}

template<class T>
typename T::iterator erase(T *bufobj, typename T::iterator iter)
{
    while (iter + 1 != bufobj->end())
    {
        *iter = *(iter + 1);
        ++iter;
    }
    pop_back(bufobj);
    return iter;
}

template<class T>
sizet index_from_ptr(T *bufobj, typename T::value_type *item)
{
    sizet offset = (item - bufobj->buf.data);
    if (offset < bufobj->buf.size)
        return offset;
    return npos;
}

} // namespace noble_steed
