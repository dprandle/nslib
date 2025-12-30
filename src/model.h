#pragma once
#include "robj_common.h"
#include "math/vector4.h"
#include "containers/array.h"
#include "containers/hset.h"

namespace nslib
{

inline constexpr sizet JOINTS_PER_VERTEX = 4;
inline constexpr sizet MAX_SUBMESH_COUNT = 16;
using ind_t = u16;

enum mat_sampler_slot
{
    MAT_SAMPLER_SLOT_DIFFUSE,
    MAT_SAMPLER_SLOT_NORMAL,
    MAT_SAMPLER_SLOT_COUNT
};

struct texture
{
    ROBJ(TEXTURE);
    byte_array pixels;
    uvec2 size;
    u8 channels;
};

// Material references textures and pipelines, which both must be uploaded to GPUa
struct material
{
    ROBJ(MATERIAL);
    vec4 col;
    hset<rid> pipelines;
    static_array<rid, MAT_SAMPLER_SLOT_COUNT> textures{.size=MAT_SAMPLER_SLOT_COUNT};
};

pup_func(material)
{
    pup_member(col);
    pup_member(pipelines);
    pup_member(textures);
}

struct vertex
{
    vec3 pos;
    vec2 uv;
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

void init_texture(texture *tex, const string &name, mem_arena *arena);
sizet get_texture_memsize(const texture *tex);
u32 get_texture_pixel_count(const texture *tex);
bool load_texture(texture *tex, const char *path, cstr *err);
void terminate_texture(texture *tex);

void init_material(material *mat, const string &name, mem_arena *arena);
void terminate_material(material *mat);

void make_rect(mesh *msh, const string &name, mem_arena *arena);
void make_cube(mesh *msh, const string &name, mem_arena *arena);

void init_mesh(mesh *msh, const string &name, mem_arena *arena);
void terminate_mesh(mesh *msh);
void init_submesh(submesh *sm, mem_arena *arena);
void terminate_submesh(submesh *sm);

} // namespace nslib
