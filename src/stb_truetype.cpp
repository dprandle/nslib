#include "memory.h"
#define STBTT_malloc(x, u) nslib::mem_alloc(x, (nslib::mem_arena*)u)
#define STBTT_free(x, u) ((void)(u), nslib::mem_free(x))
#define STB_TRUETYPE_IMPLEMENTATION
#include "stb_truetype.h"
