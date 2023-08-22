// Copyright 2020 Joshua J Baker. All rights reserved.
// Use of this source code is governed by an MIT-style
// license that can be found in the LICENSE file.

#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "../basic_types.h"

namespace nslib
{
struct hashmap;

struct hashmap *hashmap_new(sizet elsize,
                            sizet cap,
                            u64 seed0,
                            u64 seed1,
                            u64 (*hash)(const void *item, u64 seed0, u64 seed1),
                            int (*compare)(const void *a, const void *b, void *udata),
                            void (*elfree)(void *item),
                            void *udata);

struct hashmap *hashmap_new_with_allocator(void *(*malloc)(sizet),
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

void hashmap_free(struct hashmap *map);
void hashmap_clear(struct hashmap *map, bool update_cap);
sizet hashmap_count(struct hashmap *map);
bool hashmap_oom(struct hashmap *map);
const void *hashmap_get(struct hashmap *map, const void *item);
const void *hashmap_set(struct hashmap *map, const void *item);
const void *hashmap_delete(struct hashmap *map, const void *item);
const void *hashmap_probe(struct hashmap *map, u64 position);
bool hashmap_scan(struct hashmap *map, bool (*iter)(const void *item, void *udata), void *udata);
bool hashmap_iter(struct hashmap *map, sizet *i, void **item);

u64 hashmap_sip(const void *data, sizet len, u64 seed0, u64 seed1);
u64 hashmap_murmur(const void *data, sizet len, u64 seed0, u64 seed1);
u64 hashmap_xxhash3(const void *data, sizet len, u64 seed0, u64 seed1);

const void *hashmap_get_with_hash(struct hashmap *map, const void *key, u64 hash);
const void *hashmap_delete_with_hash(struct hashmap *map, const void *key, u64 hash);
const void *hashmap_set_with_hash(struct hashmap *map, const void *item, u64 hash);
void hashmap_set_grow_by_power(struct hashmap *map, sizet power);

// DEPRECATED: use `hashmap_new_with_allocator`
void hashmap_set_allocator(void *(*malloc)(sizet), void (*free)(void *));
} // namespace nslib
