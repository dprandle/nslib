#include "model.h"

namespace nslib
{

intern const vertex RECT_VERTS[] = {{{-0.5f, -0.5f, 0.0f}, {0.0f, 0.0f}, 0xa0ff0000}, // Colors are ARGB - msb gets alpha
                                    {{0.5f, -0.5f, 0.0f}, {1.0f, 0.0f}, 0xa000ff00},
                                    {{0.5f, 0.5f, 0.0f}, {1.0f, 1.0f}, 0xa00000ff},
                                    {{-0.5f, 0.5f, 0.0f}, {0.0f, 1.0f}, 0xa000ffff}};

intern const ind_t RECT_INDS_TRI_LIST[] = {0, 1, 2, 2, 3, 0};

intern vertex CUBE_VERTS[] = {{{-0.5f, 0.5f, 0.5f}, {0.0f, 1.0f}, 0xff000000},
                              {{0.5f, 0.5f, 0.5f}, {1.0f, 1.0f}, 0xff0000ff},
                              {{-0.5f, -0.5f, 0.5f}, {0.0f, 0.0f}, 0xff00ff00},
                              {{0.5f, -0.5f, 0.5f}, {1.0f, 0.0f}, 0xff00ffff},
                              {{-0.5f, 0.5f, -0.5f}, {0.0f, 1.0f}, 0xffff0000},
                              {{0.5f, 0.5f, -0.5f}, {1.0f, 1.0f}, 0xffff00ff},
                              {{-0.5f, -0.5f, -0.5f}, {0.0f, 0.0f}, 0xffffff00},
                              {{0.5f, -0.5f, -0.5f}, {1.0f, 0.0f}, 0xffffffff}};

intern const ind_t CUBE_INDS_TRI_LIST[] = {0, 1, 2,          // 0
                                           1, 3, 2, 4, 6, 5, // 2
                                           5, 6, 7, 0, 2, 4, // 4
                                           4, 2, 6, 1, 5, 3, // 6
                                           5, 7, 3, 0, 4, 1, // 8
                                           4, 5, 1, 2, 3, 6, // 10
                                           6, 3, 7};

intern const ind_t CUBE_INDS_TRI_STRIP[] = {0, 1, 2, 3, 7, 1, 5, 0, 4, 2, 6, 7, 4, 5};

intern const ind_t CUBE_INDS_LINE_LIST[] = {0, 1, 0, 2, 0, 4, 1, 3, 1, 5, 2, 3, 2, 6, 3, 7, 4, 5, 4, 6, 5, 7, 6, 7};

intern const ind_t CUBE_INDS_LINE_STRIP[] = {0, 2, 3, 1, 5, 7, 6, 4, 0, 2, 6, 4, 5, 7, 3, 1, 0};

intern const ind_t CUBE_INDS_POINTS[] = {0, 1, 2, 3, 4, 5, 6, 7};

void init_texture(texture *tex, mem_arena *arena)
{
    tex->arena = arena;
}

void terminate_texture(texture *tex)
{}

void init_material(material *mat, mem_arena *arena)
{
    asrt(!mat->pipelines.hashf);
    asrt(mat->textures.size == 0);
    hset_init(&mat->pipelines, arena);
}

void terminate_material(material *mat)
{
    hset_terminate(&mat->pipelines);
}

// TODO: Need to try this
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

void make_rect(mesh *msh)
{
    asrt(msh->submeshes.size == 0);
    arr_resize(&msh->submeshes, 1);
    init_submesh(msh->submeshes.data, msh->arena);
    make_rect_submesh(msh->submeshes.data);
}

void make_cube(mesh *msh)
{
    asrt(msh->submeshes.size == 0);
    arr_resize(&msh->submeshes, 1);
    init_submesh(msh->submeshes.data, msh->arena);
    make_cube_submesh(msh->submeshes.data);
}

void init_submesh(submesh *sm, mem_arena *arena)
{
    arr_init(&sm->verts, arena, SIMD_MIN_ALIGNMENT);
    arr_init(&sm->cjoints, arena, SIMD_MIN_ALIGNMENT);
    arr_init(&sm->inds, arena);
}

void terminate_submesh(submesh *sm)
{
    arr_terminate(&sm->verts);
    arr_terminate(&sm->cjoints);
    arr_terminate(&sm->inds);
}

void init_mesh(mesh *msh, mem_arena *arena)
{
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
