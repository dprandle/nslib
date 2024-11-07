#pragma once

#include "../util.h"
#include "array.h"

namespace nslib
{
#define DEFAULT_HMAP_BUCKET_COUNT 64

enum hmap_bucket_flags
{
    HMAP_BUCKET_FLAG_USED = 1
};

template<typename Key, typename Val>
struct hmap_item
{
    Key key;
    Val val;
    sizet next{INVALID_IND};
    sizet prev{INVALID_IND};
};

template<typename Key, typename Val>
struct hmap_bucket
{
    hmap_item<Key, Val> item{};
    u64 hashed_v{};
    sizet next{INVALID_IND};
    sizet prev{INVALID_IND};
};

template<typename Key>
using hash_func = u64(const Key &, u64 seed0, u64 seed1);
 
// Since hmap manages memory, but we want it to act like a built in type in terms of copying and equality testing, we
// have to write copy ctor, dtor, assignment operator, and equality operators.
template<typename Key, typename Val>
struct hmap
{
    hash_func<Key> *hashf;
    u64 seed0;
    u64 seed1;
    array<hmap_bucket<Key, Val>> buckets;
    sizet head{INVALID_IND};
};

template<typename Key, typename Val>
void hmap_init(hmap<Key, Val> *hm,
               hash_func<Key> *hashf,
               u64 seed0 = generate_rand_seed(),
               u64 seed1 = generate_rand_seed(),
               mem_arena *arena = mem_global_arena(),
               sizet initial_capacity = DEFAULT_HMAP_BUCKET_COUNT,
               sizet mem_alignment = DEFAULT_MIN_ALIGNMENT)
{
    hm->hashf = hashf;
    hm->seed0 = seed0;
    hm->seed1 = seed1;
    arr_init(&hm->buckets, arena, initial_capacity, mem_alignment);
    arr_resize(&hm->buckets, initial_capacity);
}

template<typename Key, typename Val>
hmap_item<Key,Val>* hmap_insert(hmap<Key, Val> *hm, const Key &k, const Val &val)
{
    assert(hm->buckets.capacity > 0);
    assert(hm->hashf);
    u64 hashval = hm->hashf(k, hm->seed0, hm->seed1);
    sizet bckt_ind = hashval % hm->buckets.capacity;

    // Find an un-occupied bucket
    auto cur_bckt_ind = bckt_ind;
    sizet cur_hval = hashval;
    while (is_valid(hm->buckets[cur_bckt_ind].prev)) {
        if (hm->buckets[cur_bckt_ind].hashed_v == hashval && hm->buckets[cur_bckt_ind].item.key == k) {
            return nullptr;
        }
        
        // Check the bucket ll for existing items and return if any
        sizet n = hm->buckets[cur_bckt_ind].next;
        while (is_valid(n)) {
            if (hm->buckets[n].hashed_v == hashval && hm->buckets[n].item.key == k) {
                return nullptr;
            }
            n = hm->buckets[n].next;
        }
        
        // TODO: Check if bitwise & is any faster
        cur_bckt_ind = cur_hval++ % hm->buckets.capacity;
    }

    assert(!is_valid(hm->buckets[cur_bckt_ind].item.next));
    assert(!is_valid(hm->buckets[cur_bckt_ind].item.prev));
    
    // Set the key/value/hashed_v
    hm->buckets[cur_bckt_ind].hashed_v = hashval;
    hm->buckets[cur_bckt_ind].item.key = k;
    hm->buckets[cur_bckt_ind].item.val = val;

    // If this is the first item, we create head pointing to itself as prev
    if (!is_valid(hm->head)) {
        hm->head = cur_bckt_ind;
        hm->buckets[cur_bckt_ind].item.prev = cur_bckt_ind;
    }
    else {
        // Our bucket's prev should point to the last item, which is head's prev, our next will remain pointing to
        // invalid to indicate it is the last item. Then head's prev should point to us
        hm->buckets[cur_bckt_ind].item.prev = hm->buckets[hm->head].item.prev;
        hm->buckets[hm->head].item.prev = cur_bckt_ind;

        // And finally the bucket before us (which was previously the last bucket) should now point to us as next
        assert(!is_valid(hm->buckets[hm->buckets[cur_bckt_ind].item.prev].item.next));
        hm->buckets[hm->buckets[cur_bckt_ind].item.prev].item.next = cur_bckt_ind;
    }
    
    // And now, we need to insert the item in the bucket item's linked list chain if cur_bckt_ind != bckt_ind which
    assert(!is_valid(hm->buckets[cur_bckt_ind].next));
    assert(!is_valid(hm->buckets[cur_bckt_ind].prev));
        
    if (cur_bckt_ind != bckt_ind) {
        assert(is_valid(hm->buckets[bckt_ind].prev));

        // Set our prev to the bucket that was previously at the end
        hm->buckets[cur_bckt_ind].prev = hm->buckets[bckt_ind].prev;
    
        assert(!is_valid(hm->buckets[hm->buckets[cur_bckt_ind].prev].next));
        hm->buckets[hm->buckets[cur_bckt_ind].prev].next = cur_bckt_ind;
    }
    hm->buckets[bckt_ind].prev = cur_bckt_ind;
    return &hm->buckets[cur_bckt_ind].item;
}

template<typename Key, typename Val>
hmap_item<Key, Val>* hmap_first(hmap<Key, Val> *hm) {
    if (is_valid(hm->head)) {
        return &hm->buckets[hm->head].item;
    }
    return nullptr;
}

template<typename Key, typename Val>
hmap_item<Key, Val>* hmap_last(hmap<Key, Val> *hm) {
    if (is_valid(hm->head)) {
        assert(is_valid(hm->buckets[hm->head].item.prev));
        return &hm->buckets[hm->buckets[hm->head].item.prev].item;
    }
    return nullptr;
}

template<typename Key, typename Val>
hmap_item<Key, Val>* hmap_next(hmap<Key, Val> *hm, hmap_item<Key, Val> *item)
{
    if (!item) {
        item = hmap_first(hm);
    }
    if (item && is_valid(item->next)) {
        return &hm->buckets[item->next].item;
    }
    return nullptr;
}

template<typename Key, typename Val>
hmap_item<Key, Val>* hmap_prev(hmap<Key, Val> *hm, hmap_item<Key, Val> *item)
{
    if (!item) {
        item = hmap_last(hm);
    }
    if (item && item != &hm->buckets[hm->head].item && is_valid(item->prev)) {
        return &hm->buckets[item->prev].item;
    }
    return nullptr;
}

template<typename Key, typename Val>
void hmap_terminate(hmap<Key, Val> *hm)
{
    arr_terminate(&hm->buckets);
}

} // namespace nslib
