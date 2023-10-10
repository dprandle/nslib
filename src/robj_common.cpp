#include <cstring>

#include "robj_common.h"
#include "hashfuncs.h"

namespace nslib {

    rid gen_id(const char *str)
{
    return {str, hash_type(str, 0, 0)};
}

}
