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
    T *data{};
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
void arr_init(array<T> *arr, mem_arena *arena, sizet initial_capacity=0)
{
    arr->arena = arena;
    arr_set_capacity(arr, initial_capacity);
}

template<class T>
void arr_terminate(array<T> *arr)
{
    mem_free(arr->data, arr->arena);
    *arr = {};
}

template<class T>
typename T::iterator arr_begin(T *arrobj)
{
    return &arrobj->data[0];
}

template<class T>
typename T::iterator arr_end(T *arrobj)
{
    return arr_begin(arrobj) + arrobj->size;
}

template<class T>
typename T::const_iterator arr_begin(const T *arrobj)
{
    return &arrobj->data[0];
}

template<class T>
typename T::const_iterator arr_end(const T *arrobj)
{
    return arr_begin(arrobj) + arrobj->size;
}

template<class T>
void arr_copy(array<T> *dest, const array<T> *source)
{
    arr_resize(dest, source->size);
    for (sizet i = 0; i < source->size; ++i) {
        dest[i] = source[i];
    }
}

template<class T>
void arr_set_capacity(array<T> *arr, sizet new_cap)
{
    // New cap can't be any smaller than mem_nod since we are using free list allocator
    while (new_cap * sizeof(T) < sizeof(mem_node))
        ++new_cap;
    arr->data = (T*)mem_realloc(arr->data, new_cap*sizeof(T), arr->arena);
    arr->capacity = new_cap;

    // Shrink the old size if its greater than the new capacity (so we only copy those items)
    if (arr->size > arr->capacity)
        arr->size = arr->capacity;

#if !defined(NDEBUG)
    for (sizet i = arr->size; i < arr->capacity; ++i)
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
    assert(arr->size <= arr->capacity);
    if (arr->size == arr->capacity) {
        arr_set_capacity(arr, arr->capacity * 2);
    }

    T *ret = &arr->data[arr->size];
    *ret = item;
    ++arr->buf.size;
    return ret;
}

template<class T, sizet N>
T *arr_push_back(static_array<T, N> *arr, const T &item)
{
    if (arr->size == arr->capacity)
        return nullptr;
    T *ret = &arr->data[arr->size];
    *ret = item;
    ++arr->size;
    return ret;
}

template<class T, class... Args>
T *arr_emplace_back(array<T> *arr, Args &&...args)
{
    assert(arr->size <= arr->capacity);
    if (arr->size == arr->capacity) {
        arr_set_capacity(arr, arr->capacity * 2);
    }

    T *ret = &arr->data[arr->size];
    new (ret) T(std::forward<Args>(args)...);
    ++arr->size;
    return ret;
}

template<class T, sizet N, class... Args>
T *arr_emplace_back(static_array<T, N> *arr, Args &&...args)
{
    if (arr->size == arr->capacity)
        return nullptr;
    T *ret = &arr->data[arr->size];
    new (ret) T(std::forward<Args>(args)...);
    ++arr->size;
    return ret;
}

template<class T>
void arr_clear_to(T *bufobj, const typename T::value_type &item)
{
    for (int i = 0; i < bufobj->capacity; ++i)
        bufobj->data[i] = item;
}

template<class T>
void arr_pop_back(T *bufobj)
{
    if (bufobj->size == 0)
        return;
#if !defined(NDEBUG)
    bufobj->data[bufobj->size - 1] = {};
#endif
    --bufobj->size;
}

template<class T>
void arr_clear(T *bufobj)
{
    bufobj->size = 0;
#if !defined(NDEBUG)
    for (int i = 0; i < bufobj->capacity; ++i)
        bufobj->data[i] = {};
#endif
}

template<class T>
typename T::value_type *arr_back(T *bufobj)
{
    if (bufobj->size > 0)
        return &bufobj->data[bufobj->size - 1];
    return {};
}

template<class T>
typename T::value_type *arr_front(T *bufobj)
{
    if (bufobj->size > 0)
        return &bufobj->data[0];
    return {};
}

template<class T>
typename T::iterator arr_find(T *bufobj, const typename T::value_type &item)
{
    auto iter = arr_begin(bufobj);
    while (iter != arr_end(bufobj)) {
        if (*iter == item) {
            return iter;
        }
        ++iter;
    }
    return iter;
}

template<class T>
void arr_resize(array<T> *arr, sizet new_size)
{
    if (arr->size == new_size)
        return;

    // Make sure our current size doesn't exceed the capacity - it shouldnt that would definitely be a bug if it did.
    assert(arr->size <= arr->capacity);

    sizet cap = arr->capacity;
    if (new_size > cap) {
        if (cap < 1) {
            cap = 1;
        }
        while (cap < new_size)
            cap *= 2;
        arr_set_capacity(arr, cap);
    }

// If in debug - clear the removed data (if any) to 0
#if !defined(NDEBUG)
    for (sizet i = new_size; i < arr->size; ++i)
        arr->data[i] = {};
#endif
    arr->size = new_size;
}

template<class T>
void arr_resize(array<T> *arr, sizet new_size, T default_val)
{
    assert(arr->size <= arr->capacity);
    sizet cap = arr->capacity;
    if (new_size > cap) {
        if (cap < 1) {
            cap = 1;
        }
        while (cap < new_size)
            cap *= 2;
        arr_set_capacity(arr, cap);
    }
    for (int i = arr->buf.size; i < new_size; ++i) {
        arr->buf.data[i] = default_val;
    }

// If in debug - clear the removed data (if any) to 0
#if !defined(NDEBUG)
    for (int i = new_size; i < arr->size; ++i)
        arr->data[i] = {};
#endif
    arr->size = new_size;
}

template<class T>
typename T::iterator arr_erase(T *bufobj, typename T::iterator iter)
{
    if (iter == arr_end(bufobj)) {
        return iter;
    }
    auto copy_iter = iter + 1;
    while (copy_iter != arr_end(bufobj)) {
        *(copy_iter - 1) = *copy_iter;
        ++copy_iter;
    }

    arr_pop_back(bufobj);
    return iter;
}

template<class T>
typename T::iterator arr_erase(T *bufobj, typename T::iterator first, typename T::iterator last)
{
    assert(first >= arr_begin(bufobj));
    assert(last <= arr_end(bufobj));

    // Shift all items after the range over
    int i = 0;
    while ((first + i) <= last && (last + i) != arr_end(bufobj)) {
        *(first + i) = *(last + i);
        ++i;
    }
    sizet reduce_size = (last - first);
    if (last != arr_end(bufobj)) {
        ++reduce_size;
    }
    arr_resize(bufobj, bufobj->size - reduce_size);
    return first;
}

// Remove the item at index by copying the last item in the array to its spot and popping the last item. This does not
// preserve the order of the array.
template<class T>
bool arr_remove(T *bufobj, sizet index)
{
    if (index >= bufobj->size)
        return false;

    bufobj->data[index] = *arr_back(bufobj);
    arr_pop_back(bufobj);
    return true;
}

template<class T>
sizet arr_index_of(T *bufobj, typename T::value_type *item)
{
    sizet offset = (item - bufobj->data);
    if (offset < bufobj->size) {
        return offset;
    }
    return npos;
}

} // namespace nslib
