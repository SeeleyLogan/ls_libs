/*
 * ls_valloc.h - v1.0 - layered memory allocator - Logan Seeley 2026
 *
 * Overview
 *
 *  This library eliminates fragmentation for allocations
 *  larger than or equal to page size, and uses O(1)
 *  memory reallocation for all memory larger than an
 *  arbitrary threshold.
 *
 * Documentation
 *
 *  Compilation
 *
 *      _GNU_SOURCE must be defined on linux as
 *      it exposes mremap, which is used in the
 *      O(1) page remapping.
 *
 *  Usage
 *
 *      Include this file to use any of this library's functions.
 *      However, by default, including the file does not
 *      include its implementation.
 * 
 *      To include the implementation for this library: add
 *      #define LS_LALLOC_IMPL above #include "ls_lalloc.h"
 *      This must only happen in one translation unit (TU).
 * 
 *      Currently, this library expects a second header
 *      file called ls_macros.h to be placed along side it.
 *      https://github.com/SeeleyLogan/ls_libs/blob/main/ls_macros.h
 *
 *      You can define LS_LALLOC_PREFIX_NAMES
 *      which will add "ls_" in front of all the
 *      library's functions, handling name collisions.
 *
 *      Any functions suffixed with '_' are
 *      internal helper functions and should never 
 *      be called. They are also not exposed outside
 *      the TU containing the implementation.
 *
 *      Even though memory remapping is O(1), the
 *      time it takes the operating system to
 *      complete the task can still be longer than
 *      a memcpy. Therefore an arbitrary threshold
 *      called LS_LALLOC_MEMCPY_THRES is set.
 *      See the defintion for furhur information.
 *
 *  void* lalloc(u64 size)
 *      Returns a memory region of [size]
 *      rounded up to the nearest exponent
 *      of 2, or NULL on fail.
 *
 *  void* relalloc(void* mem, u64 size)
 *      Copies the contents of [mem] into a new
 *      allocation of [size] rounded up to the
 *      nearest exponent of 2, or NULL on fail.
 *      [mem] must 1. have been returned by either
 *      [lalloc] or [relalloc] - 2. be NULL, in
 *      which case will behave as lalloc(size).
 *
 *  void lfree(mem)
 *      Frees [mem]. [mem] must be returned
 *      by [lalloc] or [relalloc].
 */


#if !defined(LS_LALLOC_INC_)
#define LS_LALLOC_INC_


#include "./ls_macros.h"


#if defined(LS_UNSUPPORTED_OS)
    #error "operating system not supported"
#endif


#ifndef LS_LALLOC_PREFIX_NAMES
    #define lalloc      ls_lalloc
    #define relalloc    ls_relalloc
    #define lfree       ls_lfree
#endif


#if !defined(LS_LALLOC_IMPL)

    /* API */

    extern void* ls_lalloc  (ls_u64_t size);
    extern void* ls_relalloc(void*    mem, ls_u64_t size);
    extern void  ls_lfree   (void*    mem);

#else


#include <stdatomic.h>

#if defined(LS_WINDOWS_OS)
    #include <windows.h>
#elif defined(LS_UNIX_OS)
    #include <sys/mman.h>
    #include <unistd.h>
#endif


/* These numbers are calculated, do not change */
#define LS_LALLOC_VSPACE_Z_     0x230000000000llu /* 35 TiB */
#define LS_LALLOC_MIN_Z_         64llu             /* bytes */
#define LS_LALLOC_MIN_SHIFT_    6                 /* log2(LS_LALLOC_MIN_Z_) */
#define LS_LALLOC_MAX_Z_         0x10000000000llu  /* 1 TiB */
#define LS_LALLOC_LAYER_Z_      LS_LALLOC_MAX_Z_
#define LS_LALLOC_LAYER_C_      35llu

/* Arbitrary constant, used as a threshold to
 * decide when to switch from memcpy to remapping.
 * Different systems scale differently, profile
 * resize if you want to find the optimal threshold.
 * Must be larger than the page size, almost always
 * 4096. */
#define LS_LALLOC_MEMCPY_THRES  0x800000llu  /* 8 MiB */


typedef struct
{
    void*       layer_p;       /* address of start of layer */
    ls_u64_t    block_z;       /* size of block in current layer (pow of 2) */
    ls_u64_t    block_c;       /* amount of blocks in current layer */
    ls_u64_t    block_max;     /* max amount of blocks that can fit in this layer */
    ls_u64_t    head_i;        /* index of block furthest in the layer */
    void*       deleted_head;  /* see implementation details */
}
ls_lalloc_layer_header_;


