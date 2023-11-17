#include <cstring>

#include "containers/string.h"
#include "robj_common.h"
#include "hashfuncs.h"

namespace nslib
{

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


void cache_init(robj_cache *cache, i32 rtype, sizet item_size, sizet item_budget, mem_arena *upstream) {
    cache->arena.mpool.chunk_size = item_size;
    cache->arena.upstream_allocator = upstream;
    mem_init_arena(item_budget, mem_alloc_type::POOL, &cache->arena);
}

void cache_terminate(robj_cache *cache) {
    mem_terminate_arena(&cache->arena);
}

} // namespace nslib
