// Copyright 2020 Joshua J Baker. All rights reserved.
// Use of this source code is governed by an MIT-style
// license that can be found in the LICENSE file.

#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "../basic_types.h"
#include "../hashfuncs.h"

namespace nslib
{

using malloc_func_type = void *(sizet);
using realloc_func_type = void *(void *, sizet);
using free_func_type = void(void *);

struct ihashmap_bucket
{
    u64 hash : 48;
    u64 dib : 16;
};

struct ihashmap
{
    // Always allow a zero initialized hmap
    ihashmap(){};

    // Deep copy the contents of copy in to this hashmap - use the same allocators as copy as well
    ihashmap(const ihashmap &copy);

    // Clear all items and release associated memory
    ~ihashmap();

    // Operator= uses copy and swap idiom
    ihashmap &operator=(ihashmap rhs);

    malloc_func_type *malloc;
    realloc_func_type *realloc;
    free_func_type *free;
    sizet elsize;
    sizet cap;
    u64 seed0;
    u64 seed1;
    u64 (*hash)(const void *item, u64 seed0, u64 seed1);
    int (*compare)(const void *a, const void *b, void *udata);
    void (*elfree)(void *item);
    void *udata;
    sizet bucketsz;
    sizet nbuckets;
    sizet count;
    sizet mask;
    sizet growat;
    sizet shrinkat;
    u8 growpower;
    bool oom;
    void *buckets;
    void *spare;
    void *edata;
};

// hashmap_new returns a new hash map.
// Param `elsize` is the size of each element in the tree. Every element that
// is inserted, deleted, or retrieved will be this size.
// Param `cap` is the default lower capacity of the hashmap. Setting this to
// zero will default to 16.
// Params `seed0` and `seed1` are optional seed values that are passed to the
// following `hash` function. These can be any value you wish but it's often
// best to use randomly generated values.
// Param `hash` is a function that generates a hash value for an item. It's
// important that you provide a good hash function, otherwise it will perform
// poorly or be vulnerable to Denial-of-service attacks. This implementation
// comes with two helper functions `hashmap_sip()` and `hashmap_murmur()`.
// Param `compare` is a function that compares items in the tree. See the
// qsort stdlib function for an example of how this function works.
// The hashmap must be freed with hashmap_free().
// Param `elfree` is a function that frees a specific item. This should be NULL
// unless you're storing some kind of reference data in the hash.
struct ihashmap *ihashmap_new(sizet elsize,
                              sizet cap,
                              u64 seed0,
                              u64 seed1,
                              u64 (*hash)(const void *item, u64 seed0, u64 seed1),
                              int (*compare)(const void *a, const void *b, void *udata),
                              void (*elfree)(void *item),
                              void *udata);

struct ihashmap *ihashmap_new(const struct ihashmap *copy_from);

// hashmap_new_with_allocator returns a new hash map using a custom allocator.
// See hashmap_new for more information information
struct ihashmap *ihashmap_new_with_allocator(void *(*malloc)(sizet),
                                             void *(*realloc)(void *, sizet),
                                             void (*free)(void *),
                                             sizet elsize,
                                             sizet cap,
                                             u64 seed0,
                                             u64 seed1,
                                             u64 (*hash)(const void *item, u64 seed0, u64 seed1),
                                             int (*compare)(const void *a, const void *b, void *udata),
                                             void (*elfree)(void *item),
                                             void *udata);

// hashmap_free frees the hash map
// Every item is called with the element-freeing function given in hashmap_new,
// if present, to free any data referenced in the elements of the hashmap.
void ihashmap_free(struct ihashmap *map);

// hashmap_clear quickly clears the map.
// Every item is called with the element-freeing function given in hashmap_new,
// if present, to free any data referenced in the elements of the hashmap.
// When the update_cap is provided, the map's capacity will be updated to match
// the currently number of allocated buckets. This is an optimization to ensure
// that this operation does not perform any allocations.
void ihashmap_clear(struct ihashmap *map, bool update_cap);

// hashmap_count returns the number of items in the hash map.
sizet ihashmap_count(struct ihashmap *map);

// hashmap_oom returns true if the last hashmap_set() call failed due to the
// system being out of memory.
bool ihashmap_oom(struct ihashmap *map);

// hashmap_get returns the item based on the provided key. If the item is not
// found then NULL is returned.
const void *ihashmap_get(struct ihashmap *map, const void *item);

// hashmap_set inserts or replaces an item in the hash map. If an item is
// replaced then it is returned otherwise NULL is returned. This operation
// may allocate memory. If the system is unable to allocate additional
// memory then NULL is returned and hashmap_oom() returns true.
const void *ihashmap_set(struct ihashmap *map, const void *item);

// hashmap_delete removes an item from the hash map and returns it. If the
// item is not found then NULL is returned.
const void *ihashmap_delete(struct ihashmap *map, const void *item);

// hashmap_probe returns the item in the bucket at position or NULL if an item
// is not set for that bucket. The position is 'moduloed' by the number of
// buckets in the hashmap.
const void *ihashmap_probe(struct ihashmap *map, u64 position);

// hashmap_scan iterates over all items in the hash map
// Param `iter` can return false to stop iteration early.
// Returns false if the iteration has been stopped early.
bool ihashmap_scan(struct ihashmap *map, bool (*iter)(const void *item, void *udata), void *udata);

// hashmap_iter iterates one key at a time yielding a reference to an
// entry at each iteration. Useful to write simple loops and avoid writing
// dedicated callbacks and udata structures, as in hashmap_scan.
//
// map is a hash map handle. i is a pointer to a sizet cursor that
// should be initialized to 0 at the beginning of the loop. item is a void
// pointer pointer that is populated with the retrieved item. Note that this
// is NOT a copy of the item stored in the hash map and can be directly
// modified.
//
// Note that if hashmap_delete() is called on the hashmap being iterated,
// the buckets are rearranged and the iterator must be reset to 0, otherwise
// unexpected results may be returned after deletion.
//
// This function has not been tested for thread safety.
//
// The function returns true if an item was retrieved; false if the end of the
// iteration has been reached.
bool ihashmap_iter(const struct ihashmap *map, sizet *i, void **item);

// hashmap_get_with_hash works like hashmap_get but you provide your
// own hash. The 'hash' callback provided to the hashmap_new function
// will not be called
const void *ihashmap_get_with_hash(struct ihashmap *map, const void *key, u64 hash);

// hashmap_delete_with_hash works like hashmap_delete but you provide your
// own hash. The 'hash' callback provided to the hashmap_new function
// will not be called
const void *ihashmap_delete_with_hash(struct ihashmap *map, const void *key, u64 hash);

// hashmap_set_with_hash works like hashmap_set but you provide your
// own hash. The 'hash' callback provided to the hashmap_new function
// will not be called
const void *ihashmap_set_with_hash(struct ihashmap *map, const void *item, u64 hash);

void ihashmap_set_grow_by_power(struct ihashmap *map, sizet power);

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

int generate_rand_seed();
malloc_func_type *get_global_malloc_func();
realloc_func_type *get_global_realloc_func();
free_func_type *get_global_free_func();

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
pair<const K,T> *hashmap_set(hashmap<K, T> *hm, const K &key, const T *value)
{
    assert(hm->hm);
    pair<const K,T> item{key, *value};
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
    
    hm->hm = ihashmap_new_with_allocator(get_global_malloc_func(),
                                         get_global_realloc_func(),
                                         get_global_free_func(),
                                         sizeof(pair<Key,Value>),
                                         0,
                                         seed0,
                                         seed1,
                                         hash_func,
                                         compare_func,
                                         nullptr,
                                         hm);
}

} // namespace nslib
