/*
 * ls_mallocs.h - v1.0.0 - memory allocators - Logan Seeley 2025
 * 
 * This header file library contains all types, function declarations, and definitions,
 * for easy-to-use memory allocators in C/C++. .
 * 
 * To include function definitions, do this in one C/C++ file (and only in one translation unit):
 *  #define LS_MALLOCS_IMPLEMENTATION
 *  #include <ls_mallocs.h>
 * 
 * Documentation
 * 
 *  Compile-time Options
 * 
 *      #define LS_MALLOCS_NO_SHORT_NAMES
 *          All functions, types, and macros in this file have a prefix (ls_),
 *          this is to prevent name collisions. By default these prefixes are
 *          wrapped by a macro to remove the prefix. If you want to disable
 *          this freature (to handle name collisions), define LS_MALLOCS_NO_SHORT_NAMES.
 * 
 *      #define LS_CHUNK_MALLOC <1llu << POSITIVE_INTEGER>
 *          The chunk allocator, evident by the name, allocates in chunks.
 *          By default, the chunk sizes are set to 2^14 (16 MiB). If you
 *          want to change the chunk size (which I dont recommend), define
 *          LS_CHUNK_MALLOC with any positive integer that is an exponent
 *          of 2.
 * 
 *      *All definitions above must be defined globally during compile time.
 * 
 *  Virtual Memory Allocator
 * 
 *      void* ls_vmalloc() - vmalloc
 *          Reserves as many virtual addresses as _ls_memtotal() specifies.
 *          Returns NULL on failure. Max of [2^47/RAM_TOTAL] allocations at once.
 * 
 *      void ls_vfree(void* ptr) - vfree
 *          Decommits all committed pages and unreserves all virtual addresses
 *          of [ptr]. [ptr] must have been returned by [ls_vmalloc()].
 * 
 *      void ls_pfree(void* ptr) - pfree
 *          Decommmits all committed pages of [ptr].
 * 
 *      void ls_pfree_range(void* ptr, size_t offset, size_t range) - pfree_range
 *          Decommits pages past an offset and to a range.
 * 
 *      void ls_pcommit_range_win32(void* ptr, size_t offset, size_t range) - pcommit_range_win32
 *          Windows specific - Before writing to memory, you must specify where
 *          you plan on writing to.
 * 
 *      *Decommitting ranges smaller than the page size does nothing.
 *       OS specific functions can be called from Linux (preprocessed out).
 * 
 *  Chunk Memory Allocator
 */

#ifndef LS_MALLOCS_H
#define LS_MALLOCS_H

#include <stddef.h>

#ifndef LS_MALLOCS_NO_SHORT_NAMES
    // virtual memory allocator
    #define vmalloc             ls_vmalloc
    #define vfree               ls_vfree
    #define pfree               ls_pfree
    #define pfree_range         ls_pfree_range
    #define pcommit_range_win32 ls_pcommit_range_win32

    // chunk memory allocator
    #define chunk_allocator_t   ls_chunk_allocator_t
    #define cmalloc_t           ls_cmalloc_t
    #define new_chunk_allocator ls_new_chunk_allocator
    #define cmalloc             ls_cmalloc
    #define cmalloc_write       ls_cmalloc_write
#endif

#ifndef LS_CHUNK_SIZE
    #define LS_CHUNK_SIZE 0x4000  // 16 MiB
#endif

typedef char _ls_chunk[LS_CHUNK_SIZE];
typedef struct _ls_chunk_allocator_s ls_chunk_allocator_t;

// chunk memory allocator
typedef struct
{
    ls_chunk_allocator_t*   _chunk_allocator;  // the chunk allocator this cmalloc was made with
    size_t                  _id;               // index of the cmalloc to the chunk indices
    size_t                  _size;             // size (dynamic) of the allocation 
    char                    success;           // success code of [ls_cmalloc()]
}
ls_cmalloc_t;

struct _ls_chunk_allocator_s
{
    _ls_chunk*      _chunk_v;                  // vmalloc where chunks reside
    size_t          _chunk_c;                  // index of last committed chunk + 1
    size_t          _chunk_v_capacity;         // max amount of chunks
    size_t          _cmalloc_c;                // index of last registered cmalloc + 1
    ls_cmalloc_t    _chunk_indices_vmalloc_v;  // array of pointers to vmallocs (to store chunk indices)
    ls_cmalloc_t    _deleted_chunk_v;          // array of deleted chunk IDs
    ls_cmalloc_t    _deleted_cmalloc_v;        // array of deleted cmalloc IDs
    char            success;                   // success code of [ls_new_chunk_allocator()]
};

