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
const sizet MAX_FREE_SBUFFER_NODE_COUNT = 1024;
const sizet MIN_VERT_FREE_BLOCK_SIZE = sizeof(vertex) * 4;
const sizet MIN_IND_FREE_BLOCK_SIZE = sizeof(u16) * 6;

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

using sbuffer_entry_slnode = slnode<sbuffer_entry>;

struct sbuffer_info
{
    u32 buf_ind;
    sizet min_free_block_size;
    slist<sbuffer_entry> fl;
    mem_arena node_pool;
};

struct rsubmesh_entry
{
    sbuffer_entry verts;
    sbuffer_entry inds;
};

struct rmesh_entry
{
    static_array<rsubmesh_entry, MAX_SUBMESH_COUNT> submesh_entrees;
};

struct rmesh_info
{
    hashmap<rid, rmesh_entry> meshes;
    sbuffer_info verts;
    sbuffer_info inds;
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

    // Contains all info about meshes and where they are in the vert/ind buffers
    rmesh_info rmi;
    
    sizet default_image_ind;
    sizet default_image_view_ind;
    sizet default_sampler_ind;

    sizet swapchain_fb_depth_stencil_iview_ind{INVALID_IND};
    sizet swapchain_fb_depth_stencil_im_ind{INVALID_IND};
};

bool upload_to_gpu(mesh *msh, renderer *rdnr);

int init_renderer(renderer *rndr, void *win_hndl, mem_arena *fl_arena);

int render_frame(renderer *rndr, sim_region *rgn, camera *cam, int finished_frame_count);

void terminate_renderer(renderer *rndr);

} // namespace nslib
