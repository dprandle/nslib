#include "archive_common.h"
#include "basic_types.h"
#include "hashfuncs.h"

#pragma once
namespace nslib
{

struct rid
{
    const char *str{};
    u64 id{0};
};

rid gen_id(const char *str);

inline u64 hash_type(const rid &id) {
    return id.id;
}

struct robj_cache_common
{};

struct robj_common
{
    u32 id;
};

pup_func(robj_common)
{
    pup_member(id);
}

} // namespace nslib