#ifdef __cplusplus
extern "C"
{
#endif

// virtual memory allocator
extern void* ls_vmalloc     ();
extern void  ls_vfree       (void* ptr);
extern void  ls_pfree       (void* ptr);
extern void  ls_pfree_range (void* ptr, size_t offset, size_t range);

// chunk memory allocator
extern ls_chunk_allocator_t ls_new_chunk_allocator  ();
extern ls_cmalloc_t         ls_cmalloc              (ls_chunk_allocator_t* chunk_allocator);
extern size_t               ls_cmalloc_write        (ls_cmalloc_t* cmalloc, void* src, size_t size, size_t dst);
extern size_t               _ls_get_free_chunk      (ls_chunk_allocator_t* chunk_allocator);

#ifdef _WIN32
    extern void ls_pcommit_range_win32(void* ptr, size_t offset, size_t range);
#else
    #define ls_pcommit_range_win32(...)
#endif

#ifdef __cplusplus
}
#endif

unsigned long _ls_page_size();
size_t        _ls_memtotal();

#endif  // #ifndef LS_MALLOCS_H

#ifdef LS_MALLOCS_IMPLEMENTATION

#ifdef _WIN32
    /*
     * Included because it contains methods used for
     * all virtualization in the Windows implementation
     */
    #include <windows.h>
#else
    /*
    * Included because it contains methods used for
    * all virtualization in the Linux implementation.
    */
    #include <sys/mman.h>

    /*
    * Included for the function sysconf, which is needed
    * retrieve the page sizes for this system.
    */
    #include <unistd.h>
#endif

#define _LS_MULT_TO(n, m) ((n) - ((n)%(m)))  // rounds down to nearest multiple of m, integers only

