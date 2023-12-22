#include "memory.h"
#define STBI_MALLOC mem_alloc
#define STBI_REALLOC mem_realloc
#define STBI_FREE mem_free
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
