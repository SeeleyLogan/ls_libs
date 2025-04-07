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
    #define cmalloc_t               ls_cmalloc_t
    #define cmalloc                 ls_cmalloc
    #define cmalloc_write           ls_cmalloc_write
    #define cmalloc_index_to_addr   ls_cmalloc_index_to_addr
    #define cmalloc_set_size        ls_cmalloc_set_size
#endif

#ifndef LS_CHUNK_SIZE
#define LS_CHUNK_SIZE 0x4000  // 16 MiB
#endif

typedef char _ls_chunk[LS_CHUNK_SIZE];

// chunk memory allocator
typedef struct
{
    size_t*                 _chunk_indices_vmalloc;  // address of the chunk indices' vmalloc
    size_t                  _id;                     // index of the cmalloc to the chunk indices
    size_t                  _size;                   // size (dynamic) of the allocation in bytes
    char                    success;                 // success code of [ls_cmalloc()]
}
ls_cmalloc_t;

typedef struct
{
    _ls_chunk*      _chunk_v;                  // vmalloc where chunks reside
    size_t          _chunk_c;                  // index of last committed chunk + 1
    size_t          _chunk_v_capacity;         // max amount of chunks
    size_t          _cmalloc_c;                // index of last registered cmalloc + 1
    ls_cmalloc_t    _chunk_indices_vmalloc_v;  // array of pointers to vmallocs (to store chunk indices)
    ls_cmalloc_t    _deleted_chunk_v;          // array of deleted chunk IDs
    ls_cmalloc_t    _deleted_cmalloc_v;        // array of deleted cmalloc IDs
}
_ls_chunk_allocator_t;

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
extern ls_cmalloc_t ls_cmalloc              ();
extern void*        ls_cmalloc_index_to_addr(ls_cmalloc_t* cmalloc, size_t i);
extern size_t       ls_cmalloc_write        (ls_cmalloc_t* cmalloc, size_t dst_i, void* src, size_t size);
extern char         _ls_cmalloc_grow        (ls_cmalloc_t* cmalloc, size_t new_size);
extern char         _ls_cmalloc_shrink      (ls_cmalloc_t* cmalloc, size_t new_size);

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

char _ls_init_chunk_allocator();  // returns success or fail

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

char _ls_chunk_allocator_initialized = 0;
_ls_chunk_allocator_t _chunk_allocator;

char _ls_init_chunk_allocator()
{
    if (_ls_chunk_allocator_initialized)
        return 1;
    
    size_t mem_total = _ls_memtotal();
    
    _chunk_allocator = (_ls_chunk_allocator_t)
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
        }
    };
    
    if (!_chunk_allocator._chunk_v)
        return 0;
    
    void* first_chunk_indices_vmalloc = ls_vmalloc();
    if (!first_chunk_indices_vmalloc)
    {
        ls_vfree(_chunk_allocator._chunk_v);
        return 0;
    }
    
    ls_pcommit_range_win32(_chunk_allocator._chunk_v, 0, LS_CHUNK_SIZE);
    ((void**) (_chunk_allocator._chunk_v[0]))[0] = first_chunk_indices_vmalloc;
    
    ls_pcommit_range_win32(first_chunk_indices_vmalloc, 0, LS_CHUNK_SIZE);
    ((size_t*) first_chunk_indices_vmalloc)[0] = 0;
    
    _chunk_allocator._chunk_indices_vmalloc_v._chunk_indices_vmalloc = first_chunk_indices_vmalloc;
    _chunk_allocator._deleted_chunk_v        ._chunk_indices_vmalloc = first_chunk_indices_vmalloc;
    _chunk_allocator._deleted_cmalloc_v      ._chunk_indices_vmalloc = first_chunk_indices_vmalloc;
    
    _ls_chunk_allocator_initialized = 1;
    return 1;
}

#define ls_cmalloc_set_size(cmalloc, size) ((size > cmalloc->_size) ? _ls_cmalloc_grow(cmalloc, size) : _ls_cmalloc_shrink(cmalloc, size))

ls_cmalloc_t ls_cmalloc()
{
    ls_cmalloc_t cm =
    {
        ._size   = 0,
        .success = 0
    };

    if (!_ls_chunk_allocator_initialized && !_ls_init_chunk_allocator())
        return cm;

    if (_chunk_allocator._deleted_cmalloc_v._size > 0)
    {
        cm._id = *(size_t*) ls_cmalloc_index_to_addr(&_chunk_allocator._deleted_cmalloc_v, _chunk_allocator._deleted_cmalloc_v._size);
        _chunk_allocator._deleted_cmalloc_v._size -= 8;  // eventually replace with shrink
    }
    else
    {
        cm._id = _chunk_allocator._cmalloc_c;
        _chunk_allocator._cmalloc_c++;
    }

    if (cm._id / (LS_CHUNK_SIZE >> 3) > _chunk_allocator._chunk_indices_vmalloc_v._size / 8)
    {
        void* new_chunk_indices_vmalloc = ls_vmalloc();
        if (!new_chunk_indices_vmalloc)
            return cm;

        ls_cmalloc_write(&_chunk_allocator._chunk_indices_vmalloc_v, _chunk_allocator._chunk_indices_vmalloc_v._size, new_chunk_indices_vmalloc, sizeof(void*));
    }

    cm.success = 1;
    return cm;
}

// returns amount of bytes written
inline size_t ls_cmalloc_write(ls_cmalloc_t* cmalloc, size_t dst_i, void* src, size_t size)
{
    if (dst_i > cmalloc->_size)
    {
        size  = dst_i - cmalloc->_size;
        dst_i = cmalloc->_size;
    }

    if (dst_i + size > cmalloc->_size && !ls_cmalloc_set_size(cmalloc, dst_i + size))
        return 0;
    
    return 1;
}

inline void* ls_cmalloc_index_to_addr(ls_cmalloc_t* cmalloc, size_t i)
{
    size_t chunk_indices_i = cmalloc->_id - _LS_MULT_TO(cmalloc->_id, LS_CHUNK_SIZE >> 3);
    size_t chunk_i         = i / LS_CHUNK_SIZE;
    size_t byte_i          = i % LS_CHUNK_SIZE;

    void*  chunk_indices = (void*) (((size_t) cmalloc->_chunk_indices_vmalloc) + chunk_indices_i * (LS_CHUNK_SIZE >> 3));
    size_t chunk_id      = ((size_t*) chunk_indices)[chunk_i];

    return &_chunk_allocator._chunk_v[chunk_id];
}

inline char _ls_cmalloc_grow(ls_cmalloc_t* cmalloc, size_t new_size)
{
    size_t chunks_needed = (_LS_MULT_TO(new_size, LS_CHUNK_SIZE) - _LS_MULT_TO(cmalloc->_size, LS_CHUNK_SIZE)) / LS_CHUNK_SIZE;

    printf("%llu\n", chunks_needed);

    return 0;
}

inline char _ls_cmalloc_shrink(ls_cmalloc_t* cmalloc, size_t new_size)
{
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
