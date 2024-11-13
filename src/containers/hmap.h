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
using hash_func = u64(Key, u64, u64);

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
    sizet count;
};

template<typename Key, typename Val>
void hmap_debug_print(const array<hmap_bucket<Key, Val>> &buckets)
{
    for (sizet i = 0; i < buckets.size; ++i) {
        auto b = &buckets[i];
        dlog("Bucket: %lu  hval:%lu  prev:%lu  next:%lu  item [key:%d  val:%s  prev:%lu  next:%lu]",
             i,
             b->hashed_v,
             b->prev,
             b->next,
             b->item.key,
             str_cstr(b->item.val),
             b->item.prev,
             b->item.next);
    }
}

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
sizet hmap_find_bucket(hmap<Key, Val> *hm, const Key &k)
{
    assert(hm->hashf);
    if (hm->buckets.size == 0) {
        return false;
    }
    u64 hashval = hm->hashf(k, hm->seed0, hm->seed1);
    sizet bckt_ind = hashval % hm->buckets.size;
    if (is_valid(hm->buckets[bckt_ind].prev)) {
        while (hashval != hm->buckets[bckt_ind].hashed_v && is_valid(hm->buckets[bckt_ind].next)) {
            bckt_ind = hm->buckets[bckt_ind].next;
            if (hm->buckets[bckt_ind].hashed_v == hashval && hm->buckets[bckt_ind].item.key == k) {
                return bckt_ind;
            }
        }
    }
    return INVALID_IND;
}

template<typename Key, typename Val>
void hmap_clear_bucket(hmap<Key, Val> *hm, sizet bckt_ind)
{
    assert(bckt_ind < hm->buckets.size);
    // If our next index is valid, use it to get the next bucket - set the next bucket's prev index to our prev index
    if (is_valid(hm->buckets[bckt_ind].next)) {
        hm->buckets[hm->buckets[bckt_ind].next].prev = hm->buckets[bckt_ind].prev;
    }
    // If the prev bucket's next index is invalid, it means we are the head node of the bucket. If the prev bucket is us
    // then we are the only node. If we are the head node, but not the only node in the bucket, then we want to preserve
    // our next index so the other nodes can still be found even though we are removed
    if (is_valid(hm->buckets[hm->buckets[bckt_ind].prev].next) || hm->buckets[bckt_ind].prev == bckt_ind) {
        hm->buckets[hm->buckets[bckt_ind].prev].next = hm->buckets[bckt_ind].next;
        hm->buckets[bckt_ind].next = INVALID_IND;
    }
    // Set the previous ind to invalid - we don't set the hashed_v on purpose that is never used
    hm->buckets[bckt_ind].prev = INVALID_IND;

    // Do the same thing we did for the bucket indexes except now with the item indices
    if (is_valid(hm->buckets[bckt_ind].item.next)) {
        hm->buckets[hm->buckets[bckt_ind].item.next].item.prev = hm->buckets[bckt_ind].item.prev;
    }
    // If the previous item has a valid next index (ie if we are not the head node) then set the previous item's next
    // index to our next index to continue the chain
    if (is_valid(hm->buckets[hm->buckets[bckt_ind].item.prev].item.next)) {
        hm->buckets[hm->buckets[bckt_ind].item.prev].item.next = hm->buckets[bckt_ind].item.next;
    }
    // Reset the bucket item
    hm->buckets[bckt_ind].item = {};
}

template<typename Key, typename Val>
hmap_item<Key, Val> *hmap_erase(hmap<Key, Val> *hm, const hmap_item<Key, Val> *item)
{
    hmap_item<Key, Val> *ret{};
    if (!item || !is_valid(item->prev)) {
        return ret;
    }
    if (is_valid(item->next)) {
        ret = &hm->buckets[item->next].item;
    }

    // Get our bucket ind from previous item's next.. if the previous item's next is invalid it means our previous item
    // was ourself (ie we are the only item)
    sizet bckt_ind = hm->buckets[item->prev].next;
    if (!is_valid(bckt_ind)) {
        bckt_ind = item->prev;
    }
    hmap_clear_bucket(hm, bckt_ind);
    return ret;
}

template<typename Key, typename Val>
bool hmap_remove(hmap<Key, Val> *hm, const Key &k)
{
    sizet bckt_ind = hmap_find_bucket(hm, k);
    if (bckt_ind != INVALID_IND) {
        hmap_clear_bucket(hm, bckt_ind);
        return true;
    }
    return false;
}

template<typename Key, typename Val>
hmap_item<Key, Val> *hmap_find(hmap<Key, Val> *hm, const Key &k)
{
    sizet bucket_ind = hmap_find_bucket(hm, k);
    if (bucket_ind != INVALID_IND) {
        return &hm->buckets[bucket_ind].item;
    }
    return nullptr;
}

