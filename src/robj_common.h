#include "archive_common.h"
#include "hashfuncs.h"
#include "containers/string.h"

#pragma once
namespace nslib
{
struct rid
{
    rid(){}
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
    rid id;
};

pup_func(robj_common)
{
    pup_member(id);
}

} // namespace nslib
