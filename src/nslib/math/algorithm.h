#pragma once

#include "../basic_types.h"
#include <cmath>
//#include <iomanip>
#include <type_traits>
#include <algorithm>

#define NOBLE_STEED_SSE_BIT (0x00000001)
#define NOBLE_STEED_SSE2_BIT (0x00000002)
#define NOBLE_STEED_SSE3_BIT (0x00000004)
#define NOBLE_STEED_SSSE3_BIT (0x0000008)
#define NOBLE_STEED_SSE41_BIT (0x00000010)
#define NOBLE_STEED_SSE42_BIT (0x00000020)
#define NOBLE_STEED_AVX_BIT (0x00000040)
#define NOBLE_STEED_AVX2_BIT (0x00000080)
#define NOBLE_STEED_SSE_SQRT_BIT (0x00000100)

#define NOBLE_STEED_USE_SSE (NOBLE_STEED_SSE_BIT)
#define NOBLE_STEED_USE_SSE2 (NOBLE_STEED_SSE2_BIT | NOBLE_STEED_USE_SSE)
#define NOBLE_STEED_USE_SSE3 (NOBLE_STEED_SSE3_BIT | NOBLE_STEED_USE_SSE2)
#define NOBLE_STEED_USE_SSSE3 (NOBLE_STEED_SSSE3_BIT | NOBLE_STEED_USE_SSE3)
#define NOBLE_STEED_USE_SSE41 (NOBLE_STEED_SSE41_BIT | NOBLE_STEED_USE_SSSE3)
#define NOBLE_STEED_USE_SSE42 (NOBLE_STEED_SSE42_BIT | NOBLE_STEED_USE_SSE41)
#define NOBLE_STEED_USE_AVX (NOBLE_STEED_AVX_BIT | NOBLE_STEED_SSE42)
#define NOBLE_STEED_USE_AVX2 (NOBLE_STEED_AVX2_BIT | NOBLE_STEED_AVX)

#define NOBLE_STEED_SIMD (NOBLE_STEED_USE_SSE41) // | NOBLE_STEED_SSE_SQRT_BIT)

#if NOBLE_STEED_SIMD
#include <immintrin.h>
namespace nslib
{
template<typename T, i8 SZ>
struct simd_traits
{
    using _simd_type = T[SZ];
};

template<>
struct simd_traits<float, 4>
{
    using _simd_type = __m128;
    // etc
};

inline __m128 _sse_dp(const __m128 &left, const __m128 &right)
{
#if NOBLE_STEED_SIMD & NOBLE_STEED_SSE41_BIT
    return _mm_dp_ps(left, right, 0xff);
#elif NOBLE_STEED_SIMD & NOBLE_STEED_SSE3_BIT
    __m128 mul0 = _mm_mul_ps(left, right);
    __m128 hadd0 = _mm_hadd_ps(mul0, mul0);
    __m128 hadd1 = _mm_hadd_ps(hadd0, hadd0);
    return hadd1;
#else
    __m128 mul0 = _mm_mul_ps(left, right);
    __m128 swp0 = _mm_shuffle_ps(mul0, mul0, _MM_SHUFFLE(2, 3, 0, 1));
    __m128 add0 = _mm_add_ps(mul0, swp0);
    mul0 = _mm_shuffle_ps(add0, add0, _MM_SHUFFLE(0, 1, 2, 3));
    add0 = _mm_add_ps(add0, mul0);
    return add0;
#endif
}
} // namespace nslib
#endif