static struct
{
    ls_bool_t initialized;

    void*     vspace_p;
    ls_u64_t  page_z;  
    ls_lalloc_layer_header_ header_a[LS_LALLOC_LAYER_C_];

    atomic_flag spinlock;

    #if defined(LS_WINDOWS_OS)
    HANDLE proc_h;
    #endif
}
ls_lalloc_meta_  =
{
    .initialized = LS_FALSE,
    .spinlock    = ATOMIC_FLAG_INIT
};


static ls_bool_t ls_lalloc_init_(void);

void* ls_lalloc     (ls_u64_t size);
void* ls_relalloc   (void*    mem, ls_u64_t size);
void  ls_lalloc_free(void*    mem);

static void* ls_lalloc_layer_get_spot_    (ls_u8_t layer_i);
static void* ls_lalloc_layer_get_del_spot_(ls_u8_t layer_i);
static void  ls_lalloc_layer_del_spot_    (ls_u8_t layer_i, void* spot);

static ls_u64_t ls_lalloc_page_size_(void);

static void ls_lalloc_spinlock_  (void);
static void ls_lalloc_spinunlock_(void);


static LS_INLINE ls_bool_t ls_lalloc_init_(void)
{
    #if defined(LS_WINDOWS_OS)
        #warning "incomplete windows implementation"
    #elif defined(LS_UNIX_OS)
        ls_lalloc_meta_.vspace_p = mmap(LS_NULL, LS_LALLOC_VSPACE_Z_, PROT_NONE,
            MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);

        if (ls_lalloc_meta_.vspace_p == MAP_FAILED)
        {
            return LS_FALSE;
        }
    #endif

    for (ls_u8_t i = 0; i < LS_LALLOC_LAYER_C_; i += 1)
    {
        ls_lalloc_meta_.header_a[i] = (ls_lalloc_layer_header_)
        {
            .layer_p = LS_PARITHM(ls_lalloc_meta_.vspace_p) + i * LS_LALLOC_MAX_Z_,

            /* each layer's block size is twice the one below it */
            .block_z      = LS_LALLOC_MIN_Z_ << i,

            .block_c      = 0,
            .block_max    = LS_LALLOC_MAX_Z_ / (LS_LALLOC_MIN_Z_ << i),

            .head_i       = 0,
            .deleted_head = LS_NULL,
        };
    }

    ls_lalloc_meta_.page_z   = ls_lalloc_page_size_();
    #if defined(LS_WINDOWS_OS)
        ls_lalloc_meta_.proc_h = GetCurrentProcess();
    #endif

    ls_lalloc_meta_.initialized = LS_TRUE;
    return LS_TRUE;
}


void* ls_lalloc(ls_u64_t size)
{
    ls_lalloc_spinlock_();
    
    if (ls_lalloc_meta_.initialized != LS_TRUE && ls_lalloc_init_() != LS_TRUE)
    {
        return LS_NULL;
    }
    
    if (size > LS_LALLOC_MAX_Z_)
    {
        return LS_NULL;
    }

    ls_u64_t block_z = 1llu << LS_CEIL_LOG2(LS_MIN(size, LS_LALLOC_MIN_Z_));
    ls_u8_t layer_i  = LS_CEIL_LOG2(LS_MIN(size, LS_LALLOC_MIN_Z_)) - LS_LALLOC_MIN_SHIFT_;

    void* spot = ls_lalloc_layer_get_spot_(layer_i);

    #if defined(LS_WINDOWS_OS)
        #warning "incomplete windows implementation"
    #elif defined(LS_UNIX_OS)
        /* all this alignment (rounding) is for spots smaller than page size*/
        mprotect(LS_CAST(LS_ROUND_DOWN_TO(LS_CAST(spot, ls_u64_t), ls_lalloc_meta_.page_z), void*),
            LS_ROUND_UP_TO(block_z, ls_lalloc_meta_.page_z), PROT_READ | PROT_WRITE);
    #endif

    ls_lalloc_spinunlock_();

    return spot;
}

