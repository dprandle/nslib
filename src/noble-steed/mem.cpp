#include <stdlib.h>
#include <numeric>
#include <assert.h>

#include "platform.h"
#include "mem.h"
#include "math/algorithm.h"

namespace noble_steed
{

static mem_store *g_mem_store{};

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

void intern find_first(mem_free_list *mfl, sizet size, sizet alignment, sizet &padding, mem_node *&prev_node, mem_node *&found_node)
{
    // Iterate list and return the first free block with a size >= than given size
    mem_node *it = mfl->free_list.head, *it_prev = nullptr;

    while (it != nullptr) {
        padding = calc_padding_with_header((sizet)it, alignment, sizeof(alloc_header));
        sizet required_space = size + padding;
        if (it->data.block_size >= required_space) {
            break;
        }
        it_prev = it;
        it = it->next;
    }
    prev_node = it_prev;
    found_node = it;
}

void intern find_best(mem_free_list *mfl, sizet size, sizet alignment, sizet &padding, mem_node *&prev_node, mem_node *&found_node)
{
    // Iterate WHOLE list keeping a pointer to the best fit
    sizet smallest_diff = std::numeric_limits<sizet>::max();
    mem_node *best_block = nullptr;
    mem_node *it = mfl->free_list.head, *it_prev = nullptr;
    while (it != nullptr) {
        padding = calc_padding_with_header((sizet)it, alignment, sizeof(alloc_header));
        sizet required_space = size + padding;
        if (it->data.block_size >= required_space && (it->data.block_size - required_space < smallest_diff)) {
            best_block = it;
        }
        it_prev = it;
        it = it->next;
    }
    prev_node = it_prev;
    found_node = best_block;
}

intern void find(mem_free_list *mfl, sizet size, sizet alignment, sizet &padding, mem_node *&prev_node, mem_node *&found_node)
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

intern void *mem_free_list_alloc(mem_store *mem, sizet size, sizet alignment)
{
    sizet alloc_header_size = sizeof(alloc_header);
    assert(size >= sizeof(mem_node));
    assert(alignment >= 8);

    // Search through the free list for a free block that has enough space to allocate our data
    sizet padding;
    mem_node *affected_node, *prev_node;
    find(&mem->mfl, size, alignment, padding, prev_node, affected_node);
    assert(affected_node);

    sizet alignment_padding = padding - alloc_header_size;
    sizet required_size = size + padding;
    sizet rest = affected_node->data.block_size - required_size;

    if (rest > 0) {
        // We have to split the block into the data block and a free block of size 'rest'
        mem_node *new_free_node = (mem_node *)((sizet)affected_node + required_size);
        new_free_node->data.block_size = rest;
        ll_insert(&mem->mfl.free_list, affected_node, new_free_node);
    }
    ll_remove(&mem->mfl.free_list, prev_node, affected_node);

    // Setup data block
    sizet header_addr = (sizet)affected_node + alignment_padding;
    sizet data_addr = header_addr + alloc_header_size;
    ((alloc_header *)header_addr)->block_size = required_size;
    ((alloc_header *)header_addr)->padding = alignment_padding;

    mem->used += required_size;
    mem->peak = std::max(mem->peak, mem->used);
    return (void *)data_addr;
}

intern void mem_free_list_free(mem_store *mem, void *ptr)
{
    // Insert it in a sorted position by the address number
    sizet current_addr = (sizet)ptr;
    sizet header_addr = current_addr - sizeof(alloc_header);
    const alloc_header *aheader{(alloc_header *)header_addr};

    mem_node *free_node = (mem_node *)(header_addr);
    free_node->data.block_size = aheader->block_size + aheader->padding;
    free_node->next = nullptr;

    mem_node *it = mem->mfl.free_list.head;
    mem_node *it_prev = nullptr;
    while (it != nullptr) {
        if (ptr < it) {
            ll_insert(&mem->mfl.free_list, it_prev, free_node);
            break;
        }
        it_prev = it;
        it = it->next;
    }

    mem->used -= free_node->data.block_size;

    // Merge contiguous nodes
    coalescence(&mem->mfl, it_prev, free_node);
}

intern void *mem_pool_alloc(mem_store *mem)
{
    mem_node *free_pos = ll_pop(&mem->mpool.free_list);
    assert(free_pos);
    mem->used += mem->mpool.chunk_size;
    mem->peak = std::max(mem->peak, mem->used);
    return (void *)free_pos;
}

intern void mem_pool_free(mem_store *mem, void *ptr)
{
    mem->used -= mem->mpool.chunk_size;
    ll_push(&mem->mpool.free_list, (mem_node *)ptr);
}

intern void *mem_stack_alloc(mem_store *mem, sizet size, sizet alignment)
{
    sizet current_addr = (sizet)mem->start + mem->mstack.offset;

    char padding = calc_padding_with_header(current_addr, alignment, sizeof(stack_alloc_header));

    if (mem->mstack.offset + padding + size > mem->total_size)
        return nullptr;

    mem->mstack.offset += padding;

    sizet next_addr = current_addr + padding;
    sizet header_addr = next_addr - sizeof(stack_alloc_header);
    stack_alloc_header alloc_header{padding};
    stack_alloc_header *header_ptr = (stack_alloc_header *)header_addr;
    header_ptr = &alloc_header;

    mem->mstack.offset += size;
    mem->used = mem->mstack.offset;
    mem->peak = std::max(mem->peak, mem->used);
    return (void *)next_addr;
}

intern void mem_stack_free(mem_store *mem, void *ptr)
{
    // Move offset back to clear address
    sizet current_addr = (sizet)ptr;
    sizet header_addr = current_addr - sizeof(stack_alloc_header);
    stack_alloc_header *alloc_header{(stack_alloc_header *)header_addr};

    mem->mstack.offset = current_addr - alloc_header->padding - (sizet)mem->start;
    mem->used = mem->mstack.offset;
}

intern void *mem_linear_alloc(mem_store *mem, sizet size, sizet alignment)
{
    sizet padding = 0;
    sizet current_addr = (sizet)mem->start + mem->mlin.offset;

    // Alignment is required. Find the next aligned memory address and update offset
    if ((alignment != 0) && ((mem->mlin.offset % alignment) != 0))
        padding = calc_padding(current_addr, alignment);

    if (mem->mlin.offset + padding + size > mem->total_size)
        return nullptr;

    mem->mlin.offset += padding;
    sizet next_addr = current_addr + padding;
    mem->mlin.offset += size;
    mem->used = mem->mlin.offset;
    mem->peak = std::max(mem->peak, mem->used);
    return (void *)next_addr;
}

intern void mem_linear_free(mem_store *mem, void *ptr)
{
    // NO OP
}

void *ns_alloc(sizet size, mem_store *mem, sizet alignment)
{
    if (!mem) {
        mem = global_allocator();
    }
    if (mem) {
        switch (mem->alloc_type) {
        case (MEM_ALLOC_FREE_LIST):
            return mem_free_list_alloc(mem, size, alignment);
        case (MEM_ALLOC_POOL):
            assert(size == mem->mpool.chunk_size);
            return mem_pool_alloc(mem);
        case (MEM_ALLOC_STACK):
            return mem_stack_alloc(mem, size, alignment);
        case (MEM_ALLOC_LINEAR):
            return mem_linear_alloc(mem, size, alignment);
        }
    }
    else {
        return platform_alloc(size);
    }
    return nullptr;
}

void ns_free(mem_store *mem, void *ptr)
{
    if (!ptr)
        return;

    if (!mem) {
        mem = global_allocator();
    }
    if (mem) {
        switch (mem->alloc_type) {
        case (MEM_ALLOC_FREE_LIST):
            mem_free_list_free(mem, ptr);
            break;
        case (MEM_ALLOC_POOL):
            mem_pool_free(mem, ptr);
            break;
        case (MEM_ALLOC_STACK):
            mem_stack_free(mem, ptr);
            break;
        case (MEM_ALLOC_LINEAR):
            mem_linear_free(mem, ptr);
            break;
        }
    }
    else {
        platform_free(ptr);
    }
}

void mem_store_reset(mem_store *mem)
{
    mem->used = 0;
    mem->peak = 0;
    
    switch (mem->alloc_type) {
    case (MEM_ALLOC_POOL): {
        // Create a linked-list with all free positions
        int nchunks = mem->total_size / mem->mpool.chunk_size;
        for (int i = 0; i < nchunks; ++i) {
            sizet address = (sizet)mem->start + i * mem->mpool.chunk_size;
            ll_push(&mem->mpool.free_list, (mem_node *)address);
        }
    } break;
    case (MEM_ALLOC_FREE_LIST): {
        mem_node *first_node = (mem_node *)mem->start;
        first_node->data.block_size = mem->total_size;
        first_node->next = nullptr;
        mem->mfl.free_list.head = nullptr;
        ll_node<free_header> *dummy = nullptr;
        ll_insert(&mem->mfl.free_list, dummy, first_node);
        mem->alloc_type = MEM_ALLOC_FREE_LIST;
    } break;
    case (MEM_ALLOC_STACK): {
        mem->mstack.offset = 0;
    } break;
    case (MEM_ALLOC_LINEAR): {
        mem->mlin.offset = 0;
    } break;
    }
}

void mem_store_init(sizet total_size, mem_alloc_type mtype, mem_store *mem)
{
    mem->total_size = total_size;
    mem->alloc_type = mtype;
    
    // Make sure user filled out a size before passsing in
    assert(mem->total_size != 0);

    // If pool allocator total size must be multiple of chunk size, and chunk size must not be zero
    assert(mem->alloc_type != MEM_ALLOC_POOL || (((mem->total_size % mem->mpool.chunk_size) == 0) && (mem->mpool.chunk_size >= 8)));

    if (!mem->upstream_allocator)
        mem->start = platform_alloc(mem->total_size);
    else
        mem->start = ns_alloc(mem->total_size, mem->upstream_allocator);

    mem_store_reset(mem);
}


void mem_store_terminate(mem_store *mem)
{
    platform_free(mem->start);
}

mem_store *global_allocator()
{
    return g_mem_store;
}

void set_global_allocator(mem_store *alloc)
{
    g_mem_store = alloc;
}

} // namespace noble_steed
