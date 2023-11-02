#pragma once
#include "archive_common.h"
#include <type_traits>

namespace nslib
{

template<class T>
struct is_binary_archive
{
    static constexpr bool value = false;
};

struct binary_buffer_archive
{
    u8 *data;
    archive_opmode opmode;
    sizet cur_offset{0};
};

// Enable type trait
template<>
struct is_binary_archive<binary_buffer_archive>
{
    static constexpr bool value = true;
};

template<sizet N>
struct binary_fixed_buffer_archive
{
    static constexpr sizet size = N;
    archive_opmode opmode;
    sizet cur_offset{0};
    u8 data[size];
};

// Enable type trait
template<sizet N>
struct is_binary_archive<binary_fixed_buffer_archive<N>>
{
    static constexpr bool value = true;
};

template<class T>
concept binary_archive_type = is_binary_archive<T>::value;

template<binary_archive_type ArchiveT, class T>
void pack_unpack_begin(ArchiveT *, T &, const pack_var_info &vinfo) {
    dlog("Pack binary archive %s begin", vinfo.name);
}

template<binary_archive_type ArchiveT, class T>
void pack_unpack_end(ArchiveT *, T &, const pack_var_info &vinfo) {
    dlog("Pack binary archive %s end", vinfo.name);
}

template<binary_archive_type ArchiveT, arithmetic_type T>
void pack_unpack(ArchiveT *ar, T &val, const pack_var_info &vinfo)
{
    char logging_statement[]{"Packing %s %d bytes for %s with value xx"};
    const char *flag = get_flag_for_type(val);
    memcpy(strstr(logging_statement, "xx"), flag, 2);

    sizet sz = sizeof(T);
    if (ar->opmode == archive_opmode::UNPACK)
        memcpy(&val, ar->data + ar->cur_offset, sz);
    else
        memcpy(ar->data + ar->cur_offset, &val, sz);
    tlog(logging_statement, (ar->opmode == archive_opmode::PACK) ? "out" : "in", sz, vinfo.name, val);
    ar->cur_offset += sz;
}

// Special function for fixed size arrays of arithmetic type
template<binary_archive_type ArchiveT, arithmetic_type T, sizet N>
void pack_unpack(ArchiveT *ar, T (&val)[N], const pack_var_info &vinfo)
{
    sizet sz = sizeof(T);
    if (test_flags(vinfo.meta.flags, pack_va_flags::FIXED_ARRAY_CUSTOM_SIZE))
        sz *= *((u32 *)vinfo.meta.data);
    else
        sz *= N;

    if (ar->dir == archive_opmode::UNPACK)
        memcpy(val, ar->data + ar->cur_offset, sz);
    else
        memcpy(ar->data + ar->cur_offset, val, sz);

    tlog("Packing %s %d bytes for %s (array)", (ar->dir) ? "out" : "in", sz, vinfo.name);
    ar->cur_offset += sz;
}

// Special function for fixed size arrays of non arithmetic type (we call pack_)
template<binary_archive_type ArchiveT, class T, sizet N>
void pack_unpack(ArchiveT *ar, T (&val)[N], const pack_var_info &vinfo)
{
    sizet size = 0;
    if (test_flags(vinfo.meta.flags, pack_va_flags::FIXED_ARRAY_CUSTOM_SIZE))
        size = *((u32 *)vinfo.meta.data);
    else
        size = N;

    for (int i = 0; i < size; ++i) {
        pup_var(ar, val[i], vinfo);
    }
}

template<binary_archive_type T>
sizet packed_sizeof()
{
    constexpr sizet max_size = sizeof(T);
    static T inst{};
    static binary_fixed_buffer_archive<max_size> buf{};
    pack_unpack(buf, inst, {});
    sizet ret = buf.cur_offset;
    buf.cur_offset = 0;
    return ret;
}

} // namespace nslib
