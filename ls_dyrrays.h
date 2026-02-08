/*
 * ls_dyrrays.h - v2.2 - Dynamic Arrays - Logan Seeley 2026
 *
 * Overview
 *
 *  This library leverages its dependency ls_lalloc.h
 *  to create fast-growing dynamic arrays, without
 *  fragmentation. Much of the implementation was
 *  referenced directly from Sean Barrett (nothings)'s
 *  stb_ds.h.
 *
 * Documentation
 *
 *  Compilation
 *
 *      Refer to ls_lalloc.h
 *
 *  Usage
 *
 *      This library depends on ls_lalloc.h and its
 *      dependency ls_macros.h.
 * 
 *      This file contains only macros and
 *      translation unit (TU) local functions, meaning
 *      it is intended to be included whenever it
 *      is intended to be used.
 * 
 *      If a dynamic array is created without a heap
 *      specified, it will use the first heap created 
 *      by lalloc's new_heap. Meaning you should only
 *      free that heap at the end of the program.
 * 
 *      That also means you must create a heap before
 *      creating any dynamic arrays.
 * 
 *      Dynamic arrays are created whenever [dypush],
 *      [dysetlen], and [dysetheap] are called with a
 *      null-pointer passed in.
 * 
 *      Recommended usage is to create the dynamic
 *      array by setting the heap with [dysetheap],
 *      then setting the starting array size with
 *      [dysetlen].
 * 
 *      All dynamic arrays are continuous memory,
 *      meaning accessing elements is as simple as
 *      array[i] = value.
 * 
 *      Array size changes may cause the dynamic array
 *      passed to the resizing macro to be changed.
 *      To avoid use-after-moves, only have one
 *      dynamic array location, and use references
 *      for any copy.
 * 
 *      The [dyrray] parameters below are pointers
 *      to whatever element type you want your
 *      dynamic array to be. They can all be NULL,
 *      in which case a new array is created.
 * 
 *  void dypush(dyrray, typeof(*dyrray) value)
 *      Extends the dynamic array by pushing value
 *      to the end of the array.
 * 
 *  void dydel(dyrray, u64_t index)
 *      Swap-pops element at [index] with
 *      element at end of dynamic array.
 *      Does not return the deleted element.
 * 
 *  void* dydupe(dyrray)
 *      Creates a new dynamic array of same
 *      size as [dyrray], and fills it with
 *      same contents of [dyrray].
 * 
 *  void dypop(dyrray)
 *      Deletes the last element of the
 *      dynamic array. Does not return
 *      the deleted element.
 * 
 *  void dylen(dyrray)
 *      Returns the length in elements of
 *      the dynamic array as a u64.
 * 
 *  void dysetlen(dyrray, u64_t length)
 *      Sets the length (in elements) of the
 *      array.
 * 
 *  void dysetheap(dyrray, heap_t heap)
 *      This can be called at any point during
 *      the dynamic array's lifetime: it
 *      copies the content of the dyrray to
 *      a new dynamic array on the specified heap.
 * 
 *  void dyfree(dyrray)
 *      Frees the memory of a dynamic array.
 */


#if !defined(LS_DYRRAYS_INC_)
#define LS_DYRRAYS_INC_


#if !defined(LS_DYRRAYS_PREFIX_NAMES)

    #define dypush      ls_dypush
    #define dydel       ls_dydel
    #define dydupe      ls_dydupe
    #define dypop       ls_dypop
    #define dylen       ls_dylen
    #define dysetlen    ls_dysetlen
    #define dysetheap   ls_dysetheap 
    #define dyfree      ls_dyfree

#endif


/* This includes the header, therefore it must be
 * larger than the size of the header (16 bytes) */
#define LS_DYRRAYS_MIN_Z_ 64


#include "./ls_macros.h"
#include "./ls_lalloc.h"


typedef struct
{
    /* length and capacity in elements */
    ls_u64_t length;
    ls_u64_t capacity;
    ls_lalloc_heap_t heap;
}
ls_dyrrays_header_st_;


static void* ls_dysetsize_ (void* dyrray, ls_u64_t element_c, ls_u64_t element_z)                        LS_LIBFN;
static void* ls_dymoveheap_(void* dyrray, ls_u64_t element_c, ls_u64_t element_z, ls_lalloc_heap_t heap) LS_LIBFN;
static void* ls_dydupe_    (void* dyrray, ls_u64_t element_c, ls_u64_t element_z)                        LS_LIBFN;


#define ls_dyheader_(a) (*((ls_dyrrays_header_st_*) (a) - 1))

#define ls_dyfit_(dyrray, ele_c) ((                             \
    ((dyrray) == LS_NULL)                                       \
    || ((ele_c) > ls_dyheader_(dyrray).capacity)                \
    || (((ele_c) < (ls_dyheader_(dyrray).capacity / 4))         \
    && (ele_c) > 16)) ?                                         \
    ls_dysetsize_(dyrray, ele_c, sizeof(*(dyrray))) :           \
    (ls_dyheader_(dyrray).length +=                             \
        ele_c - ls_dyheader_(dyrray).length, dyrray))

