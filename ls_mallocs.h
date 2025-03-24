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
 *      #define LS_VADDR_RESERVE_SIZE 0x100000000
 *          [ls_vmalloc()], by default, reserves 4 GiB of virtual address space.
 *          Unfortunately, this means you run out of virtual address space once
 *          2^15 allocations exist at once. The formula for calculating how many
 *          allocations can exist is: 2**47 / LS_VADDR_RESERVE_SIZE. Note that
 *          LS_VADDR_RESERVE_SIZE has to be a multiple of the page size. Define
 *          LS_VADDR_RESERVE_SIZE to any size you want.
 *
 *      *All definitions above must be defined globally during compile time.
 *
 *  Virtual Memory Allocator
 *
 *      void* ls_vmalloc() - vmalloc
 *          Reserves as many virtual addresses as LS_VADDR_RESERVE_SIZE specifies.
 *          Returns NULL on failure.
 *
 *      void ls_vfree(void* ptr) - vfree
 *          Decommits all committed pages and unreserves all virtual addresses
 *          of [ptr]. [ptr] must have been returned by [ls_vmalloc()].
 *
 *      void ls_pfree(void* ptr) - pfree
 *          Decommmits all committed pages of [ptr].
 *
 *      void ls_pshrink(void* ptr, size_t using) - pshrink
 *          Decommits committed pages past an offset ([using]), but keeps reserved
 *          virtual addresses. [using] is the offset of the last byte from [ptr]
 *          that you are still using.
 *
 *      void ls_pfree_range(void* ptr, size_t offset, size_t range) - pfree_range
 *          Decommits committed pages past an offset and to a range, but keeps
 *          reserved virtual addresses. [offset] is the offset of the first byte from
 *          [ptr] that you are no longer using, and [range] is the offset of the last
 *          byte from [offset] that you are no longer using.
 */

#ifndef LS_MALLOCS_H
#define LS_MALLOCS_H

#include <stddef.h>

#ifndef LS_MALLOCS_NO_SHORT_NAMES
    #define vmalloc     ls_vmalloc
    #define vfree       ls_vfree
    #define pfree       ls_pfree
    #define pshrink     ls_pshrink
    #define pfree_range ls_pfree_range
#endif

#ifdef __cplusplus
extern "C"
{
#endif

extern void* ls_vmalloc();
extern void  ls_vfree(void* ptr);
extern void  ls_pfree(void* ptr);
extern void  ls_pshrink(void* ptr, size_t using);
extern void  ls_pfree_range(void* ptr, size_t offset, size_t range);

#ifdef __cplusplus
}
#endif

#ifdef LS_MALLOCS_IMPLEMENTATION

#ifndef LS_VADDR_RESERVE_SIZE
    #define LS_VADDR_RESERVE_SIZE 0x100000000  // 1 << 32
#endif

#ifdef _WIN32

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

#define _LS_MULT_TO(n, m) ((n) - ((n)%(m)))  // rounds down to nearest multiple of m, works only on integers

inline void* ls_vmalloc()
{
    #ifdef _WIN32

    #else
        void* buf = mmap(NULL, LS_VADDR_RESERVE_SIZE, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        return (buf == MAP_FAILED) ? NULL : buf;
    #endif
}

inline void ls_vfree(void* ptr)
{
    #ifdef _WIN32

    #else
        munmap(ptr, LS_VADDR_RESERVE_SIZE);
    #endif
}

inline void ls_pfree(void* ptr)
{
    madvise(ptr, LS_VADDR_RESERVE_SIZE, MADV_DONTNEED);
}

inline void ls_pshrink(void* ptr, size_t using)
{
    #ifdef _WIN32

    #else
        size_t page_size = sysconf(_SC_PAGESIZE);
        if (!using)
            madvise(ptr, LS_VADDR_RESERVE_SIZE, MADV_DONTNEED);  // use pfree instead
        else
            madvise(ptr + _LS_MULT_TO(using, page_size) + page_size,  LS_VADDR_RESERVE_SIZE - _LS_MULT_TO(using, page_size) - page_size, MADV_DONTNEED);
    #endif
}

inline void ls_pfree_range(void* ptr, size_t offset, size_t range)
{
    #ifdef _WIN32

    #else
        size_t page_size = sysconf(_SC_PAGESIZE);
        offset = _LS_MULT_TO(offset, page_size) + page_size;
        range  = _LS_MULT_TO(range, page_size);

        if (!range || offset >= LS_VADDR_RESERVE_SIZE)
            return;  // nothing to do

        if (offset + range > LS_VADDR_RESERVE_SIZE)
            range = LS_VADDR_RESERVE_SIZE - offset;

        madvise((void*) (((size_t) ptr) + offset), range, MADV_DONTNEED);
    #endif
}

#endif  // #ifdef LS_MALLOCS_IMPLEMENTATION
#endif  // #ifndef LS_MALLOCS_H

/*
 * Copyright 2025, Logan Seeley
 *
 * Copying and distribution of this file, with or without modification,
 * are permitted in any medium without royalty, provided the copyright
 * notice and this notice are preserved. This file is offered as-is, 
 * without any warranty.
 */
