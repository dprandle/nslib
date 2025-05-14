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

struct input_trigger
{
    const char *name{};
    const platform_input_event *ev;
};

using input_event_func = void(const input_trigger *ev, void *user);

struct input_keymap_entry
{
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
    // Key (id): Starting from MSB to LSB
    // | ---- kmcode (10 bits) --- | ---- keymods (14 bits) ---- | ---- mbutton_mask (8 bits) ---- |
    hmap<u32, input_keymap_entry> hm{};
};

// Keymaps are owned elsewhere - likely an asset as they will just have keyboard shortcuts.... assigned to what?
struct input_keymap_stack
{
    input_keymap *kmaps[MAX_INPUT_CONTEXT_STACK_COUNT] = {};
    u8 count{0};
};

// Get the hash key for the passed in key/mouse button, modifiers, action combination
u32 generate_keymap_id(input_kmcode code, u16 keymods, u8 mbutton_mask);

// Get the key/mouse button code from the hash key
input_kmcode get_kmcode_from_keymap_id(u32 id);

// Get the modifiers from the hash key
u16 get_keymods_from_keymap_id(u32 id);

// Get the action from the hash key
u8 get_mbutton_mask_from_keymap_id(u32 key);

// Set keymap entry overwriting an existing one if its there
void set_keymap_entry(u32 id, const input_keymap_entry *entry, input_keymap *km);

// Insert keymap entry - if it exists return null otherwise return the inserted entry
input_keymap_entry *insert_keymap_entry(u32 id, const input_keymap_entry *entry, input_keymap *km);

// Find keymap entry by key and return it - return null if no match is found
input_keymap_entry *find_keymap_entry(u32 id, input_keymap *km);

// Find keymap entry by key and return it - return null if no match is found
const input_keymap_entry *find_keymap_entry(u32 id, const input_keymap *km);

// Find the keymap entry with name and return it - return null if no match is found
input_keymap_entry *find_keymap_entry(const char *name, input_keymap *km);

// Find the keymap entry with name and return it - return null if no match is found
const input_keymap_entry *find_keymap_entry(const char *name, const input_keymap *km);

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
