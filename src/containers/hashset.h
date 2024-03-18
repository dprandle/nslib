#pragma once

#include "ihashmap.h"
#include "../hashfuncs.h"
#include "string.h"

namespace nslib
{

template<class T>
struct hashset
{
    using value_type = T;

    // Always allow a zero initialized hmap
    hashset() : hm{}
    {}

    // Deep copy the contents of copy in to this hashset - use the same allocators as copy as well
    hashset(const hashset &copy)
    {
        hashset_init(this);
        auto hs_item = hashset_iter(&copy);
        while (hs_item) {
            hashset_set(*this, hs_item);
        }
    }

    // Clear all items and release associated memory
    ~hashset()
    {
        hashset_terminate(this);
    }

    // Operator= uses copy and swap idiom
    hashset &operator=(hashset rhs)
    {
        auto tmp = hm;
        hm = rhs.hm;
        rhs.hm = tmp;
        return *this;
    }

    ihashmap *hm{nullptr};
};

template<class T, class LambdaFunc>
void hashset_for_each(hashset<T> *hs, LambdaFunc func)
{
    assert(hs->hm);
    sizet i{};
    auto item = hashset_iter(hs, &i);
    while (item) {
        if (!func(item)) {
            return;
        }
        item = hashset_iter(hs, &i);
    }
}

template<class T, class LambdaFunc>
void hashset_for_each(const hashset<T> *hs, LambdaFunc func)
{
    assert(hs->hm);
    sizet i{};
    auto item = hashset_iter(hs, &i);
    while (item) {
        if (!func(item)) {
            return;
        }
        item = hashset_iter(hs, &i);
    }
}

template<class T>
const T *hashset_iter(const hashset<T> *hs, sizet *i)
{
    assert(hs->hm);
    assert(i);
    T *item{nullptr};
    ihashmap_iter(hs->hm, i, (void **)&item);
    return item;
}

template<class T>
T *hashset_iter(hashset<T> *hs, sizet *i)
{
    assert(hs->hm);
    assert(i);
    T *item{nullptr};
    ihashmap_iter(hs->hm, i, (void **)&item);
    return item;
}

template<class T>
bool hashset_next(const hashset<T> *hs, sizet *i, const T **item)
{
    assert(hs->hm);
    assert(i);
    return ihashmap_iter(hs->hm, i, item);
}

template<class T>
bool hashset_next(hashset<T> *hs, sizet *i, T **item)
{
    assert(hs->hm);
    assert(i);
    return ihashmap_iter(hs->hm, i, item);
}

template<class T>
T *hashset_insert(hashset<T> *hs, const T &value)
{
    assert(hs->hm);
    if (hashset_find(hs, value)) {
        return nullptr;
    }
    return (T *)ihashmap_set(hs->hm, &value);
}

template<class T>
sizet hashset_count(hashset<T> *hs)
{
    assert(hs);
    return ihashmap_count(hs->hm);
}

template<class T>
T *hashset_clear(hashset<T> *hs, bool update_cap)
{
    assert(hs->hm);
    ihashmap_clear(hs, update_cap);
}

template<class T>
T *hashset_set(hashset<T> *hs, const T &value)
{
    assert(hs->hm);
    return (T *)ihashmap_set(hs->hm, &value);
}

template<class T>
const T *hashset_find(const hashset<T> *hs, const T &val)
{
    assert(hs->hm);
    return (T *)ihashmap_get(hs->hm, &val);
}

template<class T>
T *hashset_find(hashset<T> *hs, const T &val)
{
    assert(hs->hm);
    return (T *)ihashmap_get(hs->hm, &val);
}

template<class T>
T *hashset_remove(hashset<T> *hs, const T &val)
{
    assert(hs->hm);
    return (T *)ihashmap_delete(hs->hm, &val);
}

template<class T>
void hashset_terminate(hashset<T> *hs)
{
    if (hs && hs->hm) {
        ihashmap_free(hs->hm);
        hs->hm = nullptr;
    }
}

template<class T>
void hashset_init(hashset<T> *hs, mem_arena *arena)
{
    int seed0 = generate_rand_seed();
    int seed1 = generate_rand_seed();

    // Hash func attempts to call the hash_type function
    auto hash_func = [](const void *item, u64 seed0, u64 seed1) -> u64 {
        auto cast = (const T *)item;
        return hash_type(*cast, seed0, seed1);
    };

    // We only care about == so jsut return 1 in all other cases - If the hashed value of the keys are equal then we
    // check to see if the key itself is equal
    auto compare_func = [](const void *a, const void *b, void *) -> i32 {
        auto cast_a = (const T *)a;
        auto cast_b = (const T *)b;
        return (*cast_a == *cast_b) ? 0 : 1;
    };

    hs->hm = ihashmap_new_with_allocator(
        mem_alloc, mem_realloc, mem_free, arena, sizeof(T), 0, seed0, seed1, hash_func, compare_func, nullptr, hs);
}

template<class ArchiveT, class T>
void pack_unpack(ArchiveT *ar, hashset<T> &val, const pack_var_info &vinfo)
{
    sizet cnt = hashset_count(&val);
    pup_var(ar, cnt, {"count"});
    sizet i{0};
    if (ar->opmode == archive_opmode::UNPACK) {
        while (i < cnt) {
            T item{};
            pup_var(ar, item, {to_cstr("[%d]", i)});
            hashset_set(&val, item);
            ++i;
        }
    }
    else {
        sizet bucket_i{0};
        while (auto iter = hashset_iter(&val, &bucket_i)) {
            pup_var(ar, *iter, {to_cstr("[%d]", i)});
            ++i;
        }
    }
}

template<class T>
string to_str(const hashset<T> &hs)
{
    string ret("\nhashset {");
    auto for_each = [&ret](const T *item) {
        ret += "\n" + to_str(*item);
        return true;
    };
    hashset_for_each(&hs, for_each);
    ret += "\n}";
    return ret;
}

} // namespace nslib
