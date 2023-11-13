#include "string_archive.h"

namespace nslib
{
void handle_varname(string *txt, const char *vname)
{
    if (vname) {
        str_append(txt, vname);
        str_append(txt, ": ");
    }
}

// Strings
void pack_unpack_begin(string_archive *ar, string &, const pack_var_info &vinfo)
{
    ar->txt += ar->cur_indent;
    handle_varname(&ar->txt, vinfo.name);
}

void pack_unpack_end(string_archive *ar, string &, const pack_var_info &vinfo)
{
    ar->txt += ";\n";
}

void pack_unpack(string_archive *ar, string &val, const pack_var_info &vinfo)
{
    ar->txt += val;
}
} // namespace nslib
