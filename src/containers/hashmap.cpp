// Copyright 2020 Joshua J Baker. All rights reserved.
// Use of this source code is governed by an MIT-style
// license that can be found in the LICENSE file.

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <stddef.h>

#include "../memory.h"
#include "hashmap.h"
#include "../hashfuncs.h"

#define GROW_AT 0.60
#define SHRINK_AT 0.10

namespace nslib
{

static void *(*__malloc)(sizet) = NULL;
static void *(*__realloc)(void *, sizet) = NULL;
static void (*__free)(void *) = NULL;

void ihashmap_set_grow_by_power(struct ihashmap *map, sizet power)
{
    map->growpower = (u8)(power < 1 ? 1 : power > 16 ? 16 : power);
}

static struct ihashmap_bucket *bucket_at0(void *buckets, sizet bucketsz, sizet i)
{
    return (struct ihashmap_bucket *)(((char *)buckets) + (bucketsz * i));
}

static struct ihashmap_bucket *bucket_at(const struct ihashmap *map, sizet index)
{
    return bucket_at0(map->buckets, map->bucketsz, index);
}

static void *bucket_item(struct ihashmap_bucket *entry)
{
    return ((char *)entry) + sizeof(struct ihashmap_bucket);
}

static u64 clip_hash(u64 hash)
{
    return hash & 0xFFFFFFFFFFFF;
}

static u64 get_hash(struct ihashmap *map, const void *key)
{
    return clip_hash(map->hash(key, map->seed0, map->seed1));
}

struct ihashmap *ihashmap_new_with_allocator(void *(*_malloc)(sizet),
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
    sizet bucketsz = sizeof(struct ihashmap_bucket) + elsize;
    while (bucketsz & (sizeof(uintptr_t) - 1)) {
        bucketsz++;
    }
    // hashmap + spare + edata
    sizet size = sizeof(struct ihashmap) + bucketsz * 2;
    ihashmap *map = (ihashmap *)_malloc(size);
    if (!map) {
        return NULL;
    }
    memset(map, 0, sizeof(struct ihashmap));
    map->elsize = elsize;
    map->bucketsz = bucketsz;
    map->seed0 = seed0;
    map->seed1 = seed1;
    map->hash = hash;
    map->compare = compare;
    map->elfree = elfree;
    map->udata = udata;
    map->spare = ((char *)map) + sizeof(struct ihashmap);
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

struct ihashmap *ihashmap_new(sizet elsize,
                             sizet cap,
                             u64 seed0,
                             u64 seed1,
                             u64 (*hash)(const void *item, u64 seed0, u64 seed1),
                             int (*compare)(const void *a, const void *b, void *udata),
                             void (*elfree)(void *item),
                             void *udata)
{
    return ihashmap_new_with_allocator(NULL, NULL, NULL, elsize, cap, seed0, seed1, hash, compare, elfree, udata);
}

int generate_rand_seed()
{
    return rand();
}

malloc_func_type *get_global_malloc_func()
{
    return mem_alloc;
}

realloc_func_type *get_global_realloc_func()
{
    return mem_realloc;
}

free_func_type *get_global_free_func()
{
    return mem_free;
}

static void free_elements(struct ihashmap *map)
{
    if (map->elfree) {
        for (sizet i = 0; i < map->nbuckets; i++) {
            struct ihashmap_bucket *bucket = bucket_at(map, i);
            if (bucket->dib)
                map->elfree(bucket_item(bucket));
        }
    }
}

void ihashmap_clear(struct ihashmap *map, bool update_cap)
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

static bool resize0(struct ihashmap *map, sizet new_cap)
{
    struct ihashmap *map2 = ihashmap_new_with_allocator(
        map->malloc, map->realloc, map->free, map->elsize, new_cap, map->seed0, map->seed1, map->hash, map->compare, map->elfree, map->udata);
    if (!map2)
        return false;
    for (sizet i = 0; i < map->nbuckets; i++) {
        struct ihashmap_bucket *entry = bucket_at(map, i);
        if (!entry->dib) {
            continue;
        }
        entry->dib = 1;
        sizet j = entry->hash & map2->mask;
        while (1) {
            struct ihashmap_bucket *bucket = bucket_at(map2, j);
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

static bool resize(struct ihashmap *map, sizet new_cap)
{
    return resize0(map, new_cap);
}

const void *ihashmap_set_with_hash(struct ihashmap *map, const void *item, u64 hash)
{
    hash = clip_hash(hash);
    map->oom = false;
    if (map->count == map->growat) {
        if (!resize(map, map->nbuckets * i32(1 << map->growpower))) {
            map->oom = true;
            return NULL;
        }
    }

    ihashmap_bucket *entry = (ihashmap_bucket *)map->edata;
    entry->hash = hash;
    entry->dib = 1;
    void *eitem = bucket_item(entry);
    memcpy(eitem, item, map->elsize);

    void *bitem;
    sizet i = entry->hash & map->mask;
    while (1) {
        struct ihashmap_bucket *bucket = bucket_at(map, i);
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

const void *ihashmap_set(struct ihashmap *map, const void *item)
{
    return ihashmap_set_with_hash(map, item, get_hash(map, item));
}

const void *ihashmap_get_with_hash(struct ihashmap *map, const void *key, u64 hash)
{
    hash = clip_hash(hash);
    sizet i = hash & map->mask;
    while (1) {
        struct ihashmap_bucket *bucket = bucket_at(map, i);
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

const void *ihashmap_get(struct ihashmap *map, const void *key)
{
    return ihashmap_get_with_hash(map, key, get_hash(map, key));
}

const void *ihashmap_probe(struct ihashmap *map, u64 position)
{
    sizet i = position & map->mask;
    struct ihashmap_bucket *bucket = bucket_at(map, i);
    if (!bucket->dib) {
        return NULL;
    }
    return bucket_item(bucket);
}

const void *ihashmap_delete_with_hash(struct ihashmap *map, const void *key, u64 hash)
{
    hash = clip_hash(hash);
    map->oom = false;
    sizet i = hash & map->mask;
    while (1) {
        struct ihashmap_bucket *bucket = bucket_at(map, i);
        if (!bucket->dib) {
            return NULL;
        }
        void *bitem = bucket_item(bucket);
        if (bucket->hash == hash && (!map->compare || map->compare(key, bitem, map->udata) == 0)) {
            memcpy(map->spare, bitem, map->elsize);
            bucket->dib = 0;
            while (1) {
                struct ihashmap_bucket *prev = bucket;
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

const void *ihashmap_delete(struct ihashmap *map, const void *key)
{
    return ihashmap_delete_with_hash(map, key, get_hash(map, key));
}

sizet ihashmap_count(struct ihashmap *map)
{
    return map->count;
}

void ihashmap_free(struct ihashmap *map)
{
    if (!map)
        return;
    free_elements(map);
    map->free(map->buckets);
    map->free(map);
}

bool ihashmap_oom(struct ihashmap *map)
{
    return map->oom;
}

bool ihashmap_scan(struct ihashmap *map, bool (*iter)(const void *item, void *udata), void *udata)
{
    for (sizet i = 0; i < map->nbuckets; i++) {
        struct ihashmap_bucket *bucket = bucket_at(map, i);
        if (bucket->dib && !iter(bucket_item(bucket), udata)) {
            return false;
        }
    }
    return true;
}

bool ihashmap_iter(const struct ihashmap *map, sizet *i, void **item)
{
    struct ihashmap_bucket *bucket;
    do {
        if (*i >= map->nbuckets)
            return false;
        bucket = bucket_at(map, *i);
        (*i)++;
    } while (!bucket->dib);
    *item = bucket_item(bucket);
    return true;
}

} // namespace nslib
