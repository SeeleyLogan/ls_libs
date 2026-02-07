/*
 * ls_dyrrays.h - v1.0.0 - Dynamic Arrays - Logan Seeley 2025
 */


#if !defined(LS_DYRRAYS_INC_)
#define LS_DYRRAYS_INC_


#if !defined(LS_DYRRAYS_PREFIX_NAMES)
    #define dypush   ls_dypush
    #define dydel    ls_dydel
    #define dypop    ls_dypop
    #define dylen    ls_dylen
    #define dysetlen ls_dysetlen
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
}
ls_dyrrays_header_s_;


static void* ls_dysetsize_(void* dyrray, ls_u64_t element_c, ls_u64_t element_z);


#define ls_dyheader_(a) (*((ls_dyrrays_header_s_*) (a) - 1))

#define ls_dyfit_(dyrray, ele_c) ((                             \
    ((dyrray) == LS_NULL)                                       \
    || ((ele_c) > ls_dyheader_(dyrray).capacity)                \
    || (((ele_c) < (ls_dyheader_(dyrray).capacity / 4))         \
    && (ele_c) > 16)) ?                                         \
    ls_dysetsize_(dyrray, ele_c, sizeof(*dyrray)) :             \
    (ls_dyheader_(dyrray).length += ele_c - ls_dyheader_(dyrray).length, dyrray))

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

#define ls_dypop(dyrray) ls_dydel(dyrray, ls_dyheader_(dyrray).length - 1)

#define ls_dylen(dyrray) (((dyrray) == LS_NULL) ? 0 : ls_dyheader_(dyrray).length)

#define ls_dysetlen(dyrray, n) ((dyrray) = ls_dyfit_(dyrray, n))


static LS_INLINE void* ls_dysetsize_(void* dyrray, ls_u64_t element_c, ls_u64_t element_z)
{
    ls_u64_t raw_z_needed     = element_c * element_z + sizeof(ls_dyrrays_header_s_);
    ls_u64_t aligned_z_needed = LS_ALIGN_UP(raw_z_needed, element_z);
    ls_u64_t actual_z_needed  = LS_MAX(LS_DYRRAYS_MIN_Z_, 1llu << LS_CEIL_LOG2(aligned_z_needed));

    ls_u64_t new_capacity     = (actual_z_needed - LS_ALIGN_UP(sizeof(ls_dyrrays_header_s_),
        element_z)) / element_z;
    
    void* dyrray_base_p = (dyrray != LS_NULL) ?
        (LS_PARITHM(dyrray) - LS_MAX(sizeof(ls_dyrrays_header_s_), element_z)) :
        LS_NULL;

    void* new_dyrray = LS_PARITHM(ls_relalloc(dyrray_base_p, actual_z_needed)) + LS_MAX(sizeof(ls_dyrrays_header_s_), element_z);

    ls_dyheader_(new_dyrray).capacity = new_capacity;
    ls_dyheader_(new_dyrray).length = element_c;

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
