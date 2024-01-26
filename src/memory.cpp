#include <stdlib.h>
#include <cstring>
#include <numeric>

#include "logging.h"
#include "platform.h"
#include "memory.h"
#include "math/algorithm.h"

#include <iostream>

#define DO_DEBUG_PRINT false
#define DO_DEBUG_LINEAR_ALLOC false

namespace nslib
{

intern mem_arena *g_fl_arena{};
intern mem_arena *g_stack_arena{};
intern mem_arena *g_frame_linear_arena{};

intern sizet calc_padding(sizet base_addr, sizet alignment)
{
    sizet multiplier = (base_addr / alignment) + 1;
    sizet aligned_addr = multiplier * alignment;
    sizet padding = aligned_addr - base_addr;
    return padding;
}

intern sizet calc_padding_with_header(sizet base_addr, sizet alignment, sizet header_size)
{
    sizet padding = calc_padding(base_addr, alignment);
    sizet needed_space = header_size;

    if (padding < needed_space) {
        // Header does not fit - Calculate next aligned address that header fits
        needed_space -= padding;

        // How many alignments I need to fit the header
        if (needed_space % alignment > 0) {
            padding += alignment * (1 + (needed_space / alignment));
        }
        else {
            padding += alignment * (needed_space / alignment);
        }
    }

    return padding;
}

intern void find_first(mem_free_list *mfl, sizet size, sizet alignment, sizet *padding, mem_node **prev_node, mem_node **found_node)
{
    // Iterate list and return the first free block with a size >= than given size
    mem_node *it = mfl->free_list.head, *it_prev = nullptr;

    while (it != nullptr) {
        *padding = calc_padding_with_header((sizet)it, alignment, sizeof(alloc_header));
        sizet required_space = size + *padding;
        if (it->data.block_size >= required_space) {
            break;
        }
        it_prev = it;
        it = it->next;
    }
    *prev_node = it_prev;
    *found_node = it;
}

intern void find_best(mem_free_list *mfl, sizet size, sizet alignment, sizet *padding, mem_node **prev_node, mem_node **found_node)
{
    // Iterate WHOLE list keeping a pointer to the best fit
    sizet smallest_diff = std::numeric_limits<sizet>::max();
    mem_node *best_block = nullptr;
    mem_node *it = mfl->free_list.head, *it_prev = nullptr;
    while (it != nullptr) {
        *padding = calc_padding_with_header((sizet)it, alignment, sizeof(alloc_header));
        sizet required_space = size + *padding;
        if (it->data.block_size >= required_space && (it->data.block_size - required_space < smallest_diff)) {
            best_block = it;
        }
        it_prev = it;
        it = it->next;
    }
    *prev_node = it_prev;
    *found_node = best_block;
}

intern void find(mem_free_list *mfl, sizet size, sizet alignment, sizet *padding, mem_node **prev_node, mem_node **found_node)
{
    switch (mfl->p_policy) {
    case FIND_FIRST:
        find_first(mfl, size, alignment, padding, prev_node, found_node);
        break;
    case FIND_BEST:
        find_best(mfl, size, alignment, padding, prev_node, found_node);
        break;
    }
}

intern void coalescence(mem_free_list *mfl, mem_node *prev_node, mem_node *free_node)
{
    if (free_node->next != nullptr && (sizet)free_node + free_node->data.block_size == (sizet)free_node->next) {
        free_node->data.block_size += free_node->next->data.block_size;
        ll_remove(&mfl->free_list, free_node, free_node->next);
    }

    if (prev_node != nullptr && (sizet)prev_node + prev_node->data.block_size == (sizet)free_node) {
        prev_node->data.block_size += free_node->data.block_size;
        ll_remove(&mfl->free_list, prev_node, free_node);
    }
}

intern void *mem_free_list_alloc(mem_arena *arena, sizet size, sizet alignment_p)
{
    sizet alloc_header_size = sizeof(alloc_header);
    if (size < sizeof(mem_node)) {
        size = sizeof(mem_node);
    }
    sizet alignment(alignment_p);
    if (alignment < 8) {
        alignment = 8;
    }

    // Padding is the amount of padding we need considering the passed in alignment and the size of our header address
    sizet padding{};

    // Search through the free list for a free block that has enough space to allocate our data
    mem_node *affected_node{}, *prev_node{};
    find(&arena->mfl, size, alignment, &padding, &prev_node, &affected_node);
    assert(affected_node && "Not enough memory");

    // The total required size for this block (including header and alignment paddnig which are both included in padding)
    sizet required_size = size + padding;

    // The amount of alignment needed for the returned data address to be aligned
    sizet alignment_padding = padding - alloc_header_size;

    // Now subtract our required block size from the chosen node block size - this is the remaining of the chunk that we
    // don't need
    sizet rest = affected_node->data.block_size - required_size;

    // If the remainder is less than the header size, it is too small to be used as another block.. we need to add it to
    // our required size or else
    if (rest > (sizeof(alloc_header))) {
        // We have to split the block into the data block and a free block of size 'rest'
        mem_node *new_free_node = (mem_node *)((sizet)affected_node + required_size);
        new_free_node->data.block_size = rest;
        ll_insert(&arena->mfl.free_list, affected_node, new_free_node);
    }
    else {
        required_size += rest;
        rest = 0;
    }

    ll_remove(&arena->mfl.free_list, prev_node, affected_node);

    //////////////////////////////////////////////////////////////////////////////////////////
    // The affected node is at the start of the block, and the block as allocated like this //
    // Header | Alignment Padding | Aligned Base Address                                    //
    //////////////////////////////////////////////////////////////////////////////////////////
    sizet header_addr = (sizet)affected_node + alignment_padding;
    sizet aligned_data_addr = (sizet)affected_node + padding;
    ((alloc_header *)header_addr)->block_size = required_size;
    ((alloc_header *)header_addr)->algn_padding = alignment_padding;

    arena->used += required_size;
    arena->peak = std::max(arena->peak, arena->used);

#if DO_DEBUG_PRINT
    dlog("Blck:%p Hdr:%p Dptr:%p RqstS:%lu RqrdS:%lu BlkSz:%lu AlgnPdng:%lu Pdng:%lu Mused:%lu Rest:%lu",
         (void *)affected_node,
         (void *)header_addr,
         (void *)aligned_data_addr,
         size,
         required_size,
         ((alloc_header *)header_addr)->block_size,
         alignment_padding,
         padding,
         arena->used,
         rest);
#endif
    return (void *)aligned_data_addr;
}

intern sizet mem_free_list_linear_block_size(void *ptr)
{
    // Insert it in a sorted position by the address number
    sizet current_addr = (sizet)ptr;
    sizet header_addr = current_addr - sizeof(alloc_header);
    auto aheader = (alloc_header *)header_addr;
    return aheader->block_size;
}

intern sizet mem_free_list_linear_block_user_size(void *ptr)
{
    // Insert it in a sorted position by the address number
    sizet current_addr = (sizet)ptr;
    sizet header_addr = current_addr - sizeof(alloc_header);
    auto aheader = (alloc_header *)header_addr;
    return aheader->block_size - (aheader->algn_padding + sizeof(alloc_header));
}

intern void mem_free_list_free(mem_arena *arena, void *ptr)
{
    // Insert it in a sorted position by the address number
    sizet current_addr = (sizet)ptr;
    sizet header_addr = current_addr - sizeof(alloc_header);
    auto aheader = (alloc_header *)header_addr;
#if DO_DEBUG_PRINT
    sizet algn_padding = aheader->algn_padding;
#endif

    mem_node *free_node = (mem_node *)(header_addr - aheader->algn_padding);
    free_node->data.block_size = aheader->block_size;
    free_node->next = nullptr;

    mem_node *it = arena->mfl.free_list.head;
    mem_node *it_prev = nullptr;
    while (it != nullptr) {
        if (ptr < it) {
            ll_insert(&arena->mfl.free_list, it_prev, free_node);
            break;
        }
        it_prev = it;
        it = it->next;
    }

    arena->used -= free_node->data.block_size;
#if DO_DEBUG_PRINT
    sizet orig_sz = free_node->data.block_size - algn_padding - sizeof(alloc_header);
    dlog("Dptr:%p FHdr:%p AHdr:%p BlckS:%lu APdng:%lu Size:%lu Mused:%lu",
         ptr,
         (void *)free_node,
         (void *)aheader,
         free_node->data.block_size,
         algn_padding,
         orig_sz,
         arena->used);
#endif
    assert(arena->used <= arena->total_size);

    // Merge contiguous nodes
    coalescence(&arena->mfl, it_prev, free_node);
}

intern void *mem_pool_alloc(mem_arena *arena)
{
    mem_node *free_pos = ll_pop(&arena->mpool.free_list);
    assert(free_pos);
    arena->used += arena->mpool.chunk_size;
    arena->peak = std::max(arena->peak, arena->used);
    return (void *)free_pos;
}

intern sizet mem_pool_block_size(mem_arena *arena, void *ptr)
{
    return arena->mpool.chunk_size;
}

intern void mem_pool_free(mem_arena *mem, void *ptr)
{
    mem->used -= mem->mpool.chunk_size;
    ll_push(&mem->mpool.free_list, (mem_node *)ptr);
}

intern void *mem_stack_alloc(mem_arena *arena, sizet size, sizet alignment)
{
    sizet current_addr = (sizet)arena->start + arena->mstack.offset;
    sizet padding = calc_padding_with_header(current_addr, alignment, sizeof(stack_alloc_header));

    if ((arena->mstack.offset + padding + size) > arena->total_size)
        return nullptr;

    arena->mstack.offset += padding + size;
    sizet next_addr = current_addr + padding;
    sizet header_addr = next_addr - sizeof(stack_alloc_header);
    ((stack_alloc_header *)header_addr)->padding = padding;

    arena->mstack.offset += size;
    arena->used = arena->mstack.offset;
    arena->peak = std::max(arena->peak, arena->used);
    return (void *)next_addr;
}

intern void mem_stack_free(mem_arena *arena, void *ptr)
{
    // Move offset back to clear address
    sizet current_addr = (sizet)ptr;
    sizet header_addr = current_addr - sizeof(stack_alloc_header);
    stack_alloc_header *alloc_header{(stack_alloc_header *)header_addr};

    arena->mstack.offset = current_addr - alloc_header->padding - (sizet)arena->start;
    arena->used = arena->mstack.offset;
}

intern void *mem_linear_alloc(mem_arena *arena, sizet size, sizet alignment)
{
    sizet header_size = sizeof(alloc_header);
    sizet padding = header_size;
    sizet block_addr = (sizet)arena->start + arena->mlin.offset;

    // Alignment is required. Find the next aligned memory address and update offset
    if ((alignment != 0) && (((arena->mlin.offset + header_size) % alignment) != 0)) {
        padding = calc_padding_with_header(block_addr, alignment, header_size);
    }

    assert(arena->mlin.offset + padding + size <= arena->total_size);

    // Setting up a block header is purely to make realloc work with a linear allocator
    auto alignment_padding = padding - header_size;
    auto hdr_address = block_addr + alignment_padding;
    auto hdr = (alloc_header*)hdr_address;
    hdr->algn_padding = alignment_padding;
    hdr->block_size = padding + size;
    
    arena->mlin.offset += padding + size;
    sizet next_addr = hdr_address + header_size;
    arena->used = arena->mlin.offset;
    arena->peak = std::max(arena->peak, arena->used);

#if DO_DEBUG_LINEAR_ALLOC
    dlog("Dptr:%p BlckS:%lu Mused:%lu", (void *)next_addr, padding + size, arena->used);
#endif
    return (void *)next_addr;
}

intern void mem_linear_free(mem_arena *, void *)
{
    // NO OP
}

void *mem_alloc(sizet bytes)
{
    return mem_alloc(bytes, nullptr, 8);
}

void *mem_alloc(sizet bytes, mem_arena *arena, sizet alignment)
{
    void *ret{nullptr};
    if (!arena) {
        arena = g_fl_arena;
    }
    if (arena) {
        switch (arena->alloc_type) {
        case (mem_alloc_type::FREE_LIST):
            ret = mem_free_list_alloc(arena, bytes, alignment);
            break;
        case (mem_alloc_type::POOL):
            assert(bytes == arena->mpool.chunk_size);
            ret = mem_pool_alloc(arena);
            break;
        case (mem_alloc_type::STACK):
            ret = mem_stack_alloc(arena, bytes, alignment);
            break;
        case (mem_alloc_type::LINEAR):
            ret = mem_linear_alloc(arena, bytes, alignment);
            break;
        }
    }
    else {
        ret = platform_alloc(bytes);
    }
#if !defined(NDEBUG)
    memset(ret, 0, bytes);
#endif
    return ret;
}

sizet mem_block_size(void *ptr, mem_arena *arena)
{
    if (arena->alloc_type == mem_alloc_type::FREE_LIST || arena->alloc_type == mem_alloc_type::LINEAR) {
        return mem_free_list_linear_block_size(ptr);
    }
    else if (arena->alloc_type == mem_alloc_type::POOL) {
        return mem_pool_block_size(arena, ptr);
    }
    return 0;
}

sizet mem_block_user_size(void *ptr, mem_arena *arena)
{
    if (arena->alloc_type == mem_alloc_type::FREE_LIST || arena->alloc_type == mem_alloc_type::LINEAR) {
        return mem_free_list_linear_block_user_size(ptr);
    }
    else if (arena->alloc_type == mem_alloc_type::POOL) {
        return mem_pool_block_size(arena, ptr);
    }
    return 0;
}

void *mem_realloc(void *ptr, sizet new_size, mem_arena *arena, sizet alignment, bool free_ptr_after_copy)
{
    if (!arena) {
        arena = g_fl_arena;
    }
    if (arena) {
        // Create a new block and copy the mem to it from the old block (we use the lesser of the block sizes)
        auto new_block = mem_alloc(new_size, arena, alignment);
        sizet old_block_size{0};

        if (ptr) {
            old_block_size = mem_block_user_size(ptr, arena);
            sizet block_size{new_size};
            assert(old_block_size > 0);

            // We only want to copy the lesser size of the blocks bytes
            if (new_size > old_block_size) {
                block_size = old_block_size;
            }

            memcpy(new_block, ptr, block_size);
            if (free_ptr_after_copy) {
                mem_free(ptr, arena);
            }
        }
        return new_block;
    }
    else {
        return platform_realloc(ptr, new_size);
    }
}

void *mem_realloc(void *ptr, sizet size)
{
    return mem_realloc(ptr, size, nullptr);
}

void mem_free(void *item)
{
    mem_free(item, nullptr);
}

void mem_free(void *ptr, mem_arena *arena)
{
    if (!ptr)
        return;

    if (!arena) {
        arena = g_fl_arena;
    }

    if (arena) {
        switch (arena->alloc_type) {
        case (mem_alloc_type::FREE_LIST):
            mem_free_list_free(arena, ptr);
            break;
        case (mem_alloc_type::POOL):
            mem_pool_free(arena, ptr);
            break;
        case (mem_alloc_type::STACK):
            mem_stack_free(arena, ptr);
            break;
        case (mem_alloc_type::LINEAR):
            mem_linear_free(arena, ptr);
            break;
        }
    }
    else {
        platform_free(ptr);
    }
}

void mem_reset_arena(mem_arena *arena)
{
    arena->used = 0;
    arena->peak = 0;

    switch (arena->alloc_type) {
    case (mem_alloc_type::POOL): {
        // Create a linked-list with all free positions
        sizet nchunks = arena->total_size / arena->mpool.chunk_size;
        for (sizet i = 0; i < nchunks; ++i) {
            sizet address = (sizet)arena->start + i * arena->mpool.chunk_size;
            ll_push(&arena->mpool.free_list, (mem_node *)address);
        }
    } break;
    case (mem_alloc_type::FREE_LIST): {
        mem_node *first_node = (mem_node *)arena->start;
        first_node->data.block_size = arena->total_size;
        first_node->next = nullptr;
        arena->mfl.free_list.head = nullptr;
        ll_node<free_header> *dummy = nullptr;
        ll_insert(&arena->mfl.free_list, dummy, first_node);
        arena->alloc_type = mem_alloc_type::FREE_LIST;
    } break;
    case (mem_alloc_type::STACK): {
        arena->mstack.offset = 0;
    } break;
    case (mem_alloc_type::LINEAR): {
        arena->mlin.offset = 0;
    } break;
    }
}

void mem_init_arena(sizet total_size, mem_alloc_type mtype, mem_arena *arena)
{
    arena->total_size = total_size;
    arena->alloc_type = mtype;
    ilog("Initializing %s arena with %lu available", mem_arena_type_str(arena->alloc_type), arena->total_size);

    // Make sure user filled out a size before passsing in
    assert(arena->total_size != 0);

    // If pool allocator total size must be multiple of chunk size, and chunk size must not be zero
    assert(arena->alloc_type != mem_alloc_type::POOL ||
           (((arena->total_size % arena->mpool.chunk_size) == 0) && (arena->mpool.chunk_size >= 8)));

    if (!arena->upstream_allocator)
        arena->start = platform_alloc(arena->total_size);
    else
        arena->start = mem_alloc(arena->total_size, arena->upstream_allocator);

    mem_reset_arena(arena);
}

void mem_terminate_arena(mem_arena *arena)
{
    ilog("Terminating %s arena with %lu used of %lu allocated and %lu peak",
         mem_arena_type_str(arena->alloc_type),
         arena->used,
         arena->total_size,
         arena->peak);
    mem_reset_arena(arena);
    if (arena->upstream_allocator) {
        mem_free(arena->start, arena->upstream_allocator);
    }
    else {
        platform_free(arena->start);
    }
    arena->start = nullptr;
}

const char *mem_arena_type_str(mem_alloc_type atype)
{
    switch (atype) {
    case (mem_alloc_type::FREE_LIST):
        return "free list";
    case (mem_alloc_type::POOL):
        return "pool";
    case (mem_alloc_type::STACK):
        return "stack";
    case (mem_alloc_type::LINEAR):
        return "linear";
    default:
        return "unknown";
    }
}

mem_arena *mem_global_arena()
{
    return g_fl_arena;
}

void mem_set_global_arena(mem_arena *arena)
{
    if (arena) {
        assert(arena->alloc_type == mem_alloc_type::FREE_LIST);
    }
    g_fl_arena = arena;
}

mem_arena *mem_global_stack_arena()
{
    return g_stack_arena;
}

void mem_set_global_stack_arena(mem_arena *arena)
{
    if (arena) {
        assert(arena->alloc_type == mem_alloc_type::STACK);
    }
    g_stack_arena = arena;
}

mem_arena *mem_global_frame_lin_arena()
{
    return g_frame_linear_arena;
}

void mem_set_global_frame_lin_arena(mem_arena *arena)
{
    if (arena) {
        assert(arena->alloc_type == mem_alloc_type::LINEAR);
    }
    g_frame_linear_arena = arena;
}

} // namespace nslib
