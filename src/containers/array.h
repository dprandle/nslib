#pragma once

#include <utility>
#include <algorithm>

#include "../archive_common.h"
#include "../memory.h"

namespace nslib
{
template<class T, sizet N>
struct static_array
{
    using iterator = T *;
    using const_iterator = const T *;
    using value_type = T;
    static inline constexpr sizet capacity = N;

    T data[N]{};
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

    mem_arena *arena{};
    T *data{};
    sizet size{};
    sizet capacity{};
    sizet mem_alignment{};

    array(mem_arena *arena = mem_global_arena(), sizet initial_capacity = 0, sizet mem_alignment = DEFAULT_MIN_ALIGNMENT)
    {
        arr_init(this, arena, initial_capacity, mem_alignment);
    }

    array(const array &copy)
    {
        arr_init(this, copy.arena, copy.capacity, copy.mem_alignment);
        arr_copy(this, &copy);
    }

    ~array()
    {
        arr_terminate(this);
    }

    array &operator=(array rhs)
    {
        swap(this, &rhs);
        return *this;
    }

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
void swap(array<T> *lhs, array<T> *rhs)
{
    std::swap(lhs->arena, rhs->arena);
    std::swap(lhs->size, rhs->size);
    std::swap(lhs->capacity, rhs->capacity);
    std::swap(lhs->data, rhs->data);
}

template<class T>
void arr_init(array<T> *arr, mem_arena *arena = mem_global_arena(), sizet initial_capacity = 0, sizet mem_alignment = DEFAULT_MIN_ALIGNMENT)
{
    arr->arena = arena;
    arr->mem_alignment = mem_alignment;
    arr_set_capacity(arr, initial_capacity);
}

template<class T>
void arr_terminate(array<T> *arr)
{
    arr_set_capacity(arr, 0);
}

template<class T>
typename T::iterator arr_begin(T *arrobj)
{
    return arrobj->data;
}

template<class T>
sizet arr_len(const array<T> *arr)
{
    return arr->size;
}

template<class T>
sizet arr_len(const array<T> &arr)
{
    return arr.size;
}

template<class T>
sizet arr_sizeof(const array<T> *arr)
{
    return sizeof(T) * arr->size;
}

template<class T>
sizet arr_sizeof(const array<T> &arr)
{
    return sizeof(T) * arr.size;
}

// Get the used byte size of the static array (the capacity is just N)
template<class T, sizet N>
sizet arr_sizeof(const static_array<T, N> *arr)
{
    return sizeof(T) * arr->size;
}

// Get the used byte size of the static array (the capacity is just N)
template<class T, sizet N>
sizet arr_sizeof(const static_array<T, N> &arr)
{
    return sizeof(T) * arr.size;
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
        (*dest)[i] = (*source)[i];
    }
}

template<class T>
void arr_copy(array<T> *dest, const T *src, sizet src_size)
{
    arr_resize(dest, src_size);
    for (sizet i = 0; i < src_size; ++i) {
        (*dest)[i] = src[i];
    }
}

template<class T>
void arr_append(array<T> *arr, const T *src, sizet src_size)
{
    sizet start_ind = arr->size;
    arr_resize(arr, start_ind + src_size);
    for (sizet i = 0; i < src_size; ++i) {
        (*arr)[start_ind + i] = src[i];
    }
}

template<class T>
void arr_append(array<T> *arr, const array<T> *source)
{
    sizet start_ind = arr->size;
    arr_resize(arr, start_ind + source->size);
    for (sizet i = 0; i < source->size; ++i) {
        (*arr)[start_ind + i] = (*source)[i];
    }
}

template<class T>
void arr_set_capacity(array<T> *arr, sizet new_cap)
{
    if (new_cap == arr->capacity) {
        return;
    }
    
    // Shrink the old size if its greater than the new capacity calling dtor on each item
    if (arr->size > new_cap) {
        for (sizet i = new_cap; i < arr->size; ++i) {
            arr->data[i].~T();
        }
        arr->size = new_cap;
    }

    if (new_cap > 0) {
        // New cap can't be any smaller than mem_nod since we are using free list allocator
        while (new_cap * sizeof(T) < sizeof(mem_node)) {
            ++new_cap;
        }
        arr->data = (T *)mem_realloc(arr->data, new_cap * sizeof(T), arr->arena, arr->mem_alignment);
    }
    else if (arr->data) {
        mem_free(arr->data, arr->arena);
        arr->data = nullptr;
    }
    arr->capacity = new_cap;
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
    sizet sz = arr->size;
    arr_resize(arr, sz + 1);
    (*arr)[sz] = item;
    return &(*arr)[sz];
}

template<class T, sizet N>
T *arr_push_back(static_array<T, N> *arr, const T &item)
{
    assert(arr->size < arr->capacity);
    sizet sz = arr->size;
    ++arr->size;
    arr->data[sz] = item;
    return &arr->data[sz];
}

template<class T, class... Args>
T *arr_emplace_back(array<T> *arr, Args &&...args)
{
    sizet sz = arr->size;
    arr_resize(arr, sz + 1);
    T *ret = &arr->data[sz];
    new (ret) T(std::forward<Args>(args)...);
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

// Assigns each element in the array to item - does not change array size or capacity.
template<class T>
void arr_clear_to(T *bufobj, const typename T::value_type &item)
{
    for (int i = 0; i < bufobj->size; ++i) {
        bufobj->data[i] = item;
    }
}

// Call destructor on all items in the array and set size to 0, does not affect the capacity.
template<class T>
void arr_clear(T *bufobj)
{
    using MT = typename T::value_type;
    for (int i = 0; i < bufobj->size; ++i) {
        bufobj->data[i].~MT();
    }
    bufobj->size = 0;
}

template<class T>
void arr_pop_back(T *bufobj)
{
    if (bufobj->size == 0)
        return;
    using MT = typename T::value_type;
    bufobj->data[bufobj->size - 1].~MT();
    bufobj->data[bufobj->size - 1] = {};
    --bufobj->size;
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
    return std::find(arr_begin(bufobj), arr_end(bufobj), item);
}

template<class T, class... Args>
array<T> *arr_resize(array<T> *arr, sizet new_size, Args &&...args)
{
    if (arr->size == new_size)
        return arr;

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
    for (sizet i = arr->size; i < new_size; ++i) {
        new (&arr->data[i]) T(std::forward<Args>(args)...);
    }
    arr->size = new_size;
    return arr;
}

template<class T, sizet N, class... Args>
static_array<T, N> *arr_resize(static_array<T, N> *arr, sizet new_size, Args &&...args)
{
    assert(new_size <= N);
    for (sizet i = arr->size; i < new_size; ++i) {
        new (&arr->data[i]) T(std::forward<Args>(args)...);
    }
    arr->size = new_size;
    return arr;
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
    sizet reduce_size = (last - first);
    if (reduce_size > bufobj->size || reduce_size == 0) {
        return last;
    }

    // Shift all items after the range over to the first item in the range, until we reach to end of the data
    while (last != arr_end(bufobj)) {
        *first = *last;
        ++first;
        ++last;
    }

    arr_resize(bufobj, bufobj->size - reduce_size);
    return first;
}

// Remove the item at index by copying the last item in the array to its spot and popping the last item. This does not
// preserve the order of the array.
template<class T>
bool arr_swap_remove(T *bufobj, sizet index)
{
    if (index >= bufobj->size)
        return false;

    bufobj->data[index] = *arr_back(bufobj);
    arr_pop_back(bufobj);
    return true;
}

// Remove the item at index by copying all items > index to their previous element, and then popping the last item.
template<class T>
bool arr_remove(T *bufobj, sizet index)
{
    if (index >= bufobj->size)
        return false;

    // Copy the items back one spot
    for (sizet i = index + 1; i < bufobj->size; ++i) {
        bufobj->data[i - 1] = bufobj->data[i];
    }

    // Pop the last item
    arr_pop_back(bufobj);
    return true;
}

// Remove the item at index by copying all items > index to their previous element, and then popping the last item.
template<class T>
sizet arr_remove(T *bufobj, const typename T::value_type &val)
{
    auto iter = std::remove(arr_begin(bufobj), arr_end(bufobj), val);
    sizet ret = arr_end(bufobj) - iter;
    arr_resize(bufobj, bufobj->size - ret);
    return ret;
}

template<class T>
sizet arr_index_of(T *bufobj, typename T::value_type *item)
{
    sizet offset = (item - bufobj->data);
    if (offset < bufobj->size) {
        return offset;
    }
    return INVALID_IND;
}

using byte_array = array<u8>;

template<class ArchiveT, class T, sizet N>
void pack_unpack(ArchiveT *ar, static_array<T, N> &val, const pack_var_info &vinfo)
{
    pup_var(ar, val.size, {"size"});
    pup_var(ar, val.data, {"data", {pack_va_flags::FIXED_ARRAY_CUSTOM_SIZE, &val.size}});
}

} // namespace nslib
