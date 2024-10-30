// Copyright 2020 Joshua J Baker. All rights reserved.
// Use of this source code is governed by an MIT-style
// license that can be found in the LICENSE file.
#pragma once
#include "../basic_types.h"

namespace nslib
{
struct mem_arena;
using malloc_func_type = void *(sizet, mem_arena *, sizet);
using realloc_func_type = void *(void *, sizet, mem_arena *, sizet);
using free_func_type = void(void *, mem_arena *);

using hash_item_func = u64(const void *, u64, u64);
using compare_item_func = int(const void *, const void *, void *);
using free_item_func = void(void *);

struct ihashmap_bucket
{
    u64 hash : 48;
    u64 dib : 16;
};

struct ihashmap
{
    malloc_func_type *malloc;
    realloc_func_type *realloc;
    free_func_type *free;
    mem_arena *arena;
    sizet mem_alignment;
    sizet elsize;
    sizet cap;
    u64 seed0;
    u64 seed1;
    hash_item_func *hash;
    compare_item_func *compare;
    free_item_func *elfree;
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
ihashmap *ihashmap_new(sizet elsize,
                       sizet cap,
                       u64 seed0,
                       u64 seed1,
                       hash_item_func *hash,
                       compare_item_func *compare,
                       free_item_func *elfree,
                       void *udata);

ihashmap *ihashmap_new(malloc_func_type *malloc,
                       realloc_func_type *realloc,
                       free_func_type *free,
                       mem_arena *arena,
                       sizet mem_alighment,
                       sizet elsize,
                       sizet cap,
                       u64 seed0,
                       u64 seed1,
                       hash_item_func *hash,
                       compare_item_func *compare,
                       free_item_func *elfree,
                       void *udata);

ihashmap *ihashmap_new(const struct ihashmap *copy_from);

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
const void *ihashmap_get(struct ihashmap *map, const void *key);

// hashmap_set inserts or replaces an item in the hash map. If an item is
// replaced then it is returned otherwise NULL is returned. This operation
// may allocate memory. If the system is unable to allocate additional
// memory then NULL is returned and hashmap_oom() returns true.
const void *ihashmap_set(struct ihashmap *map, const void *key, const void *item);

// hashmap_delete removes an item from the hash map and returns it. If the
// item is not found then NULL is returned.
const void *ihashmap_delete(struct ihashmap *map, const void *key);

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

int generate_rand_seed();

} // namespace nslib
