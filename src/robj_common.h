#include "archive_common.h"
#include "basic_types.h"
#include "hashfuncs.h"

#pragma once
namespace nslib
{

struct robj_cache_common
{
    
};

struct robj_common
{
    u32 id;
};

pup_func(robj_common)
{
    pup_member(id);
}



} // namespace nslib