void* ls_relalloc(void* mem, ls_u64_t size)
{
    if (mem == LS_NULL)
    {
        return ls_lalloc(size);
    }

    ls_lalloc_spinlock_();

    if (ls_lalloc_meta_.initialized != LS_TRUE && ls_lalloc_init_() != LS_TRUE)
    {
        return LS_NULL;
    }

    ls_u64_t block_z = 1llu << LS_CEIL_LOG2(LS_MIN(size, LS_LALLOC_MIN_Z_));

    ls_u8_t new_layer_i  = LS_CEIL_LOG2(LS_MIN(size, LS_LALLOC_MIN_Z_)) - LS_LALLOC_MIN_SHIFT_;
    ls_u8_t old_layer_i = LS_CAST(LS_PARITHM(mem) - LS_PARITHM(ls_lalloc_meta_.vspace_p), ls_u64_t) / LS_LALLOC_LAYER_Z_;

    void* spot = ls_lalloc_layer_get_spot_(new_layer_i);

    if (ls_lalloc_meta_.header_a[new_layer_i].block_z < LS_LALLOC_MEMCPY_THRES)
    {
        #if defined(LS_WINDOWS_OS)
            #warning "incomplete windows implementation"
        #elif defined(LS_UNIX_OS)
            /* all this alignment (rounding) is for spots smaller than page size*/
            mprotect(LS_CAST(LS_ROUND_DOWN_TO(LS_CAST(spot, ls_u64_t), ls_lalloc_meta_.page_z), void*),
                LS_ROUND_UP_TO(block_z, ls_lalloc_meta_.page_z), PROT_READ | PROT_WRITE);
        #endif

        LS_MEMCPY(spot, mem, ls_lalloc_meta_.header_a[old_layer_i].block_z);
    }
    else
    {
        #define LS_HEADER_TMP_ ls_lalloc_meta_.header_a[old_layer_i]

        #if defined(LS_WINDOWS_OS)
            #warning "incomplete windows implementation"
        #elif defined(LS_UNIX_OS)
            /* if you find yourself here, you forgot to add 
             * -D_GNU_SOURCE to your compiler flags */
            mremap(mem, LS_HEADER_TMP_.block_z, LS_HEADER_TMP_.block_z,
                MREMAP_FIXED | MREMAP_MAYMOVE | MREMAP_DONTUNMAP, spot);

            mprotect(LS_PARITHM(spot) + LS_HEADER_TMP_.block_z,
                LS_HEADER_TMP_.block_z, PROT_READ | PROT_WRITE);
            
            mprotect(mem,
                ls_lalloc_meta_.page_z, PROT_READ | PROT_WRITE);
        #endif  /* #if defined(LS_WINDOWS_OS) */
    
        #undef LS_HEADER_TMP_
    }

    ls_lalloc_layer_del_spot_(old_layer_i, mem);

    ls_lalloc_spinunlock_();

    return spot;
}

void ls_lfree(void* mem)
{
    ls_lalloc_spinlock_();

    ls_u8_t layer_i = LS_CAST(LS_PARITHM(mem) - LS_PARITHM(ls_lalloc_meta_.vspace_p), ls_u64_t) / LS_LALLOC_LAYER_Z_;
    ls_lalloc_layer_del_spot_(layer_i, mem);

    ls_lalloc_spinunlock_();
}


static LS_INLINE void* ls_lalloc_layer_get_spot_(ls_u8_t layer_i)
{
    /* the amount of things you'd need to go wrong
     * to trigger this error makes this check redundant */
    /*
    if (ls_lalloc_meta_.header_a[layer_i].block_c == ls_lalloc_meta_.header_a[layer_i].block_max)
    {
        return LS_NULL;
    }
    */

    #define LS_HEADER_TMP_ ls_lalloc_meta_.header_a[layer_i]
    if (LS_HEADER_TMP_.deleted_head == LS_NULL)
    {
        void* spot = LS_PARITHM(LS_HEADER_TMP_.layer_p) + LS_HEADER_TMP_.head_i * LS_HEADER_TMP_.block_z;

        LS_HEADER_TMP_.head_i  += 1;
        LS_HEADER_TMP_.block_c += 1;

        return spot;
    }
    #undef LS_HEADER_TMP_

    return ls_lalloc_layer_get_del_spot_(layer_i);
}

