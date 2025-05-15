#pragma once

#include "vector3.h"

namespace nslib
{

template<class T>
struct rectangle
{
    T x;
    T y;
    T w;
    T h;
};

pup_func_tt(rectangle)
{
    pup_member(x);
    pup_member(y);
    pup_member(w);
    pup_member(h);
}

template<class T>
struct cube_generic
{
    vector3<T> min;
    vector3<T> max;
};

pup_func_tt(cube_generic)
{
    pup_member(min);
    pup_member(max);
}

using s8rect = rectangle<s8>;
using s16rect = rectangle<s16>;
using srect = rectangle<s32>;
using s64rect = rectangle<s64>;
using u8rect = rectangle<u8>;
using u16rect = rectangle<u16>;
using urect = rectangle<u32>;
using u64rect = rectangle<u64>;
using rect = rectangle<f32>;
using f64rect = rectangle<f64>;

using s8cube = cube_generic<s8>;
using s16cube = cube_generic<s16>;
using scube = cube_generic<s32>;
using s64cube = cube_generic<s64>;
using u8cube = cube_generic<u8>;
using u16cube = cube_generic<u16>;
using ucube = cube_generic<u32>;
using u64cube = cube_generic<u64>;
using cube = cube_generic<f32>;
using f64cube = cube_generic<f64>;
} // namespace nslib
