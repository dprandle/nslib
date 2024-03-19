#include <cstring>

#include "containers/string.h"
#include "robj_common.h"
#include "hashfuncs.h"

// robj type headers
#include "model.h"

namespace nslib
{

intern const sizet ROBJ_TYPE_SIZE[ROBJ_TYPE_USER] = {sizeof(mesh)};

rid::rid(const string &_str) : str(_str), id(hash_type(str, 0, 0))
{}

rid::rid(const char *_str) : str(_str), id(hash_type(str, 0, 0))
{}

string to_str(const rid &rid)
{
    string ret;
    ret += "\nrid {\nid:" + to_str(rid.id) + "\nstr:" + rid.str + "\n}";
    return ret;
}

rid generate_id()
{
    // Generate in this format 774f0899-9666-471a-b1f9
    rid ret{};
    u32 r1 = rand();
    u32 r2 = rand();
    u16 r3 = rand();
    str_printf(&ret.str, "%08x-%08x-%04x", r1, r2, r3);
    ret.id = hash_type(ret.str, 0, 0);
    return ret;
}

void init_cache_group(robj_cache_group *cg, mem_arena *arena)
{
    arr_init(&cg->caches, arena);
}

void init_cache_group_default_types(robj_cache_group *cg, mem_arena *arena)
{
    init_cache_group(cg, arena);
    for (u32 i = ROBJ_TYPE_USER; i != 0; --i) {
        add_cache(i - 1, ROBJ_TYPE_SIZE[i - 1], ROBJ_TYPE_DEFAULT_BUDGET[i - 1], cg);
    }
}

void terminate_cache_group(robj_cache_group *cg)
{
    for (int i = 0; i < cg->caches.size; ++i) {
        terminate_cache(cg->caches[i]);
        mem_free(cg->caches[i]);
        cg->caches[i] = {};
    }
    arr_terminate(&cg->caches);
}

void init_cache(robj_cache *cache, u32 rtype, sizet item_size, sizet item_budget, mem_arena *upstream)
{
    hashmap_init(&cache->rmap, upstream);
    cache->arena.mpool.chunk_size = item_size;
    cache->arena.upstream_allocator = upstream;
    cache->rtype = rtype;
    mem_init_arena(item_budget * item_size, mem_alloc_type::POOL, &cache->arena);
}

void terminate_cache(robj_cache *cache)
{
    mem_terminate_arena(&cache->arena);
    hashmap_terminate(&cache->rmap);
}

robj_cache *add_cache(u32 rtype, sizet item_size, sizet item_budget, robj_cache_group *cg)
{
    if ((rtype + 1) > cg->caches.size) {
        arr_resize(&cg->caches, rtype + 1);
    }
    if (!cg->caches[rtype]) {
        auto cache = (robj_cache *)mem_alloc(sizeof(robj_cache), cg->caches.arena);
        init_cache(cache, rtype, item_size, item_budget, cg->caches.arena);
        cg->caches[rtype] = cache;
    }
    return cg->caches[rtype];
}

robj_cache *get_cache(u32 rtype, const robj_cache_group *cg)
{
    return cg->caches[rtype];
}

bool remove_cache(u32 rtype, robj_cache_group *cg)
{
    auto cache = get_cache(rtype, cg);
    if (cache) {
        terminate_cache(cache);
        mem_free(cache, cg->caches.arena);
        cg->caches[rtype] = {};
        return true;
    }
    return false;
}

bool remove_cache(robj_cache *cache, robj_cache_group *cg)
{
    return remove_cache(cache->rtype, cg);
}

void *add_robj(const rid &id, robj_cache *cache)
{
    auto ret = mem_alloc(cache->arena.mpool.chunk_size, &cache->arena);
    memset(ret, 0, cache->arena.mpool.chunk_size);
    hashmap_set(&cache->rmap, id, ret);
    return ret;
}

void *add_robj(robj_cache *cache)
{
    return add_robj(generate_id(), cache);
}

void *get_robj(const rid &id, const robj_cache *cache)
{
    auto item = hashmap_find(&cache->rmap, id);
    if (item) {
        return item->value;
    }
    return nullptr;
}

void *get_robj(const rid &id, u32 rtype, const robj_cache_group *cg)
{
    auto cache = get_cache(rtype, cg);
    return get_robj(id, cache);
}

bool remove_robj(const rid &id, robj_cache *cache)
{
    auto obj = get_robj(id, cache);
    if (obj) {
        mem_free(obj, &cache->arena);
        hashmap_remove(&cache->rmap, id);
    }
    return false;
}


} // namespace nslib
