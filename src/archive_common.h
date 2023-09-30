#pragma once
#include <cstring>

#include "logging.h"
#include "basic_types.h"
#include "basic_type_traits.h"

namespace nslib
{
enum pack_dir
{
    PACK_DIR_IN,
    PACK_DIR_OUT
};

namespace pack_va_flags
{
enum : u64
{
    FIXED_ARRAY_CUSTOM_SIZE = 1 // Specify that there is a custom sizet specified by the void* to pack/unpack the array
                                // to
};
}

struct pack_var_meta
{
    u64 flags{0};
    void *data{nullptr};
};

struct pack_var_info
{
    const char *name{};
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
const char* get_flag_for_type(T &var)
{
    return "%d";
}

#define pup_func(type)                                                                                                                     \
    template<class ArchiveT>                                                                                                               \
    void pack_unpack(ArchiveT &ar, type &val, const pack_var_info &vinfo)

#define pup_member(mem_name) pack_unpack(ar, val.mem_name, {#mem_name, {}})
#define pup_member_meta(mem_name, ...) pack_unpack(ar, val.mem_name, {#mem_name, {__VA_ARGS__}})
#define pup_member_name(mem_name, name) pack_unpack(ar, val.mem_name, {name, {}})
#define pup_member_info(mem_name, info) pack_unpack(ar, val.mem_name, info)

} // namespace nslib
