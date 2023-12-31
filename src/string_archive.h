#pragma once

#include "archive_common.h"
#include "containers/string.h"
#include "containers/hashmap.h"
#include "containers/hashset.h"
#include "robj_common.h"

namespace nslib
{

struct string_archive
{
    const archive_opmode opmode = archive_opmode::PACK;
    
    string txt;
    string cur_indent;
    i8 indent_per_level{4};
};

void handle_varname(string *txt, const char *vname);

// All other types should appear as objects
template<class T>
void pack_unpack_begin(string_archive *ar, T &, const pack_var_info &vinfo) {
    ar->txt += ar->cur_indent;
    handle_varname(&ar->txt, vinfo.name);
    ar->txt += "{\n";
    str_resize(&ar->cur_indent, str_len(ar->cur_indent) + ar->indent_per_level, ' ');
}

template<class T>
void pack_unpack_end(string_archive *ar, T &, const pack_var_info &vinfo) {
    str_resize(&ar->cur_indent, str_len(ar->cur_indent) - ar->indent_per_level);
    ar->txt += ar->cur_indent + "}\n";
}

// Arithmetic types
template<arithmetic_type T>
void pack_unpack_begin(string_archive *ar, T &, const pack_var_info &vinfo) {
    ar->txt += ar->cur_indent;
    handle_varname(&ar->txt, vinfo.name);
}

template<arithmetic_type T>
void pack_unpack_end(string_archive *ar, T &, const pack_var_info &vinfo) {
    ar->txt += ";\n";
}

template<arithmetic_type T>
void pack_unpack(string_archive *ar, T &val, const pack_var_info &vinfo)
{
    ar->txt += to_str(val);
}

// Strings
void pack_unpack_begin(string_archive *ar, string &, const pack_var_info &vinfo);
void pack_unpack_end(string_archive *ar, string &, const pack_var_info &vinfo);
void pack_unpack(string_archive *ar, string &val, const pack_var_info &vinfo);

// Make rids not register as their own object (we only pup the str member)
inline void pack_unpack_begin(string_archive *ar, rid &id, const pack_var_info &vinfo) {}
inline void pack_unpack_end(string_archive *ar, rid &id, const pack_var_info &vinfo) {}

// Static arrays (actual ones)
template<class T, sizet N>
void pack_unpack_begin(string_archive *ar, T (&)[N], const pack_var_info &vinfo)
{
    ar->txt += ar->cur_indent;
    handle_varname(&ar->txt, vinfo.name);
    ar->txt += "[\n";
    str_resize(&ar->cur_indent, str_len(ar->cur_indent) + ar->indent_per_level, ' ');
}

template<class T, sizet N>
void pack_unpack_end(string_archive *ar, T (&)[N], const pack_var_info &vinfo)
{
    str_resize(&ar->cur_indent, str_len(ar->cur_indent) - ar->indent_per_level);
    ar->txt += ar->cur_indent + "]\n";
}

template<class T, sizet N>
void pack_unpack(string_archive *ar, T (&val)[N], const pack_var_info &vinfo)
{
    sizet size{};
    if (test_flags(vinfo.meta.flags, pack_va_flags::FIXED_ARRAY_CUSTOM_SIZE))
        size = *((sizet *)vinfo.meta.data);
    else
        size = N;
    for (sizet i = 0; i < size; ++i) {
        pup_var(ar, val[i], {});
    }
}

// Static arrays - reuse actual static array code above but passing in the size
template<class T, sizet N>
void pack_unpack(string_archive *ar, static_array<T, N> &val, const pack_var_info &vinfo)
{
    pup_var(ar, val.data, {"data", {pack_va_flags::FIXED_ARRAY_CUSTOM_SIZE, &val.size}});
}

// Dynamic Arrays
template<class T>
void pack_unpack_begin(string_archive *ar, array<T> &, const pack_var_info &vinfo)
{
    ar->txt += ar->cur_indent;
    handle_varname(&ar->txt, vinfo.name);
    ar->txt += "[\n";
    str_resize(&ar->cur_indent, str_len(ar->cur_indent) + ar->indent_per_level, ' ');
}

template<class T>
void pack_unpack_end(string_archive *ar, array<T> &, const pack_var_info &vinfo)
{
    str_resize(&ar->cur_indent, str_len(ar->cur_indent) - ar->indent_per_level);
    ar->txt += ar->cur_indent + "]\n";
}

template<class T>
void pack_unpack(string_archive *ar, array<T> &val, const pack_var_info &vinfo)
{
    for (int i = 0; i < val.size; ++i) {
        pup_var(ar, val[i], {});
    }
}

// Hashset
template<class T>
void pack_unpack_begin(string_archive *ar, hashset<T> &, const pack_var_info &vinfo)
{
    ar->txt += ar->cur_indent;
    handle_varname(&ar->txt, vinfo.name);
    ar->txt += "[\n";
    str_resize(&ar->cur_indent, str_len(ar->cur_indent) + ar->indent_per_level, ' ');
}

template<class T>
void pack_unpack_end(string_archive *ar, hashset<T> &, const pack_var_info &vinfo)
{
    str_resize(&ar->cur_indent, str_len(ar->cur_indent) - ar->indent_per_level);
    ar->txt += ar->cur_indent + "]\n";
}

template<class T>
void pack_unpack(string_archive *ar, hashset<T> &val, const pack_var_info &vinfo)
{
    sizet i{};
    while (auto iter = hashset_iter(&val, &i)) {
        pup_var(ar, *iter, {});
    }
}

// Hashmap with string key
template<class T>
void pack_unpack(string_archive *ar, hashmap<string, T> &val, const pack_var_info &vinfo)
{
    sizet i{};
    while (auto iter = hashmap_iter(&val, &i)) {
        pup_var(ar, iter->value, {str_cstr(iter->key)});
    }
}

// Hashmap with rid key
template<class T>
void pack_unpack(string_archive *ar, hashmap<rid, T> &val, const pack_var_info &vinfo)
{
    sizet i{};
    while (auto iter = hashmap_iter(&val, &i)) {
        pup_var(ar, iter->value, {str_cstr(iter->key.str)});
    }
}

// Hashmap with integral key type
template<integral K, class T>
void pack_unpack(string_archive *ar, hashmap<K, T> &val, const pack_var_info &vinfo)
{
    sizet i{};
    while (auto iter = hashmap_iter(&val, &i)) {
        pup_var(ar, iter->value, {to_cstr(iter->key)});
    }
}

template<class T>
string to_str(const T &item) {
    string_archive sa{};
    T & no_const = (T &)item;
    pup_var(&sa, no_const, {});
    return sa.txt;
}

} // namespace nslib
