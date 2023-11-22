#pragma once

#include "archive_common.h"

namespace nslib
{

struct version_info
{
    int major;
    int minor;
    int patch;
};
pup_func(version_info)
{
    pup_member(major);
    pup_member(minor);
    pup_member(patch);
}    

template<class F, class S>
union pair
{
    struct
    {
        F first;
        S second;
    };
    struct
    {
        F key;
        S value;
    };

    ~pair()
    {
        first.~F();
        second.~S();
    }
};

pup_func_tt(pair)
{
    if (test_flags(vinfo.meta.flags, pack_va_flags::PACK_PAIR_KEY_VAL)) {
        pup_member(key);
        pup_member(value);
    }
    else {
        pup_member(first);
        pup_member(second);
    }
}
} // namespace nslib
