#pragma once
#include "robj_common.h"
#include "math/vector4.h"
#include "containers/array.h"

namespace nslib
{

const i32 JOINTS_PER_VERTEX = 4;

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
    array<u16> inds;
};

void init_submesh(submesh *sm, mem_arena *arena);
void terminate_submesh(submesh *sm);
void make_rect(submesh *sm);
void make_cube(submesh *sm);

} // namespace nslib
