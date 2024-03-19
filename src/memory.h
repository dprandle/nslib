#pragma once

#include <utility>

#include "basic_types.h"
#include "containers/linked_list.h"

namespace nslib
{

static constexpr inline const sizet DEFAULT_MIN_ALIGNMENT = 8;
static constexpr inline const sizet SIMD_MIN_ALIGNMENT = 16;

enum struct mem_alloc_type
{
    FREE_LIST,
    POOL,
    STACK,
    LINEAR
};

struct free_header
{
    sizet block_size{};
};

struct alloc_header
{
    sizet block_size;
    sizet algn_padding;
};

struct stack_alloc_header
{
    sizet padding;
};

enum placement_policy
{
    FIND_FIRST,
    FIND_BEST
};

using mem_node = ll_node<free_header>;

struct mem_free_list
{
    placement_policy p_policy;
    singly_linked_list<free_header> free_list;
};

struct mem_pool
{
    sizet chunk_size;
    singly_linked_list<free_header> free_list;
};

struct mem_stack
{
    sizet offset;
};

struct mem_linear
{
    sizet offset;
};

struct mem_arena
{
    /// Input parameter for alloc functions
    sizet total_size;

    /// Input parameter for which type of allocator we want
    mem_alloc_type alloc_type;

    /// If null, the allocator memory pool will be allocated with platform_alloc, otherwise the
    /// upstream_allocator's alloc will be used with ns_alloc (same is true for free). Once an allocator gets its memory,
    /// don't change this to something different as it will likely crash (cant free from an allocator different than allocated from)
    mem_arena * upstream_allocator {nullptr};

    sizet used{0};
    sizet peak{0};
    void *start{nullptr};
    union
    {
        mem_free_list mfl;
        mem_pool mpool;
        mem_stack mstack;
        mem_linear mlin;
    };
};

void *mem_alloc(sizet bytes, mem_arena *arena, sizet alignment);

void *mem_alloc(sizet bytes, mem_arena *arena);

void *mem_alloc(sizet bytes);

void *mem_realloc(void *ptr, sizet size, mem_arena *arena, sizet alignment, bool free_ptr_after_cpy);

void *mem_realloc(void *ptr, sizet size, mem_arena *arena, sizet alignment);

void *mem_realloc(void *ptr, sizet size, mem_arena *arena);

void *mem_realloc(void *ptr, sizet size);

// The size of the allocated block including padding and header
sizet mem_block_size(void *ptr, mem_arena *arena);

// The size of the allocated block the user requested and got back
sizet mem_block_user_size(void *ptr, mem_arena *arena);

template<class T>
T * mem_alloc(mem_arena *arena, sizet alignment=8)
{
    return (T*)mem_alloc(sizeof(T), arena, alignment);
}

template<class T, class... Args>
T * mem_new(mem_arena *arena, sizet alignment, Args&&... args)
{
    T * item = mem_alloc<T>(arena, alignment);
    new (item) T(std::forward<Args>(args)...);
    return item;
}

template<class T, class... Args>
T * mem_new(mem_arena *arena, Args&&... args)
{
    T * item = mem_alloc<T>(arena);
    new (item) T(std::forward<Args>(args)...);
    return item;
}

void mem_free(void *item, mem_arena *arena);

void mem_free(void *item);

template<class T>
void mem_delete(T *item, mem_arena *arena)
{
    item->~T();
    mem_free(arena, item);
}

// Reset the store without actually freeing the memory so it can be reused
void mem_reset_arena(mem_arena *arena);
void mem_init_arena(sizet total_size, mem_alloc_type atype, mem_arena *arena);
void mem_terminate_arena(mem_arena *arena);
const char *mem_arena_type_str(mem_alloc_type atype);

mem_arena *mem_global_arena();

// This must be a free list arena
void mem_set_global_arena(mem_arena *arena);

mem_arena *mem_global_stack_arena();
void mem_set_global_stack_arena(mem_arena *arena);

mem_arena *mem_global_frame_lin_arena();
void mem_set_global_frame_lin_arena(mem_arena *arena);

} // namespace nslib
