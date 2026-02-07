/*
 * ls_lalloc.h - v3.0 - Layered Memory Allocator - Logan Seeley 2026
 *
 * Overview
 *
 *  This library eliminates fragmentation for allocations
 *  larger than or equal to page size, and uses O(1)
 *  memory reallocation for all memory larger than an
 *  arbitrary threshold.
 * 
 *  Windows specific: all allocations are reserved
 *  with read-write-watch.
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
 *  heap_t new_heap(ls_u64_t max_alloc_z)
 *      Reserves a new heap to be used in lalloc/relalloc.
 *      [max_alloc_z] specifies the largest single
 *      allocation you intend on making. Passing 0
 *      will default to size of ram. Returns 0 on fail,
 *      otherwise a valid heap. You can only make up to
 *      256 heaps.
 *
 *  void free_heap(heap_t heap)
 *      Unreserves a created heap. All existing allocations
 *      within the heap are freed, any new allocations
 *      made with the freed heap will segfault. Deleting
 *      a heap does not reuse its ID later on.
 * 
 *  void* lalloc(heap_t heap, u64_t size)
 *      Returns a memory region of [size]
 *      rounded up to the nearest exponent
 *      of 2, or NULL on fail.
 *
 *  void* relalloc(heap_t heap, void* mem_p, u64_t size)
 *      Copies the contents of [mem] into a new
 *      allocation of [size] rounded up to the
 *      nearest exponent of 2, or NULL on fail.
 *      [mem] must 1. have been returned by either
 *      [lalloc] or [relalloc] - 2. be NULL, in
 *      which case will behave as lalloc(size).
 *
 *  void lfree(heap_t heap, void* mem_p)
 *      Frees [mem]. [mem] must be returned
 *      by [lalloc] or [relalloc].
 */


#if !defined(LS_LALLOC_INC_)
#define LS_LALLOC_INC_


#include "./ls_macros.h"


typedef ls_u8_t ls_lalloc_heap_t;


#if !defined(LS_LALLOC_PREFIX_NAMES)

    #define new_heap  ls_lalloc_new_heap
    #define free_heap ls_lalloc_free_heap
    #define lalloc    ls_lalloc
    #define relalloc  ls_relalloc
    #define lfree     ls_lfree
    #define heap_t    ls_lalloc_heap_t

#endif


#if defined(LS_UNSUPPORTED_OS)
    #error "operating system not supported"
#endif


#if !defined(LS_LALLOC_IMPL)

    /* API */

    extern ls_lalloc_heap_t ls_lalloc_new_heap (ls_u64_t         max_alloc_z);
    extern void             ls_lalloc_free_heap(ls_lalloc_heap_t heap);

    extern void* ls_lalloc  (ls_lalloc_heap_t heap_id, ls_u64_t size);
    extern void* ls_relalloc(ls_lalloc_heap_t heap_id, void*    mem, ls_u64_t size);
    extern void  ls_lfree   (ls_lalloc_heap_t heap_id, void*    mem);

#else


#include <stdatomic.h>

#if defined(LS_WINDOWS_OS)
    #include <windows.h>
#elif defined(LS_UNIX_OS)
    #include <sys/mman.h>
    #include <unistd.h>
#endif


/* Arbitrary constant, used as a threshold to
 * decide when to switch from memcpy to remapping.
 * Different systems scale differently, profile
 * resize if you want to find the optimal threshold.
 * Must be larger than the page size, almost always
 * 4096. */
#define LS_LALLOC_MEMCPY_THRES  0x800000llu  /* 8 MiB */

/* I currently can't figure out how to get read-write-watch
 * and O(1) memory reallocation working on windows. For now,
 * setting the memcpy threshold to integer limit is a dirty
 * fix to force memcpy on windows. */
#if defined(LS_WINDOWS_OS)
    #undef LS_LALLOC_MEMCPY_THRES
    #define LS_LALLOC_MEMCPY_THRES (1llu << 63)
#endif

/* do not change */
#define LS_LALLOC_MIN_Z_        64llu             /* bytes */
#define LS_LALLOC_MIN_Z_LOG2_   6                 /* log2(LS_LALLOC_MIN_Z_) */


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


