/*
 * ls_valloc.h - v1.1.0 - virtual memory allocator - Logan Seeley 2025
 *
 * Documentation
 *
 *  Functions
 * 
 *      ls_vptr_t ls_valloc_vmalloc(ls_u64_t *size) - vmalloc
 *          Reserves as many virtual addresses as _ls_memtotal() specifies.
 *          Returns LS_NULL on failure. Max of [2^47/_ls_memtotal()] allocations at once
 *          [size] out reference for virtual allocation size.
 * 
 *      void ls_valloc_vfree(ls_vptr_t ptr) - vfree
 *          Decommits all committed pages and unreserves all virtual addresses
 *          of [ptr].
 * 
 *      void ls_valloc_pfree(ls_vptr_t ptr) - pfree
 *          Decommmits all committed pages of [ptr].
 * 
 *      void ls_valloc_pfree_range(ls_vptr_t ptr, ls_u64_t offset, ls_u64_t range) - pfree_range
 *          Decommits pages past an offset and to a range.
 * 
 *      void ls_valloc_pcommit_range(ls_vptr_t ptr, ls_u64_t offset, ls_u64_t range) - pcommit_range_win32
 *          Before writing to memory, you must specify where
 *          you plan on writing to[1].
 *
 *      [^] [ptr] must have been returned by [ls_vmalloc()] for each function.
 *      
 *      [1] Note that this function only does anything on windows, use anyways (compatability).
 */


#ifndef LS_VALLOC_H
#define LS_VALLOC_H


#include <stddef.h>
#include <stdio.h>


#ifdef _WIN32
    #include <windows.h>
#else
    #include <sys/mman.h>
    #include <unistd.h>
#endif


#ifndef _LS_TYPES_INCLUDED
#define _LS_TYPES_INCLUDED

	#include <stdint.h>	

	typedef uint8_t		ls_bool_t;

	typedef uint8_t		ls_u8_t;
	typedef int8_t		ls_i8_t;
	typedef uint16_t	ls_u16_t;
	typedef int16_t		ls_i16_t;
	typedef uint32_t	ls_u32_t;
	typedef int32_t		ls_i32_t;
	typedef uint64_t	ls_u64_t;
	typedef int64_t		ls_i64_t;

	typedef float       ls_f32_t;
	typedef double      ls_f64_t;

	typedef void 	 *  ls_vptr_t;
	typedef ls_u64_t *	ls_ptr_t;

	typedef ls_u32_t	ls_result_t;

	#define LS_NULL		0

#endif


#ifndef LS_VALLOC_NO_SHORT_NAMES
    
    #define vmalloc             ls_valloc_vmalloc
    #define vfree               ls_valloc_vfree
    #define pfree               ls_valloc_pfree
    #define pfree_range         ls_valloc_pfree_range
    #define pcommit_range       ls_valloc_pcommit_range

#endif


#ifndef _LS_CAST
	#define _LS_CAST(v, t) ((t) (v))
#endif

#ifndef _LS_MULT_TO
    #define _LS_MULT_TO(n, m) ((n) - ((n)%(m)))  // rounds n down to nearest multiple of m, integers only
#endif


ls_vptr_t   ls_valloc_vmalloc    (ls_ptr_t size);
void        ls_valloc_vfree      (ls_vptr_t ptr);
void        ls_valloc_pfree      (ls_vptr_t ptr);
void        ls_valloc_pfree_range(ls_vptr_t ptr, ls_u64_t offset, ls_u64_t range);

#ifdef _WIN32
    void _ls_valloc_pcommit_range_win32(ls_vptr_t ptr, ls_u64_t offset, ls_u64_t range);

    #define ls_valloc_pcommit_range(...) _ls_valloc_pcommit_range_win32(__VA_ARGS__)
#else
    #define _LS_VALLOC_MADV_DONTNEED 4

    static long _ls_valloc_madvise(void *addr, size_t length, int advice);

    #define ls_valloc_pcommit_range(...)  // still good practice to call this
#endif

ls_u64_t _ls_valloc_page_size();
ls_u64_t _ls_valloc_memtotal();


