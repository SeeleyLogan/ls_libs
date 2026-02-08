/*
 * ls_bitarrays.h - v1.0 - Bit Arrays - Logan Seeley 2026
 *
 * Overview
 *
 *  This library provides optimized bit-arrays by
 *  using ls_dyrrays.h and its dependencies. The
 *  bit-arrays provided by this library are
 *  general-purpose and should satisfy almost every
 *  use-case.
 * 
 *  This implementation currently does not handle
 *  out-of-memory or out-of-heap erros. These issues
 *  will be addressed in future versions.
 *
 * Documentation
 *
 *  Compilation
 *
 *      Refer to ls_lalloc.h (dependency of ls_dyrrays.h)
 *
 *  Usage
 *
 *      This library depends on ls_dyrrays.h and its
 *      dependencies.
 * 
 *      This file contains only macros and
 *      translation unit (TU) local functions, meaning
 *      it is intended to be included whenever it
 *      is intended to be used.
 * 
 *      Bit-arrays can be thought of as an infinite series
 *      of zero-bits, with specified bits explicitly set.
 *      This allows peace of mind when dealing with differently
 *      sized bit-arrays.
 * 
 *      Bit-arrays are closely tied to ls_dyrrays.h, meaning
 *      each bit-array exists within a heap. You are free to
 *      preform operations on bit-arrays between heaps, and
 *      move bit-arrays across heaps. Bit-arrays will begin
 *      in the default heap (first heap created).
 * 
 *  bitarray_t new_bitarray()
 *      Creates a new empty bit-array.
 * 
 *  void free_bitarray(bitarray_t* bitarray_p)
 *      Frees all memory associated with 
 *      [bitarray_p].
 * 
 *  void move_bitarray_heap(bitarray_t* bitarray_p, heap_t heap)
 *      Moves [bitarray_p] from whatever
 *      heap it's in to [heap].
 * 
 *  void set_bit(bitarray_t* bitarray_p, u64_t index, bool_t state)
 *      Sets bit at [index] in [bitarray_p]
 *      to boolean value of [state].
 * 
 *  bool_t get_bit(bitarray_t* bitarray_p, u64_t index)
 *      Returns state of bit at [index]
 *      in [bitarray_p].
 * 
 *  void xor_bitarray(bitarray_t* a_p, bitarray_t* b_p, bitarray_t* dst_p)
 *      Preforms bitwise "xor" operation
 *      on each bit of [a_p] and [b_p] 
 *      and writes the result to [dst_p].
 *      [dst_p] can be any bitarray
 *      including [a_p] or [b_p].
 * 
 *  void and_bitarray(bitarray_t* a_p, bitarray_t* b_p, bitarray_t* dst_p)
 *      Preforms bitwise "and" operation
 *      on each bit of [a_p] and [b_p] 
 *      and writes the result to [dst_p].
 *      [dst_p] can be any bitarray
 *      including [a_p] or [b_p].
 * 
 *  void or_bitarray(bitarray_t* a_p, bitarray_t* b_p, bitarray_t* dst_p)
 *      Preforms bitwise "or" operation
 *      on each bit of [a_p] and [b_p] 
 *      and writes the result to [dst_p].
 *      [dst_p] can be any bitarray
 *      including [a_p] or [b_p].
 * 
 *  void negate_bitarray(bitarray_t* src_p, bitarray_t* dst_p)
 *      Preforms bitwise "not" operation
 *      on each bit of [src_p] and writes
 *      the result to [dst_p]. [dst_p] 
 *      can be any bitarray including 
 *      [src_p].
 * 
 *  void clear_bitarray(bitarray_t* bitarray_p)
 *      Sets each bit of [bitarray_p] to
 *      zero.
 * 
 *  bool_t compare_bitarray(bitarray_t* a_p, bitarray_t* b_p)
 *      Compares each bit of [a_p] with
 *      each bit of [b_p]. Returns 1 if
 *      each bit is equal, 0 if not.
 * 
 *  bitarray_t copy_bitarray(ls_bitarray_t* bitarray_p)
 *      Creates a new bit-array, with each
 *      bit matching each bit of [bitarray_p].
 * 
 *  void fprint_bitarray(FILE* stream, bitarray_t* bitarray_p)
 *      Writes each bit of [bitarray_p] to 
 *      [stream].
 * 
 *  void fprint_bitarray_range(FILE* stream, bitarray_t* bitarray_p, u64_t start, u64_t end)
 *      Writes bit in range of indices
 *      [start] to [end] (inclusive) to 
 *      [stream].
 */


#if !defined(LS_BITARRAYS_INC_)
#define LS_BITARRAYS_INC_