typedef struct
{
    void*     vspace_p;
    ls_u64_t  page_z;  

    ls_u64_t  vspace_z;
    ls_u64_t  layer_z;
    ls_u8_t   layer_c;

    ls_lalloc_layer_header_ header_a[256];

    atomic_flag spinlock;

    #if defined(LS_WINDOWS_OS)
    HANDLE proc_h;
    #endif
}
ls_lalloc_heap_st_;


static atomic_flag ls_lalloc_global_spinlock_ = ATOMIC_FLAG_INIT;
static ls_u8_t ls_lalloc_heap_c_ = 0;
static ls_lalloc_heap_st_ ls_lalloc_heap_sa_[256];


ls_lalloc_heap_t ls_lalloc_new_heap(ls_u64_t max_alloc_z);

void* ls_lalloc  (ls_lalloc_heap_t heap, ls_u64_t size);
void* ls_relalloc(ls_lalloc_heap_t heap, void*    mem_p, ls_u64_t size);
void  ls_lfree   (ls_lalloc_heap_t heap, void*    mem_p);

static void* ls_lalloc_layer_get_spot_    (ls_lalloc_heap_st_* heap_sp, ls_u8_t layer_i);
static void* ls_lalloc_layer_get_del_spot_(ls_lalloc_heap_st_* heap_sp, ls_u8_t layer_i);
static void  ls_lalloc_layer_del_spot_    (ls_lalloc_heap_st_* heap_sp, ls_u8_t layer_i, void* spot_p);

static ls_u64_t ls_lalloc_memtotal_ (void);
static ls_u64_t ls_lalloc_page_size_(void);

static void ls_lalloc_spinlock_  (atomic_flag* spinlock_p);
static void ls_lalloc_spinunlock_(atomic_flag* spinlock_p);


ls_lalloc_heap_t ls_lalloc_new_heap(ls_u64_t max_alloc_z)
{
    ls_u64_t layer_z;

    if (max_alloc_z == 0)
    {
        layer_z = 1llu << LS_CEIL_LOG2(ls_lalloc_memtotal_());
    }
    else
    {
        layer_z = LS_MAX(1llu << LS_CEIL_LOG2(max_alloc_z), ls_lalloc_page_size_());
    }

    ls_lalloc_spinlock_(&ls_lalloc_global_spinlock_);

    ls_lalloc_heap_t heap = ls_lalloc_heap_c_;
    ls_lalloc_heap_st_* heap_sp = &(ls_lalloc_heap_sa_[ls_lalloc_heap_c_]);

    heap_sp->layer_z  = layer_z;
    heap_sp->layer_c  = LS_FLOOR_LOG2(layer_z) - LS_LALLOC_MIN_Z_LOG2_ + 1;
    heap_sp->page_z   = ls_lalloc_page_size_();
    heap_sp->spinlock = (atomic_flag) ATOMIC_FLAG_INIT;

    #if defined(LS_WINDOWS_OS)
        heap_sp->proc_h = GetCurrentProcess();

        heap_sp->vspace_p = VirtualAllocEx(heap_sp->proc_h, LS_NULL,
            heap_sp->layer_z * heap_sp->layer_c, MEM_RESERVE | MEM_WRITE_WATCH, PAGE_NOACCESS);

        if (heap_sp->vspace_p == LS_NULL)
        {
            ls_lalloc_spinunlock_(&ls_lalloc_global_spinlock_);

            return 0;
        }
    #elif defined(LS_UNIX_OS)
        heap_sp->vspace_p = mmap(LS_NULL, heap_sp->layer_z * heap_sp->layer_c,
            PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);

        if (heap_sp->vspace_p == MAP_FAILED)
        {
            ls_lalloc_spinunlock_(&ls_lalloc_global_spinlock_);

            return 0;
        }
    #endif

    /* initialize layer headers */
    for (ls_u8_t i = 0; i < heap_sp->layer_c; i += 1)
    {
        heap_sp->header_a[i] = (ls_lalloc_layer_header_)
        {
            .layer_p = LS_PARITHM(heap_sp->vspace_p) + i * heap_sp->layer_z,

            /* each layer's block size is twice the one below it */
            .block_z      = LS_LALLOC_MIN_Z_ << i,

            .block_c      = 0,
            .block_max    = heap_sp->layer_z / (LS_LALLOC_MIN_Z_ << i),

            .head_i       = 0,
            .deleted_head = LS_NULL,
        };
    }

    ls_lalloc_heap_c_ += 1;

    ls_lalloc_spinunlock_(&ls_lalloc_global_spinlock_);

    return heap + 1;
}