#define ls_dygrow_check_(dyrray, ele_c) ((                      \
    ((dyrray) == LS_NULL)) ?                                    \
    ls_dyfit_(dyrray, (ele_c)) :                                \
    ls_dyfit_(dyrray, ls_dyheader_(dyrray).length + (ele_c)))

#define ls_dyshrink_check_(dyrray, ele_c) (                     \
    ls_dyfit_(dyrray, ls_dyheader_(dyrray).length - (ele_c)))

#define ls_dypush(dyrray, value) (                              \
    (dyrray) = ls_dygrow_check_(dyrray, 1),                     \
    (dyrray)[ls_dyheader_(dyrray).length - 1] = (value))

#define ls_dydel(dyrray, i) (                                   \
    (dyrray)[i] = (dyrray)[ls_dyheader_(dyrray).length - 1],    \
    (dyrray) = ls_dyshrink_check_(dyrray, 1))

#define ls_dydupe(dyrray) (ls_dydupe_(dyrray,                   \
    ls_dyheader_(dyrray).length, sizeof(*dyrray)))

#define ls_dypop(dyrray) ls_dydel(dyrray,                       \
    ls_dyheader_(dyrray).length - 1)

#define ls_dylen(dyrray) (((dyrray) == LS_NULL) ?               \
    0 : ls_dyheader_(dyrray).length)

#define ls_dysetlen(dyrray, n) ((dyrray) = ls_dyfit_(dyrray, n))

#define ls_dysetheap(dyrray, heap) (                            \
    (dyrray) = ls_dygrow_check_(dyrray, 1),                     \
    (dyrray) = ls_dymoveheap_(dyrray,                           \
        ls_dyheader_(dyrray).length, sizeof(*(dyrray)),         \
        heap),                                                  \
    ls_dyheader_(dyrray).length -= 1)

#define ls_dyfree(dyrray) (                                     \
    ls_lfree(ls_dyheader_(dyrray).heap,                         \
    LS_PARITHM(dyrray) - LS_MAX(sizeof(ls_dyrrays_header_st_),  \
    sizeof(*dyrray))))


static LS_INLINE void* ls_dysetsize_(void* dyrray, ls_u64_t element_c, ls_u64_t element_z)
{
    ls_u64_t aligned_z_needed = LS_ALIGN_UP(element_c * element_z + sizeof(ls_dyrrays_header_st_), element_z);
    ls_u64_t actual_z_needed  = LS_MAX(LS_DYRRAYS_MIN_Z_, LS_CEIL_POW2(aligned_z_needed));

    ls_u64_t new_capacity     = (actual_z_needed - LS_ALIGN_UP(sizeof(ls_dyrrays_header_st_),
        element_z)) / element_z;
    
    void* dyrray_base_p = (dyrray != LS_NULL) ?
        (LS_PARITHM(dyrray) - LS_MAX(sizeof(ls_dyrrays_header_st_), element_z)) :
        LS_NULL;

    void* new_dyrray = (dyrray != LS_NULL) ?
        LS_PARITHM(ls_relalloc(ls_dyheader_(dyrray).heap, ls_dyheader_(dyrray).heap, dyrray_base_p, actual_z_needed)) + LS_MAX(sizeof(ls_dyrrays_header_st_), element_z) :
        LS_PARITHM(ls_relalloc(1, 1, dyrray_base_p, actual_z_needed)) + LS_MAX(sizeof(ls_dyrrays_header_st_), element_z);  /* heap 1 is first heap created/default heap */

    ls_dyheader_(new_dyrray).capacity = new_capacity;
    ls_dyheader_(new_dyrray).length = element_c;
    if (dyrray == LS_NULL) { ls_dyheader_(new_dyrray).heap = 1; }

    return new_dyrray;
}

static LS_INLINE void* ls_dymoveheap_(void* dyrray, ls_u64_t element_c, ls_u64_t element_z, ls_lalloc_heap_t heap)
{
    /* TODO: leverage O(1) mem-move capabilities on linux, and perhaps windows later on */

    ls_u64_t aligned_z_needed = LS_ALIGN_UP(element_c * element_z + sizeof(ls_dyrrays_header_st_), element_z);
    ls_u64_t actual_z_needed  = LS_MAX(LS_DYRRAYS_MIN_Z_, LS_CEIL_POW2(aligned_z_needed));
    
    void* dyrray_base_p = LS_PARITHM(dyrray) - LS_MAX(sizeof(ls_dyrrays_header_st_), element_z);
    void* new_dyrray_base_p = ls_relalloc(ls_dyheader_(dyrray).heap, heap, dyrray_base_p, actual_z_needed);
    void* new_dyrray = LS_PARITHM(new_dyrray_base_p) + LS_MAX(sizeof(ls_dyrrays_header_st_), element_z);

    ls_dyheader_(new_dyrray).heap = heap;

    return new_dyrray;
}

static LS_INLINE void* ls_dydupe_(void* dyrray, ls_u64_t element_c, ls_u64_t element_z)
{
    void* new_dyrray = ls_dysetsize_(LS_NULL, element_c, element_z);

    LS_MEMCPY(new_dyrray, dyrray, element_c * element_z);

    return new_dyrray;
}

#endif  /* #if !defined(LS_DYRRAYS_INC_) */


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
