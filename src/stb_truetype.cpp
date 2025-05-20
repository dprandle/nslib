#include "logging.h"
#include "memory.h"
#define STBTT_assert asrt
#define STBTT_malloc(x, u) nslib::mem_alloc(x, (nslib::mem_arena*)u)
#define STBTT_free(x, u) nslib::mem_free(x, (nslib::mem_arena *)u)
#define STB_TRUETYPE_IMPLEMENTATION
#include "stb_truetype.h"