#if !defined(LS_DYRRAYS_PREFIX_NAMES)

    #define new_bitarray        ls_new_bitarray
    #define free_bitarray       ls_free_bitarray
    #define move_bitarray_heap  ls_move_bitarray_heap

    #define set_bit             ls_bitarrays_set_bit
    #define get_bit             ls_bitarrays_get_bit

    #define xor_bitarray        ls_xor_bitarray
    #define and_bitarray        ls_and_bitarray
    #define or_bitarray         ls_or_bitarray
    #define negate_bitarray     ls_negate_bitarray

    #define clear_bitarray      ls_clear_bitarray
    #define compare_bitarray    ls_compare_bitarray
    #define copy_bitarray       ls_copy_bitarray

    #define fprint_bitarray         ls_fprint_bitarray
    #define fprint_bitarray_range   ls_fprint_bitarray_range

#endif


#include "./ls_macros.h"
#include "./ls_dyrrays.h"
#include <stdio.h>


#define LS_BITARRAY_BITS_PER_BLOCK_  512
#define LS_BITARRAY_BYTES_PER_BLOCK_ (LS_BITARRAY_BITS_PER_BLOCK_ / sizeof(ls_u64_t))
#define LS_BITARRAY_INTS_PER_BLOCK_  LS_BITARRAY_BYTES_PER_BLOCK_
#define LS_BITARRAY_INT_Z_BYTES_     sizeof(ls_u64_t)
#define LS_BITARRAY_INT_Z_BITS_      (LS_BITARRAY_INT_Z_BYTES_ * 8)


typedef ls_u64_t __attribute__((vector_size(LS_BITARRAY_BYTES_PER_BLOCK_))) ls_bitarray_block_t_;
typedef ls_bitarray_block_t_* ls_bitarray_t;

#define LS_BITARRAYS_EMPTY_BLOCK_ ((ls_bitarray_block_t_) {  0ull,  0ull,  0ull,  0ull,  0ull,  0ull,  0ull,  0ull, })
#define LS_BITARRAYS_FULL_BLOCK_  ((ls_bitarray_block_t_) { -1ull, -1ull, -1ull, -1ull, -1ull, -1ull, -1ull, -1ull, })


static ls_bitarray_t ls_new_bitarray      ()                                                        LS_LIBFN;
static void          ls_free_bitarray     (ls_bitarray_t*   bitarray_p)                             LS_LIBFN;
static void          ls_move_bitarray_heap(ls_bitarray_t*   bitarray_p, ls_lalloc_heap_t heap)      LS_LIBFN;

static void      ls_bitarrays_set_bit(ls_bitarray_t* bitarray_p, ls_u64_t index, ls_bool_t state)   LS_LIBFN;
static ls_bool_t ls_bitarrays_get_bit(ls_bitarray_t* bitarray_p, ls_u64_t index)                    LS_LIBFN;

static void ls_xor_bitarray   (ls_bitarray_t* a_p, ls_bitarray_t* b_p, ls_bitarray_t* dst_p)        LS_LIBFN;
static void ls_and_bitarray   (ls_bitarray_t* a_p, ls_bitarray_t* b_p, ls_bitarray_t* dst_p)        LS_LIBFN;
static void ls_or_bitarray    (ls_bitarray_t* a_p, ls_bitarray_t* b_p, ls_bitarray_t* dst_p)        LS_LIBFN;
static void ls_negate_bitarray(ls_bitarray_t* src_p,                   ls_bitarray_t* dst_p)        LS_LIBFN;

static void          ls_clear_bitarray  (ls_bitarray_t* bitarray_p)                                 LS_LIBFN;
static ls_bool_t     ls_compare_bitarray(ls_bitarray_t* a_p,           ls_bitarray_t* b_p)          LS_LIBFN;
static ls_bitarray_t ls_copy_bitarray   (ls_bitarray_t* bitarray_p)                                 LS_LIBFN;

static void ls_fprint_bitarray(FILE* stream, ls_bitarray_t* bitarray_p)                             LS_LIBFN;
static void ls_fprint_bitarray_range(FILE* stream, ls_bitarray_t* bitarray_p,
    ls_u64_t start, ls_u64_t end)                                                                   LS_LIBFN;


static LS_INLINE ls_bitarray_t ls_new_bitarray()
{
    ls_bitarray_t bitarray = LS_NULL;
    ls_dypush(bitarray, LS_BITARRAYS_EMPTY_BLOCK_);

    /* first u64 of first block represents
     * the index of the furthest bit set + 64 */
    bitarray[0][0] = 64;

    return bitarray;
}

static LS_INLINE void ls_free_bitarray(ls_bitarray_t* bitarray_p)
{
    ls_dyfree(*bitarray_p);
}