inline void* ls_vmalloc()
{
    #ifdef _WIN32
        return VirtualAlloc(NULL, _ls_memtotal(), MEM_RESERVE, PAGE_READWRITE);
    #else
        void* buf = mmap(NULL, _ls_memtotal(), PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        return (buf == MAP_FAILED) ? NULL : buf;
    #endif
}

inline void ls_vfree(void* ptr)
{
    #ifdef _WIN32
        VirtualFree(ptr, _ls_memtotal(), MEM_RELEASE);
    #else
        munmap(ptr, _ls_memtotal());
    #endif
}

inline void ls_pfree(void* ptr)
{
    #ifdef _WIN32
        VirtualFree(ptr, _ls_memtotal(), MEM_DECOMMIT);
    #else
        madvise(ptr, _ls_memtotal(), MADV_DONTNEED);
    #endif
}

inline void ls_pfree_range(void* ptr, size_t offset, size_t range)
{
    size_t page_size = _ls_page_size();

    offset = _LS_MULT_TO(offset, page_size) + page_size;
    range  = _LS_MULT_TO(range, page_size);

    if (!range || offset >= _ls_memtotal())
        return;  // nothing to do

    if (offset + range > _ls_memtotal())
        range = _ls_memtotal() - offset;

    #ifdef _WIN32
        VirtualFree((void*) (((size_t) ptr) + offset), range, MEM_DECOMMIT);
    #else
        madvise((void*) (((size_t) ptr) + offset), range, MADV_DONTNEED);
    #endif
}

#ifdef _WIN32
    inline void ls_pcommit_range_win32(void* ptr, size_t offset, size_t range)
    {
        size_t page_size = _ls_page_size();

        offset = _LS_MULT_TO(offset, page_size);
        range  = _LS_MULT_TO(range, page_size);

        if (!range || offset >= _ls_memtotal())
            return;  // nothing to do

        if (offset + range > _ls_memtotal())
            range = _ls_memtotal() - offset;
        
        VirtualAlloc((void*) (((size_t) ptr) + offset), range, MEM_COMMIT, PAGE_READWRITE);
    }
#endif

ls_chunk_allocator_t ls_new_chunk_allocator()
{
    size_t mem_total = _ls_memtotal();

    ls_chunk_allocator_t chunk_allocator =
    {
        ._chunk_v                 = ls_vmalloc(),
        ._chunk_c                 = 1,
        ._chunk_v_capacity        = mem_total / LS_CHUNK_SIZE,
        ._cmalloc_c               = 3,
        ._chunk_indices_vmalloc_v =
        {
            ._id   = 0,
            ._size = 8
        },
        ._deleted_chunk_v =
        {
            ._id   = 1,
            ._size = 0
        },
        ._deleted_cmalloc_v =
        {
            ._id   = 2,
            ._size = 0
        },
        .success = 1
    };

    if (!chunk_allocator._chunk_v)
        return chunk_allocator;
    
    void* first_chunk_indices_vmalloc = ls_vmalloc();
    if (!first_chunk_indices_vmalloc)
    {
        ls_vfree(chunk_allocator._chunk_v);
        return chunk_allocator;
    }

    chunk_allocator._chunk_indices_vmalloc_v._chunk_allocator = &chunk_allocator;
    chunk_allocator._deleted_chunk_v._chunk_allocator         = &chunk_allocator;
    chunk_allocator._deleted_cmalloc_v._chunk_allocator       = &chunk_allocator;
    
    ls_pcommit_range_win32(chunk_allocator._chunk_v, 0, LS_CHUNK_SIZE);
    ((void**) (chunk_allocator._chunk_v[0]))[0] = first_chunk_indices_vmalloc;
    
    ls_pcommit_range_win32(first_chunk_indices_vmalloc, 0, LS_CHUNK_SIZE);
    ((size_t*) first_chunk_indices_vmalloc)[0] = 0;

    chunk_allocator.success = 0;
    return chunk_allocator;
}

ls_cmalloc_t ls_cmalloc(ls_chunk_allocator_t* chunk_allocator)
{
    ls_cmalloc_t cmalloc = 
    {
        ._chunk_allocator   = chunk_allocator,
        ._id                = chunk_allocator->_cmalloc_c,
        ._size              = 0,
        .success            = 0
    };

    size_t mem_total = _ls_memtotal();
    size_t chunk_indices_per_vmalloc = mem_total / (mem_total / LS_CHUNK_SIZE * 8);

    // ran out of space for new chunk indices, vmalloc new chunk indices
    if (cmalloc._id / chunk_indices_per_vmalloc + 1 > chunk_allocator->_chunk_indices_vmalloc_v._size / 8)
    {
        void* new_chunk_indices = ls_vmalloc();
        if (!new_chunk_indices || !ls_cmalloc_write
            (
                &chunk_allocator->_chunk_indices_vmalloc_v,
                new_chunk_indices, sizeof(new_chunk_indices),
                chunk_allocator->_chunk_indices_vmalloc_v._size
            )
        )
            return cmalloc;
    }

    chunk_allocator->_cmalloc_c++;
    cmalloc.success = 1;
    return cmalloc;
}

size_t ls_cmalloc_write(ls_cmalloc_t* cmalloc, void* src, size_t size, size_t dst)
{
    if (dst + size > _ls_memtotal() || !size)
        return 0;

    if (dst + size > cmalloc->_size)
        cmalloc->_size = dst + size;
}

size_t _ls_get_free_chunk(ls_chunk_allocator_t* chunk_allocator)
{
    if (chunk_allocator->_deleted_chunk_v._size == 0)
        return chunk_allocator->_chunk_c;
    
    return 0;
}

inline unsigned long _ls_page_size()
{
    #ifdef _WIN32
        SYSTEM_INFO sysinfo;
        GetSystemInfo(&sysinfo);

        return sysinfo.dwPageSize;
    #else
        return sysconf(_SC_PAGESIZE);
    #endif
}

inline size_t _ls_memtotal()
{
    #ifdef _WIN32
        MEMORYSTATUS memstat;
        GlobalMemoryStatus(&memstat);

        return memstat.dwTotalPhys;
    #else
        FILE*  meminfo_f = fopen("/proc/meminfo", "rb");
        size_t memtotal;

        fscanf(meminfo_f, "MemTotal:%llu", &memtotal);

        fclose(meminfo_f);

        return memtotal * 1024;
    #endif
}

#endif  // #ifdef LS_MALLOCS_IMPLEMENTATION

/*
 * Copyright 2025, Logan Seeley
 *
 * Copying and distribution of this file, with or without modification,
 * are permitted in any medium without royalty, provided the copyright
 * notice and this notice are preserved. This file is offered as-is, 
 * without any warranty.
 */
