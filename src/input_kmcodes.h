#pragma once

#include "basic_types.h"

namespace nslib
{
enum keycode : u16
{
    KEY_UNKNOWN,             // 0
    KEY_RETURN = '\r',       // '\r'
    KEY_ESCAPE = '\x1B',     // '\x1B'
    KEY_BACKSPACE = '\b',    // '\b'
    KEY_TAB = '\t',          // '\t'
    KEY_SPACE = ' ',         // ' '
    KEY_EXCLAIM,             // '!'
    KEY_DBLAPOSTROPHE,       // '"'
    KEY_HASH,                // '#'
    KEY_DOLLAR,              // '$'
    KEY_PERCENT,             // '%'
    KEY_AMPERSAND,           // '&'
    KEY_APOSTROPHE,          // '\''
    KEY_LEFTPAREN,           // '('
    KEY_RIGHTPAREN,          // ')'
    KEY_ASTERISK,            // '*'
    KEY_PLUS,                // '+'
    KEY_COMMA,               // ','
    KEY_MINUS,               // '-'
    KEY_PERIOD,              // '.'
    KEY_SLASH,               // '/'
    KEY_0,                   // '0'
    KEY_1,                   // '1'
    KEY_2,                   // '2'
    KEY_3,                   // '3'
    KEY_4,                   // '4'
    KEY_5,                   // '5'
    KEY_6,                   // '6'
    KEY_7,                   // '7'
    KEY_8,                   // '8'
    KEY_9,                   // '9'
    KEY_COLON,               // ':'
    KEY_SEMICOLON,           // ';'
    KEY_LESS,                // '<'
    KEY_EQUALS,              // '='
    KEY_GREATER,             // '>'
    KEY_QUESTION,            // '?'
    KEY_AT,                  // '@'
    KEY_LEFTBRACKET = '[',   // '['
    KEY_BACKSLASH,           // '\\'
    KEY_RIGHTBRACKET,        // ']'
    KEY_CARET,               // '^'
    KEY_UNDERSCORE,          // '_'
    KEY_GRAVE,               // '`'
    KEY_A,                   // 'a'
    KEY_B,                   // 'b'
    KEY_C,                   // 'c'
    KEY_D,                   // 'd'
    KEY_E,                   // 'e'
    KEY_F,                   // 'f'
    KEY_G,                   // 'g'
    KEY_H,                   // 'h'
    KEY_I,                   // 'i'
    KEY_J,                   // 'j'
    KEY_K,                   // 'k'
    KEY_L,                   // 'l'
    KEY_M,                   // 'm'
    KEY_N,                   // 'n'
    KEY_O,                   // 'o'
    KEY_P,                   // 'p'
    KEY_Q,                   // 'q'
    KEY_R,                   // 'r'
    KEY_S,                   // 's'
    KEY_T,                   // 't'
    KEY_U,                   // 'u'
    KEY_V,                   // 'v'
    KEY_W,                   // 'w'
    KEY_X,                   // 'x'
    KEY_Y,                   // 'y'
    KEY_Z,                   // 'z'
    KEY_LEFTBRACE,           // '{'
    KEY_PIPE,                // '|'
    KEY_RIGHTBRACE,          // '}'
    KEY_TILDE,               // '~'7
    KEY_DELETE,              // '\x7F'
    KEY_PLUSMINUS = 0x00b1u, // '\xB1'
    KEY_CAPSLOCK,            // Now starting with non printable chars
    KEY_F1,
    KEY_F2,
    KEY_F3,
    KEY_F4,
    KEY_F5,
    KEY_F6,
    KEY_F7,
    KEY_F8,
    KEY_F9,
    KEY_F10,
    KEY_F11,
    KEY_F12,
    KEY_PRINTSCREEN,
    KEY_SCROLLLOCK,
    KEY_PAUSE,
    KEY_INSERT,
    KEY_HOME,
    KEY_PAGEUP,
    KEY_END,
    KEY_PAGEDOWN,
    KEY_RIGHT,
    KEY_LEFT,
    KEY_DOWN,
    KEY_UP,
    KEY_NUMLOCKCLEAR,
    KEY_KP_DIVIDE,
    KEY_KP_MULTIPLY,
    KEY_KP_MINUS,
    KEY_KP_PLUS,
    KEY_KP_ENTER,
    KEY_KP_1,
    KEY_KP_2,
    KEY_KP_3,
    KEY_KP_4,
    KEY_KP_5,
    KEY_KP_6,
    KEY_KP_7,
    KEY_KP_8,
    KEY_KP_9,
    KEY_KP_0,
    KEY_KP_PERIOD,
    KEY_APPLICATION,
    KEY_POWER,
    KEY_KP_EQUALS,
    KEY_F13,
    KEY_F14,
    KEY_F15,
    KEY_F16,
    KEY_F17,
    KEY_F18,
    KEY_F19,
    KEY_F20,
    KEY_F21,
    KEY_F22,
    KEY_F23,
    KEY_F24,
    KEY_EXECUTE,
    KEY_HELP,
    KEY_MENU,
    KEY_SELECT,
    KEY_STOP,
    KEY_AGAIN,
    KEY_UNDO,
    KEY_CUT,
    KEY_COPY,
    KEY_PASTE,
    KEY_FIND,
    KEY_MUTE,
    KEY_VOLUMEUP,
    KEY_VOLUMEDOWN,
    KEY_KP_COMMA,
    KEY_KP_EQUALSAS400,
    KEY_ALTERASE,
    KEY_SYSREQ,
    KEY_CANCEL,
    KEY_CLEAR,
    KEY_PRIOR,
    KEY_RETURN2,
    KEY_SEPARATOR,
    KEY_OUT,
    KEY_OPER,
    KEY_CLEARAGAIN,
    KEY_CRSEL,
    KEY_EXSEL,
    KEY_KP_00,
    KEY_KP_000,
    KEY_THOUSANDSSEPARATOR,
    KEY_DECIMALSEPARATOR,
    KEY_CURRENCYUNIT,
    KEY_CURRENCYSUBUNIT,
    KEY_KP_LEFTPAREN,
    KEY_KP_RIGHTPAREN,
    KEY_KP_LEFTBRACE,
    KEY_KP_RIGHTBRACE,
    KEY_KP_TAB,
    KEY_KP_BACKSPACE,
    KEY_KP_A,
    KEY_KP_B,
    KEY_KP_C,
    KEY_KP_D,
    KEY_KP_E,
    KEY_KP_F,
    KEY_KP_XOR,
    KEY_KP_POWER,
    KEY_KP_PERCENT,
    KEY_KP_LESS,
    KEY_KP_GREATER,
    KEY_KP_AMPERSAND,
    KEY_KP_DBLAMPERSAND,
    KEY_KP_VERTICALBAR,
    KEY_KP_DBLVERTICALBAR,
    KEY_KP_COLON,
    KEY_KP_HASH,
    KEY_KP_SPACE,
    KEY_KP_AT,
    KEY_KP_EXCLAM,
    KEY_KP_MEMSTORE,
    KEY_KP_MEMRECALL,
    KEY_KP_MEMCLEAR,
    KEY_KP_MEMADD,
    KEY_KP_MEMSUBTRACT,
    KEY_KP_MEMMULTIPLY,
    KEY_KP_MEMDIVIDE,
    KEY_KP_PLUSMINUS,
    KEY_KP_CLEAR,
    KEY_KP_CLEARENTRY,
    KEY_KP_BINARY,
    KEY_KP_OCTAL,
    KEY_KP_DECIMAL,
    KEY_KP_HEXADECIMAL,
    KEY_LCTRL,
    KEY_LSHIFT,
    KEY_LALT,
    KEY_LGUI,
    KEY_RCTRL,
    KEY_RSHIFT,
    KEY_RALT,
    KEY_RGUI,
    KEY_MODE,
    KEY_SLEEP,
    KEY_WAKE,
    KEY_CHANNEL_INCREMENT,
    KEY_CHANNEL_DECREMENT,
    KEY_MEDIA_PLAY,
    KEY_MEDIA_PAUSE,
    KEY_MEDIA_RECORD,
    KEY_MEDIA_FAST_FORWARD,
    KEY_MEDIA_REWIND,
    KEY_MEDIA_NEXT_TRACK,
    KEY_MEDIA_PREVIOUS_TRACK,
    KEY_MEDIA_STOP,
    KEY_MEDIA_EJECT,
    KEY_MEDIA_PLAY_PAUSE,
    KEY_MEDIA_SELECT,
    KEY_AC_NEW,
    KEY_AC_OPEN,
    KEY_AC_CLOSE,
    KEY_AC_EXIT,
    KEY_AC_SAVE,
    KEY_AC_PRINT,
    KEY_AC_PROPERTIES,
    KEY_AC_SEARCH,
    KEY_AC_HOME,
    KEY_AC_BACK,
    KEY_AC_FORWARD,
    KEY_AC_STOP,
    KEY_AC_REFRESH,
    KEY_AC_BOOKMARKS,
    KEY_SOFTLEFT,
    KEY_SOFTRIGHT,
    KEY_CALL,
    KEY_ENDCALL,
    KEY_LEFT_TAB,          // Extended key Left Tab
    KEY_LEVEL5_SHIFT,      // Extended key Level 5 Shift
    KEY_MULTI_KEY_COMPOSE, // Extended key Multi-key Compose
    KEY_LMETA,             // Extended key Left Meta
    KEY_RMETA,             // Extended key Right Meta
    KEY_LHYPER,            // Extended key Left Hyper
    KEY_RHYPER             // Extended key Right Hyper
};

// Modifier Masks
inline constexpr u16 KEY_MOD_NONE = 0u;                                 // no modifier is applicable
inline constexpr u16 KEY_MOD_LSHIFT = 1u << 0;                          // the left Shift key is down.
inline constexpr u16 KEY_MOD_RSHIFT = 1u << 1;                          // the right Shift key is down.
inline constexpr u16 KEY_MOD_LEVEL5 = 1u << 2;                          // the Level 5 Shift key is down.
inline constexpr u16 KEY_MOD_LCTRL = 1u << 3;                           // the left Ctrl (Control) key is down.
inline constexpr u16 KEY_MOD_RCTRL = 1u << 4;                           // the right Ctrl (Control) key is down.
inline constexpr u16 KEY_MOD_LALT = 1u << 5;                            // the left Alt key is down.
inline constexpr u16 KEY_MOD_RALT = 1u << 6;                            // the right Alt key is down.
inline constexpr u16 KEY_MOD_LGUI = 1u << 7;                            // the left GUI key (often the Windows key) is down.
inline constexpr u16 KEY_MOD_RGUI = 1u << 8;                            // the right GUI key (often the Windows key) is down.
inline constexpr u16 KEY_MOD_NUM = 1u << 9;                             // the Num Lock key (may be located on an extended keypad) is down.
inline constexpr u16 KEY_MOD_CAPS = 1u << 10;                           // the Caps Lock key is down.
inline constexpr u16 KEY_MOD_MODE = 1u << 11;                           // the !AltGr key is down.
inline constexpr u16 KEY_MOD_SCROLL = 1u << 12;                         // the Scroll Lock key is down.
inline constexpr u16 KEY_MOD_ANY = 1u << 13;                            // Any modifier key - essentially we don't care which
inline constexpr u16 KEY_MOD_CTRL = (KEY_MOD_LCTRL | KEY_MOD_RCTRL);    // Any Ctrl key is down.
inline constexpr u16 KEY_MOD_SHIFT = (KEY_MOD_LSHIFT | KEY_MOD_RSHIFT); // Any Shift key is down.
inline constexpr u16 KEY_MOD_ALT = (KEY_MOD_LALT | KEY_MOD_RALT);       // Any Alt key is down.
inline constexpr u16 KEY_MOD_GUI = (KEY_MOD_LGUI | KEY_MOD_RGUI);       // Any GUI key is down.

inline constexpr u16 MOUSE_BTN_LEFT = 1;
inline constexpr u16 MOUSE_BTN_MIDDLE = 2;
inline constexpr u16 MOUSE_BTN_RIGHT = 3;
inline constexpr u16 MOUSE_BTN_X1 = 4;
inline constexpr u16 MOUSE_BTN_X2 = 5;

#define MOUSE_BTN_MASK(X) (1u << ((X) - 1))
inline constexpr u16 MOUSE_BTN_LMASK = MOUSE_BTN_MASK(MOUSE_BTN_LEFT);
inline constexpr u16 MOUSE_BTN_MMASK = MOUSE_BTN_MASK(MOUSE_BTN_MIDDLE);
inline constexpr u16 MOUSE_BTN_RMASK = MOUSE_BTN_MASK(MOUSE_BTN_RIGHT);
inline constexpr u16 MOUSE_BTN_X1MASK = MOUSE_BTN_MASK(MOUSE_BTN_X1);
inline constexpr u16 MOUSE_BTN_X2MASK = MOUSE_BTN_MASK(MOUSE_BTN_X2);

inline const u16 SCROLL_CHANGE = 8;
inline const u16 CURSOR_POS_CHANGE = 9;

inline const s8 INPUT_ACTION_PRESS = 1;
inline const s8 INPUT_ACTION_RELEASE = 0;
inline const s8 INPUT_ACTION_REPEAT = 2;
} // namespace nslib
