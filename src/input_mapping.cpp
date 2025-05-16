#include <cstring>
#include <stdlib.h>

#include "input_mapping.h"
#include "json_archive.h"
#include "hashfuncs.h"

#include "platform.h"

namespace nslib
{

bool operator==(const input_keymap_entry &lhs, const input_keymap_entry &rhs)
{
    return (lhs.name == rhs.name);
}

u32 generate_keymap_id(u16 code, u16 keymods, u8 mbutton_mask)
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

void init_keymap(input_keymap *km, const char *name, mem_arena *arena)
{
    assert(km);
    assert(name);
    int seed0 = rand();
    int seed1 = rand();
    km->name = name;
    hmap_init(&km->hm, hash_type, arena, 121);
}

void terminate_keymap(input_keymap *km)
{
    assert(km);
    hmap_terminate(&km->hm);
    memset(km, 0, sizeof(input_keymap));
}

void init_keymap_stack(input_keymap_stack *stack, mem_arena *arena)
{
    assert(stack);
    hmap_init(&stack->trigger_funcs, hash_type, arena);
}

void terminate_keymap_stack(input_keymap_stack *stack)
{
    arr_clear(&stack->kmaps);
    hmap_terminate(&stack->trigger_funcs);
}
void set_keymap_entry(input_keymap *km, u32 id, const input_keymap_entry &entry)
{
    assert(km);
    hmap_set(&km->hm, id, entry);
}

// Set keymap entry overwriting an existing one if its there

void set_keymap_entry(input_keymap *km, input_kmcode kmcode, u16 keymods, u8 mbutton_mask, const input_keymap_entry &entry)
{
    auto id = generate_keymap_id(kmcode, keymods, mbutton_mask);
    set_keymap_entry(km, id, entry);
}

bool add_keymap_entry(input_keymap *km, u32 id, const input_keymap_entry &entry)
{
    assert(km);
    auto item = hmap_insert(&km->hm, id, entry);
    return item;
}

bool add_keymap_entry(input_keymap *km, input_kmcode kmcode, u16 keymods, u8 mbutton_mask, const input_keymap_entry &entry)
{
    auto id = generate_keymap_id(kmcode, keymods, mbutton_mask);
    return add_keymap_entry(km, id, entry);
}

input_keymap_entry *find_keymap_entry(input_keymap *km, u32 id)
{
    assert(km);
    auto item = hmap_find(&km->hm, id);
    if (item) {
        return &item->val;
    }
    return nullptr;
}

const input_keymap_entry *find_keymap_entry(const input_keymap *km, u32 id)
{
    assert(km);
    auto item = hmap_find(&km->hm, id);
    if (item) {
        return &item->val;
    }
    return nullptr;
}

input_keymap_entry *find_keymap_entry(input_keymap *km, const char *name)
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

const input_keymap_entry *find_keymap_entry(const input_keymap *km, const char *name)
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

bool remove_keymap_entry(input_keymap *km, u32 key)
{
    assert(km);
    return hmap_remove(&km->hm, key);
}

input_keymap **push_keymap(input_keymap_stack *stack, input_keymap *km)
{
    assert(km);
    assert(stack);
    assert(stack->kmaps.size + 1 <= MAX_INPUT_CONTEXT_STACK_COUNT);
    return arr_push_back(&stack->kmaps, km);
}

bool keymap_in_stack(const input_keymap *km, const input_keymap_stack *stack)
{
    assert(km);
    assert(stack);
    for (int i = 0; i < stack->kmaps.size; ++i) {
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
    auto back = arr_back(&stack->kmaps);
    if (back) {
        arr_pop_back(&stack->kmaps);
        return *back;
    }
    return nullptr;
}

void map_input_event(const input_keymap_stack *stack, const platform_input_event *raw)
{
    assert(raw);
    assert(stack);
    input_trigger t{};
    t.ev = raw;

    for (u8 i = stack->kmaps.size; i != 0; --i) {
        const input_keymap *cur_map = stack->kmaps[i - 1];
        u8 mbutton_mask = raw->mbutton_mask & cur_map->mbutton_mask;
        u16 keymods = raw->keymods & cur_map->kmod_mask;
        auto id = generate_keymap_id(raw->kmcode, keymods, mbutton_mask);
        const input_keymap_entry *kentry = find_keymap_entry(cur_map, id);
        if (kentry) {
            t.name = str_cstr(kentry->name);
            u64 nkey = hash_type(t.name, 0, 0);
            auto fiter = hmap_find(&stack->trigger_funcs, nkey);
            if (fiter) {
                if (fiter->val.func) {
                    fiter->val.func(t, fiter->val.user);
                }
            }
            else {
                wlog("No trigger func found for %s: %s", t.name, js(stack->trigger_funcs));
            }
            if (!test_flags(kentry->flags, KEYMAP_ENTRY_FLAG_DONT_CONSUME)) {
                return;
            }
        }
    }
}

void map_input_frame(const input_keymap_stack *stack, const platform_frame_input_events *frame)
{
    assert(frame);
    assert(stack);
    assert(frame->events.size <= frame->events.capacity);
    for (sizet i = 0; i < frame->events.size; ++i) {
        map_input_event(stack, &frame->events[i]);
    }
}

bool add_input_trigger_func(input_keymap_stack *stack, const char *name, const input_trigger_cb &cb)
{
    assert(stack);
    if (!name) {
        wlog("Cannot add trigger func under empty name");
        return false;
    }
    u64 nkey = hash_type(name, 0, 0);
    auto ins = hmap_insert(&stack->trigger_funcs, nkey, cb);
    return ins;
}

void set_input_trigger_func(input_keymap_stack *stack, const char *name, const input_trigger_cb &cb)
{
    assert(stack);
    if (!name) {
        wlog("Cannot add trigger func under empty name");
        return;
    }
    u64 nkey = hash_type(name, 0, 0);
    hmap_set(&stack->trigger_funcs, nkey, cb);
}

bool remove_input_trigger_func(input_keymap_stack *stack, const char *name)
{
    u64 nkey = hash_type(name, 0, 0);
    return hmap_remove(&stack->trigger_funcs, nkey);
}

} // namespace nslib
