#pragma once
#include "robj_common.h"
#include "math/vector4.h"
#include "containers/array.h"
#include "containers/hset.h"

namespace nslib
{

const sizet JOINTS_PER_VERTEX = 4;
const sizet MAX_SUBMESH_COUNT = 16;
const sizet TEXTURE_SLOT_COUNT = 8;
using ind_t = u16;

struct texture
{
    ROBJ(TEXTURE);
    mem_arena *arena;
};

// Material references textures and pipelines, which both must be uploaded to GPUa
struct material
{
    ROBJ(MATERIAL);
    hset<rid> pipelines;
    static_array<rid,TEXTURE_SLOT_COUNT> textures;
    vec4 col;
};

pup_func(material)
{
    pup_member(pipelines);
    pup_member(textures);
    pup_member(col);
}

struct vertex
{
    vec3 pos;
    vec2 tc;
    u32 color;
};

// Connected vertex joint ids and their weights for animation
struct vertex_cjoints
{
    u32 joint_ids[JOINTS_PER_VERTEX];
    f32 weights[JOINTS_PER_VERTEX];
};

struct submesh
{
    array<vertex> verts;
    array<vertex_cjoints> cjoints;
    array<ind_t> inds;
};

struct mesh
{
    ROBJ(MESH);
    static_array<submesh, MAX_SUBMESH_COUNT> submeshes;
    mem_arena *arena;
};

pup_func(mesh)
{
    pup_member(submeshes);
}

void init_texture(texture *tex, mem_arena *arena);
void terminate_texture(texture *tex);

void init_material(material *mat, mem_arena *arena);
void terminate_material(material *mat);

void init_submesh(submesh *sm, mem_arena *arena);
void terminate_submesh(submesh *sm);
void make_rect(mesh *msh);
void make_cube(mesh *msh);
void init_mesh(mesh *msh, mem_arena *arena);
void terminate_mesh(mesh *msh);

inline void terminate_robj(material *mat)
{
    terminate_material(mat);
}
inline void terminate_robj(texture *tex)
{
    terminate_texture(tex);
}
inline void terminate_robj(mesh *msh)
{
    terminate_mesh(msh);
}

} // namespace nslib
