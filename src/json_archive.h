#pragma once

#include "archive_common.h"
#include <type_traits>
#include "cJSON.h"
#include "containers/string.h"

namespace nslib
{

struct json_archive
{
    pack_dir dir{};
    cJSON* root{};
};

inline void jsa_init(json_archive *jsa)
{
    jsa->root = cJSON_CreateObject();
}

inline void jsa_terminate(json_archive *jsa)
{
    cJSON_Delete(jsa->root);
}

template<class T>
void pack_unpack_begin(json_archive *ar, T &, const pack_var_info &vinfo) {
}

template<class T>
void pack_unpack_end(json_archive *ar, T &, const pack_var_info &vinfo) {
}

template<arithmetic_type T>
void pack_unpack_begin(json_archive *ar, T &, const pack_var_info &vinfo) {
}

template<arithmetic_type T>
void pack_unpack_end(json_archive *ar, T &, const pack_var_info &vinfo) {
}

template<arithmetic_type T>
void pack_unpack(json_archive *ar, T &val, const pack_var_info &vinfo)
{
}

inline void pack_unpack_begin(json_archive *ar, string &, const pack_var_info &vinfo) {
}

inline void pack_unpack_end(json_archive *ar, string &, const pack_var_info &vinfo) {
}

inline void pack_unpack(json_archive *ar, string &val, const pack_var_info &vinfo)
{
}

template<class T, sizet N>
void pack_unpack_begin(json_archive *ar, T (&val)[N], const pack_var_info &vinfo)
{
}

template<class T, sizet N>
void pack_unpack_end(json_archive *ar, T (&val)[N], const pack_var_info &vinfo)
{
}

template<class T, sizet N>
void pack_unpack(json_archive *ar, T (&val)[N], const pack_var_info &vinfo)
{
}

} // namespace nslib
