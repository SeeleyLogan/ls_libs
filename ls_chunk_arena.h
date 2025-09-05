/*
 * ls_chunk_arena.h - v1.0.0 - chunk arena allocator - Logan Seeley 2025
 *
 * Documentation
 *
 *	Behaviour & Safety
 *
 *		This arena allocator expects memory to be provided to it.
 *		The lifetime of any [ls_chunk_arena_t] must be less than
 *		that of the memory provided to it.
 *
 *		The arena does not free deleted chunks; it tries to reuse
 *		them. Meaning this allocator does not work well with
 *		programs that fluctuate greatly with memory usage.
 *
 *	Usage
 *
 *		Memory allocators compatible with this arena allocator
 *		likely require memory to be committed before used.
 *		Define [_ls_chunk_arena_alloca_commit_range] to the
 *		commit function provided by your allocator **before**
 *		including this file.
 *
 *	Functions
 *
 * 		ls_chunk_arena_t ls_chunk_arena_init(ls_vptr_t memory, ls_u64_t memory_size, ls_u64_t chunk_size) - arena_init
 *			[memory] must be aligned to [chunk_size]
 *			and by divisible by such. [chunk_size]
 *			and [memory] must be a power of 2.
 *
 *		ls_vptr_t ls_chunk_arena_get_chunk(ls_chunk_arena_t *chunk_arena, ls_u32_t *status) - arena_get_chunk
 *			[status] is out
 *			[*status] = LS_CHUNK_ARENA_SUCCESS
 *			[*status] = LS_CHUNK_ARENA_MEM_FULL -> could not fetch chunk: none left. [return] will also be LS_NULL
 *
 *		void ls_chunk_arena_delete_chunk(ls_chunk_arena_t *chunk_arena, ls_vptr_t chunk_ptr) - arena_delete_chunk
 *			[chunk_ptr] must have been returned by [ls_chunk_arena_get_chunk]
 */


#ifndef LS_CHUNK_ARENA_H
#define LS_CHUNK_ARENA_H


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


#ifndef LS_CHUNK_ARENA_NO_SHORT_NAMES
    
	#define arena_t    	 	   	ls_chunk_arena_t
    #define arena_init		 	ls_chunk_arena_init
	#define arena_get_chunk    	ls_chunk_arena_get_chunk
	#define arena_delete_chunk	ls_chunk_arena_delete_chunk

#endif


#define _LS_CHUNK_ARENA_RESULT_SIGNATURE(tag) (0x43484100 | tag)

#define LS_CHUNK_ARENA_SUCCESS  0
#define LS_CHUNK_ARENA_MEM_FULL	_LS_CHUNK_ARENA_RESULT_SIGNATURE(2)

// IMPORTANT: arena allocator expects user to provide memory
// meaning it requires a function to handle said memory
// your allocator may provide it for you, simply wrap it in a macro
// if the chunk allocator's memory is physical and continuous: these can be empty defines
#ifndef _ls_chunk_arena_alloca_commit_range

    #error "chunk arena is missing allocator binding"

#endif


#ifndef _LS_MULT_TO
    #define _LS_MULT_TO(n, m) ((n) - ((n)%(m)))  // rounds n down to nearest multiple of m, integers only
#endif

#define _LS_CHUNK_ARENA_INDEX_TO_PTR(chunk_arena, index) ((ls_vptr_t) ((index) * chunk_arena->_chunk_size + chunk_arena->_memory))


typedef struct ls_chunk_arena_s
{
    ls_vptr_t	_memory;
    ls_u64_t	_max_chunk_c;
	ls_u64_t	_chunk_size;
	ls_u64_t	_chunk_c;
	ls_u64_t	_next_committed_chunk;
	ls_u64_t	_last_deleted_chunk;
}
ls_chunk_arena_t;


ls_chunk_arena_t 	ls_chunk_arena_init							(ls_vptr_t 		   	 memory, 		ls_u64_t 		memory_size, 	ls_u64_t 	chunk_size);
ls_vptr_t 			ls_chunk_arena_get_chunk					(ls_chunk_arena_t   *chunk_arena, 	ls_u32_t   *status);
ls_vptr_t 			_ls_chunk_arena_revive_last_deleted_chunk	(ls_chunk_arena_t 	*chunk_arena);
void				ls_chunk_arena_delete_chunk					(ls_chunk_arena_t 	*chunk_arena, 	ls_vptr_t 	chunk_ptr);


inline ls_chunk_arena_t ls_chunk_arena_init(ls_vptr_t memory, ls_u64_t memory_size, ls_u64_t chunk_size)
{
    ls_chunk_arena_t chunk_arena = 
    {
        ._memory        		= memory,
        ._max_chunk_c   		= memory_size >> __builtin_ctzll(chunk_size),
		._chunk_size			= chunk_size,
		._chunk_c				= 0,
		._next_committed_chunk 	= 1,
		._last_deleted_chunk	= 0
    };

    return chunk_arena;
}


inline ls_vptr_t ls_chunk_arena_get_chunk(ls_chunk_arena_t *chunk_arena, ls_u32_t *status)
{
	if (chunk_arena->_max_chunk_c == chunk_arena->_chunk_c)
	{
		*status = LS_CHUNK_ARENA_MEM_FULL;
		return LS_NULL;
	}
	else
		*status = LS_CHUNK_ARENA_SUCCESS;

	chunk_arena->_chunk_c++;
	
	if (!chunk_arena->_last_deleted_chunk)
	{
		ls_vptr_t chunk_ptr = _LS_CHUNK_ARENA_INDEX_TO_PTR(chunk_arena, chunk_arena->_next_committed_chunk - 1);
		_ls_chunk_arena_alloca_commit_range(chunk_arena->_memory, chunk_ptr, chunk_arena->_chunk_size);

		chunk_arena->_next_committed_chunk++;

		return chunk_ptr;
	}
	else
		return _ls_chunk_arena_revive_last_deleted_chunk(chunk_arena);
}

inline ls_vptr_t _ls_chunk_arena_revive_last_deleted_chunk(ls_chunk_arena_t *chunk_arena)
{
	ls_ptr_t deleted_chunk = (ls_ptr_t) _LS_CHUNK_ARENA_INDEX_TO_PTR(chunk_arena, chunk_arena->_last_deleted_chunk - 1);

	chunk_arena->_last_deleted_chunk = deleted_chunk[0];

	return deleted_chunk;
}


inline void ls_chunk_arena_delete_chunk(ls_chunk_arena_t *chunk_arena, ls_vptr_t chunk_ptr)
{
	chunk_ptr = (ls_vptr_t) _LS_MULT_TO((ls_u64_t) chunk_ptr, chunk_arena->_chunk_size);

	ls_u64_t chunk_i = ((ls_u64_t) (chunk_ptr - chunk_arena->_memory)) >> __builtin_ctzll(chunk_arena->_chunk_size);

	((ls_ptr_t) chunk_ptr)[0] = chunk_arena->_last_deleted_chunk;

	chunk_arena->_last_deleted_chunk = chunk_i + 1;
	chunk_arena->_chunk_c--;
}


#endif  // #ifndef LS_CHUNK_ARENA_H


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
