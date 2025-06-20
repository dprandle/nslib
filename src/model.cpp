#include "platform.h"
#include "stb_image.h"
#include "model.h"

namespace nslib
{

// Colors are ARGB - msb gets alpha
intern const vertex RECT_VERTS[] = {
    {
        {-0.5f, -0.5f, 0.0f},
        {0.0f, 0.0f},
        0xffff0000,
    },
    {
        {0.5f, -0.5f, 0.0f},
        {1.0f, 0.0f},
        0xff00ff00,
    },
    {
        {0.5f, 0.5f, 0.0f},
        {1.0f, 1.0f},
        0xff0000ff,
    },
    {
        {-0.5f, 0.5f, 0.0f},
        {0.0f, 1.0f},
        0xff00ffff,
    },
};

intern const ind_t RECT_INDS_TRI_LIST[] = {
    0,
    1,
    2,
    2,
    3,
    0,
};

// Colors are ARGB - msb gets alpha
intern vertex CUBE_VERTS[] = {
    {
        {-0.5f, 0.5f, 0.5f},
        {0.0f, 1.0f},
        0xff000000,
    },
    {
        {0.5f, 0.5f, 0.5f},
        {1.0f, 1.0f},
        0xff0000ff,
    },
    {
        {-0.5f, -0.5f, 0.5f},
        {0.0f, 0.0f},
        0xff00ff00,
    },
    {
        {0.5f, -0.5f, 0.5f},
        {1.0f, 0.0f},
        0xff00ffff,
    },
    {
        {-0.5f, 0.5f, -0.5f},
        {0.0f, 1.0f},
        0xffff0000,
    },
    {
        {0.5f, 0.5f, -0.5f},
        {1.0f, 1.0f},
        0xffff00ff,
    },
    {
        {-0.5f, -0.5f, -0.5f},
        {0.0f, 0.0f},
        0xffffff00,
    },
    {
        {0.5f, -0.5f, -0.5f},
        {1.0f, 0.0f},
        0xffffffff,
    },
};

intern const ind_t CUBE_INDS_TRI_LIST[] = {
    0, 1, 2, // 0
    1, 3, 2, // 1
    4, 6, 5, // 2
    5, 6, 7, // 3
    0, 2, 4, // 4
    4, 2, 6, // 5
    1, 5, 3, // 6
    5, 7, 3, // 7
    0, 4, 1, // 8
    4, 5, 1, // 9
    2, 3, 6, // 10
    6, 3, 7, // 11
};

void init_texture(texture *tex, const string &name, mem_arena *arena)
{
    tex->name = name;
    arr_init(&tex->pixels, arena);
}

u32 get_texture_pixel_count(const texture *tex)
{
    return tex->size.w * tex->size.h;
}

sizet get_texture_memsize(const texture *tex)
{
    return get_texture_pixel_count(tex) * tex->channels;
}

bool load_texture(texture *tex, const char *path, cstr *err)
{

    auto stb_pixels = stbi_load(path, (s32 *)&tex->size.w, (s32 *)&tex->size.h, (s32 *)&tex->channels, STBI_rgb_alpha);
    if (stb_pixels) {
        if (tex->channels != STBI_rgb_alpha) {
            ilog("Converted %s from %d to %d channels", path, tex->channels, STBI_rgb_alpha);
            tex->channels = STBI_rgb_alpha;
        }
        auto sz = get_texture_memsize(tex);
        arr_resize(&tex->pixels, sz); // 4 channels for RGBA

        memcpy(tex->pixels.data, stb_pixels, sz);
        stbi_image_free(stb_pixels);
        return true;
    }
    else if (err) {
        *err = stbi_failure_reason();
    }
    return false;
}

void terminate_texture(texture *tex)
{
    arr_terminate(&tex->pixels);
}

void init_material(material *mat, const string &name, mem_arena *arena)
{
    mat->name = name;
    asrt(!mat->pipelines.hashf);
    asrt(mat->textures.size == 0);
    hset_init(&mat->pipelines, arena);
}

void terminate_material(material *mat)
{
    hset_terminate(&mat->pipelines);
}

intern void make_cube_submesh(submesh *sm)
{
    arr_copy(&sm->verts, CUBE_VERTS, 8);
    arr_copy(&sm->inds, CUBE_INDS_TRI_LIST, sizeof(CUBE_INDS_TRI_LIST) / sizeof(ind_t));
}

intern void make_rect_submesh(submesh *sm)
{
    arr_copy(&sm->verts, RECT_VERTS, 4);
    arr_copy(&sm->inds, RECT_INDS_TRI_LIST, sizeof(RECT_INDS_TRI_LIST) / sizeof(ind_t));
}

void make_rect(mesh *msh, const string &name, mem_arena *arena)
{
    init_mesh(msh, name, arena);
    asrt(msh->submeshes.size == 0);
    arr_resize(&msh->submeshes, 1);
    init_submesh(msh->submeshes.data, msh->arena);
    make_rect_submesh(msh->submeshes.data);
}

void make_cube(mesh *msh, const string &name, mem_arena *arena)
{
    init_mesh(msh, name, arena);
    asrt(msh->submeshes.size == 0);
    arr_resize(&msh->submeshes, 1);
    init_submesh(msh->submeshes.data, msh->arena);
    make_cube_submesh(msh->submeshes.data);
}

void init_submesh(submesh *sm, mem_arena *arena)
{
    arr_init(&sm->verts, arena);
    arr_init(&sm->cjoints, arena);
    arr_init(&sm->inds, arena);
}

void terminate_submesh(submesh *sm)
{
    arr_terminate(&sm->verts);
    arr_terminate(&sm->cjoints);
    arr_terminate(&sm->inds);
}

void init_mesh(mesh *msh, const string &name, mem_arena *arena)
{
    msh->name = name;
    asrt(msh->submeshes.size == 0);
    msh->arena = arena;
}

void terminate_mesh(mesh *msh)
{
    for (int i = 0; i < msh->submeshes.size; ++i) {
        terminate_submesh(&msh->submeshes[i]);
    }
}

} // namespace nslib
