#include "archive_common.h"
#include "basic_types.h"
#include "hashfuncs.h"
#include "containers/string.h"

#pragma once
namespace nslib
{
struct rid
{
    rid()
    {}
    explicit rid(const string &str);
    explicit rid(const char *str);

    string str;
    u64 id{0};
};

string to_string(const rid &rid);

inline u64 hash_type(const rid &id, u64, u64)
{
    return id.id;
}

inline bool operator==(const rid &lhs, const rid &rhs)
{
    return lhs.id == rhs.id;
}

inline bool operator!=(const rid &lhs, const rid &rhs)
{
    return !(lhs == rhs);
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
