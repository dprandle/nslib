#pragma once

#include "string.h"
#include "ihashmap.h"
#include "../hashfuncs.h"
#include "../archive_common.h"
#include "../util.h"

namespace nslib
{

// hashmap is an open addressed hash map using robinhood hashing.
// Since hashmap manages memory, but we want it to act like a built in type in terms of copying and equality testing, we
// have to write copy ctor, dtor, assignment operator, and equality operators.
template<class Key, class Value>
struct hashmap
{
    using value_type = pair<Key, Value>;
    using iterator = pair<Key, Value> *;
    using const_iterator = const pair<Key, Value> *;
    using mapped_type = Value;
    using key_type = Key;

    // Always allow a zero initialized hmap
    hashmap() : hm{}
    {}

    // Deep copy the contents of copy in to this hashmap - use the same allocators as copy as well
    hashmap(const hashmap &copy)
    {
        hashmap_init(this);
        auto hm_item = hashmap_iter(&copy);
        while (hm_item) {
            hashmap_set(*this, hm_item);
        }
    }

    // Clear all items and release associated memory
    ~hashmap()
    {
        hashmap_terminate(this);
    }

    // Operator= uses copy and swap idiom
    hashmap &operator=(hashmap rhs)
    {
        auto tmp = hm;
        hm = rhs.hm;
        rhs.hm = tmp;
        return *this;
    }

    inline mapped_type &operator[](key_type key)
    {
        auto iter = hashmap_set(this, key, mapped_type{});
        if (!iter) {
            iter = hashmap_find(this, key);
        }
        return iter->second;
    }

