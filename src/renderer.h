#pragma once

#include "logging.h"
#include "model.h"
#include "math/matrix4.h"
#include "containers/array.h"

namespace nslib
{
#define INVALID_IND ((sizet)-1)

struct vkr_context;
struct sim_region;
struct camera;

const sizet DEFAULT_VERT_BUFFER_SIZE = sizeof(vertex) * 10000000;
const sizet DEFAULT_IND_BUFFER_SIZE = sizeof(u16) * 50000000;

namespace err_code
{
enum render
{
    RENDER_NO_ERROR,
    RENDER_INIT_FAIL,
    RENDER_LOAD_SHADERS_FAIL,
    RENDER_ACQUIRE_IMAGE_FAIL,
    RENDER_WAIT_FENCE_FAIL,
    RENDER_RESET_FENCE_FAIL,
    RENDER_SUBMIT_QUEUE_FAIL,
    RENDER_PRESENT_KHR_FAIL
};
}

struct push_constants
{
    mat4 model;
};

struct uniform_buffer_object
{
    mat4 proj_view;
};

struct sbuffer_entry
{
    sizet offset;
    sizet avail;
};

struct sbuffer_free_list
{
    slist<sbuffer_entry> fl;
    mem_arena node_pool;
};

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

    sizet swapchain_fb_depth_stencil_iview_ind{INVALID_IND};
    sizet swapchain_fb_depth_stencil_im_ind{INVALID_IND};

    submesh rect;
};

void upload_to_gpu(mesh *msh, renderer *rdnr);

int init_renderer(renderer *rndr, void *win_hndl, mem_arena *fl_arena);

int render_frame(renderer *rndr, sim_region *rgn, camera *cam, int finished_frame_count);

void terminate_renderer(renderer *rndr);

} // namespace nslib
