#pragma once

#include "algorithm.h"

namespace nslib
{

template<class T>
struct vector4;

template<class T>
struct vector3;

template<class T>
struct rectangle
{
    rectangle(const vector4<T> & center_wh);
    rectangle(T x, T y, T width, T height);
    vector4<T> rect;
};

template<class T>
struct cube_base
{
    cube_base(const vector3<T> &min_ = vector3<T>(), const vector3<T> &max_ = vector3<T>());

    cube_base<T> operator+(const cube_base<T> &rhs);
    cube_base<T> operator-(const cube_base<T> &rhs);
    cube_base<T> &operator+=(const cube_base<T> &rhs);
    cube_base<T> &operator-=(const cube_base<T> &rhs);

    union
    {
        struct
        {
            vector3<T> min;
            vector3<T> max;
        };

        struct
        {
            vector3<T> a;
            vector3<T> b;
        };
    };
};

using i8cube = cube_base<i8>;
using i16cube = cube_base<i16>;
using icube = cube_base<i32>;
using i64cube = cube_base<i64>;
using ui8cube = cube_base<u8>;
using ui16cube = cube_base<u16>;
using uicube = cube_base<u32>;
using ui64cube = cube_base<u64>;
using cube = cube_base<float>;
using dcube = cube_base<double>;
using ldcube = cube_base<ldouble>;

}
