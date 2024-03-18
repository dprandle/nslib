#include "archive_common.h"
#include "containers/string.h"
#include "containers/hashmap.h"

#pragma once
namespace nslib
{
namespace robj_types
{
enum
{
    GEOMETRY
};
}

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

struct robj_common
{
    rid id;
};

pup_func(robj_common)
{
    pup_member(id);
}

struct robj_cache
{
    hashmap<rid, robj_common*> rmap;
    mem_arena arena;
};

void init_cache(robj_cache *cache, u32 rtype, sizet item_size, sizet item_budget, mem_arena *upstream);

template<class T>
void init_cache(robj_cache *cache, sizet item_budget, mem_arena *upstream)
{
    init_cache(cache, T::type, sizeof(T), item_budget, upstream);
}

void terminate_cache(robj_cache *cache);

} // namespace nslib
