#pragma once

#include "algorithm.h"

namespace noble_steed
{

template<class T>
struct vector2
{
    vector2(T init_ = 0) : data{init_, init_}
    {}

    vector2(T x_, T y_) : data{x_, y_}
    {}

    vector2(T data_[2]): data{data_[0], data_[1]}
    {}

    COMMON_OPERATORS(vector2, 2, T)

    union
    {
        T data[size_];

        struct
        {
            T x;
            T y;
        };

        struct
        {
            T w;
            T h;
        };

        struct
        {
            T s;
            T t;
        };

        struct
        {
            T u;
            T v;
        };
    };
};

// Enable type trait
template<class U>
struct is_vec<vector2<U>>
{
    static constexpr bool value = true;
};

namespace math
{
#if NOBLE_STEED_SIMD
template<>
inline float dot(const vector2<float> &lhs, const vector2<float> &rhs)
{
    __m128 l = _mm_set_ps(0.0f, 0.0f, lhs.y, lhs.x);
    __m128 r = _mm_set_ps(0.0f, 0.0f, rhs.y, rhs.x);
    return _mm_cvtss_f32(_sse_dp(l, r));
}

#endif

template<floating_pt T>
vector2<T> polar_to_cartesian(const vector2<T> &polar)
{
    return {polar.x * math::cos(polar.y), polar.x * math::sin(polar.y)};
}

template<integral T>
vector2<T> polar_to_cartesian(const vector2<T> &polar)
{
    return {polar.x * math::cos((float)polar.y), polar.x * math::sin((float)polar.y)};
}

template<class T>
vector2<T> cartesian_to_polar(const vector2<T> &cartesian)
{
    return {length(cartesian), angle(cartesian, vector2<T>(1, 0))};
}

} // namespace math

// Math typedefs
using i8vec2 = vector2<i8>;
using i16vec2 = vector2<i16>;
using ivec2 = vector2<i32>;
using i64vec2 = vector2<i64>;
using u8vec2 = vector2<u8>;
using u16vec2 = vector2<u16>;
using uvec2 = vector2<u32>;
using u64vec2 = vector2<u64>;
using vec2 = vector2<float>;
using dvec2 = vector2<double>;
using ldvec2 = vector2<ldouble>;
} // namespace noble_steed