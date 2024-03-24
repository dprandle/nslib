#pragma once
#include "robj_common.h"
#include "math/vector4.h"
#include "containers/array.h"

namespace nslib
{

const sizet JOINTS_PER_VERTEX = 4;
const sizet MAX_SUBMESH_COUNT = 32;
using ind_t = u16;

struct material{
    rid id;
};

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

struct mesh {
    ROBJ(MESH);
    static_array<submesh, MAX_SUBMESH_COUNT> submeshes;
    mem_arena *sub_arena;
};

pup_func(mesh)
{
    pup_member(submeshes);
}

void init_submesh(submesh *sm, mem_arena *arena);
void terminate_submesh(submesh *sm);

void make_rect(mesh *msh);
void make_cube(mesh *msh);

void init_mesh(mesh *msh, mem_arena *arena);
void terminate_mesh(mesh *msh);

void terminate_robj(mesh *msh);

} // namespace nslib