static LS_INLINE void* ls_lalloc_layer_get_del_spot_(ls_u8_t layer_i)
{
    #define LS_HEADER_TMP_ ls_lalloc_meta_.header_a[layer_i]

    if (LS_HEADER_TMP_.block_z < ls_lalloc_meta_.page_z)
    {
        /* unpacked backwards linked list */

        void* spot = LS_HEADER_TMP_.deleted_head;

        LS_HEADER_TMP_.deleted_head = LS_CAST(spot, void**)[0];

        return spot;
    }

    /* packed backwards linked list */

    /* bytes 8 - 16 in a deleted node encode
     * how many links to previous nodes exist
     * in the current node */
    ls_u64_t* link_c = &(LS_CAST(LS_HEADER_TMP_.deleted_head, ls_u64_t*)[1]);
    
    void* spot = LS_CAST(LS_HEADER_TMP_.deleted_head, void**)[*link_c + 1];  /* +1 accounts for backlink */

    *link_c -= 1;

    if (*link_c == 0)
    {
        /* node is empty */

        void* old_head_node = LS_HEADER_TMP_.deleted_head;

        LS_HEADER_TMP_.deleted_head = LS_CAST(old_head_node, void**)[0];

        /* free the now empty node */
        #if defined(LS_WINDOWS_OS)
            #warning "incomplete windows implementation"
        #elif defined(LS_UNIX_OS)
            madvise(old_head_node, ls_lalloc_meta_.page_z, MADV_DONTNEED);
            mprotect(old_head_node, ls_lalloc_meta_.page_z, PROT_NONE);
        #endif
    }

    return spot;

    #undef LS_HEADER_TMP_
}

static LS_INLINE void ls_lalloc_layer_del_spot_(ls_u8_t layer_i, void* spot)
{
    #define LS_HEADER_TMP_ ls_lalloc_meta_.header_a[layer_i]

    if (LS_HEADER_TMP_.block_z < ls_lalloc_meta_.page_z)
    {
        /* unpacked backwards linked list */
        LS_CAST(spot, void**)[0]    = LS_HEADER_TMP_.deleted_head;
        LS_CAST(spot, ls_u64_t*)[1] = 0;  /* zero link count */
        LS_HEADER_TMP_.deleted_head = spot;
    }

    /* packed backwards linked list */

    /* bytes 8 - 16 in a deleted node encode
     * how many links to previous nodes exist
     * in the current node */
    ls_u64_t* link_c = &(LS_CAST(LS_HEADER_TMP_.deleted_head, ls_u64_t*)[1]);
    
    if ((LS_HEADER_TMP_.deleted_head == LS_NULL) || (*link_c == ls_lalloc_meta_.page_z / sizeof(void*) - 2))
    {
        /* node is full */

        LS_CAST(spot, void**)[0]    = LS_HEADER_TMP_.deleted_head;
        LS_HEADER_TMP_.deleted_head = spot;
        link_c = &(LS_CAST(LS_HEADER_TMP_.deleted_head, ls_u64_t*)[1]);
        *link_c = 0;

        /* free the rest of the new spot. note that for
         * resizing allocations, the pages being freed
         * we're previously freed and this step is redundant */
        #if defined(LS_WINDOWS_OS)
            #warning "incomplete windows implementation"
        #elif defined(LS_UNIX_OS)
            madvise(PARITHM(spot) + ls_lalloc_meta_.page_z,
                LS_HEADER_TMP_.block_z - ls_lalloc_meta_.page_z, MADV_DONTNEED);
            mprotect(PARITHM(spot) + ls_lalloc_meta_.page_z,
                LS_HEADER_TMP_.block_z - ls_lalloc_meta_.page_z, PROT_NONE);
        #endif 
    }

    LS_CAST(LS_HEADER_TMP_.deleted_head, void**)[*link_c + 2] = spot;  /* +2 accounts for backlink and link count */
    *link_c += 1;

    #undef LS_HEADER_TMP_
}


static LS_INLINE ls_u64_t ls_lalloc_page_size_(void)
{
    #if defined(LS_WINDOWS_OS)
        SYSTEM_INFO sysinfo;
        GetSystemInfo(&sysinfo);
    
        return sysinfo.dwPageSize;
    #elif defined(LS_UNIX_OS)
        return sysconf(_SC_PAGESIZE);
    #endif
}


static LS_INLINE void ls_lalloc_spinlock_(void)
{
    while (atomic_flag_test_and_set_explicit(&ls_lalloc_meta_.spinlock, memory_order_acquire))
    {
        ;
    }
}

static LS_INLINE void ls_lalloc_spinunlock_(void)
{
    atomic_flag_clear_explicit(&ls_lalloc_meta_.spinlock, memory_order_release);
}


#endif  /* #if !defined(LS_LALLOC_IMPL) */
#endif  /* #if !defined(LS_LALLOC_INC_) */
