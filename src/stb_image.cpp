#include "memory.h"
#include "logging.h"
#define STBI_MALLOC(sz) nslib::mem_alloc(sz, nslib::mem_global_arena())
#define STBI_REALLOC(ptr, sz) nslib::mem_realloc(ptr, sz, nslib::mem_global_arena())
#define STBI_FREE(ptr) nslib::mem_free(ptr, nslib::mem_global_arena())
#define STBI_ASSERT asrt
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
