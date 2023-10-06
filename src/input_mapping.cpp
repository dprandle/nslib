#include <cstring>
#include <stdlib.h>
#include <GLFW/glfw3.h>

#include "logging.h"
#include "input_mapping.h"
#include "containers/hashmap.h"
#include "platform.h"

namespace nslib
{

intern void fill_event_from_platform_event(const platform_input_event *raw, input_event *ev)
{
    ev->modifiers = raw->mods;
    if (raw->type == PLATFORM_INPUT_EVENT_TYPE_CURSOR_POS) {
        ev->type = IEVENT_TYPE_CURSOR;
        ev->cursor_data.pos = raw->pos;
        ev->cursor_data.norm_pos = raw->pos / platform_window_size(raw->win_hndl);
    }
    else if (raw->type == PLATFORM_INPUT_EVENT_TYPE_SCROLL) {
        ev->type = IEVENT_TYPE_SCROLL;
        ev->scroll_data.offset = raw->offset;
    }
    else {
        ev->type = IEVENT_TYPE_BTN;
        ev->btn_data.action = raw->action;
        ev->btn_data.key_or_button = raw->key_or_button;
    }
}

u64 hash_type(const pair<const u32, input_keymap_entry> *item, u32 seed0, u32 seed1)
{
    return item->first;
}

bool operator==(const input_keymap_entry &lhs, const input_keymap_entry &rhs)
{
    return strcmp(lhs.name, rhs.name);
}

u32 input_keymap_button_key(int key_or_button, int modifiers, int action)
{
    return (key_or_button << 18) | (modifiers << 8) | action;
}

u32 input_keymap_cursor_key(int modifiers)
{
    return input_keymap_button_key(CURSOR_POS_CHANGE, modifiers, 0);
}

u32 input_keymap_scroll_key(int modifiers)
{
    return input_keymap_button_key(SCROLL_CHANGE, modifiers, 0);
}

int input_button_from_key(u32 key)
{
    return (key >> 18);
}

int input_mods_from_key(u32 key)
{
    return ((key >> 8) & 0x000003FF);
}

int input_action_from_key(u32 key)
{
    return (key & 0x000000FF);
}

void input_init_keymap(const char *name, input_keymap *km)
{
    assert(km);
    assert(name);
    int seed0 = rand();
    int seed1 = rand();
    strncpy(km->name, name, SMALL_STR_LEN);
    hashmap_init(&km->hm);
}

void input_terminate_keymap(input_keymap *km)
{
    assert(km);
    hashmap_terminate(&km->hm);
    memset(km, 0, sizeof(input_keymap));
}

bool input_set_keymap_entry(u32 key, const input_keymap_entry *entry, input_keymap *km)
{
    assert(entry);
    assert(km);
    return hashmap_set(&km->hm, key, entry);
}

const input_keymap_entry *input_get_keymap_entry(u32 key, const input_keymap *km)
{
    assert(km);
    auto item = hashmap_find(&km->hm, key);
    if (item) {
        return &item->second;
    }
    return nullptr;
}

const input_keymap_entry *input_get_keymap_entry(const char *name, const input_keymap *km)
{
    assert(name);
    assert(km);
    sizet i{0};
    auto item = hashmap_iter(&km->hm, &i);
    while (item) {
        if (strncmp(name, item->second.name, SMALL_STR_LEN) == 0) {
            return &item->second;
        }
        item = hashmap_iter(&km->hm, &i);
    }
    return nullptr;
}

bool input_remove_keymap_entry(u32 key, input_keymap *km)
{
    assert(km);
    return hashmap_remove(&km->hm, key);
}

// Push km to the top of the keymap stack - top is highest priority in input_map_event
void input_push_keymap(input_keymap *km, input_keymap_stack *stack)
{
    assert(km);
    assert(stack);
    assert(stack->count + 1 <= MAX_INPUT_CONTEXT_STACK_COUNT);
    stack->kmaps[stack->count] = km;
    ++stack->count;
}

bool input_keymap_in_stack(const input_keymap *km, const input_keymap_stack *stack)
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
input_keymap *input_pop_keymap(input_keymap_stack *stack)
{
    assert(stack);
    if (stack->count == 0) {
        return nullptr;
    }
    --stack->count;
    return stack->kmaps[stack->count];
}

void input_map_event(const platform_input_event *raw, const input_keymap_stack *stack)
{
    assert(raw);
    assert(stack);
    input_event ev{};
    u32 key = input_keymap_button_key(raw->key_or_button, raw->mods, raw->action);
    u32 key_any = input_keymap_button_key(raw->key_or_button, MOD_ANY, raw->action);

    for (u8 i = stack->count; i != 0; --i) {
        const input_keymap *cur_map = stack->kmaps[i - 1];
        const input_keymap_entry *kentry = input_get_keymap_entry(key, cur_map);
        const input_keymap_entry *kanymod = input_get_keymap_entry(key_any, cur_map);

        bool should_return = false;
        if (kentry) {
            ev.name = kentry->name;
            fill_event_from_platform_event(raw, &ev);
            if (kentry->cb) {
                kentry->cb(&ev, kentry->cb_user_param);
            }
            should_return = !test_flags(kentry->flags, IEVENT_FLAG_DONT_CONSUME);
        }
        if (kanymod && kanymod != kentry) {
            ev.name = kanymod->name;
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

void input_map_frame(const platform_frame_input *frame, const input_keymap_stack *stack)
{
    assert(frame);
    assert(stack);
    for (int i = 0; i < frame->count; ++i) {
        input_map_event(&frame->events[i], stack);
    }
}

/* The unknown key */
dllapi const i16 KEY_UNKNOWN = GLFW_KEY_UNKNOWN;

/* Printable */
dllapi const i16 KEY_SPACE = GLFW_KEY_SPACE;
dllapi const i16 KEY_APOSTROPHE = GLFW_KEY_APOSTROPHE;
dllapi const i16 KEY_COMMA = GLFW_KEY_COMMA;
dllapi const i16 KEY_MINUS = GLFW_KEY_MINUS;
dllapi const i16 KEY_PERIOD = GLFW_KEY_PERIOD;
dllapi const i16 KEY_SLASH = GLFW_KEY_SLASH;
dllapi const i16 KEY_N0 = GLFW_KEY_0;
dllapi const i16 KEY_N1 = GLFW_KEY_1;
dllapi const i16 KEY_N2 = GLFW_KEY_2;
dllapi const i16 KEY_N3 = GLFW_KEY_3;
dllapi const i16 KEY_N4 = GLFW_KEY_4;
dllapi const i16 KEY_N5 = GLFW_KEY_5;
dllapi const i16 KEY_N6 = GLFW_KEY_6;
dllapi const i16 KEY_N7 = GLFW_KEY_7;
dllapi const i16 KEY_N8 = GLFW_KEY_8;
dllapi const i16 KEY_N9 = GLFW_KEY_9;
dllapi const i16 KEY_SEMICOLON = GLFW_KEY_SEMICOLON;
dllapi const i16 KEY_EQUAL = GLFW_KEY_EQUAL;
dllapi const i16 KEY_A = GLFW_KEY_A;
dllapi const i16 KEY_B = GLFW_KEY_B;
dllapi const i16 KEY_C = GLFW_KEY_C;
dllapi const i16 KEY_D = GLFW_KEY_D;
dllapi const i16 KEY_E = GLFW_KEY_E;
dllapi const i16 KEY_F = GLFW_KEY_F;
dllapi const i16 KEY_G = GLFW_KEY_G;
dllapi const i16 KEY_H = GLFW_KEY_H;
dllapi const i16 KEY_I = GLFW_KEY_I;
dllapi const i16 KEY_J = GLFW_KEY_J;
dllapi const i16 KEY_K = GLFW_KEY_K;
dllapi const i16 KEY_L = GLFW_KEY_L;
dllapi const i16 KEY_M = GLFW_KEY_M;
dllapi const i16 KEY_N = GLFW_KEY_N;
dllapi const i16 KEY_O = GLFW_KEY_O;
dllapi const i16 KEY_P = GLFW_KEY_P;
dllapi const i16 KEY_Q = GLFW_KEY_Q;
dllapi const i16 KEY_R = GLFW_KEY_R;
dllapi const i16 KEY_S = GLFW_KEY_S;
dllapi const i16 KEY_T = GLFW_KEY_T;
dllapi const i16 KEY_U = GLFW_KEY_U;
dllapi const i16 KEY_V = GLFW_KEY_V;
dllapi const i16 KEY_W = GLFW_KEY_W;
dllapi const i16 KEY_X = GLFW_KEY_X;
dllapi const i16 KEY_Y = GLFW_KEY_Y;
dllapi const i16 KEY_Z = GLFW_KEY_Z;
dllapi const i16 KEY_LEFT_BRACKET = GLFW_KEY_LEFT_BRACKET;
dllapi const i16 KEY_BACKSLASH = GLFW_KEY_BACKSLASH;
dllapi const i16 KEY_RIGHT_BRACKET = GLFW_KEY_RIGHT_BRACKET;
dllapi const i16 KEY_GRAVE_ACCENT = GLFW_KEY_GRAVE_ACCENT;
dllapi const i16 KEY_WORLD_1 = GLFW_KEY_WORLD_1;
dllapi const i16 KEY_WORLD_2 = GLFW_KEY_WORLD_2;

/* */
dllapi const i16 KEY_ESCAPE = GLFW_KEY_ESCAPE;
dllapi const i16 KEY_ENTER = GLFW_KEY_ENTER;
dllapi const i16 KEY_TAB = GLFW_KEY_TAB;
dllapi const i16 KEY_BACKSPACE = GLFW_KEY_BACKSPACE;
dllapi const i16 KEY_INSERT = GLFW_KEY_INSERT;
dllapi const i16 KEY_DELETE = GLFW_KEY_DELETE;
dllapi const i16 KEY_RIGHT = GLFW_KEY_RIGHT;
dllapi const i16 KEY_LEFT = GLFW_KEY_LEFT;
dllapi const i16 KEY_DOWN = GLFW_KEY_DOWN;
dllapi const i16 KEY_UP = GLFW_KEY_UP;
dllapi const i16 KEY_PAGE_UP = GLFW_KEY_PAGE_UP;
dllapi const i16 KEY_PAGE_DOWN = GLFW_KEY_PAGE_DOWN;
dllapi const i16 KEY_HOME = GLFW_KEY_HOME;
dllapi const i16 KEY_END = GLFW_KEY_END;
dllapi const i16 KEY_CAPS_LOCK = GLFW_KEY_CAPS_LOCK;
dllapi const i16 KEY_SCROLL_LOCK = GLFW_KEY_SCROLL_LOCK;
dllapi const i16 KEY_NUM_LOCK = GLFW_KEY_NUM_LOCK;
dllapi const i16 KEY_PRINT_SCREEN = GLFW_KEY_PRINT_SCREEN;
dllapi const i16 KEY_PAUSE = GLFW_KEY_PAUSE;
dllapi const i16 KEY_F1 = GLFW_KEY_F1;
dllapi const i16 KEY_F2 = GLFW_KEY_F2;
dllapi const i16 KEY_F3 = GLFW_KEY_F3;
dllapi const i16 KEY_F4 = GLFW_KEY_F4;
dllapi const i16 KEY_F5 = GLFW_KEY_F5;
dllapi const i16 KEY_F6 = GLFW_KEY_F6;
dllapi const i16 KEY_F7 = GLFW_KEY_F7;
dllapi const i16 KEY_F8 = GLFW_KEY_F8;
dllapi const i16 KEY_F9 = GLFW_KEY_F9;
dllapi const i16 KEY_F10 = GLFW_KEY_F10;
dllapi const i16 KEY_F11 = GLFW_KEY_F11;
dllapi const i16 KEY_F12 = GLFW_KEY_F12;
dllapi const i16 KEY_F13 = GLFW_KEY_F13;
dllapi const i16 KEY_F14 = GLFW_KEY_F14;
dllapi const i16 KEY_F15 = GLFW_KEY_F15;
dllapi const i16 KEY_F16 = GLFW_KEY_F16;
dllapi const i16 KEY_F17 = GLFW_KEY_F17;
dllapi const i16 KEY_F18 = GLFW_KEY_F18;
dllapi const i16 KEY_F19 = GLFW_KEY_F19;
dllapi const i16 KEY_F20 = GLFW_KEY_F20;
dllapi const i16 KEY_F21 = GLFW_KEY_F21;
dllapi const i16 KEY_F22 = GLFW_KEY_F22;
dllapi const i16 KEY_F23 = GLFW_KEY_F23;
dllapi const i16 KEY_F24 = GLFW_KEY_F24;
dllapi const i16 KEY_F25 = GLFW_KEY_F25;
dllapi const i16 KEY_KP_0 = GLFW_KEY_KP_0;
dllapi const i16 KEY_KP_1 = GLFW_KEY_KP_1;
dllapi const i16 KEY_KP_2 = GLFW_KEY_KP_2;
dllapi const i16 KEY_KP_3 = GLFW_KEY_KP_3;
dllapi const i16 KEY_KP_4 = GLFW_KEY_KP_4;
dllapi const i16 KEY_KP_5 = GLFW_KEY_KP_5;
dllapi const i16 KEY_KP_6 = GLFW_KEY_KP_6;
dllapi const i16 KEY_KP_7 = GLFW_KEY_KP_7;
dllapi const i16 KEY_KP_8 = GLFW_KEY_KP_8;
dllapi const i16 KEY_KP_9 = GLFW_KEY_KP_9;
dllapi const i16 KEY_KP_DECIMAL = GLFW_KEY_KP_DECIMAL;
dllapi const i16 KEY_KP_DIVIDE = GLFW_KEY_KP_DIVIDE;
dllapi const i16 KEY_KP_MULTIPLY = GLFW_KEY_KP_MULTIPLY;
dllapi const i16 KEY_KP_SUBTRACT = GLFW_KEY_KP_SUBTRACT;
dllapi const i16 KEY_KP_ADD = GLFW_KEY_KP_ADD;
dllapi const i16 KEY_KP_ENTER = GLFW_KEY_KP_ENTER;
dllapi const i16 KEY_KP_EQUAL = GLFW_KEY_KP_EQUAL;
dllapi const i16 KEY_LEFT_SHIFT = GLFW_KEY_LEFT_SHIFT;
dllapi const i16 KEY_LEFT_CONTROL = GLFW_KEY_LEFT_CONTROL;
dllapi const i16 KEY_LEFT_ALT = GLFW_KEY_LEFT_ALT;
dllapi const i16 KEY_LEFT_SUPER = GLFW_KEY_LEFT_SUPER;
dllapi const i16 KEY_RIGHT_SHIFT = GLFW_KEY_RIGHT_SHIFT;
dllapi const i16 KEY_RIGHT_CONTROL = GLFW_KEY_RIGHT_CONTROL;
dllapi const i16 KEY_RIGHT_ALT = GLFW_KEY_RIGHT_ALT;
dllapi const i16 KEY_RIGHT_SUPER = GLFW_KEY_RIGHT_SUPER;
dllapi const i16 KEY_MENU = GLFW_KEY_MENU;

dllapi const i16 KEY_MOD_SHIFT = GLFW_MOD_SHIFT;
dllapi const i16 KEY_MOD_CONTROL = GLFW_MOD_CONTROL;
dllapi const i16 KEY_MOD_ALT = GLFW_MOD_ALT;
dllapi const i16 KEY_MOD_SUPER = GLFW_MOD_SUPER;
dllapi const i16 KEY_MOD_CAPS_LOCK = GLFW_MOD_CAPS_LOCK;
dllapi const i16 KEY_MOD_NUM_LOCK = GLFW_MOD_NUM_LOCK;
dllapi const i16 CURSOR_SCROLL_MOD_MOUSE_LEFT = 0x0040;
dllapi const i16 CURSOR_SCROLL_MOD_MOUSE_RIGHT = 0x0080;
dllapi const i16 CURSOR_SCROLL_MOD_MOUSE_MIDDLE = 0x0100;
dllapi const i16 MOD_ANY = 0x0200;
dllapi const i16 MOD_NONE = 0;

dllapi const i16 MOUSE_BTN_1 = GLFW_MOUSE_BUTTON_1;
dllapi const i16 MOUSE_BTN_2 = GLFW_MOUSE_BUTTON_2;
dllapi const i16 MOUSE_BTN_3 = GLFW_MOUSE_BUTTON_3;
dllapi const i16 MOUSE_BTN_4 = GLFW_MOUSE_BUTTON_4;
dllapi const i16 MOUSE_BTN_5 = GLFW_MOUSE_BUTTON_5;
dllapi const i16 MOUSE_BTN_6 = GLFW_MOUSE_BUTTON_6;
dllapi const i16 MOUSE_BTN_7 = GLFW_MOUSE_BUTTON_7;
dllapi const i16 MOUSE_BTN_8 = GLFW_MOUSE_BUTTON_8;
dllapi const i16 MOUSE_BTN_LAST = GLFW_MOUSE_BUTTON_LAST;
dllapi const i16 MOUSE_BTN_LEFT = GLFW_MOUSE_BUTTON_LEFT;
dllapi const i16 MOUSE_BTN_RIGHT = GLFW_MOUSE_BUTTON_RIGHT;
dllapi const i16 MOUSE_BTN_MIDDLE = GLFW_MOUSE_BUTTON_MIDDLE;
dllapi const i16 SCROLL_CHANGE = 8;
dllapi const i16 CURSOR_POS_CHANGE = 9;

dllapi const i8 INPUT_ACTION_PRESS = GLFW_PRESS;
dllapi const i8 INPUT_ACTION_RELEASE = GLFW_RELEASE;
dllapi const i8 INPUT_ACTION_REPEAT = GLFW_REPEAT;

} // namespace nslib