static void ls_move_bitarray_heap(ls_bitarray_t* bitarray_p, ls_lalloc_heap_t heap)
{
    ls_dysetheap(*bitarray_p, heap);
}


static LS_INLINE void ls_bitarrays_set_bit(ls_bitarray_t* bitarray_p, ls_u64_t index, ls_bool_t state)
{
    index += LS_BITARRAY_INT_Z_BITS_;

    if ((index / LS_BITARRAY_BITS_PER_BLOCK_) > ((*bitarray_p)[0][0] / LS_BITARRAY_BITS_PER_BLOCK_))
    {
        ls_dysetlen(*bitarray_p, index / LS_BITARRAY_BITS_PER_BLOCK_ + 1);
        /* assume memory returned by OS is all zero */
    }

    if (index > (*bitarray_p)[0][0])
    {
        (*bitarray_p)[0][0] = index;
    }

    ls_u64_t* bit_int_p = &((*bitarray_p)
        [index / LS_BITARRAY_BITS_PER_BLOCK_]
        [(index / LS_BITARRAY_INTS_PER_BLOCK_) % LS_BITARRAY_INT_Z_BYTES_]);

    (*bit_int_p) = ((*bit_int_p) & ~(1ull << (index % LS_BITARRAY_INT_Z_BITS_))) |
        (LS_CAST(state != 0, ls_u64_t) << (index % LS_BITARRAY_INT_Z_BITS_));
}

static LS_INLINE ls_bool_t ls_bitarrays_get_bit(ls_bitarray_t* bitarray_p, ls_u64_t index)
{
    index += LS_BITARRAY_INT_Z_BITS_;

    if (index > (*bitarray_p)[0][0])
    {
        return 0;
    }

    return ((*bitarray_p)
        [index / LS_BITARRAY_BITS_PER_BLOCK_]
        [(index / LS_BITARRAY_INTS_PER_BLOCK_) % LS_BITARRAY_INT_Z_BYTES_]
        & (1ull << (index % LS_BITARRAY_INT_Z_BITS_))) >> (index % LS_BITARRAY_INT_Z_BITS_);
}


#define LS_BITARRAYS_CONSTRUCT_FN_(bitop, overflow)                             \
    ls_bitarray_t* biggest_bitarr_p;                                            \
    (void) biggest_bitarr_p;                                                    \
    ls_u64_t min_bit_c;                                                         \
    ls_u64_t max_bit_c;                                                         \
    if ((*a_p)[0][0] > (*b_p)[0][0])                                            \
    {                                                                           \
        biggest_bitarr_p = a_p;                                                 \
        max_bit_c = (*a_p)[0][0];                                               \
        min_bit_c = (*b_p)[0][0];                                               \
    }                                                                           \
    else                                                                        \
    {                                                                           \
        biggest_bitarr_p = b_p;                                                 \
        max_bit_c = (*b_p)[0][0];                                               \
        min_bit_c = (*a_p)[0][0];                                               \
    }                                                                           \
    if ((max_bit_c / LS_BITARRAY_BITS_PER_BLOCK_) >                             \
        ((*dst_p)[0][0] / LS_BITARRAY_BITS_PER_BLOCK_))                         \
    {                                                                           \
        ls_dysetlen(*dst_p, max_bit_c / LS_BITARRAY_BITS_PER_BLOCK_ + 1);       \
    }                                                                           \
    for (ls_u64_t i = 0; i <= (min_bit_c / LS_BITARRAY_BITS_PER_BLOCK_); i += 1)\
    {                                                                           \
        (*dst_p)[i] = (*a_p)[i] bitop (*b_p)[i];                                \
    }                                                                           \
    (*dst_p)[0][0] = max_bit_c;                                                 \
    for (ls_u64_t i = (min_bit_c / LS_BITARRAY_BITS_PER_BLOCK_) + 1;            \
        i <= (max_bit_c / LS_BITARRAY_BITS_PER_BLOCK_); i += 1)                 \
    {                                                                           \
        (*dst_p)[i] = overflow;                                                 \
    }

static LS_INLINE void ls_xor_bitarray(ls_bitarray_t* a_p, ls_bitarray_t* b_p, ls_bitarray_t* dst_p)
{
    LS_BITARRAYS_CONSTRUCT_FN_(^, (*biggest_bitarr_p)[i])
}

static LS_INLINE void ls_and_bitarray(ls_bitarray_t* a_p, ls_bitarray_t* b_p, ls_bitarray_t* dst_p)
{
    LS_BITARRAYS_CONSTRUCT_FN_(&, LS_BITARRAYS_EMPTY_BLOCK_)
}

static LS_INLINE void ls_or_bitarray(ls_bitarray_t* a_p, ls_bitarray_t* b_p, ls_bitarray_t* dst_p)
{
    LS_BITARRAYS_CONSTRUCT_FN_(|, (*biggest_bitarr_p)[i])
}