void ls_lalloc_free_heap(ls_lalloc_heap_t heap)
{
    ls_lalloc_heap_st_* heap_sp = &(ls_lalloc_heap_sa_[heap - 1]);

    #if defined(LS_WINDOWS_OS)
        VirtualFree(heap_sp->vspace_p, heap_sp->vspace_z, MEM_RELEASE);
    #else
        munmap(heap_sp->vspace_p, heap_sp->vspace_z);
    #endif
}


void* ls_lalloc(ls_lalloc_heap_t heap, ls_u64_t size)
{
    ls_lalloc_heap_st_* heap_sp = &(ls_lalloc_heap_sa_[heap - 1]);

    ls_lalloc_spinlock_(&heap_sp->spinlock);
    
    if (size > heap_sp->layer_z)
    {
        return LS_NULL;
    }

    ls_u64_t block_z = 1llu << LS_CEIL_LOG2(LS_MAX(size, LS_LALLOC_MIN_Z_));
    ls_u8_t layer_i  = LS_CEIL_LOG2(LS_MAX(size, LS_LALLOC_MIN_Z_)) - LS_LALLOC_MIN_Z_LOG2_;

    void* spot_p = ls_lalloc_layer_get_spot_(heap_sp, layer_i);

    /* all this alignment (rounding) is for spots smaller than page size*/
    #if defined(LS_WINDOWS_OS)
        VirtualAllocEx(heap_sp->proc_h,
            LS_CAST(LS_ALIGN_DOWN(LS_CAST(spot_p, ls_u64_t), heap_sp->page_z), void*),
            LS_ALIGN_UP(block_z, heap_sp->page_z), MEM_COMMIT, PAGE_READWRITE);
    #elif defined(LS_UNIX_OS)
        mprotect(LS_CAST(LS_ALIGN_DOWN(LS_CAST(spot_p, ls_u64_t), heap_sp->page_z), void*),
            LS_ALIGN_UP(block_z, heap_sp->page_z), PROT_READ | PROT_WRITE);
    #endif

    ls_lalloc_spinunlock_(&heap_sp->spinlock);

    return spot_p;
}

void* ls_relalloc(ls_lalloc_heap_t heap, void* mem_p, ls_u64_t size)
{
    if (mem_p == LS_NULL)
    {
        return ls_lalloc(heap, size);
    }

    ls_lalloc_heap_st_* heap_sp = &(ls_lalloc_heap_sa_[heap - 1]);

    ls_lalloc_spinlock_(&heap_sp->spinlock);

    ls_u64_t block_z = 1llu << LS_CEIL_LOG2(LS_MAX(size, LS_LALLOC_MIN_Z_));

    ls_u8_t new_layer_i  = LS_CEIL_LOG2(LS_MAX(size, LS_LALLOC_MIN_Z_)) - LS_LALLOC_MIN_Z_LOG2_;
    ls_u8_t old_layer_i = LS_CAST(LS_PARITHM(mem_p) - LS_PARITHM(heap_sp->vspace_p), ls_u64_t) / heap_sp->layer_z;

    void* spot_p = ls_lalloc_layer_get_spot_(heap_sp, new_layer_i);

    /* compiler should optimize out the if-statement on windows while the
     * copy threshold is u64 max */
    if (heap_sp->header_a[new_layer_i].block_z <= LS_LALLOC_MEMCPY_THRES)
    {
        #if defined(LS_WINDOWS_OS)
            VirtualAllocEx(heap_sp->proc_h,
                LS_CAST(LS_ALIGN_DOWN(LS_CAST(spot_p, ls_u64_t), heap_sp->page_z), void*),
                LS_ALIGN_UP(block_z, heap_sp->page_z), MEM_COMMIT, PAGE_READWRITE);
        #elif defined(LS_UNIX_OS)
            mprotect(LS_CAST(LS_ALIGN_DOWN(LS_CAST(spot_p, ls_u64_t), heap_sp->page_z), void*),
                LS_ALIGN_UP(block_z, heap_sp->page_z), PROT_READ | PROT_WRITE);
        #endif

        LS_MEMCPY(spot_p, mem_p, LS_MIN(heap_sp->header_a[old_layer_i].block_z,
            heap_sp->header_a[new_layer_i].block_z));
    }
    else
    {
        #define LS_HEADER_TMP_ heap_sp->header_a[old_layer_i]

        #if defined(LS_WINDOWS_OS)
            // #warning "incomplete windows implementation"
        #elif defined(LS_UNIX_OS)
            /* if you find yourself here, you forgot to add 
             * -D_GNU_SOURCE to your compiler flags */
            mremap(mem_p, LS_HEADER_TMP_.block_z, LS_HEADER_TMP_.block_z,
                MREMAP_FIXED | MREMAP_MAYMOVE | MREMAP_DONTUNMAP, spot_p);

            mprotect(LS_PARITHM(spot_p) + LS_HEADER_TMP_.block_z,
                LS_HEADER_TMP_.block_z, PROT_READ | PROT_WRITE);
            
            mprotect(mem_p,
                heap_sp->page_z, PROT_READ | PROT_WRITE);
        #endif  /* #if defined(LS_WINDOWS_OS) */
    
        #undef LS_HEADER_TMP_
    }

    ls_lalloc_layer_del_spot_(heap_sp, old_layer_i, mem_p);

    ls_lalloc_spinunlock_(&heap_sp->spinlock);

    return spot_p;
}

