#pragma once
#include "../basic_types.h"
#include "hashmap.h"
#include "string.h"

namespace nslib
{

enum struct variant_type
{
    INVALID,
    BOOL,
    I8,
    U8,
    I16,
    U16,
    I32,
    U32,
    I64,
    U64,
    CHAR,
    UCHAR,
    SIZET,
    FLOAT,
    DOUBLE,
    STRING,
    STRING_LIST,
    VARIANT_HASHMAP,
    VARIANT_LIST
};

struct variant;
using variant_hashmap = hashmap<string, variant>;
using variant_array = array<variant>;

struct variant
{
    variant_type type{variant_type::INVALID};
    union {
        bool _b;
        i8 _i8;
        u8 _u8;
        i16 _i16;
        u16 _u16;
        i32 _i32;
        u32 _u32;
        i64 _i64;
        u64 _u64;
        char _ch;
        uchar _uch;
        sizet _szt;
        float _flt;
        double _dble;
        string _str;
        string_array _str_arr;
        variant_hashmap _var_hm;
        variant_array _var_arr;
    };
};
} // namespace nslib
