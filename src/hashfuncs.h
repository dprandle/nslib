#pragma once

#include "basic_types.h"
#include "basic_type_traits.h"

namespace nslib
{
//-----------------------------------------------------------------------------
// SipHash reference C implementation
//
// Copyright (c) 2012-2016 Jean-Philippe Aumasson
// <jeanphilippe.aumasson@gmail.com>
// Copyright (c) 2012-2014 Daniel J. Bernstein <djb@cr.yp.to>
//
// To the extent possible under law, the author(s) have dedicated all copyright
// and related and neighboring rights to this software to the public domain
// worldwide. This software is distributed without any warranty.
//
// You should have received a copy of the CC0 Public Domain Dedication along
// with this software. If not, see
// <http://creativecommons.org/publicdomain/zero/1.0/>.
//
// default: SipHash-2-4
//-----------------------------------------------------------------------------
u64 siphash(const u8 *in, const sizet inlen, u64 seed0, u64 seed1);

//-----------------------------------------------------------------------------
// MurmurHash3 was written by Austin Appleby, and is placed in the public
// domain. The author hereby disclaims copyright to this source code.
//
// Murmur3_86_128
//-----------------------------------------------------------------------------
u32 murmurhash2(const void *key, sizet len, u32 seed);
u64 murmurhash3(const void *key, sizet len, u32 seed);


//-----------------------------------------------------------------------------
// xxHash Library
// Copyright (c) 2012-2021 Yann Collet
// All rights reserved.
//
// BSD 2-Clause License (https://www.opensource.org/licenses/bsd-license.php)
//
// xxHash3
//-----------------------------------------------------------------------------
u64 xxhash3(const void *data, sizet len, u64 seed);

void crc32(const void *key, int len, u32 seed, void *out);

// hashmap_sip returns a hash value for `data` using SipHash-2-4.
u64 hash_ptr_sip(const void *data, sizet len, u64 seed0, u64 seed1);

// hashmap_murmur returns a hash value for `data` using Murmur3_86_128.
u64 hash_ptr_murmur(const void *data, sizet len, u64 seed0, u64 seed1);

// hashmap_xxhash3 returns a hash value for `data` using xxhash algorithm
u64 hash_ptr_xxhash3(const void *data, sizet len, u64 seed0, u64 seed1);

// Simply 1 for 1 hash - the key has been computed already and we just make use of the hash buckets
template<integral T>
inline u64 hash_type(T key, u64, u64) {
    return (T)key;
}

// Hash strings
u64 hash_type(const char* key, u64, u64);


} // namespace nslib