void ls_lfree(ls_lalloc_heap_t heap, void* mem_p)
{
    ls_lalloc_heap_st_* heap_sp = &(ls_lalloc_heap_sa_[heap - 1]);

    ls_lalloc_spinlock_(&heap_sp->spinlock);

    ls_u8_t layer_i = LS_CAST(LS_PARITHM(mem_p) - LS_PARITHM(heap_sp->vspace_p), ls_u64_t) / heap_sp->layer_z;
    ls_lalloc_layer_del_spot_(heap_sp, layer_i, mem_p);

    ls_lalloc_spinunlock_(&heap_sp->spinlock);
}


static LS_INLINE void* ls_lalloc_layer_get_spot_(ls_lalloc_heap_st_* heap_sp, ls_u8_t layer_i)
{
    /* the amount of things you'd need to go wrong
     * to trigger this error makes this check redundant */
    /*
    if (heap_sp->header_a[layer_i].block_c == heap_sp->header_a[layer_i].block_max)
    {
        return LS_NULL;
    }
    */

    #define LS_HEADER_TMP_ heap_sp->header_a[layer_i]
    if (LS_HEADER_TMP_.deleted_head == LS_NULL)
    {
        void* spot_p = LS_PARITHM(LS_HEADER_TMP_.layer_p) + LS_HEADER_TMP_.head_i * LS_HEADER_TMP_.block_z;

        LS_HEADER_TMP_.head_i  += 1;
        LS_HEADER_TMP_.block_c += 1;

        return spot_p;
    }
    #undef LS_HEADER_TMP_

    return ls_lalloc_layer_get_del_spot_(heap_sp, layer_i);
}

static LS_INLINE void* ls_lalloc_layer_get_del_spot_(ls_lalloc_heap_st_* heap_sp, ls_u8_t layer_i)
{
    #define LS_HEADER_TMP_ heap_sp->header_a[layer_i]

    if (LS_HEADER_TMP_.block_z < heap_sp->page_z)
    {
        /* unpacked backwards linked list */

        void* spot_p = LS_HEADER_TMP_.deleted_head;

        LS_HEADER_TMP_.deleted_head = LS_CAST(spot_p, void**)[0];

        return spot_p;
    }

    /* packed backwards linked list */

    /* bytes 8 - 16 in a deleted node encode
     * how many links to previous nodes exist
     * in the current node */
    ls_u64_t* link_c = &(LS_CAST(LS_HEADER_TMP_.deleted_head, ls_u64_t*)[1]);
    
    void* spot_p = LS_CAST(LS_HEADER_TMP_.deleted_head, void**)[*link_c + 1];  /* +1 accounts for backlink */

    *link_c -= 1;

    if (*link_c == 0)
    {
        /* node is empty */

        void* old_head_node = LS_HEADER_TMP_.deleted_head;

        LS_HEADER_TMP_.deleted_head = LS_CAST(old_head_node, void**)[0];

        /* free the now empty node */
        #if defined(LS_WINDOWS_OS)
            DWORD old_prot;
            VirtualFreeEx(heap_sp->proc_h, old_head_node,
                heap_sp->page_z, MEM_DECOMMIT);
            VirtualProtectEx(heap_sp->proc_h, old_head_node,
                heap_sp->page_z, PAGE_READWRITE, &old_prot);
        #elif defined(LS_UNIX_OS)
            madvise(old_head_node, heap_sp->page_z, MADV_DONTNEED);
            mprotect(old_head_node, heap_sp->page_z, PROT_NONE);
        #endif
    }

    return spot_p;

    #undef LS_HEADER_TMP_
}

