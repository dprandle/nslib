#include "memory.h"
#define STBI_MALLOC nslib::mem_alloc
#define STBI_REALLOC nslib::mem_realloc
#define STBI_FREE nslib::mem_free
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
