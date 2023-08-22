// Copyright 2020 Joshua J Baker. All rights reserved.
// Use of this source code is governed by an MIT-style
// license that can be found in the LICENSE file.

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <stddef.h>
#include "hashmap.h"
#include "../hashfuncs.h"

#define GROW_AT 0.60
#define SHRINK_AT 0.10

namespace nslib
{

static void *(*__malloc)(sizet) = NULL;
static void *(*__realloc)(void *, sizet) = NULL;
static void (*__free)(void *) = NULL;

// hashmap_set_allocator allows for configuring a custom allocator for
// all hashmap library operations. This function, if needed, should be called
// only once at startup and a prior to calling hashmap_new().
void hashmap_set_allocator(void *(*malloc)(sizet), void (*free)(void *))
{
    __malloc = malloc;
    __free = free;
}

struct bucket
{
    u64 hash : 48;
    u64 dib : 16;
};

// hashmap is an open addressed hash map using robinhood hashing.
struct hashmap
{
    void *(*malloc)(sizet);
    void *(*realloc)(void *, sizet);
    void (*free)(void *);
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

void hashmap_set_grow_by_power(struct hashmap *map, sizet power)
{
    map->growpower = (u8)(power < 1 ? 1 : power > 16 ? 16 : power);
}

static struct bucket *bucket_at0(void *buckets, sizet bucketsz, sizet i)
{
    return (struct bucket *)(((char *)buckets) + (bucketsz * i));
}

static struct bucket *bucket_at(struct hashmap *map, sizet index)
{
    return bucket_at0(map->buckets, map->bucketsz, index);
}

static void *bucket_item(struct bucket *entry)
{
    return ((char *)entry) + sizeof(struct bucket);
}

static u64 clip_hash(u64 hash)
{
    return hash & 0xFFFFFFFFFFFF;
}

static u64 get_hash(struct hashmap *map, const void *key)
{
    return clip_hash(map->hash(key, map->seed0, map->seed1));
}

// hashmap_new_with_allocator returns a new hash map using a custom allocator.
// See hashmap_new for more information information
struct hashmap *hashmap_new_with_allocator(void *(*_malloc)(sizet),
                                           void *(*_realloc)(void *, sizet),
                                           void (*_free)(void *),
                                           sizet elsize,
                                           sizet cap,
                                           u64 seed0,
                                           u64 seed1,
                                           u64 (*hash)(const void *item, u64 seed0, u64 seed1),
                                           int (*compare)(const void *a, const void *b, void *udata),
                                           void (*elfree)(void *item),
                                           void *udata)
{
    _malloc = _malloc ? _malloc : __malloc ? __malloc : malloc;
    _realloc = _realloc ? _realloc : __realloc ? __realloc : realloc;
    _free = _free ? _free : __free ? __free : free;
    sizet ncap = 16;
    if (cap < ncap) {
        cap = ncap;
    }
    else {
        while (ncap < cap) {
            ncap *= 2;
        }
        cap = ncap;
    }
    // printf("%d\n", (int)cap);
    sizet bucketsz = sizeof(struct bucket) + elsize;
    while (bucketsz & (sizeof(uintptr_t) - 1)) {
        bucketsz++;
    }
    // hashmap + spare + edata
    sizet size = sizeof(struct hashmap) + bucketsz * 2;
    hashmap *map = (hashmap*)_malloc(size);
    if (!map) {
        return NULL;
    }
    memset(map, 0, sizeof(struct hashmap));
    map->elsize = elsize;
    map->bucketsz = bucketsz;
    map->seed0 = seed0;
    map->seed1 = seed1;
    map->hash = hash;
    map->compare = compare;
    map->elfree = elfree;
    map->udata = udata;
    map->spare = ((char *)map) + sizeof(struct hashmap);
    map->edata = (char *)map->spare + bucketsz;
    map->cap = cap;
    map->nbuckets = cap;
    map->mask = map->nbuckets - 1;
    map->buckets = _malloc(map->bucketsz * map->nbuckets);
    if (!map->buckets) {
        _free(map);
        return NULL;
    }
    memset(map->buckets, 0, map->bucketsz * map->nbuckets);
    map->growpower = 1;
    map->growat = (sizet)(map->nbuckets * GROW_AT);
    map->shrinkat = (sizet)(map->nbuckets * SHRINK_AT);
    map->malloc = _malloc;
    map->realloc = _realloc;
    map->free = _free;
    return map;
}

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
struct hashmap *hashmap_new(sizet elsize,
                            sizet cap,
                            u64 seed0,
                            u64 seed1,
                            u64 (*hash)(const void *item, u64 seed0, u64 seed1),
                            int (*compare)(const void *a, const void *b, void *udata),
                            void (*elfree)(void *item),
                            void *udata)
{
    return hashmap_new_with_allocator(NULL, NULL, NULL, elsize, cap, seed0, seed1, hash, compare, elfree, udata);
}

static void free_elements(struct hashmap *map)
{
    if (map->elfree) {
        for (sizet i = 0; i < map->nbuckets; i++) {
            struct bucket *bucket = bucket_at(map, i);
            if (bucket->dib)
                map->elfree(bucket_item(bucket));
        }
    }
}

// hashmap_clear quickly clears the map.
// Every item is called with the element-freeing function given in hashmap_new,
// if present, to free any data referenced in the elements of the hashmap.
// When the update_cap is provided, the map's capacity will be updated to match
// the currently number of allocated buckets. This is an optimization to ensure
// that this operation does not perform any allocations.
void hashmap_clear(struct hashmap *map, bool update_cap)
{
    map->count = 0;
    free_elements(map);
    if (update_cap) {
        map->cap = map->nbuckets;
    }
    else if (map->nbuckets != map->cap) {
        void *new_buckets = map->malloc(map->bucketsz * map->cap);
        if (new_buckets) {
            map->free(map->buckets);
            map->buckets = new_buckets;
        }
        map->nbuckets = map->cap;
    }
    memset(map->buckets, 0, map->bucketsz * map->nbuckets);
    map->mask = map->nbuckets - 1;
    map->growat = (sizet)(map->nbuckets * 0.75);
    map->shrinkat = (sizet)(map->nbuckets * 0.10);
}

static bool resize0(struct hashmap *map, sizet new_cap)
{
    struct hashmap *map2 = hashmap_new_with_allocator(
        map->malloc, map->realloc, map->free, map->elsize, new_cap, map->seed0, map->seed1, map->hash, map->compare, map->elfree, map->udata);
    if (!map2)
        return false;
    for (sizet i = 0; i < map->nbuckets; i++) {
        struct bucket *entry = bucket_at(map, i);
        if (!entry->dib) {
            continue;
        }
        entry->dib = 1;
        sizet j = entry->hash & map2->mask;
        while (1) {
            struct bucket *bucket = bucket_at(map2, j);
            if (bucket->dib == 0) {
                memcpy(bucket, entry, map->bucketsz);
                break;
            }
            if (bucket->dib < entry->dib) {
                memcpy(map2->spare, bucket, map->bucketsz);
                memcpy(bucket, entry, map->bucketsz);
                memcpy(entry, map2->spare, map->bucketsz);
            }
            j = (j + 1) & map2->mask;
            entry->dib += 1;
        }
    }
    map->free(map->buckets);
    map->buckets = map2->buckets;
    map->nbuckets = map2->nbuckets;
    map->mask = map2->mask;
    map->growat = map2->growat;
    map->shrinkat = map2->shrinkat;
    map->free(map2);
    return true;
}

static bool resize(struct hashmap *map, sizet new_cap)
{
    return resize0(map, new_cap);
}

// hashmap_set_with_hash works like hashmap_set but you provide your
// own hash. The 'hash' callback provided to the hashmap_new function
// will not be called
const void *hashmap_set_with_hash(struct hashmap *map, const void *item, u64 hash)
{
    hash = clip_hash(hash);
    map->oom = false;
    if (map->count == map->growat) {
        if (!resize(map, map->nbuckets * i32(1 << map->growpower))) {
            map->oom = true;
            return NULL;
        }
    }

    bucket *entry = (bucket*)map->edata;
    entry->hash = hash;
    entry->dib = 1;
    void *eitem = bucket_item(entry);
    memcpy(eitem, item, map->elsize);

    void *bitem;
    sizet i = entry->hash & map->mask;
    while (1) {
        struct bucket *bucket = bucket_at(map, i);
        if (bucket->dib == 0) {
            memcpy(bucket, entry, map->bucketsz);
            map->count++;
            return NULL;
        }
        bitem = bucket_item(bucket);
        if (entry->hash == bucket->hash && (!map->compare || map->compare(eitem, bitem, map->udata) == 0)) {
            memcpy(map->spare, bitem, map->elsize);
            memcpy(bitem, eitem, map->elsize);
            return map->spare;
        }
        if (bucket->dib < entry->dib) {
            memcpy(map->spare, bucket, map->bucketsz);
            memcpy(bucket, entry, map->bucketsz);
            memcpy(entry, map->spare, map->bucketsz);
            eitem = bucket_item(entry);
        }
        i = (i + 1) & map->mask;
        entry->dib += 1;
    }
}

// hashmap_set inserts or replaces an item in the hash map. If an item is
// replaced then it is returned otherwise NULL is returned. This operation
// may allocate memory. If the system is unable to allocate additional
// memory then NULL is returned and hashmap_oom() returns true.
const void *hashmap_set(struct hashmap *map, const void *item)
{
    return hashmap_set_with_hash(map, item, get_hash(map, item));
}

// hashmap_get_with_hash works like hashmap_get but you provide your
// own hash. The 'hash' callback provided to the hashmap_new function
// will not be called
const void *hashmap_get_with_hash(struct hashmap *map, const void *key, u64 hash)
{
    hash = clip_hash(hash);
    sizet i = hash & map->mask;
    while (1) {
        struct bucket *bucket = bucket_at(map, i);
        if (!bucket->dib)
            return NULL;
        if (bucket->hash == hash) {
            void *bitem = bucket_item(bucket);
            if (!map->compare || map->compare(key, bitem, map->udata) == 0) {
                return bitem;
            }
        }
        i = (i + 1) & map->mask;
    }
}

// hashmap_get returns the item based on the provided key. If the item is not
// found then NULL is returned.
const void *hashmap_get(struct hashmap *map, const void *key)
{
    return hashmap_get_with_hash(map, key, get_hash(map, key));
}

// hashmap_probe returns the item in the bucket at position or NULL if an item
// is not set for that bucket. The position is 'moduloed' by the number of
// buckets in the hashmap.
const void *hashmap_probe(struct hashmap *map, u64 position)
{
    sizet i = position & map->mask;
    struct bucket *bucket = bucket_at(map, i);
    if (!bucket->dib) {
        return NULL;
    }
    return bucket_item(bucket);
}

// hashmap_delete_with_hash works like hashmap_delete but you provide your
// own hash. The 'hash' callback provided to the hashmap_new function
// will not be called
const void *hashmap_delete_with_hash(struct hashmap *map, const void *key, u64 hash)
{
    hash = clip_hash(hash);
    map->oom = false;
    sizet i = hash & map->mask;
    while (1) {
        struct bucket *bucket = bucket_at(map, i);
        if (!bucket->dib) {
            return NULL;
        }
        void *bitem = bucket_item(bucket);
        if (bucket->hash == hash && (!map->compare || map->compare(key, bitem, map->udata) == 0)) {
            memcpy(map->spare, bitem, map->elsize);
            bucket->dib = 0;
            while (1) {
                struct bucket *prev = bucket;
                i = (i + 1) & map->mask;
                bucket = bucket_at(map, i);
                if (bucket->dib <= 1) {
                    prev->dib = 0;
                    break;
                }
                memcpy(prev, bucket, map->bucketsz);
                prev->dib--;
            }
            map->count--;
            if (map->nbuckets > map->cap && map->count <= map->shrinkat) {
                // Ignore the return value. It's ok for the resize operation to
                // fail to allocate enough memory because a shrink operation
                // does not change the integrity of the data.
                resize(map, map->nbuckets / 2);
            }
            return map->spare;
        }
        i = (i + 1) & map->mask;
    }
}

// hashmap_delete removes an item from the hash map and returns it. If the
// item is not found then NULL is returned.
const void *hashmap_delete(struct hashmap *map, const void *key)
{
    return hashmap_delete_with_hash(map, key, get_hash(map, key));
}

// hashmap_count returns the number of items in the hash map.
sizet hashmap_count(struct hashmap *map)
{
    return map->count;
}

// hashmap_free frees the hash map
// Every item is called with the element-freeing function given in hashmap_new,
// if present, to free any data referenced in the elements of the hashmap.
void hashmap_free(struct hashmap *map)
{
    if (!map)
        return;
    free_elements(map);
    map->free(map->buckets);
    map->free(map);
}

// hashmap_oom returns true if the last hashmap_set() call failed due to the
// system being out of memory.
bool hashmap_oom(struct hashmap *map)
{
    return map->oom;
}

// hashmap_scan iterates over all items in the hash map
// Param `iter` can return false to stop iteration early.
// Returns false if the iteration has been stopped early.
bool hashmap_scan(struct hashmap *map, bool (*iter)(const void *item, void *udata), void *udata)
{
    for (sizet i = 0; i < map->nbuckets; i++) {
        struct bucket *bucket = bucket_at(map, i);
        if (bucket->dib && !iter(bucket_item(bucket), udata)) {
            return false;
        }
    }
    return true;
}

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
bool hashmap_iter(struct hashmap *map, sizet *i, void **item)
{
    struct bucket *bucket;
    do {
        if (*i >= map->nbuckets)
            return false;
        bucket = bucket_at(map, *i);
        (*i)++;
    } while (!bucket->dib);
    *item = bucket_item(bucket);
    return true;
}

// hashmap_sip returns a hash value for `data` using SipHash-2-4.
u64 hashmap_sip(const void *data, sizet len, u64 seed0, u64 seed1)
{
    return siphash((u8 *)data, len, seed0, seed1);
}

// hashmap_murmur returns a hash value for `data` using Murmur3_86_128.
u64 hashmap_murmur(const void *data, sizet len, u64 seed0, u64)
{
    return murmurhash3(data, len, (u32)seed0);
}

u64 hashmap_xxhash3(const void *data, sizet len, u64 seed0, u64)
{
    return xxhash3(data, len, seed0);
}

} // namespace nslib
