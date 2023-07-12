#pragma once

#include "math/vector2.h"
#include "input_kmcodes.h"

namespace nslib
{
constexpr const u8 MAX_INPUT_CONTEXT_STACK_COUNT = 32;

struct hashmap;
struct mem_arena;
struct platform_input_event;
struct platform_frame_input;

enum input_event_type
{
    IEVENT_TYPE_BTN,
    IEVENT_TYPE_CURSOR,
    IEVENT_TYPE_SCROLL
};

enum input_event_flags
{
    // If this flag is set, then input_keymap_entries that have the same key but are lower in the stack will also be
    // called back
    IEVENT_FLAG_DONT_CONSUME = 1
};

struct input_button_event
{
    int key_or_button{};
    int action{};
};

struct input_cursor_event
{
    dvec2 pos{};
    dvec2 norm_pos{};
};

struct input_scroll_event
{
    dvec2 offset{};
};

struct input_event
{
    const char *name{};
    int type{-1};
    int modifiers{};
    union
    {
        input_button_event btn_data;
        input_cursor_event cursor_data;
        input_scroll_event scroll_data;
    };
};

using input_event_func = void(const input_event *ev, void *user);

struct input_keymap_entry
{
    // First two MSB are the key or button, the mods ord together are the next byte, and the action is the LSB
    small_str name{};
    u32 key{};
    u32 flags{};
    input_event_func *cb{};
    void *cb_user_param{};
};

struct input_keymap
{
    small_str name{};
    hashmap *hm{};
};

// The most important keymap is at the back of the array
struct input_keymap_stack
{
    input_keymap *kmaps[MAX_INPUT_CONTEXT_STACK_COUNT] = {};
    u8 count{0};
};

u32 input_keymap_button_key(int key_or_button, int modifiers, int action);
u32 input_keymap_cursor_key(int modifiers);
u32 input_keymap_scroll_key(int modifiers);
int input_button_from_key(u32 key);
int input_mods_from_key(u32 key);
int input_action_from_key(u32 key);

void input_init_keymap(const char *name, input_keymap *km);
void input_terminate_keymap(input_keymap *km);

const input_keymap_entry *input_set_keymap_entry(const input_keymap_entry *entry, input_keymap *km);

const input_keymap_entry *input_get_keymap_entry(const input_keymap_entry *entry, const input_keymap *km);
const input_keymap_entry *input_get_keymap_entry(u32 key, const input_keymap *km);
const input_keymap_entry *input_get_keymap_entry(const char *name, const input_keymap *km);

const input_keymap_entry *input_remove_keymap_entry(const input_keymap_entry *entry, input_keymap *km);

// Map the platform event to input_keymap_entries
void input_map_event(const platform_input_event *raw, const input_keymap_stack *stack);

// Map the frame platform events to input_keymap_entries
void input_map_frame(const platform_frame_input *frame, const input_keymap_stack *stack);

// Push km to the top of the keymap stack - top is highest priority in input_map_event
void input_push_keymap(input_keymap *km, input_keymap_stack *stack);

// Returns true if km is found in stack, otherwise returns false
bool input_keymap_in_stack(const input_keymap *km, const input_keymap_stack *stack);

// Pop to top keymap from the stack and return it
input_keymap *input_pop_keymap(input_keymap_stack *stack);

} // namespace nslib
