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

string to_string(const rid &rid)
{
    string ret;
    ret += "\nrid {\nid:" + to_string(rid.id) + "\nstr:" + rid.str + "\n}";
    return ret;
}


} // namespace nslib
