#pragma once

#include "archive_common.h"
#include "rid.h"
#include "containers/hashmap.h"

namespace nslib
{

enum robj_type : u32
{
    ROBJ_TYPE_MESH,
    ROBJ_TYPE_TEXTURE,
    ROBJ_TYPE_MATERIAL,
    ROBJ_TYPE_USER,
};

const sizet ROBJ_TYPE_DEFAULT_BUDGET[ROBJ_TYPE_USER] = {256,256,256};


#define ROBJ(type)                                                                                                                         \
    static constexpr const char *type_str = #type;                                                                                         \
    static constexpr const u32 type_id = ROBJ_TYPE_##type;                                                                                 \
    rid id;                                                                                                                                \
    u64 flags;

#define PUP_ROBJ                                                                                                                           \
    pup_member(id);                                                                                                                        \
    pup_member(flags);

template<class T>
struct robj_cache
{
    using robj_type = T;
    using iter = key_val_pair<rid, T *> *;

    // Resource id to pointer to resource obj
    hashmap<rid, T *> rmap{};
    mem_arena arena{};
    sizet mem_alignment{};
};

using mesh_cache = robj_cache<struct mesh>;
using material_cache = robj_cache<struct material>;

struct robj_cache_group
{
    array<void *> caches{};
};

// Initialize cache group with arena - all caches added to group will use this area
void init_cache_group(robj_cache_group *cg, mem_arena *arena);

// Initialize cache group with arena - all caches added to group will use this area
void init_cache_group_default_types(robj_cache_group *cg, mem_arena *arena);

void terminate_cache_group(robj_cache_group *cg);

// Terminate all of the default robj types from the above enum and typedefs
void terminate_cache_group_default_types(robj_cache_group *cg);

// Initialize cache of type rtype with a mem pool of total size item_budget * sizeof(item_size)
template<class T>
void init_cache(robj_cache<T> *cache, sizet item_budget, sizet mem_alignment, mem_arena *upstream)
{
    cache->mem_alignment = mem_alignment;
    hashmap_init(&cache->rmap, upstream);
    mem_init_pool_arena(&cache->arena, sizeof(T), item_budget, upstream);
}

template<class T>
void terminate_cache(robj_cache<T> *cache)
{
    // NOTE: We might want to call terminate on each robj here
    sizet i{};
    while (auto iter = cache_iter(&i, cache)) {
        terminate_robj(iter->value);
        mem_free(iter->value, &cache->arena);
    }
    mem_terminate_arena(&cache->arena);
    hashmap_terminate(&cache->rmap);
}

// Add and initialize a cache to the passed in cache group
template<class T>
robj_cache<T> *add_cache(sizet item_budget, sizet mem_alignment, robj_cache_group *cg)
{
    if ((T::type_id + 1) > cg->caches.size) {
        arr_resize(&cg->caches, T::type_id + 1);
    }
    if (!cg->caches[T::type_id]) {
        auto cache = mem_alloc<robj_cache<T>>(cg->caches.arena);
        memset(cache, 0, sizeof(robj_cache<T>));
        init_cache(cache, item_budget, mem_alignment, cg->caches.arena);
        cg->caches[T::type_id] = cache;
    }
    return (robj_cache<T> *)cg->caches[T::type_id];
}

template<class T>
robj_cache<T> *add_cache(sizet item_budget, robj_cache_group *cg)
{
    return add_cache<T>(item_budget, DEFAULT_MIN_ALIGNMENT, cg);
}

template<class T>
robj_cache<T> *get_cache(const robj_cache_group *cg)
{
    if (T::type_id < cg->caches.size) {
        return (robj_cache<T> *)cg->caches[T::type_id];
    }
    return nullptr;
}

template<class T>
typename robj_cache<T>::iter cache_iter(sizet *i, robj_cache<T> *cache)
{
    return hashmap_iter(&cache->rmap, i);
}

// Remove and terminates cache from the cache group - true on success or if the cache is not there false
template<class T>
bool remove_cache(robj_cache<T> *cache, robj_cache_group *cg)
{
    if (cache) {
        assert(cache == cg->caches[T::type_id]);
        terminate_cache(cache);
        mem_free(cache, cg->caches.arena);
        cg->caches[T::type_id] = {};
        return true;
    }
    return false;
}

// Remove and terminate a cache of type rtype from the cache group - true on success or if the cache is not there false
template<class T>
bool remove_cache(robj_cache_group *cg)
{
    auto cache = get_cache<T>(cg);
    return remove_cache(cache, cg);
}

// This will add a new robj to the cache, but it will be completely zeroed out so you gotta set the id manually
template<class T>
T *add_robj(const rid &id, robj_cache<T> *cache)
{
    auto ret = (T *)mem_alloc(cache->arena.mpool.chunk_size, &cache->arena, cache->mem_alignment);
    memset(ret, 0, cache->arena.mpool.chunk_size);
    ret->id = id;
    hashmap_set(&cache->rmap, id, ret);
    return ret;
}

// This will add a new robj to the cache, zero it out, set it to the copy, and then set the id to copy_id
template<class T>
T *add_robj(const T &copy, const rid &copy_id, robj_cache<T> *cache)
{
    auto cpy = add_robj<T>(cache, copy_id);
    *cpy = copy;
    cpy->id = copy_id;
    return cpy;
}

// This will add a new robj to the cache, zero it out, then set the id of it
template<class T>
T *add_robj(robj_cache<T> *cache)
{
    return add_robj<T>(generate_id(), cache);
}

// This will add a new robj to the cache, zero it out, set it to the copy, and then set the id to a generated id
template<class T>
T *add_robj(const T &copy, robj_cache<T> *cache)
{
    return add_robj<T>(copy, generate_id(), cache);
}

template<class T>
T *get_robj(const rid &id, const robj_cache<T> *cache)
{
    auto item = hashmap_find(&cache->rmap, id);
    if (item) {
        return item->value;
    }
    return nullptr;
}

template<class T>
T *get_robj(const rid &id, const robj_cache_group *cg)
{
    auto cache = get_cache<T>(cg);
    return get_robj(id, cache);
}

template<class T>
bool remove_robj(const rid &id, robj_cache<T> *cache)
{
    auto obj = get_robj(id, cache);
    return remove_robj(obj, cache);
}

template<class T>
bool remove_robj(const T &item, robj_cache<T> *cache)
{
    if (item) {
        hashmap_remove(&cache->rmap, item->id);
        mem_free(item, &cache->arena);
        return true;
    }
    return false;
}

template<class T>
void terminate_robj(T *robj) {
    ilog("Teminate %s id %s", robj->type_str, str_cstr(robj->id.str));
}

} // namespace nslib
