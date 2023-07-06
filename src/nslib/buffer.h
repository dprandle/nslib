#pragma once

#include "basic_types.h"
#include "nsmemory.h"
#include "nsdebug.h"

namespace nslib
{
template<class T>
struct buffer
{
    sizet cap{};
    sizet size{};
    T *data{};
};

template<class T>
void swap(buffer<T> * lhs, buffer<T> * rhs)
{
    std::swap(lhs->cap, rhs->cap);
    std::swap(lhs->size, rhs->size);
    std::swap(lhs->data, rhs->data);
}

template<class T, sizet N>
struct array
{
    using iterator = T *;
    using const_iterator = const T *;
    using value_type = T;

    array(): mem{}, buf{N, 0, mem}
    {}

    array(const array<T,N> & copy): mem{}, buf{N, copy.buf.size, mem}
    {
        for (int i = 0; i < N; ++i)
            mem[i] = copy.mem[i];
    }

    template<sizet M>
    array(const array<T,M> & copy): mem{}, buf{N, 0, mem}
    {
        buf.size = std::min(buf.cap, copy.buf.size);
        for (int i = 0; i < buf.size; ++i)
            mem[i] = copy.mem[i];
    }

    array<T,N> & operator=(array<T,N> rhs)
    {
        swap(this, &rhs);
        return *this;
    }

    T mem[N];
    buffer<T> buf{N, 0, mem};

    inline iterator begin()
    {
        return buf.data;
    }

    inline const_iterator begin() const
    {
        return buf.data;
    }

    inline iterator end()
    {
        return (buf.data + buf.size);
    }

    inline const_iterator end() const
    {
        return (buf.data + buf.size);
    }

    inline sizet size() const
    {
        return buf.size;
    }

    inline bool empty() const
    {
        return (buf.size == 0);
    }

    inline sizet capacity() const
    {
        return buf.cap;
    }

    inline const T &operator[](sizet ind) const
    {
        return buf.data[ind];
    }
    inline T &operator[](sizet ind)
    {
        return buf.data[ind];
    }
};

template<class T, sizet N>
void swap(array<T,N> * lhs, array<T,N> *rhs)
{
    std::swap(lhs->buf.size, rhs->buf.size);
    std::swap(lhs->mem, rhs->mem);
}

template<class T>
struct vector
{
    using iterator = T *;
    using const_iterator = const T *;
    using value_type = T;

    vector(mem_store *alloc_ = nullptr): alloc(alloc_)
    {
        if (!alloc)
            alloc = global_allocator();
    }

    vector(const vector<T> &rhs): alloc(rhs.alloc)
    {
        if (!alloc)
            alloc = global_allocator();
        buf_resize(this, rhs.size());
        for (int i = 0; i < buf->size; ++i)
            buf.data[i] = rhs.buf.data[i];
    }

    vector<T> & operator=(vector<T> rhs)
    {
        swap(this, &rhs);
        return *this;
    }

    ~vector()
    {
        if (buf.data)
        {
            ns_free(alloc, buf.data);
        }
    }

    buffer<T> buf{0, 0, nullptr};
    mem_store *alloc{};

    inline iterator begin()
    {
        return buf.data;
    }

    inline const_iterator begin() const
    {
        return buf.data;
    }

    inline iterator end()
    {
        return (buf.data + buf.size);
    }

    inline const_iterator end() const
    {
        return (buf.data + buf.size);
    }

    inline sizet size() const
    {
        return buf.size;
    }

    inline bool empty() const
    {
        return (buf->size == 0);
    }

    inline sizet capacity() const
    {
        return buf.cap;
    }

