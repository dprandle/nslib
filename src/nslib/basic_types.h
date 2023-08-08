#pragma once

#include <cstdint>
#include <cstddef>
#include <cassert>

#include "limits.h"

#if defined (__unix__) || (defined (__APPLE__) && defined (__MACH__))
#define IS_POSIX_SYSTEM
#endif

// Check if all of the flags in provided flags
#define check_all_flags(bitmask, flags) (((bitmask) & (flags)) == (flags))

// Check if any of the bits provided in \param flags are set in \param bitmask
#define check_flags(bitmask, flags) (((bitmask) & (flags)) != 0)

// Unset all of the bits provided by \param flags in \param bitmask
#define unset_flags(bitmask, flags) ((bitmask) &= ~(flags))

// Set all of the bits provided by \param flags in \param bitmask
#define set_flags(bitmask, flags) ((bitmask) |= (flags))

#define set_flag_from_bool(bitmask, flag, boolval) ((boolval) ? set_flags(bitmask, flag) : unset_flags(bitmask, flag))

#define intern static

#define SMALL_STR_LEN 24

namespace nslib
{
using i8 = int8_t;
using u8 = uint8_t;
using i16 = int16_t;
using u16 = uint16_t;
using i32 = int32_t;
using u32 = uint32_t;
using i64 = int64_t;
using u64 = uint64_t;
using uchar = unsigned char;
using wchar = wchar_t;
using c16 = char16_t;
using c32 = char32_t;
using sizet = std::size_t;
using f32 = float;
using f64 = double;
using ldouble = long double;

const int KB_SIZE = 1024;
const int MB_SIZE = 1024 * KB_SIZE;

using small_str = char[SMALL_STR_LEN];

inline constexpr const sizet npos = LONG_MAX;

} // namespace nslib
