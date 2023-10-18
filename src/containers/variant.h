#pragma once
#include "../basic_types.h"
#include "string.h"

namespace nslib
{
    
enum struct variant_type
{
    I8,
    UI8,
    I16,
    UI16,
    I32,
    UI32,
    I64,
    UI64,
    CHAR,
    UCHAR,
    SIZET,
    FLOAT,
    DOUBLE,
};

struct variant
{
    variant_type type;
};
} // namespace nslib