static LS_INLINE void ls_negate_bitarray(ls_bitarray_t* src_p, ls_bitarray_t* dst_p)
{
    ls_u64_t index = (*src_p)[0][0];

    if ((index / LS_BITARRAY_BITS_PER_BLOCK_) >
        ((*dst_p)[0][0] / LS_BITARRAY_BITS_PER_BLOCK_))
    {
        ls_dysetlen(*dst_p, index / LS_BITARRAY_BITS_PER_BLOCK_ + 1);
    }

    for (ls_u64_t i = 0; i <= (index / LS_BITARRAY_BITS_PER_BLOCK_); i += 1)
    {
        (*dst_p)[i] = ~((*src_p)[i]);
    }

    (*dst_p)[0][0] = index;
}

static LS_INLINE void ls_clear_bitarray(ls_bitarray_t* bitarray)
{
    ls_u64_t index = (*bitarray)[0][0];

    for (ls_u64_t i = 0; i <= (index / LS_BITARRAY_BITS_PER_BLOCK_); i += 1)
    {
        (*bitarray)[i] = LS_BITARRAYS_EMPTY_BLOCK_;
    }

    (*bitarray)[0][0] = index;
}

static LS_INLINE ls_bool_t ls_compare_bitarray(ls_bitarray_t* a_p, ls_bitarray_t* b_p)
{
    ls_bitarray_t* biggest_bitarr_p;

    ls_u64_t min_bit_c;
    ls_u64_t max_bit_c;

    if ((*a_p)[0][0] > (*b_p)[0][0])
    {
        biggest_bitarr_p = a_p;
        max_bit_c = (*a_p)[0][0];
        min_bit_c = (*b_p)[0][0];
    }
    else
    {
        biggest_bitarr_p = b_p;
        max_bit_c = (*b_p)[0][0];
        min_bit_c = (*a_p)[0][0];
    }

    ls_bool_t is_equal = 1;

    ls_u64_t a_index = (*a_p)[0][0];
    ls_u64_t b_index = (*b_p)[0][0];
    (*a_p)[0][0] = 0;
    (*b_p)[0][0] = 0;

    is_equal &= LS_MEMCMP(*a_p, *b_p, min_bit_c / 8) == 0;

    (*a_p)[0][0] = a_index;
    (*b_p)[0][0] = b_index;

    if (!is_equal)
    {
        return 0;
    }

    for (ls_u64_t i = (min_bit_c / LS_BITARRAY_BITS_PER_BLOCK_) + 1;
        i <= (max_bit_c / LS_BITARRAY_BITS_PER_BLOCK_); i += 1)
    {
        ls_bitarray_block_t_ equal_result = (*biggest_bitarr_p)[i] == LS_BITARRAYS_EMPTY_BLOCK_;
        is_equal &= (LS_MEMCMP(&equal_result, &LS_BITARRAYS_FULL_BLOCK_, sizeof(ls_bitarray_block_t_)) == 0);
    }

    return is_equal;
}


static LS_INLINE ls_bitarray_t ls_copy_bitarray(ls_bitarray_t* bitarray_p)
{
    ls_bitarray_t new_bitarray = ls_dydupe(*bitarray_p);

    return new_bitarray;
}


static LS_INLINE void ls_fprint_bitarray(FILE* stream, ls_bitarray_t* bitarray_p)
{
    for (ls_u64_t i = 0; i <= ((*bitarray_p)[0][0] - 64); i += 1)
    {
        fputc(ls_bitarrays_get_bit(bitarray_p, i) + '0', stream);
    }
}

static LS_INLINE void ls_fprint_bitarray_range(FILE* stream, ls_bitarray_t* bitarray_p,
    ls_u64_t start, ls_u64_t end)
{
    for (ls_u64_t i = start; i <= LS_MIN((*bitarray_p)[0][0] - 64, end); i += 1)
    {
        fputc(ls_bitarrays_get_bit(bitarray_p, i) + '0', stream);
    }
}


#undef LS_BITARRAY_BITS_PER_BLOCK_
#undef LS_BITARRAY_BYTES_PER_BLOCK_
#undef LS_BITARRAY_INTS_PER_BLOCK_
#undef LS_BITARRAY_INT_Z_BYTES_
#undef LS_BITARRAY_INT_Z_BITS_
#undef LS_BITARRAYS_EMPTY_BLOCK_
#undef LS_BITARRAYS_FULL_BLOCK_


#endif  /* !defined(LS_BITARRAYS_INC_) */


/*
 * Copyright (C) 2025  Logan Seeley
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License as published
 * by the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Affero General Public License for more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */
