#pragma once

#include "archive_common.h"
#include <type_traits>
#include "containers/string.h"

namespace nslib
{

struct string_archive
{
    const archive_opmode opmode = archive_opmode::UNPACK;
    
    string txt;
    string cur_indent;
    i8 indent_per_level{4};
};

inline void handle_varname(string *txt, const char *vname)
{
    if (vname) {
        str_append(txt, vname);
        str_append(txt, ": ");
    }
}

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
    ar->txt += makestr(val);
}

inline void pack_unpack_begin(string_archive *ar, string &, const pack_var_info &vinfo) {
    ar->txt += ar->cur_indent;
    handle_varname(&ar->txt, vinfo.name);
}

inline void pack_unpack_end(string_archive *ar, string &, const pack_var_info &vinfo) {
    ar->txt += ";\n";
}

inline void pack_unpack(string_archive *ar, string &val, const pack_var_info &vinfo)
{
    ar->txt += val;
}

//Special function for fixed size arrays of non arithmetic type (we call pack_unpack on each item)
template<class T, sizet N>
void pack_unpack_begin(string_archive *ar, T (&val)[N], const pack_var_info &vinfo)
{
    ar->txt += ar->cur_indent;
    handle_varname(&ar->txt, vinfo.name);
    ar->txt += "[\n";
    str_resize(&ar->cur_indent, str_len(ar->cur_indent) + ar->indent_per_level, ' ');
}

template<class T, sizet N>
void pack_unpack_end(string_archive *ar, T (&val)[N], const pack_var_info &vinfo)
{
    str_resize(&ar->cur_indent, str_len(ar->cur_indent) - ar->indent_per_level);
    ar->txt += ar->cur_indent + "]\n";
}

template<class T, sizet N>
void pack_unpack(string_archive *ar, T (&val)[N], const pack_var_info &vinfo)
{
    for (sizet i = 0; i < N; ++i) {
        pup_var(ar, val[i], {});
    }
}

template<class T>
string makestr(const T &item) {
    string_archive sa{};
    T & no_const = (T &)item;
    pup_var(&sa, no_const, {});
    return sa.txt;
}

} // namespace nslib