inline ls_vptr_t ls_valloc_vmalloc(ls_ptr_t size)
{
    *size = _ls_valloc_memtotal();

    #ifdef _WIN32
        return VirtualAlloc(LS_NULL, *size, MEM_RESERVE, PAGE_READWRITE);
    #else
        ls_vptr_t buf = mmap(LS_NULL, *size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        return (buf == MAP_FAILED) ? LS_NULL : buf;
    #endif
}


inline void ls_valloc_vfree(ls_vptr_t ptr)
{
    #ifdef _WIN32
        VirtualFree(ptr, _ls_valloc_memtotal(), MEM_RELEASE);
    #else
        munmap(ptr, _ls_valloc_memtotal());
    #endif
}


inline void ls_valloc_pfree(ls_vptr_t ptr)
{
    #ifdef _WIN32
        VirtualFree(ptr, _ls_valloc_memtotal(), MEM_DECOMMIT);
    #else
        _ls_valloc_madvise(ptr, _ls_valloc_memtotal(), _LS_VALLOC_MADV_DONTNEED);
    #endif
}


inline void ls_valloc_pfree_range(ls_vptr_t ptr, ls_u64_t offset, ls_u64_t range)
{
    ls_u64_t page_size = _ls_valloc_page_size();

    offset = _LS_MULT_TO(offset, page_size) + page_size;
    range  = _LS_MULT_TO(range, page_size);
    
    if (!range || offset >= _ls_valloc_memtotal())
        return;  // nothing to do
    
    if (offset + range > _ls_valloc_memtotal())
        range = _ls_valloc_memtotal() - offset;
    
    #ifdef _WIN32
        VirtualFree(_LS_CAST(_LS_CAST(ptr, ls_u64_t) + offset, ls_vptr_t), range, MEM_DECOMMIT);
    #else
        _ls_valloc_madvise(_LS_CAST(_LS_CAST(ptr, ls_u64_t) + offset, ls_vptr_t), range, _LS_VALLOC_MADV_DONTNEED);
    #endif
}


#ifdef _WIN32

inline void _ls_valloc_pcommit_range_win32(ls_vptr_t ptr, ls_u64_t offset, ls_u64_t range)
{
    ls_u64_t page_size = _ls_valloc_page_size();
    
    offset = _LS_MULT_TO(offset, page_size);
    range  = _LS_MULT_TO(range, page_size);
    
    if (!range || offset >= _ls_valloc_memtotal())
        return;  // nothing to do
    
    if (offset + range > _ls_valloc_memtotal())
        range = _ls_valloc_memtotal() - offset;
    
    VirtualAlloc(_LS_CAST(_LS_CAST(ptr, ls_u64_t) + offset, ls_vptr_t), range, MEM_COMMIT, PAGE_READWRITE);
}
#else

static inline long _ls_valloc_madvise(void *addr, size_t length, int advice)
{
    long ret;

#if defined(__x86_64__)

    const long syscall_nr = 28;

    __asm__ volatile (
        "syscall"
        : "=a" (ret)
        : "a"(syscall_nr),
          "D"(addr),
          "S"(length),
          "d"(advice)
        : "rcx", "r11", "memory"
    );

#elif defined(__i386__)

    const long syscall_nr = 219;

    __asm__ volatile (
        "int $0x80"
        : "=a" (ret)
        : "a"(syscall_nr),
          "b"(addr),
          "c"(length),
          "d"(advice)
        : "memory"
    );

#elif defined(__aarch64__)

    register long x0 __asm__("x0") = (long)addr;
    register long x1 __asm__("x1") = (long)length;
    register long x2 __asm__("x2") = (long)advice;
    register long x8 __asm__("x8") = 233;

    __asm__ volatile (
        "svc #0"
        : "=r" (x0)
        : "r" (x0), "r" (x1), "r" (x2), "r" (x8)
        : "memory"
    );

    ret = x0;

#elif defined(__arm__)

    register long r0 __asm__("r0") = (long)addr;
    register long r1 __asm__("r1") = (long)length;
    register long r2 __asm__("r2") = (long)advice;
    register long r7 __asm__("r7") = 192;

    __asm__ volatile (
        "swi 0"
        : "=r"(r0)
        : "r"(r0), "r"(r1), "r"(r2), "r"(r7)
        : "memory"
    );

    ret = r0;

#else
    #error "ls_valloc.h does not support this architecture"
#endif

    return ret;
}

#endif


inline ls_u64_t _ls_valloc_page_size()
{
    #ifdef _WIN32
        SYSTEM_INFO sysinfo;
        GetSystemInfo(&sysinfo);
    
        return sysinfo.dwPageSize;
    #else
        return sysconf(_SC_PAGESIZE);
    #endif
}


inline ls_u64_t _ls_valloc_memtotal()
{
    #ifdef _WIN32
        MEMORYSTATUS memstat;
        GlobalMemoryStatus(&memstat);
    
        return memstat.dwTotalPhys;
    #else
        FILE*  meminfo_f = fopen("/proc/meminfo", "rb");
        ls_u64_t memtotal;
    
        fscanf(meminfo_f, "MemTotal:%zu", &memtotal);
    
        fclose(meminfo_f);
    
        return memtotal * 1024;
    #endif
}


#endif  // #ifdef LS_VALLOC_H


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
