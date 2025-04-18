#pragma once
#include <cstring>

#include "logging.h"
#include "basic_types.h"
#include "basic_type_traits.h"

#define pup_func(type)                                                                                                                     \
    template<class ArchiveT>                                                                                                               \
    void pack_unpack(ArchiveT *ar, type &val, const pack_var_info &vinfo)

#define pup_func_tt(type)                                                                                                                  \
    template<class ArchiveT, class... Args>                                                                                                \
    void pack_unpack(ArchiveT *ar, type<Args...> &val, const pack_var_info &vinfo)

#define pup_member(mem_name) pup_var(ar, val.mem_name, {#mem_name, {}})
#define pup_member_meta(mem_name, ...) pup_var(ar, val.mem_name, {#mem_name, {__VA_ARGS__}})
#define pup_member_name(mem_name, name) pup_var(ar, val.mem_name, {name, {}})
#define pup_member_info(mem_name, info) pup_var(ar, val.mem_name, info)

namespace nslib
{
// Pack direction - OUT means packing the objects to the archive, and IN means 
enum struct archive_opmode
{
    PACK,
    UNPACK
};

namespace pack_va_flags
{
enum : u64
{
    // Specify that there is a custom sizet specified by the void* to pack/unpack the array to
    FIXED_ARRAY_CUSTOM_SIZE = 1,
    // If set, pairs should be packed/unpacked with var name key,val rather than first/second
    PACK_PAIR_KEY_VAL = 2
};
}

struct pack_var_meta
{
    u64 flags{0};
    void *data{nullptr};
};

struct pack_var_info
{
    const char *name{nullptr};
    pack_var_meta meta{};
};

// Returns the printf or logging flag needed to log this type
template<floating_pt T>
const char *get_flag_for_type(T &var)
{
    return "%f";
}

// Returns the printf or logging flag needed to log this type
template<integral T>
const char *get_flag_for_type(T &var)
{
    return "%d";
}

template<class ArchiveT, class T>
void pack_unpack_begin(ArchiveT *, T &, const pack_var_info &vinfo)
{
    dlog("Pack %s begin", vinfo.name);
}

template<class ArchiveT, class T>
void pack_unpack_end(ArchiveT *, T &, const pack_var_info &vinfo)
{
    dlog("Pack %s end", vinfo.name);
}

template<class ArchiveT, class T>
void pup_var(ArchiveT *ar, T &val, const pack_var_info &vinfo)
{
    pack_unpack_begin(ar, val, vinfo);
    pack_unpack(ar, val, vinfo);
    pack_unpack_end(ar, val, vinfo);
}

} // namespace nslib
