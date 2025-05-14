#pragma once

#include "math/vector2.h"
#include "containers/string.h"
#include "containers/hmap.h"
#include "input_kmcodes.h"

namespace nslib
{
constexpr const u8 MAX_INPUT_CONTEXT_STACK_COUNT = 32;

struct mem_arena;
struct platform_input_event;
struct platform_frame_input_events;

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
    vec2 pos{};
    vec2 norm_pos{};
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
    // First 14 bits are key/button, next 10 bits are modifiers, and last 8 bits are action
    string name{};
    u32 flags{};
    input_event_func *cb{};
    void *cb_user_param{};
};

bool operator==(const input_keymap_entry &lhs, const input_keymap_entry &rhs);
inline bool operator!=(const input_keymap_entry &lhs, const input_keymap_entry &rhs)
{
    return !(lhs == rhs);
}

struct input_keymap
{
    string name{};
    hmap<u32, input_keymap_entry> hm{};
};

// Keymaps are owned elsewhere - likely an asset as they will just have keyboard shortcuts.... assigned to what?
struct input_keymap_stack
{
    input_keymap *kmaps[MAX_INPUT_CONTEXT_STACK_COUNT] = {};
    u8 count{0};
};

// Get the hash key for the passed in key/mouse button, modifiers, action combination
u32 keymap_button_key(int key_or_button, int modifiers, int action);

// Get the hash key for a cursor movement entry with the passed in modifiers
u32 keymap_cursor_key(int modifiers);

// Get the hash key for a scroll entry with the passed in modifiers
u32 keymap_scroll_key(int modifiers);

// Get the key/mouse button code from the hash key
int button_from_key(u32 key);

// Get the modifiers from the hash key
int mods_from_key(u32 key);

// Get the action from the hash key
int action_from_key(u32 key);

// Set keymap entry overwriting an existing one if its there - returns true if overwrote an existing entry otherwise
// returns false
input_keymap_entry *set_keymap_entry(u32 key, const input_keymap_entry *entry, input_keymap *km);

// Find keymap entry by key and return it - return null if no match is found
input_keymap_entry *get_keymap_entry(u32 key, input_keymap *km);

// Find the keymap entry with name and return it - return null if no match is found
input_keymap_entry *get_keymap_entry(const char *name, input_keymap *km);

// Find keymap entry by key and return it - return null if no match is found
const input_keymap_entry *get_keymap_entry(u32 key, const input_keymap *km);

// Find the keymap entry with name and return it - return null if no match is found
const input_keymap_entry *get_keymap_entry(const char *name, const input_keymap *km);
    
// Remove a keymap entry - returns true if removed
bool remove_keymap_entry(u32 key, input_keymap *km);

// Map the platform event to input_keymap_entries
void map_input_event(const platform_input_event *raw, const input_keymap_stack *stack);

// Map the frame platform events to input_keymap_entries
void map_input_frame(const platform_frame_input_events *frame, const input_keymap_stack *stack);

// Push km to the top of the keymap stack - top is highest priority in input_map_event
void push_keymap(input_keymap *km, input_keymap_stack *stack);

// Returns true if km is found in stack, otherwise returns false
bool keymap_in_stack(const input_keymap *km, const input_keymap_stack *stack);

// Pop to top keymap from the stack and return it
input_keymap *pop_keymap(input_keymap_stack *stack);

// Initialize the keymap and allocate the hashmap
void init_keymap(const char *name, input_keymap *km, mem_arena *arena);

// Tear down the keymap and free the hashmap
void terminate_keymap(input_keymap *km);

} // namespace nslib