namespace nslib
{

template<class T>
struct is_vec
{
    static constexpr bool value = false;
};

template<class T>
struct is_quat
{
    static constexpr bool value = false;
};

template<class T>
struct is_mat
{
    static constexpr bool value = false;
};

template<class T>
concept vec_type = is_vec<T>::value;

template<class T>
concept mat_type = is_mat<T>::value;

template<class T>
concept quat_type = is_quat<T>::value;

template<class T>
concept vec_or_quat_type = vec_type<T> || quat_type<T>;

template<class T>
concept floating_pt = std::is_floating_point_v<T>;

template<class T>
concept integral = std::is_integral_v<T>;

template<class T>
concept signed_number = std::is_signed_v<T>;

template<class T>
concept signed_integral = integral<T> && signed_number<T>;

template<class T>
concept unsigned_integral = integral<T> && std::is_unsigned_v<T>;

template<class T>
concept basic_number = integral<T> || floating_pt<T>;

template<class T>
concept holds_floating_pt = floating_pt<typename T::value_type>;

template<class T>
concept holds_integral = integral<typename T::value_type>;

template<class T>
concept holds_signed_number = signed_number<typename T::value_type>;

template<class T>
concept holds_basic_number = basic_number<typename T::value_type>;

template<class T>
struct vector2;

template<class T>
struct vector3;

template<class T>
struct bounding_box;

template<class T>
struct cube_base;

namespace math
{
inline constexpr float PI = 3.14159265359f;
inline constexpr float FLOAT_EPS = 0.001f;
inline constexpr char PRINT_MAT_DELIMITER = '\n';
inline constexpr char PRINT_MAT_START = '\n';
inline constexpr char PRINT_MAT_END = '\n';
inline constexpr char PRINT_VEC_DELIMITER = ' ';
inline constexpr char PRINT_START_VEC = '[';
inline constexpr char PRINT_END_VEC = ']';
inline constexpr i8 ROUND_TO_DEC = 4;

inline constexpr float TO_DEGREES = (180.0f / PI);
inline constexpr float TO_RADS = (PI / 180.0f);

i8 count_digits(i32 number);

template<floating_pt T>
T sin(T val)
{
    return std::sin(val);
}

template<floating_pt T>
T cos(T val)
{
    return std::cos(val);
}

template<floating_pt T>
T tan(T val)
{
    return std::tan(val);
}

template<floating_pt T>
T asin(T val)
{
    return std::asin(val);
}

template<floating_pt T>
T acos(T val)
{
    return std::acos(val);
}

template<floating_pt T>
T atan(T val)
{
    return std::atan2(val);
}

template<floating_pt T>
inline T sqrt(T val)
{
    return std::sqrt(val);
}

template<floating_pt T>
inline T rsqrt(T val)
{
    return T(1) / std::sqrt(val);
}

#if NOBLE_STEED_SIMD & NOBLE_STEED_USE_SSE
#if NOBLE_STEED_SIMD & NOBLE_STEED_SSE_SQRT_BIT
inline float sqrt(float val)
{
    return _mm_cvtss_f32(_mm_sqrt_ss(_mm_set_ss(val)));
}
#endif
inline float rsqrt(float val)
{
    return _mm_cvtss_f32(_mm_rsqrt_ss(_mm_set_ss(val)));
}
#endif

template<floating_pt T>
bool fequals(T left, T right, T eps = FLOAT_EPS)
{
    return (left < (right + FLOAT_EPS)) && (left > (right - FLOAT_EPS));
}

float random_float(float high_ = 1.0f, float low_ = 0.0f);

double round_decimal(double to_round, i8 decimal_places);

template<class T>
auto sum_elements(const T &veca)
{
    auto ret = decltype(veca.data[0]){0};
    for (auto &&item : veca)
        ret += item;
    return ret;
}

template<class U, class T>
auto convert_elements(const T &veca)
{
    using other_type = typename T::template container_type<U>;
    other_type ret;
    for (u8 i{0}; i < T::size_; ++i)
        ret[i] = (U)veca[i];
    return ret;
}

template<class T>
inline typename T::value_type dot(const T &veca, const T &vecb)
{
    auto ret = decltype(veca.data[0]){0};
    for (i8 i{0}; i < veca.size(); ++i)
        ret += veca[i] * vecb[i];
    return ret;
}

template<class T>
inline auto length_sq(const T &veca)
{
    return dot(veca, veca);
}

template<holds_floating_pt T>
inline auto length(const T &veca)
{
    return math::sqrt(dot(veca, veca));
}

template<holds_integral T>
inline auto length(const T &veca)
{
    return math::sqrt((float)length_sq(veca));
}

template<vec_or_quat_type T>
inline void set_length(T *vec, const typename T::value_type &new_len)
{
    auto len = math::rsqrt(dot(*vec, *vec));
    *vec *= (new_len * len);
}

template<vec_or_quat_type T>
inline T set_length(T vec, const typename T::value_type &new_len)
{
    set_length(&vec, new_len);
    return vec;
}

template<vec_type T>
auto angle(const T &veca, const T &vecb)
{
    auto dot_p = dot(veca, vecb);
    auto l = math::sqrt(length_sq(veca) * length_sq(vecb));
    if (l < FLOAT_EPS)
        return l;
    return math::acos(dot_p / l);
}

template<vec_type T>
requires holds_integral<T>
auto angle(const T &veca, const T &vecb)
{
    auto dot_p = dot(veca, vecb);
    float l = math::sqrt((float)(length_sq(veca) * length_sq(vecb)));
    if (l < FLOAT_EPS)
        return l;
    return math::acos(dot_p / l);
}

template<vec_type T>
requires holds_floating_pt<T>
void project(T *a, const T &b)
{
    using Type = typename T::value_type;
    Type denom = math::dot(b, b);
    if (fequals(denom, (Type)0.0, (Type)FLOAT_EPS))
        return;
    *a = (math::dot(*a, b) / denom) * b;
}

template<vec_type T>
void project(T *a, const T &b)
{
    using Type = typename T::value_type;
    Type denom = math::dot(b, b);
    if (denom == 0)
        return;
    *a = (math::dot(*a, b) / (float)denom) * b;
}

template<vec_type T>
T project(T a, const T &b)
{
    project(&a, b);
    return a;
}

template<vec_type T>
void project_plane(T *vec, const T &normal)
{
    *vec -= project(*vec, normal);
}

template<vec_type T>
T project_plane(T vec, const T &normal)
{
    project_plane(&vec, normal);
    return vec;
}

template<vec_type T>
void reflect(T *vec, const T &normal)
{
    using Type = typename T::value_type;
    *vec -= (Type)2 * math::dot(*vec, normal) * normal;
}

template<vec_type T>
T reflect(T vec, const T &normal)
{
    reflect(&vec, normal);
    return vec;
}

template<vec_or_quat_type T>
void normalize(T *vec_or_quat)
{
    auto len = math::rsqrt(dot(*vec_or_quat, *vec_or_quat));
    *vec_or_quat *= len;
}

template<vec_or_quat_type T>
T normalize(T vec_or_quat)
{
    normalize(&vec_or_quat);
    return vec_or_quat;
}

template<class T>
auto min_element(const T &cont)
{
    return *std::min_element(cont.begin(), cont.end());
}

template<class T>
auto max_element(const T &cont)
{
    return *std::max_element(cont.begin(), cont.end());
}

template<class T>
T minimums(T lhs_, const T &rhs_)
{
    for (i8 i{0}; i < lhs_.size(); ++i)
    {
        if (rhs_[i] < lhs_[i])
            lhs_[i] = rhs_[i];
    }
    return lhs_;
}

template<class T>
T maximums(T lhs_, const T &rhs_)
{
    for (i8 i{0}; i < lhs_.size(); ++i)
    {
        if (rhs_[i] > lhs_[i])
            lhs_[i] = rhs_[i];
    }
    return lhs_;
}

template<signed_number T>
T abs(T item)
{
    return std::abs(item);
}

template<signed_number T>
void abs(T *item)
{
    *item = std::abs(*item);
}

template<holds_signed_number T>
void abs(T *item)
{
    for (auto &&element : *item)
        abs(&element);
}

template<holds_signed_number T>
T abs(T item)
{
    abs(&item);
    return item;
}

template<floating_pt T>
T ceil(T item)
{
    return std::ceil(item);
}

template<floating_pt T>
void ceil(T *item)
{
    *item = std::ceil(*item);
}

// This changes in place
template<holds_floating_pt T>
void ceil(T *item)
{
    for (auto &&element : *item)
        ceil(&element);
}

// This just returns a copy instead of changing in place - this will work
// for any container type passed in that has a value_type that is signed
template<holds_floating_pt T>

T ceil(T item)
{
    ceil(&item);
    return item;
}

template<floating_pt T>
T floor(T item)
{
    return std::floor(item);
}

template<floating_pt T>
void floor(T *item)
{
    *item = std::floor(*item);
}

// This changes in place
template<holds_floating_pt T>
void floor(T *item)
{
    for (auto &&element : *item)
        floor(&element);
}

// This just returns a copy instead of changing in place - this will work
// for any container type passed in that has a value_type that is signed
template<holds_floating_pt T>

T floor(T item)
{
    floor(&item);
    return item;
}

template<floating_pt T>
T round(T item)
{
    return std::round(item);
}

template<floating_pt T>
void round(T *item)
{
    *item = std::round(*item);
}

// This changes in place
template<holds_floating_pt T>
void round(T *item)
{
    for (auto &&element : *item)
        round(&element);
}

// This just returns a copy instead of changing in place - this will work
// for any container type passed in that has a value_type that is signed
template<holds_floating_pt T>
T round(T item)
{
    round(&item);
    return item;
}

template<floating_pt T>
T round(T item, i8 decimal_places)
{
    return round_decimal(item, decimal_places);
}

template<floating_pt T>
void round(T *item, i8 decimal_places)
{
    *item = round_decimal(*item, decimal_places);
}

// This changes in place
template<holds_floating_pt T>
void round(T *item, i8 decimal_places)
{
    for (auto &&element : *item)
        round(&element, decimal_places);
}

// This just returns a copy instead of changing in place - this will work
// for any container type passed in that has a value_type that is signed
template<holds_floating_pt T>
T round(T item, i8 decimal_places)
{
    round(&item, decimal_places);
    return item;
}

template<mat_type T, vec_type V>
void set_mat_column(T *mat, sizet ind, const V &col)
{
    static_assert(T::size_ == V::size_);
    for (i8 i{0}; i < T::size_; ++i)
        (*mat)[i][ind] = col[i];
}

template<mat_type T>
void compwise_mult(T *lhs, const T &rhs)
{
    for (i8 i{0}; i < T::size_; ++i)
        (*lhs)[i] *= rhs[i];
}

template<mat_type T>
T compwise_mult(T lhs, const T &rhs)
{
    compwise_mult(&lhs, rhs);
    return lhs;
}

template<mat_type T>
void compwise_div(T *lhs, const T &rhs)
{
    for (i8 i{0}; i < T::size_; ++i)
        (*lhs)[i] /= rhs[i];
}

template<mat_type T>
T compwise_div(T lhs, const T &rhs)
{
    compwise_div(&lhs, rhs);
    return lhs;
}

template<mat_type T, vec_type V>
void compwise_mult_rows(T *lhs, const V &row_vec)
{
    static_assert(T::size_ == V::size_);
    for (i8 i{0}; i < T::size_; ++i)
        (*lhs)[i] *= row_vec;
}

template<mat_type T, vec_type V>
T compwise_mult_rows(T lhs, const V &row_vec)
{
    compwise_mult_rows(&lhs, row_vec);
    return lhs;
}

template<vec_type V, mat_type T>
T compwise_mult_rows(const V &row_vec, const T &rhs)
{
    return compwise_mult_rows(rhs, row_vec);
}

template<vec_type V, mat_type T>
void compwise_mult_rows(const V &row_vec, T *rhs)
{
    compwise_mult_rows(rhs, row_vec);
}

template<mat_type T, vec_type V>
void compwise_div_rows(T *lhs, const V &row_vec)
{
    static_assert(T::size_ == V::size_);
    for (i8 i{0}; i < T::size_; ++i)
        (*lhs)[i] /= row_vec;
}

template<vec_type V, mat_type T>
void compwise_div_rows(const V &row_vec, T *rhs)
{
    static_assert(T::size_ == V::size_);
    for (i8 i{0}; i < T::size_; ++i)
        (*rhs)[i] = row_vec / (*rhs)[i];
}

template<mat_type T, vec_type V>
T compwise_div_rows(T lhs, const V &row_vec)
{
    compwise_div_rows(&lhs, row_vec);
    return lhs;
}

template<vec_type V, mat_type T>
T compwise_div_rows(const V &row_vec, T rhs)
{
    compwise_div_rows(row_vec, &rhs);
    return rhs;
}

template<mat_type T, vec_type V>
void compwise_mult_columns(T *lhs, const V &column_vec)
{
    static_assert(T::size_ == V::size_);
    for (i8 rowi{0}; rowi < T::size_; ++rowi)
    {
        for (i8 coli{0}; coli < T::size_; ++coli)
            (*lhs)[rowi][coli] *= column_vec[rowi];
    }
}

template<mat_type T, vec_type V>
T compwise_mult_columns(T lhs, const V &column_vec)
{
    compwise_mult_columns(&lhs, column_vec);
    return lhs;
}

template<vec_type V, mat_type T>
T compwise_mult_columns(const V &column_vec, const T &rhs)
{
    return compwise_mult_columns(column_vec, &rhs);
}

template<mat_type T, vec_type V>
void compwise_div_columns(T *lhs, const V &column_vec)
{
    static_assert(T::size_ == V::size_);
    for (i8 rowi{0}; rowi < T::size_; ++rowi)
    {
        for (i8 coli{0}; coli < T::size_; ++coli)
            (*lhs)[rowi][coli] /= column_vec[rowi];
    }
}

template<vec_type V, mat_type T>
void compwise_div_columns(const V &column_vec, T *rhs)
{
    static_assert(T::size_ == V::size_);
    for (i8 rowi{0}; rowi < T::size_; ++rowi)
    {
        for (i8 coli{0}; coli < T::size_; ++coli)
            (*rhs)[rowi][coli] = column_vec[rowi] / (*rhs)[rowi][coli];
    }
}

template<mat_type T, vec_type V>
T compwise_div_columns(T lhs, const V &column_vec)
{
    compwise_div_columns(&lhs, column_vec);
    return lhs;
}

template<vec_type V, mat_type T>
T compwise_div_columns(const V &column_vec, T rhs)
{
    compwise_div_columns(column_vec, &rhs);
    return rhs;
}
} // namespace math

template<class T>
concept holds_basic_comparable_type = (vec_type<T> || mat_type<T> || quat_type<T>);

template<class T>
concept holds_basic_arithmetic_type = (vec_type<T> || mat_type<T> || quat_type<T>);

template<holds_basic_comparable_type T>
requires holds_floating_pt<T>
bool operator==(const T &lhs_, const T &rhs_)
{
    for (sizet i{0}; i < lhs_.size(); ++i)
    {
        if (!math::fequals(lhs_[i], rhs_[i]))
            return false;
    }
    return true;
}

template<holds_basic_comparable_type T>
requires holds_integral<T>
bool operator==(const T &lhs_, const T &rhs_)
{
    for (sizet i{0}; i < lhs_.size(); ++i)
    {
        if (!(lhs_[i] == rhs_[i]))
            return false;
    }
    return true;
}

template<holds_basic_comparable_type T>
bool operator!=(const T &lhs_, const T &rhs_)
{
    return !(lhs_ == rhs_);
}

template<holds_basic_comparable_type T>
bool operator<(const T &lhs_, const T &rhs_)
{
    for (sizet i{0}; i < lhs_.size(); ++i)
    {
        if (lhs_[i] >= rhs_[i])
            return false;
    }
    return true;
}

template<holds_basic_comparable_type T>
bool operator>(const T &lhs_, const T &rhs_)
{
    for (sizet i{0}; i < lhs_.size(); ++i)
    {
        if (lhs_[i] <= rhs_[i])
            return false;
    }
    return true;
}

template<holds_basic_comparable_type T>
bool operator<=(const T &lhs_, const T &rhs_)
{
    return (lhs_ < rhs_) || (lhs_ == rhs_);
}

template<holds_basic_comparable_type T>
bool operator>=(const T &lhs_, const T &rhs_)
{
    return (lhs_ > rhs_) || (lhs_ == rhs_);
}

template<holds_basic_arithmetic_type T>
T operator+(T lhs_, const T &rhs_)
{
    for (sizet i{0}; i < lhs_.size(); ++i)
        lhs_[i] += rhs_[i];
    return lhs_;
}

template<holds_basic_arithmetic_type T>
T operator-(T lhs_, const T &rhs_)
{
    for (sizet i{0}; i < lhs_.size(); ++i)
        lhs_[i] -= rhs_[i];
    return lhs_;
}

template<holds_basic_arithmetic_type T>
T operator*(T lhs_, const T &rhs_)
{
    for (sizet i{0}; i < lhs_.size(); ++i)
        lhs_[i] *= rhs_[i];
    return lhs_;
}

template<basic_number T, holds_basic_arithmetic_type U>
U operator*(U lhs_, T rhs_)
{
    for (auto &&element : lhs_)
        element *= rhs_;
    return lhs_;
}

template<basic_number T, holds_basic_arithmetic_type U>
U operator*(T lhs_, const U &rhs_)
{
    return rhs_ * lhs_;
}

template<holds_basic_arithmetic_type T>
T operator/(T lhs_, const T &rhs_)
{
    for (sizet i{0}; i < lhs_.size(); ++i)
        lhs_[i] /= rhs_[i];
    return lhs_;
}

template<basic_number T, holds_basic_arithmetic_type U>
U operator/(const U lhs_, T rhs_)
{
    T tmp((T)1.0 / rhs_);
    return lhs_ * tmp;
}

template<integral T, holds_basic_arithmetic_type U>
U operator/(U lhs_, T rhs_)
{
    for (auto &&element : lhs_)
        element /= rhs_;
    return lhs_;
}

template<floating_pt T, holds_basic_arithmetic_type U>
U operator/(T lhs_, U rhs_)
{
    for (auto &&element : rhs_)
        element = lhs_ / element;
    return rhs_;
}

#define COMMON_OPERATORS(type, element_count, ind_ret_type)                                                                                          \
    template<class U>                                                                                                                                \
    using container_type = vector2<U>;                                                                                                               \
    using value_type = T;                                                                                                                            \
    using iterator = ind_ret_type *;                                                                                                                 \
    using const_iterator = const ind_ret_type *;                                                                                                     \
    static constexpr u8 size_ = element_count;                                                                                                       \
    template<class U>                                                                                                                                \
    type(const type<U> &conv)                                                                                                                        \
    {                                                                                                                                                \
        for (u8 i{0}; i < size_; ++i)                                                                                                                \
            data[i] = (T)conv[i];                                                                                                                    \
    }                                                                                                                                                \
    inline type<T> &operator+=(const type<T> &rhs_)                                                                                                  \
    {                                                                                                                                                \
        return *this = *this + rhs_;                                                                                                                 \
    }                                                                                                                                                \
    inline type<T> &operator-=(const type<T> &rhs_)                                                                                                  \
    {                                                                                                                                                \
        return *this = *this - rhs_;                                                                                                                 \
    }                                                                                                                                                \
    inline type<T> &operator*=(const type<T> &rhs_)                                                                                                  \
    {                                                                                                                                                \
        return *this = *this * rhs_;                                                                                                                 \
    }                                                                                                                                                \
    inline type<T> &operator/=(const type<T> &rhs_)                                                                                                  \
    {                                                                                                                                                \
        return *this = *this / rhs_;                                                                                                                 \
    }                                                                                                                                                \
    inline type<T> &operator*=(const T &rhs_)                                                                                                        \
    {                                                                                                                                                \
        return *this = *this * rhs_;                                                                                                                 \
    }                                                                                                                                                \
    inline type<T> &operator/=(const T &rhs_)                                                                                                        \
    {                                                                                                                                                \
        return *this = *this / rhs_;                                                                                                                 \
    }                                                                                                                                                \
    inline const ind_ret_type &operator[](u8 val_) const                                                                                             \
    {                                                                                                                                                \
        return data[val_];                                                                                                                           \
    }                                                                                                                                                \
    inline ind_ret_type &operator[](u8 val_)                                                                                                         \
    {                                                                                                                                                \
        return data[val_];                                                                                                                           \
    }                                                                                                                                                \
    inline constexpr sizet size() const                                                                                                              \
    {                                                                                                                                                \
        return size_;                                                                                                                                \
    }                                                                                                                                                \
    inline constexpr iterator begin()                                                                                                                \
    {                                                                                                                                                \
        return &data[0];                                                                                                                             \
    }                                                                                                                                                \
    inline constexpr iterator end()                                                                                                                  \
    {                                                                                                                                                \
        return begin() + size_;                                                                                                                      \
    }                                                                                                                                                \
    inline constexpr const_iterator begin() const                                                                                                    \
    {                                                                                                                                                \
        return &data[0];                                                                                                                             \
    }                                                                                                                                                \
    inline constexpr const_iterator end() const                                                                                                      \
    {                                                                                                                                                \
        return begin() + size_;                                                                                                                      \
    }

} // namespace nslib