static LS_INLINE void ls_lalloc_layer_del_spot_(ls_lalloc_heap_st_* heap_sp, ls_u8_t layer_i, void* spot_p)
{
    #define LS_HEADER_TMP_ heap_sp->header_a[layer_i]

    if (LS_HEADER_TMP_.block_z < heap_sp->page_z)
    {
        /* unpacked backwards linked list */
        LS_CAST(spot_p, void**)[0]    = LS_HEADER_TMP_.deleted_head;
        LS_CAST(spot_p, ls_u64_t*)[1] = 0;  /* zero link count */
        LS_HEADER_TMP_.deleted_head = spot_p;
    }

    /* packed backwards linked list */

    /* bytes 8 - 16 in a deleted node encode
     * how many links to previous nodes exist
     * in the current node */
    ls_u64_t* link_c = &(LS_CAST(LS_HEADER_TMP_.deleted_head, ls_u64_t*)[1]);
    
    if ((LS_HEADER_TMP_.deleted_head == LS_NULL) || (*link_c == heap_sp->page_z / sizeof(void*) - 2))
    {
        /* node is full */

        LS_CAST(spot_p, void**)[0]    = LS_HEADER_TMP_.deleted_head;
        LS_HEADER_TMP_.deleted_head = spot_p;
        link_c = &(LS_CAST(LS_HEADER_TMP_.deleted_head, ls_u64_t*)[1]);
        *link_c = 0;

        /* free the rest of the new spot. note that for
         * resizing allocations, the pages being freed
         * we're previously freed and this step is redundant */
        #if defined(LS_WINDOWS_OS)
            DWORD old_prot;
            VirtualFreeEx(heap_sp->proc_h, PARITHM(spot_p) + heap_sp->page_z,
                LS_HEADER_TMP_.block_z - heap_sp->page_z, MEM_DECOMMIT);
            VirtualProtectEx(heap_sp->proc_h, PARITHM(spot_p) + heap_sp->page_z,
                LS_HEADER_TMP_.block_z - heap_sp->page_z, PAGE_READWRITE, &old_prot);
        #elif defined(LS_UNIX_OS)
            madvise(PARITHM(spot_p) + heap_sp->page_z,
                LS_HEADER_TMP_.block_z - heap_sp->page_z, MADV_DONTNEED);
            mprotect(PARITHM(spot_p) + heap_sp->page_z,
                LS_HEADER_TMP_.block_z - heap_sp->page_z, PROT_NONE);
        #endif 
    }

    LS_CAST(LS_HEADER_TMP_.deleted_head, void**)[*link_c + 2] = spot_p;  /* +2 accounts for backlink and link count */
    *link_c += 1;

    #undef LS_HEADER_TMP_
}


static LS_INLINE ls_u64_t ls_lalloc_memtotal_(void)
{
    #ifdef _WIN32
        MEMORYSTATUS memstat;
        GlobalMemoryStatus(&memstat);
    
        return memstat.dwTotalPhys;
    #else
        FILE*  meminfo_f = fopen("/proc/meminfo", "rb");
        ls_u64_t memtotal;
    
        fscanf(meminfo_f, "MemTotal:%lu", &memtotal);
    
        fclose(meminfo_f);
    
        return memtotal * 1024;
    #endif
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


static LS_INLINE void ls_lalloc_spinlock_(atomic_flag* spinlock_p)
{
    while (atomic_flag_test_and_set_explicit(spinlock_p, memory_order_acquire))
    {
        ;
    }
}

static LS_INLINE void ls_lalloc_spinunlock_(atomic_flag* spinlock_p)
{
    atomic_flag_clear_explicit(spinlock_p, memory_order_release);
}


#endif  /* #if !defined(LS_LALLOC_IMPL) */
#endif  /* #if !defined(LS_LALLOC_INC_) */
