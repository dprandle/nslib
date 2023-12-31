#pragma once

#include "logging.h"
#include "memory.h"
#include "math/matrix4.h"

namespace nslib
{

struct vkr_context;

namespace err_code
{
enum render
{
    RENDER_NO_ERROR,
    RENDER_INIT_FAIL,
    RENDER_LOAD_SHADERS_FAIL,
    RENDER_SUBMIT_QUEUE_FAIL,
};
}

struct uniform_buffer_object
{
    mat4 model;
    mat4 view;
    mat4 proj;
};

struct vertex
{
    vec3 pos;
    vec3 color;
    vec2 tex_coord;
};

const vertex verts[] = {{{-0.5f, -0.5f, 0.0f}, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f}},
                        {{0.5f, -0.5f, 0.0f}, {1.0f, 0.0f, 0.0f}, {1.0f, 0.0f}},
                        {{0.5f, 0.5f, 0.0f}, {1.0f, 1.0f, 1.0f}, {1.0f, 1.0f}},
                        {{-0.5f, 0.5f, 0.0f}, {0.0f, 1.0f, 1.0f}, {0.0f, 1.0f}}};

const u16 indices[] = {0, 1, 2, 2, 3, 0};

struct renderer
{
    // Passed in
    mem_arena *upstream_fl_arena;

    // Owned
    vkr_context *vk;
    mem_arena vk_free_list;
    mem_arena vk_frame_linear;

    sizet render_pass_ind;
    sizet pipeline_ind;
    sizet vert_buf_ind;
    sizet ind_buf_ind;
    
    sizet default_image_ind;
    sizet default_image_view_ind;
    sizet default_sampler_ind;
    uniform_buffer_object cvp;
};

int renderer_init(renderer *rndr, void *win_hndl, mem_arena *fl_arena);
int render_frame(renderer *rndr, int finished_frame_count);
void renderer_terminate(renderer *rndr);

} // namespace nslib
