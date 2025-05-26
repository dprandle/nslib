#pragma once

#include "model.h"
#include "math/matrix4.h"
#include "containers/array.h"
#include "containers/hmap.h"
#include "vk_context.h"

struct ImGuiContext;

namespace nslib
{
#define INVALID_IND ((sizet) - 1)

inline const rid FWD_RPASS("forward");
inline const rid PLINE_FWD_RPASS_S0_OPAQUE("forward-s0-opaque");

struct vkr_context;
struct sim_region;
struct camera;
struct static_model;
struct transform;

// 20 million triangles... thats a lot - works on desktop
const sizet MAX_TRIANGLE_COUNT = 20000000;
// Default ind buffer size (holding all of our inds) in ind count (not byte size)
const sizet DEFAULT_IND_BUFFER_SIZE = MAX_TRIANGLE_COUNT * 3;
// Default vert buffer size (holding all of our verts) in vert count (not byte size)
// Consider there is on average 6 shared triangles per vert - i think dividing the above by 3 is plenty
const sizet DEFAULT_VERT_BUFFER_SIZE = MAX_TRIANGLE_COUNT;
// Initial mem pool size (in element count) for our sbuffer mem pools
const sizet MAX_FREE_SBUFFER_NODE_COUNT = 1024;
// Minimum allowed sbuffer_entry block size in the free list for verts
const sizet MIN_VERT_FREE_BLOCK_SIZE = 4;
// Minimum allowed sbuffer_entry block size in the free list for indices
const sizet MIN_IND_FREE_BLOCK_SIZE = 6;
// Maximum number of render passes supported
const sizet MAX_RENDERPASS_COUNT = 16;
// Maximum number of materials the renderer supports
const sizet MAX_PIPELINE_COUNT = 1024;
// Maximum number of materials the renderer supports
const sizet MAX_MATERIAL_COUNT = 4096;
// Maximum number of objects
const sizet MAX_OBJECT_COUNT = 1000000;
// Default material
const rid DEFAULT_MAT_ID = rid("default");

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
    uvec4 test;
};

struct frame_ubo_data
{
    ivec4 frame_count;
};

struct pipeline_ubo_data
{
    mat4 proj_view;
};

struct material_ubo_data
{
    vec4 color;
};

struct obj_ubo_data
{
    mat4 transform;
};

struct sbuffer_entry
{
    sizet offset;
    sizet size;
};

using sbuffer_entry_slnode = slnode<sbuffer_entry>;

struct sbuffer_info
{
    // Index of vk buffer this shared buffer refers to
    sizet buf_ind;
    // The smallest allowable block size of free space... ie when searching through the free store we find a block that
    // will fit the request, the remaining size of that block must be bigger than this to be broken off to its own entry
    // in the free list
    sizet min_free_block_size;
    // The free list linked list - each node is allocated using the node pool arena
    slist<sbuffer_entry> fl;
    // The arena used to allocate each item in the free list
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

// We use a single vertex and indice buffer for all meshes
struct rmesh_info
{
    hmap<rid, rmesh_entry> meshes;
    sbuffer_info verts;
    sbuffer_info inds;
};
struct render_pass_draw_group;
struct imgui_ctxt;
struct profile_timepoints;

struct frame_draw_info
{
    hmap<rid, render_pass_draw_group *> rpasses;
};
// What we really want to do is have a big SSAO with all transforms for entire scene right?

struct pipeline_info
{
    sizet plind{};
    rid id{};
    rid rpass_id{};
    mat4 proj_view{};
};

struct rpass_info
{
    sizet rpind{};
    rid id{};
};

struct imgui_ctxt
{
    ImGuiContext *ctxt;
    vkr_descriptor_pool pool;
    vkr_rpass *rpass;
    sizet queue_family;
    vkr_queue queue;
    mem_arena fl;
};

struct renderer
{
    // Passed in
    mem_arena *upstream_fl_arena;

    // Owned vulkan context and mem arenas used only for vulkan stuff
    vkr_context vk{};
    mem_arena vk_free_list;

    mem_arena vk_frame_linear;
    mem_arena frame_fl;

    // Pipeline indices referenced by ids
    hmap<rid, pipeline_info> pipelines{};

    // Render pass indices referenced by ids
    hmap<rid, rpass_info> rpasses{};

    // A mapping between framebuffers and render passes
    // hmap<sizet, array<rid> *> fb_rpasses;

    // ImGUI context
    imgui_ctxt imgui{};

    // All frame draw call info
    frame_draw_info dcs;

    // Contains all info about meshes and where they are in the vert/ind buffers
    rmesh_info rmi;

    // Stored on reset render frame - used in subsequent frame calls to get the current frame
    s32 finished_frames;

    // This is incremented every frame there are no resize events
    f64 no_resize_frames;

    sizet default_image_ind;
    sizet default_image_view_ind;
    sizet default_sampler_ind;

    sizet swapchain_fb_depth_stencil_iview_ind{INVALID_IND};
    sizet swapchain_fb_depth_stencil_im_ind{INVALID_IND};
};

int rpush_sm(renderer *rndr, const static_model *sm, const transform *tf, const mesh_cache *msh_cache, const material_cache *mat_cache);

// NOTE: All of these mesh operations kind of need to wait on all rendering operations to complete as they modify the
// vertex and index buffers - not sure yet if this is better done within the functions or in the caller. Also these should be done at the
// start of a frame because any indices submitted in command buffers will be invalid after these operations. It almost seems like we should
// get a list of these and then just do it at start of frame after we wait for sync if there are any to do.

// Upload mesh data to GPU using the shared indice/vertex buffer, also "registers" the mesh with the renderer so it can
// be drawn
bool upload_to_gpu(mesh *msh, renderer *rdnr);

// Remove from gpu simply adds the meshes block to the free list, indicating that the block can be overwritten, and then
// removes the mesh from our mesh entry list. It does not do any actual gpu uploading
bool remove_from_gpu(mesh *msh, renderer *rndr);

int init_renderer(renderer *rndr, robj_cache_group *cg, void *win_hndl, mem_arena *fl_arena);

int begin_render_frame(renderer *rndr, int finished_frames);

int end_render_frame(renderer *rndr, camera *cam, f64 dt);

void terminate_renderer(renderer *rndr);

} // namespace nslib
