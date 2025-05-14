#include <cstring>
#include <stdlib.h>

#include "input_mapping.h"
#include "hashfuncs.h"
#include "platform.h"

namespace nslib
{

bool operator==(const input_keymap_entry &lhs, const input_keymap_entry &rhs)
{
    return (lhs.name == rhs.name);
}

u32 generate_keymap_id(input_kmcode code, u16 keymods, u8 mbutton_mask)
{
    // | ---- kmcode (10 bits) --- | ---- keymods (14 bits) ---- | ---- mbutton_mask (8 bits) ---- |
    // Kmcode is fine because its type safe, need to make sure keymods and mbutton_mask don't have anything in upper
    // bits so that's why they are anded with 0x3FFF (0011111111111111b) and 0x3F (00111111b)
    return (code << 22) | ((keymods & 0x3FFF) << 8) | mbutton_mask;
}

input_kmcode get_kmcode_from_keymap_id(u32 key)
{
    return (input_kmcode)(key >> 22);
}

u16 get_keymods_from_keymap_id(u32 key)
{
    return (((u16)(key >> 8)) & 0x3FFF);
}

u8 get_mbutton_mask_from_keymap_id(u32 key)
{
    return (u8)key;
}

void init_keymap(const char *name, input_keymap *km, mem_arena *arena)
{
    assert(km);
    assert(name);
    int seed0 = rand();
    int seed1 = rand();
    km->name = name;
    hmap_init(&km->hm, hash_type, arena);
}

void terminate_keymap(input_keymap *km)
{
    assert(km);
    hmap_terminate(&km->hm);
    memset(km, 0, sizeof(input_keymap));
}

void set_keymap_entry(u32 id, const input_keymap_entry *entry, input_keymap *km)
{
    assert(entry);
    assert(km);
    hmap_set(&km->hm, id, *entry);
}

// Insert keymap entry - if it exists return null otherwise return the inserted entry
input_keymap_entry *insert_keymap_entry(u32 id, const input_keymap_entry *entry, input_keymap *km)
{
    assert(entry);
    assert(km);
    auto item = hmap_insert(&km->hm, id, *entry);
    if (item) {
        return &item->val;
    }
    return nullptr;
}

input_keymap_entry *find_keymap_entry(u32 id, input_keymap *km)
{
    assert(km);
    auto item = hmap_find(&km->hm, id);
    if (item) {
        return &item->val;
    }
    return nullptr;
}

const input_keymap_entry *find_keymap_entry(u32 id, const input_keymap *km)
{
    assert(km);
    auto item = hmap_find(&km->hm, id);
    if (item) {
        return &item->val;
    }
    return nullptr;
}

input_keymap_entry *find_keymap_entry(const char *name, input_keymap *km)
{
    assert(name);
    assert(km);
    auto iter = hmap_begin(&km->hm);
    while (iter) {
        if (name == iter->val.name) {
            return &iter->val;
        }
        iter = hmap_next(&km->hm, iter);
    }
    return nullptr;
}

const input_keymap_entry *find_keymap_entry(const char *name, const input_keymap *km)
{
    assert(name);
    assert(km);
    auto iter = hmap_begin(&km->hm);
    while (iter) {
        if (name == iter->val.name) {
            return &iter->val;
        }
        iter = hmap_next(&km->hm, iter);
    }
    return nullptr;
}

bool remove_keymap_entry(u32 key, input_keymap *km)
{
    assert(km);
    return hmap_remove(&km->hm, key);
}

// Push km to the top of the keymap stack - top is highest priority in input_map_event
void push_keymap(input_keymap *km, input_keymap_stack *stack)
{
    assert(km);
    assert(stack);
    assert(stack->count + 1 <= MAX_INPUT_CONTEXT_STACK_COUNT);
    stack->kmaps[stack->count] = km;
    ++stack->count;
}

bool keymap_in_stack(const input_keymap *km, const input_keymap_stack *stack)
{
    assert(km);
    assert(stack);
    for (int i = 0; i < stack->count; ++i) {
        if (stack->kmaps[i] == km) {
            return true;
        }
    }
    return false;
}

// Pop to top keymap from the stack and return it
input_keymap *pop_keymap(input_keymap_stack *stack)
{
    assert(stack);
    if (stack->count == 0) {
        return nullptr;
    }
    --stack->count;
    return stack->kmaps[stack->count];
}

void map_input_event(const platform_input_event *raw, const input_keymap_stack *stack)
{
    // assert(raw);
    // assert(stack);
    // input_event ev{};
    // u32 key = generate_keymap_kmcode_id(raw->key_or_button, raw->mods, raw->action);
    // u32 key_any = generate_keymap_kmcode_id(raw->key_or_button, MOD_ANY, raw->action);

    // for (u8 i = stack->count; i != 0; --i) {
    //     const input_keymap *cur_map = stack->kmaps[i - 1];
    //     const input_keymap_entry *kentry = find_keymap_entry(key, cur_map);
    //     const input_keymap_entry *kanymod = find_keymap_entry(key_any, cur_map);

    //     bool should_return = false;
    //     if (kentry) {
    //         ev.name = str_cstr(kentry->name);
    //         fill_event_from_platform_event(raw, &ev);
    //         if (kentry->cb) {
    //             kentry->cb(&ev, kentry->cb_user_param);
    //         }
    //         should_return = !test_flags(kentry->flags, IEVENT_FLAG_DONT_CONSUME);
    //     }
    //     if (kanymod && kanymod != kentry) {
    //         ev.name = str_cstr(kanymod->name);
    //         fill_event_from_platform_event(raw, &ev);
    //         if (kanymod->cb) {
    //             kanymod->cb(&ev, kanymod->cb_user_param);
    //         }
    //         should_return = should_return || !test_flags(kanymod->flags, IEVENT_FLAG_DONT_CONSUME);
    //     }
    //     if (should_return) {
    //         return;
    //     }
    // }
}

void map_input_frame(const platform_frame_input_events *frame, const input_keymap_stack *stack)
{
    assert(frame);
    assert(stack);
    assert(frame->events.size <= frame->events.capacity);
    for (sizet i = 0; i < frame->events.size; ++i) {
        map_input_event(&frame->events[i], stack);
    }
    assert(frame->events.size <= frame->events.capacity);
}

} // namespace nslib
