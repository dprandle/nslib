#pragma once

#include "vector4.h"

namespace nslib
{

template<floating_pt T>
struct quaternion
{
    quaternion() : x(0), y(0), z(0), w(1)
    {}

    quaternion(T x_, T y_, T z_, T w_) : x(x_), y(y_), z(z_), w(w_)
    {}

    quaternion(T data_[4]) : data{data_[0], data_[1], data_[2], data_[3]}
    {}


    COMMON_OPERATORS(quaternion, 4, T)

#if NOBLE_STEED_SIMD
    using _simd_type = typename simd_traits<T, size_>::_simd_type;
#endif

    union
    {
        T data[size_];

        struct
        {
            T x;
            T y;
            T z;
            T w;
        };

        struct
        {
            T b;
            T c;
            T d;
            T a;
        };

        struct
        {
            T i;
            T j;
            T k;
            T alpha;
        };

#if NOBLE_STEED_SIMD
        _simd_type _v4;
#endif
    };
};

// Enable type trait
template<class U>
struct is_quat<quaternion<U>>
{
    static constexpr bool value = true;
};

namespace math
{

#if NOBLE_STEED_SIMD

template<>
inline float dot(const quaternion<float> &lhs, const quaternion<float> &rhs)
{
    return _mm_cvtss_f32(_sse_dp(lhs._v4, rhs._v4));
}

#endif

template<class T>
quaternion<T> slerp(quaternion<T> first, const quaternion<T> &second, const T &scaling_factor)
{
    if (fequals(scaling_factor, 0))
        return first;

    if (fequals(scaling_factor, 1))
    {
        first = second;
        return first;
    }

    T cos_half_theta = dot(first, second);

    if (cos_half_theta < 0)
    {
        second *= -1;
        cos_half_theta = -cos_half_theta;
    }

    if (cos_half_theta >= 1.0)
        return first;

    T half_theta = math::acos(cos_half_theta);
    T sin_half_theta = math::sqrt(1.0 - cos_half_theta * cos_half_theta);

    if (math::abs(sin_half_theta) < math::FLOAT_EPS)
    {
        first = 0.5 * (first + second);
        return first;
    }

    T ratio_a = math::sin((1 - scaling_factor) * half_theta) / sin_half_theta;
    T ratio_b = math::sin(scaling_factor * half_theta) / sin_half_theta;

    first = first * ratio_a + second * ratio_b;
    normalize(&first);
    return first;
}

template<class T>
void inverse(quaternion<T> *q)
{
    conjugate(q);
    normalize(q);
}

template<class T>
quaternion<T> inverse(quaternion<T> qt)
{
    inverse(&qt);
    return qt;
}

template<class T>
void conjugate(quaternion<T> *q)
{
    q->x *= -1;
    q->y *= -1;
    q->z *= -1;
}

template<class T>
quaternion<T> conjugate(quaternion<T> qt)
{
    conjugate(&qt);
    return qt;
}

template<class T>
vector3<T> right(const quaternion<T> &q)
{
    return {1 - 2 * (q.y * q.y + q.z * q.z), 2 * (q.x * q.y + q.z * q.w), 2 * (q.x * q.z - q.y * q.w)};
}

template<class T>
vector3<T> target(const quaternion<T> &q)
{
    return {2 * (q.x * q.z + q.y * q.w), 2 * (q.y * q.z - q.x * q.w), 1 - 2 * (q.x * q.x + q.y * q.y)};
}

template<class T>
vector3<T> up(const quaternion<T> &q)
{
    return {2 * (q.x * q.y - q.z * q.w), 1 - 2 * (q.x * q.x + q.z * q.z), 2 * (q.y * q.z + q.x * q.w)};
}

template<floating_pt T>
vector4<T> axis_angle(const quaternion<T> &orientation)
{
    // http://www.euclideanspace.com/maths/geometry/rotations/conversions/quaternionToAngle/index.htm
    vector4<T> ret;
    ret.w = 2 * math::acos(orientation.w);
    T den = math::sqrt(1 - orientation.w * orientation.w);
    if (den < math::FLOAT_EPS)
    {
        ret.x = orientation.x;
        ret.y = orientation.y;
        ret.z = orientation.z;
    }
    else
    {
        ret.x = orientation.x / den;
        ret.y = orientation.y / den;
        ret.z = orientation.z / den;
    }
    return ret;
}

template<floating_pt T>
quaternion<T> orientation(const vector4<T> &axis_angle)
{
    quaternion<T> ret;
    T ang = axis_angle.w;
    ang /= 2;
    T s_ang = math::sin(ang);
    ret.x = axis_angle.x * s_ang;
    ret.y = axis_angle.y * s_ang;
    ret.z = axis_angle.z * s_ang;
    ret.w = math::cos(ang);
    return ret;
}

template<floating_pt T>
quaternion<T> orientation(const vector3<T> &euler, typename vector3<T>::euler_order order_)
{
    quaternion<T> ret;
    T c1, c2, c3, s1, s2, s3;
    c1 = math::cos(euler.x / 2);
    c2 = math::cos(euler.y / 2);
    c3 = math::cos(euler.z / 2);
    s1 = math::sin(euler.x / 2);
    s2 = math::sin(euler.y / 2);
    s3 = math::sin(euler.z / 2);

    switch (order_)
    {
    case (vector3<T>::XYZ): {
        ret.x = s1 * c2 * c3 + c1 * s2 * s3;
        ret.y = c1 * s2 * c3 - s1 * c2 * s3;
        ret.z = c1 * c2 * s3 + s1 * s2 * c3;
        ret.w = c1 * c2 * c3 - s1 * s2 * s3;
        break;
    }
    case (vector3<T>::XZY): {
        ret.x = s1 * c2 * c3 - c1 * s2 * s3;
        ret.y = c1 * s2 * c3 - s1 * c2 * s3;
        ret.z = c1 * c2 * s3 + s1 * s2 * c3;
        ret.w = c1 * c2 * c3 + s1 * s2 * s3;
        break;
    }
    case (vector3<T>::YXZ): {
        ret.x = s1 * c2 * c3 + c1 * s2 * s3;
        ret.y = c1 * s2 * c3 - s1 * c2 * s3;
        ret.z = c1 * c2 * s3 - s1 * s2 * c3;
        ret.w = c1 * c2 * c3 + s1 * s2 * s3;
        break;
    }
    case (vector3<T>::YZX): {
        ret.x = s1 * c2 * c3 + c1 * s2 * s3;
        ret.y = c1 * s2 * c3 + s1 * c2 * s3;
        ret.z = c1 * c2 * s3 - s1 * s2 * c3;
        ret.w = c1 * c2 * c3 - s1 * s2 * s3;
        break;
    }
    case (vector3<T>::ZXY): {
        ret.x = s1 * c2 * c3 - c1 * s2 * s3;
        ret.y = c1 * s2 * c3 + s1 * c2 * s3;
        ret.z = c1 * c2 * s3 + s1 * s2 * c3;
        ret.w = c1 * c2 * c3 - s1 * s2 * s3;
        break;
    }
    case (vector3<T>::ZYX): {
        ret.x = s1 * c2 * c3 - c1 * s2 * s3;
        ret.y = c1 * s2 * c3 + s1 * c2 * s3;
        ret.z = c1 * c2 * s3 - s1 * s2 * c3;
        ret.w = c1 * c2 * c3 + s1 * s2 * s3;
        break;
    }
    }
    return ret;
}

template<floating_pt T>
quaternion<T> orientation(const vector3<T> &to_vec, const vector3<T> &from_vec = {})
{
    /* http://www.euclideanspace.com/maths/algebra/vectors/angleBetween/index.htm */
    quaternion<T> ret;
    T real = 1 + dot(from_vec, to_vec);
    vector3<T> imag = cross(from_vec, to_vec);
    if (real < FLOAT_EPS)
    {
        ret.w = 0;
        ret.x = -from_vec.z;
        ret.y = from_vec.y;
        ret.z = from_vec.x;
        normalize(&ret);
        return ret;
    }
    ret.w = real;
    ret.x = imag.x;
    ret.y = imag.y;
    ret.z = imag.z;
    normalize(&ret);
    return ret;
}
} // namespace math

#if NOBLE_STEED_SIMD

inline quaternion<float> operator+(quaternion<float> lhs, const quaternion<float> &rhs)
{
    lhs._v4 = _mm_add_ps(lhs._v4, rhs._v4);
    return lhs;
}

inline quaternion<float> operator-(quaternion<float> lhs, const quaternion<float> &rhs)
{
    lhs._v4 = _mm_sub_ps(lhs._v4, rhs._v4);
    return lhs;
}

template<arithmetic_type T>
inline quaternion<float> operator*(quaternion<float> lhs, T rhs)
{
    __m128 s = _mm_set1_ps(rhs);
    lhs._v4 = _mm_mul_ps(lhs._v4, s);
    return lhs;
}

template<arithmetic_type T>
inline quaternion<float> operator/(quaternion<float> lhs, T rhs)
{
    __m128 s = _mm_set1_ps(1.0 / rhs);
    lhs._v4 = _mm_mul_ps(lhs._v4, s);
    return lhs;
}

#endif

/// SIMD Quaternion multiply is much slower than just regular multiply - tested it and its about 3x slower on my maching
template<class T>
quaternion<T> operator*(const quaternion<T> &lhs, const quaternion<T> &rhs)
{
    quaternion<T> ret{lhs.w * rhs.x + lhs.x * rhs.w + lhs.y * rhs.z - lhs.z * rhs.y,
                      lhs.w * rhs.y - lhs.x * rhs.z + lhs.y * rhs.w + lhs.z * rhs.x,
                      lhs.w * rhs.z + lhs.x * rhs.y - lhs.y * rhs.x + lhs.z * rhs.w,
                      lhs.w * rhs.w - lhs.x * rhs.x - lhs.y * rhs.y - lhs.z * rhs.z};
    return ret;
}

template<class T>
vector3<T> operator*(const quaternion<T> &lhs, const vector3<T> &rhs)
{
    vector3<T> ret;
    // quat_ * vec3
    T ix = lhs.w * rhs.x + lhs.y * rhs.z - lhs.z * rhs.y;
    T iy = lhs.w * rhs.y + lhs.z * rhs.x - lhs.x * rhs.z;
    T iz = lhs.w * rhs.z + lhs.x * rhs.y - lhs.y * rhs.x;
    T iw = -lhs.x * rhs.x - lhs.y * rhs.y - lhs.z * rhs.z;
    // vec3 * inverse(quat_)
    ret.x = ix * lhs.w + iw * -lhs.x + iy * -lhs.z - iz * -lhs.y;
    ret.y = iy * lhs.w + iw * -lhs.y + iz * -lhs.x - ix * -lhs.z;
    ret.z = iz * lhs.w + iw * -lhs.z + ix * -lhs.y - iy * -lhs.x;
    return ret;
}

template<class T>
quaternion<T> operator/(const quaternion<T> &lhs, const quaternion<T> &rhs)
{
    return lhs * math::inverse(rhs);
}

using quat = quaternion<float>;
using dquat = quaternion<double>;
using ldquat = quaternion<ldouble>;
} // namespace nslib
