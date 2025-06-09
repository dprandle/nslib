#pragma once

#include "containers/string.h"

namespace nslib
{
struct rid
{
    string str;
    u64 id{0};
};

void set_rid(rid *id, const string &str);
void set_rid(rid *id, const char *str);

rid make_rid(const string &str);
rid make_rid(const char *str);

inline bool is_valid(const rid &id)
{
    return id.id != 0;
}

pup_func(rid)
{
    pup_member_info(str, vinfo);
    if (ar->opmode == archive_opmode::UNPACK) {
        val.id = hash_type(val.str, 0, 0);
    }
}

const string &to_str(const rid &rid);

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

} // namespace nslib
