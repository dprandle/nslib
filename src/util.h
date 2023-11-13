#pragma once

#include "archive_common.h"

namespace nslib
{
template<class F, class S>
struct pair
{
    union
    {
        struct
        {
            F first;
            S second;
        };
        struct
        {
            F key{};
            S value{};
        };
    };
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
