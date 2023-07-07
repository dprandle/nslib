#pragma once

#include "hashmap.h"
#include "input_kmcodes.h"

namespace nslib
{
constexpr const int MAX_INPUT_CONTEXT_STACK_COUNT = 16;
constexpr const u8 MAX_INPUT_FRAME_EVENTS = 255;

enum raw_input_event_type
{
    RAW_INPUT_EVENT_TYPE_KEY_PRESS,
    RAW_INPUT_EVENT_TYPE_MOUSE_BTN,
    RAW_INPUT_EVENT_TYPE_SCROLL,
    RAW_INPUT_EVENT_TYPE_CURSOR_POS
};

struct raw_input_event
{
    int type{-1};
    i32 key_or_button{};
    i32 scancode{};
    i32 action{};
    i32 mods{};
    double offset[2];
    double pos[2];
};

struct frame_input
{
    raw_input_event events[MAX_INPUT_FRAME_EVENTS];
    u8 count{0};
};

struct input_keymap_entry
{
    u32 key;
    char name[24];
};

struct input_keymap
{
    char name[24];
    hashmap *hm{};
};

using input_context_stack = input_keymap[MAX_INPUT_CONTEXT_STACK_COUNT];


} // namespace nslib