template<typename Key, typename Val>
hmap_item<Key, Val> *hmap_insert(hmap<Key, Val> *hm, const Key &k, const Val &val)
{
    assert(hm->hashf);
    if (hm->buckets.size == 0) {
        return nullptr;
    }

    u64 hashval = hm->hashf(k, hm->seed0, hm->seed1);
    sizet bckt_ind = hashval % hm->buckets.size;

    // Find an un-occupied bucket - a bucket is unoccupied if the prev bucket index is invalid. Head nodes have the prev
    // bucket index pointing to the last item in the bucket (the last item does not have its next pointing to the first
    // item however).
    auto cur_bckt_ind = bckt_ind;
    auto head_bckt_ind = INVALID_IND;
    sizet cur_hval = hashval;
    while (is_valid(hm->buckets[cur_bckt_ind].prev)) {
        // If we found a match (both hashed_v and the actual key) then return null
        if (hm->buckets[cur_bckt_ind].hashed_v == hashval && hm->buckets[cur_bckt_ind].item.key == k) {
            return nullptr;
        }

        // The head bucket will be the first bucket we find with the same hashed bucket as ours
        if (!is_valid(head_bckt_ind) && (hm->buckets[cur_bckt_ind].hashed_v % hm->buckets.size) == bckt_ind) {
            head_bckt_ind = cur_bckt_ind;
        }

        // TODO: Check if bitwise & is any faster
        cur_bckt_ind = cur_hval++ % hm->buckets.size;
    }

    // New bucket insertion
    if (!is_valid(head_bckt_ind)) {
        head_bckt_ind = cur_bckt_ind;
    }

    // Check the bucket ll for existing items and return null if any
    sizet n = hm->buckets[head_bckt_ind].next;
    while (is_valid(n)) {
        if (hm->buckets[n].hashed_v == hashval && hm->buckets[n].item.key == k) {
            return nullptr;
        }
        n = hm->buckets[n].next;
    }

    // The bucket item's next and prev should both be invalid
    assert(!is_valid(hm->buckets[cur_bckt_ind].item.next));
    assert(!is_valid(hm->buckets[cur_bckt_ind].item.prev));

    // Set the key/value/hashed_v
    hm->buckets[cur_bckt_ind].hashed_v = hashval;
    hm->buckets[cur_bckt_ind].item.key = k;
    hm->buckets[cur_bckt_ind].item.val = val;

    // If this is the first item, we create head pointing to itself as prev and leave next invalid
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

    // And now, we need to insert the item in the bucket item's linked list chain
    assert(!is_valid(hm->buckets[cur_bckt_ind].prev));

    // If we are appending to a bucket ll rather than inserting the head bucket node
    sizet head_prev_ind = cur_bckt_ind;
    if (cur_bckt_ind != head_bckt_ind) {
        // Make sure the head bucket has a valid prev ind
        assert(is_valid(hm->buckets[head_bckt_ind].prev));

        // Set our prev to the bucket that was previously at the end
        hm->buckets[cur_bckt_ind].prev = hm->buckets[head_bckt_ind].prev;

        // Asssert the previously end bucket's next index is invalid, and then set it to us
        assert(!is_valid(hm->buckets[hm->buckets[cur_bckt_ind].prev].next));
        hm->buckets[hm->buckets[cur_bckt_ind].prev].next = cur_bckt_ind;

        // Set the head bucket's prev ind to us
    }
    // If we are the head bucket and our next ind is valid, it means we are inserting this item to a previously deleted
    // head bucket, and need to find the end of the bucket ll to set our prev too
    else if (is_valid(hm->buckets[cur_bckt_ind].next)) {
        while (is_valid(hm->buckets[head_prev_ind].next)) {
            head_prev_ind = hm->buckets[head_prev_ind].next;
        }
    }
    hm->buckets[head_bckt_ind].prev = head_prev_ind;

    ++hm->count;
    return &hm->buckets[cur_bckt_ind].item;
}

template<typename Key, typename Val>
void hmap_clear(hmap<Key, Val> *hm)
{
    hm->head = INVALID_IND;
    hm->count = 0;
    arr_clear(&hm->buckets);
}

template<typename Key, typename Val>
void hmap_rehash(hmap<Key, Val> *hm, sizet new_size)
{
    auto tmp = hm->buckets;
    sizet ind = hm->head;
    hmap_clear(hm);
    arr_resize(&hm->buckets, new_size);
    do {
        hmap_insert(hm, tmp[ind].item.key, tmp[ind].item.val);
        ind = tmp[ind].item.next;
    }
    while (is_valid(ind));
}

template<typename Key, typename Val>
hmap_item<Key, Val> *hmap_first(hmap<Key, Val> *hm)
{
    if (is_valid(hm->head)) {
        return &hm->buckets[hm->head].item;
    }
    return nullptr;
}

template<typename Key, typename Val>
hmap_item<Key, Val> *hmap_last(hmap<Key, Val> *hm)
{
    if (is_valid(hm->head)) {
        assert(is_valid(hm->buckets[hm->head].item.prev));
        return &hm->buckets[hm->buckets[hm->head].item.prev].item;
    }
    return nullptr;
}

template<typename Key, typename Val>
hmap_item<Key, Val> *hmap_next(hmap<Key, Val> *hm, hmap_item<Key, Val> *item)
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
hmap_item<Key, Val> *hmap_prev(hmap<Key, Val> *hm, hmap_item<Key, Val> *item)
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
