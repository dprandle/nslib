#include <cstring>
#include <stdlib.h>

#include "input_mapping.h"
#include "hashfuncs.h"
#include "platform.h"

namespace nslib
{

intern void fill_event_from_platform_event(const platform_input_event *raw, input_event *ev)
{
    ev->modifiers = raw->mods;
    if (raw->type == platform_input_event_type::CURSOR_POS) {
        ev->type = IEVENT_TYPE_CURSOR;
        ev->pos = raw->pos;
    }
    else if (raw->type == platform_input_event_type::SCROLL) {
        ev->pos = get_cursor_pos(raw->win_hndl);
        ev->type = IEVENT_TYPE_SCROLL;
        ev->scroll_data.offset = raw->offset;
    }
    else {
        ev->pos = get_cursor_pos(raw->win_hndl);
        ev->type = IEVENT_TYPE_BTN;
        ev->btn_data.action = raw->action;
        ev->btn_data.key_or_button = raw->key_or_button;
    }
    ev->norm_pos = ev->pos / vec2(get_framebuffer_size(raw->win_hndl));
}

bool operator==(const input_keymap_entry &lhs, const input_keymap_entry &rhs)
{
    return (lhs.name == rhs.name);
}

u32 keymap_button_key(int key_or_button, int modifiers, int action)
{
    return (key_or_button << 18) | (modifiers << 8) | action;
}

u32 keymap_cursor_key(int modifiers)
{
    return keymap_button_key(CURSOR_POS_CHANGE, modifiers, 0);
}

u32 keymap_scroll_key(int modifiers)
{
    return keymap_button_key(SCROLL_CHANGE, modifiers, 0);
}

int button_from_key(u32 key)
{
    return (key >> 18);
}

int mods_from_key(u32 key)
{
    return ((key >> 8) & 0x000003FF);
}

int action_from_key(u32 key)
{
    return (key & 0x000000FF);
}

void init_keymap(const char *name, input_keymap *km, mem_arena *arena)
{
    assert(km);
    assert(name);
    int seed0 = rand();
    int seed1 = rand();
    strncpy(km->name, name, SMALL_STR_LEN);
    hmap_init(&km->hm, hash_type, arena);
}

void terminate_keymap(input_keymap *km)
{
    assert(km);
    hmap_terminate(&km->hm);
    memset(km, 0, sizeof(input_keymap));
}

input_keymap_entry *set_keymap_entry(u32 key, const input_keymap_entry *entry, input_keymap *km)
{
    assert(entry);
    assert(km);
    auto iter = hmap_set(&km->hm, key, *entry);
    if (iter) {
        return &iter->val;
    }
    return nullptr;
}

input_keymap_entry *get_keymap_entry(u32 key, input_keymap *km)
{
    assert(km);
    auto item = hmap_find(&km->hm, key);
    if (item) {
        return &item->val;
    }
    return nullptr;
}

const input_keymap_entry *get_keymap_entry(u32 key, const input_keymap *km)
{
    assert(km);
    auto item = hmap_find(&km->hm, key);
    if (item) {
        return &item->val;
    }
    return nullptr;
}

input_keymap_entry *get_keymap_entry(const char *name, input_keymap *km)
{
    assert(name);
    assert(km);
    auto iter = hmap_first(&km->hm);
    while (iter) {
        if (name == iter->val.name) {
            return &iter->val;
        }
        iter = hmap_next(&km->hm, iter);
    }
    return nullptr;
}

const input_keymap_entry *get_keymap_entry(const char *name, const input_keymap *km)
{
    assert(name);
    assert(km);
    auto iter = hmap_first(&km->hm);
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
    assert(raw);
    assert(stack);
    input_event ev{};
    u32 key = keymap_button_key(raw->key_or_button, raw->mods, raw->action);
    u32 key_any = keymap_button_key(raw->key_or_button, MOD_ANY, raw->action);

    for (u8 i = stack->count; i != 0; --i) {
        const input_keymap *cur_map = stack->kmaps[i - 1];
        const input_keymap_entry *kentry = get_keymap_entry(key, cur_map);
        const input_keymap_entry *kanymod = get_keymap_entry(key_any, cur_map);

        bool should_return = false;
        if (kentry) {
            ev.name = str_cstr(kentry->name);
            fill_event_from_platform_event(raw, &ev);
            if (kentry->cb) {
                kentry->cb(&ev, kentry->cb_user_param);
            }
            should_return = !test_flags(kentry->flags, IEVENT_FLAG_DONT_CONSUME);
        }
        if (kanymod && kanymod != kentry) {
            ev.name = str_cstr(kanymod->name);
            fill_event_from_platform_event(raw, &ev);
            if (kanymod->cb) {
                kanymod->cb(&ev, kanymod->cb_user_param);
            }
            should_return = should_return || !test_flags(kanymod->flags, IEVENT_FLAG_DONT_CONSUME);
        }
        if (should_return) {
            return;
        }
    }
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