    inline const T &operator[](sizet ind) const
    {
        return buf.data[ind];
    }
    inline T &operator[](sizet ind)
    {
        return buf.data[ind];
    }
};

template<class T, sizet N>
void swap(vector<T> * lhs, vector<T> *rhs)
{
    swap(&lhs->buf, &rhs->buf);
    std::swap(lhs->alloc, rhs->alloc);
}

template<class T>
void reallocate_and_copy(vector<T> *vec, sizet new_cap)
{
    // New cap can't be any smaller than mem_nod since we are using free list allocator
    while (new_cap * sizeof(T) < sizeof(mem_node))
        ++new_cap;

    auto old_ptr = vec->buf.data;

    vec->buf.cap = new_cap;
    vec->buf.data = (T *)ns_alloc(vec->alloc, vec->buf.cap * sizeof(T));

    // Shrink the old size if its greater than the new capacity (so we only copy those items)
    if (vec->buf.size > vec->buf.cap)
        vec->buf.size = vec->buf.cap;

    // Copy old items to new vec before updating size and adding item, and deallocating prev mem
    for (int i = 0; i < vec->size(); ++i)
    {
        vec->buf.data[i] = old_ptr[i];
    }

#ifdef DEBUG_BUILD
    for (int i = vec->buf.size; i < vec->buf.cap; ++i)
        vec->buf.data[i] = {};
#endif

    ns_free(vec->alloc, old_ptr);
}

template<class T>
void reserve(vector<T> *vec, sizet capacity)
{
    if (vec->buf.cap > capacity)
        return;
    reallocate_and_copy(vec, capacity);
}

template<class T>
void shrink_to_fit(vector<T> *vec)
{
    nsassert(vec->size() <= vec->capacity());
    if (vec->size() == vec->capacity())
        return;
    reallocate_and_copy(vec, vec->size());
}

template<class T>
T *buf_push_back(buffer<T> *buf, const T &item)
{
    nsassert(buf->size <= buf->cap);
    if (buf->size == buf->cap)
        return nullptr;

    T *ret = &buf->data[buf->size];
    *ret = item;
    ++buf->size;
    return ret;
}

template<class T>
T *buf_push_back(vector<T> *vec, const T &item)
{
    nsassert(vec->size() <= vec->capacity());
    if (vec->size() == vec->capacity())
        reallocate_and_copy(vec, vec->buf.cap * 2);
    return buf_push_back(&vec->buf, item);
}

template<class T, sizet N>
T *buf_push_back(array<T, N> *arr, const T &item)
{
    return buf_push_back(&arr->buf, item);
}

template<class T, class... Args>
T *buf_emplace_back(buffer<T> *buf, Args &&...args)
{
    if (buf->size == buf->cap)
        return nullptr;
    T *ret = &buf->data[buf->size];
    new (ret) T(std::forward<Args>(args)...);
    ++buf->size;
    return ret;
}

template<class T, class... Args>
T *buf_emplace_back(vector<T> *vec, Args &&...args)
{
    nsassert(vec->size() <= vec->capacity());
    if (vec->size() == vec->capacity())
        reallocate_and_copy(vec, vec->buf.cap * 2);
    return buf_emplace_back(&vec->buf, std::forward<Args>(args)...);
}

template<class T, sizet N, class... Args>
T *buf_emplace_back(array<T, N> *arr, Args &&...args)
{
    return buf_emplace_back(&arr->buf, std::forward<Args>(args)...);
}

template<class T>
void buf_clear_to(buffer<T> *bufobj, const T &item)
{
    for (int i = 0; i < bufobj->cap; ++i)
        bufobj->data[i] = item;
}

template<class T>
void buf_clear_to(vector<T> *vec, const T &item)
{
    return buf_clear_to(vec->buf, item);
}

template<class T, sizet N>
void buf_clear_to(array<T, N> *arr, const T &item)
{
    return buf_clear_to(arr->buf, item);
}

template<class T>
void buf_pop_back(buffer<T> *bufobj)
{
    if (bufobj->size == 0)
        return;
#ifdef DEBUG_BUILD
    bufobj->data[bufobj->size-1] = {};
#endif
    --bufobj->size;
}

template<class T>
void buf_pop_back(vector<T> *vec)
{
    buf_pop_back(&vec->buf);
}

template<class T, sizet N>
void buf_pop_back(array<T, N> *arr)
{
    buf_pop_back(&arr->buf);
}

template<class T>
void buf_clear(buffer<T> *buf)
{
    buf->size = 0;
#ifdef DEBUG_BUILD
    for (int i = 0; i < buf->cap; ++i)
        buf->data[i] = {};
#endif
}

template<class T>
void buf_clear(vector<T> *vec)
{
    buf_clear(&vec->buf);
}

template<class T, sizet N>
void buf_clear(array<T,N> *arr)
{
    buf_clear(&arr->buf);
}

template<class T>
T *buf_back(buffer<T> *buf)
{
    if (buf->size > 0)
        return &buf->data[buf->size - 1];
    return nullptr;
}

template<class T>
T *buf_back(vector<T> *vec)
{
    return buf_back(&vec->buf);
}

template<class T, sizet N>
T *buf_back(array<T, N> *arr)
{
    return buf_back(&arr->buf);
}

template<class T>
T *buf_front(buffer<T> *buf)
{
    if (buf->size > 0)
        return &buf->data[0];
    return nullptr;
}

template<class T>
T *buf_front(vector<T> *vec)
{
    return buf_front(&vec->buf);
}

template<class T, sizet N>
T *buf_front(array<T, N> *arr)
{
    return buf_front(&arr->buf);
}

template<class T>
bool buf_remove(buffer<T> *buf, sizet index)
{
    if (index >= buf->size)
        return false;

    // Copy the items after the item to delete
    for (int i = index; i < (buf->size - 1); ++i)
    {
        buf->data[i] = buf->data[i + 1];
    }
    buf_pop_back(buf);
    return true;
}

template<class T>
bool buf_remove(vector<T> *vec, sizet index)
{
    return remove(&vec->buf, index);
}

template<class T, sizet N>
bool buf_remove(array<T,N> *arr, sizet index)
{
    return remove(&arr->buf, index);
}

template<class T>
typename T::iterator buf_find(T *bufobj, const typename T::value_type &item)
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

template<class T, class... Args>
void buf_resize(vector<T> *vec, sizet new_size, Args &&...args)
{
    nsassert(vec->size() <= vec->capacity());
    sizet cap = vec->buf.cap;
    if (new_size > cap)
    {
        while (cap < new_size)
            cap *= 2;
        reallocate_and_copy(vec, cap);
    }
    for (sizet i = vec->buf.size; i < new_size; ++i)
    {
        T *item = &vec->buf.data[i];
        new (item) T(std::forward<Args>(args)...);
    }
    // If in debug - buf_clear the removed data (if any) to 0
    #ifdef DEBUG_BUILD
    for (sizet i = new_size; i < vec->buf.size; ++i)
        vec->buf.data[i] = {};
    #endif
    vec->buf.size = new_size;
}

template<class T>
typename T::iterator erase(T *bufobj, typename T::iterator iter)
{
    while (iter + 1 != bufobj->end())
    {
        *iter = *(iter + 1);
        ++iter;
    }
    buf_pop_back(bufobj);
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

} // namespace nslib
