#include "archive_common.h"
#include "containers/string.h"
#include "containers/hashmap.h"

#pragma once
namespace nslib
{

enum robj_type : u32
{
    ROBJ_TYPE_MESH,
    ROBJ_TYPE_USER
};

const sizet ROBJ_TYPE_DEFAULT_BUDGET[ROBJ_TYPE_USER] = {256};

struct rid
{
    rid()
    {}
    explicit rid(const string &str);
    explicit rid(const char *str);

    string str;
    u64 id{0};
};

pup_func(rid)
{
    pup_member_info(str, vinfo);
    if (ar->opmode == archive_opmode::UNPACK) {
        val.id = hash_type(val.str, 0, 0);
    }
}

string to_str(const rid &rid);

inline u64 hash_type(const rid &id, u64, u64)
{
    return id.id;
}

rid generate_id();

inline bool operator==(const rid &lhs, const rid &rhs)
{
    return lhs.id == rhs.id;
}

inline bool operator!=(const rid &lhs, const rid &rhs)
{
    return !(lhs == rhs);
}

#define ROBJ(type)                                                                                                                         \
    static constexpr const char *type_str = #type;                                                                                         \
    static constexpr const u32 type_id = ROBJ_TYPE_##type;                                                                                 \
    rid id;                                                                                                                                \
    u64 flags;

#define PUP_ROBJ                                                                                                                           \
    pup_member(id);                                                                                                                        \
    pup_member(flags);

struct robj_cache
{
    // Resource id to pointer to resource obj
    hashmap<rid, void *> rmap{};
    u32 rtype{};
    mem_arena arena{};
};

struct robj_cache_group
{
    array<robj_cache *> caches{};
};

// Initialize cache group with arena - all caches added to group will use this area
void init_cache_group(robj_cache_group *cg, mem_arena *arena);

// Initialize cache group with arena - all caches added to group will use this area
void init_cache_group_default_types(robj_cache_group *cg, mem_arena *arena);

void terminate_cache_group(robj_cache_group *cg);

// Initialize cache of type rtype with a mem pool of total size item_budget * sizeof(item_size)
void init_cache(robj_cache *cache, u32 rtype, sizet item_size, sizet item_budget, mem_arena *upstream);

// Initialize cache of type rtype with a mem pool of total size item_budget * sizeof(item_size)
template<class T>
void init_cache(robj_cache *cache, sizet item_budget, mem_arena *upstream)
{
    init_cache(cache, T::type, sizeof(T), item_budget, upstream);
}

void terminate_cache(robj_cache *cache);

robj_cache *add_cache(u32 rtype, sizet item_size, sizet item_budget, robj_cache_group *cg);
// Add and initialize a cache to the passed in cache group

template<class T>
robj_cache *add_cache(sizet item_budget, robj_cache_group *cg)
{
    return add_cache(T::type, sizeof(T), item_budget, cg);
}

robj_cache *get_cache(u32 rtype, const robj_cache_group *cg);

template<class T>
robj_cache *get_cache(const robj_cache_group *cg)
{
    return get_cache(T::type_id, cg);
}

// Remove and terminate a cache of type rtype from the cache group - true on success or if the cache is not there false
bool remove_cache(u32 rtype, robj_cache_group *cg);

// Remove and terminates cache from the cache group - true on success or if the cache is not there false
bool remove_cache(robj_cache *cache, robj_cache_group *cg);

// This will add a new robj to the cache, but it will be completely zeroed out so you gotta set the id manually
void *add_robj(const rid &id, robj_cache *cache);

// This will add a new robj to the cache, but it will be completely zeroed out so you gotta set the id manually
void *add_robj(robj_cache *cache);

// This will add a new robj to the cache, zero it out, then set the id of it
template<class T>
T *add_robj(const rid &id, robj_cache *cache)
{
    assert(T::type_id == cache->rtype);
    auto ret = (T *)add_robj(id, cache);
    ret->id = id;
    return ret;
}

// This will add a new robj to the cache, zero it out, then set the id of it
template<class T>
T *add_robj(robj_cache *cache)
{
    return add_robj<T>(generate_id(), cache);
}

// This will add a new robj to the cache, zero it out, set it to the copy, and then set the id to copy_id
template<class T>
T *add_robj(const T &copy, const rid &copy_id, robj_cache *cache)
{
    assert(T::type_id == cache->rtype);
    auto cpy = add_robj<T>(cache, copy_id);
    *cpy = copy;
    cpy->id = copy_id;
    return cpy;
}

// This will add a new robj to the cache, zero it out, set it to the copy, and then set the id to copy_id
template<class T>
T *add_robj(const T &copy, robj_cache *cache)
{
    return add_robj<T>(copy, generate_id(), cache);
}

void *get_robj(const rid &id, const robj_cache *cache);

void *get_robj(const rid &id, u32 rtype, const robj_cache_group *cg);

template<class T>
T *get_robj(const rid &id, const robj_cache *cache)
{
    return (T *)get_robj(id, cache);
}

template<class T>
T *get_robj(const rid &id, const robj_cache_group *cg)
{
    return (T *)get_robj(id, T::type_id, cg);
}

bool remove_robj(const rid &id, robj_cache *cache);

template<class T>
bool remove_robj(const T &item, robj_cache *cache)  {
    return remove_obj(item->id, cache);
}


} // namespace nslib