    ihashmap *hm{nullptr};
};

template<class K, class T, class LambdaFunc>
void hashmap_for_each(hashmap<K, T> *hm, LambdaFunc func)
{
    assert(hm->hm);
    sizet i{};
    auto item = hashmap_iter(hm, &i);
    while (item) {
        if (!func(item)) {
            return;
        }
        item = hashmap_iter(hm, &i);
    }
}

template<class K, class T, class LambdaFunc>
void hashmap_for_each(const hashmap<K, T> *hm, LambdaFunc func)
{
    assert(hm->hm);
    sizet i{};
    auto item = hashmap_iter(hm, &i);
    while (item) {
        if (!func(item)) {
            return;
        }
        item = hashmap_iter(hm, &i);
    }
}

template<class K, class T>
typename hashmap<K, T>::const_iterator hashmap_iter(const hashmap<K, T> *hm, sizet *i)
{
    assert(hm->hm);
    assert(i);
    typename hashmap<K, T>::const_iterator item{nullptr};
    ihashmap_iter(hm->hm, i, (void **)&item);
    return item;
}

template<class K, class T>
typename hashmap<K, T>::iterator hashmap_iter(hashmap<K, T> *hm, sizet *i)
{
    assert(hm->hm);
    assert(i);
    typename hashmap<K, T>::iterator item{nullptr};
    ihashmap_iter(hm->hm, i, (void **)&item);
    return item;
}

template<class K, class T>
bool hashmap_next(const hashmap<K, T> *hm, sizet *i, typename hashmap<K,T>::const_iterator *item)
{
    assert(hm->hm);
    assert(i);
    return ihashmap_iter(hm->hm, i, item);
}

template<class K, class T>
bool hashmap_next(hashmap<K, T> *hm, sizet *i, typename hashmap<K,T>::const_iterator *item)
{
    assert(hm->hm);
    assert(i);
    return ihashmap_iter(hm->hm, i, item);
}

template<class K, class T>
typename hashmap<K, T>::iterator hashmap_insert(hashmap<K, T> *hm, const K &key, const T &value)
{
    assert(hm->hm);
    if (hashmap_find(hm, key)) {
        return nullptr;
    }
    typename hashmap<K, T>::value_type item{key, value};
    return (typename hashmap<K, T>::iterator)ihashmap_set(hm->hm, &item);
}

template<class K, class T>
typename hashmap<K, T>::iterator hashmap_insert(hashmap<K, T> *hm, const typename hashmap<K, T>::value_type & item)
{
    assert(hm->hm);
    if (hashmap_find(hm, item.key)) {
        return nullptr;
    }
    return (typename hashmap<K, T>::iterator)ihashmap_set(hm->hm, &item);
}

template<class K, class T>
void hashmap_clear(hashmap<K, T> *hm, bool update_cap)
{
    assert(hm->hm);
    ihashmap_clear(hm->hm, update_cap);
}

template<class K, class T>
typename hashmap<K, T>::iterator hashmap_set(hashmap<K, T> *hm, const K &key, const T &value)
{
    assert(hm->hm);
    typename hashmap<K, T>::value_type item{key, value};
    return (typename hashmap<K, T>::iterator)ihashmap_set(hm->hm, &item);
}

template<class K, class T>
typename hashmap<K, T>::iterator hashmap_set(hashmap<K, T> *hm, const typename hashmap<K,T>::value_type &item)
{
    assert(hm->hm);
    return (typename hashmap<K, T>::iterator)ihashmap_set(hm->hm, &item);
}

template<class K, class T>
typename hashmap<K, T>::const_iterator hashmap_find(const hashmap<K, T> *hm, const K &key)
{
    assert(hm->hm);
    typename hashmap<K, T>::value_type find_item{key, {}};
    return (typename hashmap<K, T>::const_iterator)ihashmap_get(hm->hm, &find_item);
}

template<class K, class T>
typename hashmap<K, T>::iterator hashmap_find(hashmap<K, T> *hm, const K &key)
{
    assert(hm->hm);
    typename hashmap<K, T>::value_type find_item{key, {}};
    return (typename hashmap<K, T>::iterator)ihashmap_get(hm->hm, &find_item);
}

template<class K, class T>
typename hashmap<K, T>::iterator hashmap_remove(hashmap<K, T> *hm, const K &key)
{
    assert(hm->hm);
    typename hashmap<K, T>::value_type find_item{key, {}};
    return (typename hashmap<K, T>::iterator)ihashmap_delete(hm->hm, &find_item);
}

template<class K, class T>
void hashmap_terminate(hashmap<K, T> *hm)
{
    if (hm && hm->hm) {
        ihashmap_free(hm->hm);
        hm->hm = nullptr;
    }
}

// hashmap_count returns the number of items in the hash map.
template<class K, class T>
sizet hashmap_count(const hashmap<K, T> *hm)
{
    return ihashmap_count(hm->hm);
}

template<class Key, class Value>
void hashmap_init(hashmap<Key, Value> *hm)
{
    int seed0 = generate_rand_seed();
    int seed1 = generate_rand_seed();

    // Hash func attempts to call the hash_type function
    auto hash_func = [](const void *item, u64 seed0, u64 seed1) -> u64 {
        auto cast = (typename hashmap<Key,Value>::const_iterator)item;
        return hash_type(cast->first, seed0, seed1);
    };

    // We only care about == so jsut return 1 in all other cases - If the hashed value of the keys are equal then we
    // check to see if the key itself is equal
    auto compare_func = [](const void *a, const void *b, void *) -> i32 {
        auto cast_a = (typename hashmap<Key,Value>::const_iterator)a;
        auto cast_b = (typename hashmap<Key,Value>::const_iterator)b;
        return (cast_a->first == cast_b->first) ? 0 : 1;
    };

    hm->hm = ihashmap_new_with_allocator(global_malloc_func(),
                                         global_realloc_func(),
                                         global_free_func(),
                                         sizeof(pair<Key, Value>),
                                         0,
                                         seed0,
                                         seed1,
                                         hash_func,
                                         compare_func,
                                         nullptr,
                                         hm);
}

template<class Key, class Value>
string to_str(const hashmap<Key, Value> &hm)
{
    string ret("\nhashmap {");
    auto for_each = [&ret](typename hashmap<Key,Value>::const_iterator item) -> bool {
        ret += "\nkey: " + to_str(item->first);
        ret += "\nval: " + to_str(item->second);
        return true;
    };
    hashmap_for_each(&hm, for_each);
    ret += "\n}";
    return ret;
}

template<class ArchiveT, class K, class T>
void pack_unpack(ArchiveT *ar, hashmap<K, T> &val, const pack_var_info &vinfo)
{
    sizet sz = hashmap_count(&val);
    pup_var(ar, sz, {"count"});
    sizet i{0};
    if (ar->opmode == archive_opmode::UNPACK) {
        while (i < sz) {
            typename hashmap<K,T>::value_type item{K{},T{}};
            pup_var(ar, item, {to_cstr("{%d}", i), {pack_va_flags::PACK_PAIR_KEY_VAL}});
            hashmap_set(&val, item);
            ++i;
        }
    }
    else {
        sizet bucket_i{0};
        while (auto iter = hashmap_iter(&val, &bucket_i)) {
            pup_var(ar, *iter,{to_cstr("{%d}", i), {pack_va_flags::PACK_PAIR_KEY_VAL}});
            ++i;
        }
    }
}

} // namespace nslib
