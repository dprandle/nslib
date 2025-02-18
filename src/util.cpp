#include <cstdlib>
#include "util.h"

namespace nslib
{
u64 generate_rand_seed()
{
    return rand();
}
} // namespace nslib
