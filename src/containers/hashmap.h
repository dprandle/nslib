#pragma once

#include "string.h"
#include "ihashmap.h"
#include "../hashfuncs.h"

namespace nslib
{

// hashmap is an open addressed hash map using robinhood hashing.
// Since hashmap manages memory, but we want it to act like a built in type in terms of copying and equality testing, we
// have to write copy ctor, dtor, assignment operator, and equality operators.
template<class Key, class Value>
struct hashmap
{
    using value_type = pair<const Key, Value>;
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

    ihashmap *hm{nullptr};
};

template<class K, class T, class LambdaFunc>
void hashmap_for_each(hashmap<K, T> *hm, LambdaFunc func)
{
    assert(hm->hm);
    sizet i;
    auto item = hashmap_iter(hm, &i);
    while (item) {
        if (!func(item)) {
            return;
        }
        item = hashmap_iter(hm, &i);
    }
}

template<class K, class T>
const pair<const K, T> * hashmap_iter(const hashmap<K, T> *hm, sizet *i)
{
    assert(hm->hm);
    assert(i);
    pair<const K, T> *item{nullptr};
    ihashmap_iter(hm->hm, i, (void**)&item);
    return item;
}

template<class K, class T>
pair<const K, T> * hashmap_iter(hashmap<K, T> *hm, sizet *i)
{
    assert(hm->hm);
    assert(i);
    pair<const K, T> *item{nullptr};
    ihashmap_iter(hm->hm, i, (void**)&item);
    return item;
}


template<class K, class T>
bool hashmap_next(const hashmap<K, T> *hm, sizet *i, const pair<const K, T> **item)
{
    assert(hm->hm);
    assert(i);
    return ihashmap_iter(hm->hm, i, item);
}

template<class K, class T>
bool hashmap_next(hashmap<K, T> *hm, sizet *i, pair<const K, T> **item)
{
    assert(hm->hm);
    assert(i);
    return ihashmap_iter(hm->hm, i, item);
}

template<class K, class T>
pair<const K,T> *hashmap_insert(hashmap<K, T> *hm, const K &key, const T &value)
{
    assert(hm->hm);
    if (hashmap_find(hm, key)) {
        return nullptr;
    }
    pair<const K,T> item{key,value};
    return (pair<const K,T>*)ihashmap_set(hm->hm,&item);
}

template<class K, class T>
pair<const K,T> *hashmap_clear(hashmap<K, T> *hm, bool update_cap)
{
    assert(hm->hm);
    ihashmap_clear(hm, update_cap);
}

template<class K, class T>
pair<const K,T> *hashmap_set(hashmap<K, T> *hm, const K &key, const T &value)
{
    assert(hm->hm);
    pair<const K,T> item{key, value};
    return (pair<const K,T>*)ihashmap_set(hm->hm, &item);
}

template<class K, class T>
const pair<const K,T> *hashmap_find(const hashmap<K, T> *hm, const K &key)
{
    assert(hm->hm);
    pair<const K,T> find_item{key,{}};
    return (pair<const K,T>*)ihashmap_get(hm->hm, &find_item);
}

template<class K, class T>
pair<const K,T> *hashmap_find(hashmap<K, T> *hm, const K &key)
{
    assert(hm->hm);
    pair<const K,T> find_item{key,{}};
    return (pair<const K,T>*)ihashmap_get(hm->hm, &find_item);
}

template<class K, class T>
pair<const K,T> *hashmap_remove(hashmap<K, T> *hm, const K &key)
{
    assert(hm->hm);
    pair<const K,T> find_item{key,{}};
    return (pair<const K, T>*)ihashmap_delete(hm->hm, &find_item);
}

template<class K, class T>
void hashmap_terminate(hashmap<K, T> *hm)
{
    if (hm) {
        ihashmap_free(hm->hm);
    }
}

template<class Key, class Value>
void hashmap_init(hashmap<Key, Value> *hm)
{
    int seed0 = generate_rand_seed();
    int seed1 = generate_rand_seed();

    // Hash func attempts to call the hash_type function
    auto hash_func = [](const void *item, u64 seed0, u64 seed1) -> u64 {
        auto cast = (const pair<const Key, Value> *)item;
        return hash_type(cast->first, seed0, seed1);
    };

    // We only care about == so jsut return 1 in all other cases - If the hashed value of the keys are equal then we
    // check to see if the key itself is equal
    auto compare_func = [](const void *a, const void *b, void *) -> i32 {
        auto cast_a = (const pair<const Key, Value> *)a;
        auto cast_b = (const pair<const Key, Value> *)b;
        return (cast_a->first == cast_b->first) ? 0 : 1;
    };
    
    hm->hm = ihashmap_new_with_allocator(global_malloc_func(),
                                         global_realloc_func(),
                                         global_free_func(),
                                         sizeof(pair<Key,Value>),
                                         0,
                                         seed0,
                                         seed1,
                                         hash_func,
                                         compare_func,
                                         nullptr,
                                         hm);
}

template<class Key, class Value>
string to_string(const hashmap<Key, Value> &hm) {
    string ret("\nhashmap {");
    auto for_each = [&ret](const pair<const Key, Value> *item) {
        ret += "\nkey: " + to_string(item->first);
        ret += "\nval: " + to_string(item->second);
    };
    ret += "\n}";
    hashmap_for_each(hm, for_each);
    return ret;
}

} // namespace nslib
