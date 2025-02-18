#include "sim_region.h"
#include "containers/string.h"
#include "containers/linked_list.h"
#include "stb_image.h"
#include "platform.h"
#include "vk_context.h"
#include "renderer.h"

namespace nslib
{

#if defined(NDEBUG)
intern const u32 VALIDATION_LAYER_COUNT = 0;
intern const char **VALIDATION_LAYERS = nullptr;
#else
intern const u32 VALIDATION_LAYER_COUNT = 1;
intern const char *VALIDATION_LAYERS[VALIDATION_LAYER_COUNT] = {"VK_LAYER_KHRONOS_validation"};
#endif

#if __APPLE__
intern const VkInstanceCreateFlags INST_CREATE_FLAGS = VK_INSTANCE_CREATE_ENUMERATE_PORTABILITY_BIT_KHR;
intern const u32 ADDITIONAL_INST_EXTENSION_COUNT = 2;
intern const char *ADDITIONAL_INST_EXTENSIONS[ADDITIONAL_INST_EXTENSION_COUNT] = {VK_EXT_DEBUG_UTILS_EXTENSION_NAME,
                                                                                  VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME};
intern const u32 DEVICE_EXTENSION_COUNT = 2;
intern const char *DEVICE_EXTENSIONS[DEVICE_EXTENSION_COUNT] = {VK_KHR_SWAPCHAIN_EXTENSION_NAME, "VK_KHR_portability_subset"};
#else
intern const VkInstanceCreateFlags INST_CREATE_FLAGS = {};
intern const u32 ADDITIONAL_INST_EXTENSION_COUNT = 1;
intern const char *ADDITIONAL_INST_EXTENSIONS[ADDITIONAL_INST_EXTENSION_COUNT] = {VK_EXT_DEBUG_UTILS_EXTENSION_NAME};
intern const u32 DEVICE_EXTENSION_COUNT = 1;
intern const char *DEVICE_EXTENSIONS[DEVICE_EXTENSION_COUNT] = {VK_KHR_SWAPCHAIN_EXTENSION_NAME};
#endif

const intern rid FWD_RPASS("forward");
const intern rid PLINE_FWD_RPASS_S0_OPAQUE("forward-s0-opaque");
intern VkPipelineLayout G_FRAME_PL_LAYOUT{};

enum descriptor_set_layout
{
    DESCRIPTOR_SET_LAYOUT_FRAME,
    DESCRIPTOR_SET_LAYOUT_PIPELINE,
    DESCRIPTOR_SET_LAYOUT_MATERIAL,
    DESCRIPTOR_SET_LAYOUT_OBJECT
};

struct draw_call
{
    u32 index_count;
    u32 instance_count;
    u32 first_index;
    u32 vertex_offset;
    u32 first_instance;
    const mat4 *tf;
};

struct material_draw_group
{
    const material *mat;
    sizet set_ind;
    array<draw_call> dcs;
};

struct pipeline_draw_group
{
    sizet set_ind;
    const pipeline_info *plinfo;
    hashmap<rid, material_draw_group *> mats;
};

struct render_pass_draw_group
{
    const rpass_info *rpinfo;
    sizet fset{INVALID_IND};
    sizet oset{INVALID_IND};
    array<VkDescriptorSetLayout> sets_to_make;
    array<VkWriteDescriptorSet> desc_updates;
    hashmap<rid, pipeline_draw_group *> plines;
};

intern int setup_render_pass(renderer *rndr)
{
    auto vk = rndr->vk;
    vkr_rpass_cfg rp_cfg{};

    VkAttachmentDescription att{};
    att.format = vk->inst.device.swapchain.format;
    att.samples = VK_SAMPLE_COUNT_1_BIT;
    att.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    att.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    att.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    att.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    att.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    att.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
    arr_push_back(&rp_cfg.attachments, att);

    att.format = VK_FORMAT_D32_SFLOAT_S8_UINT;
    att.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    att.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
    arr_push_back(&rp_cfg.attachments, att);

    vkr_rpass_cfg_subpass subpass{};

    VkAttachmentReference att_ref{};
    att_ref.attachment = 0;
    att_ref.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    arr_push_back(&subpass.color_attachments, att_ref);

    att_ref.attachment = 1;
    att_ref.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
    subpass.depth_stencil_attachment = &att_ref;

    arr_push_back(&rp_cfg.subpasses, subpass);

    // Because we use this render pass each frame, this dependency makes it so that we won't begin our first subpass
    // until all color attachment and depth attachment (early fragment tests) operations are done for any subpass
    // (pipeline) associated with this render pass.
    // Since the depth image isn't 'presented', we don't have to have a separate depth image across each FIF.
    VkSubpassDependency sp_dep{};
    sp_dep.srcSubpass = VK_SUBPASS_EXTERNAL;
    sp_dep.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    sp_dep.srcAccessMask = 0;
    sp_dep.dstSubpass = 0;
    sp_dep.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    sp_dep.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
    arr_push_back(&rp_cfg.subpass_dependencies, sp_dep);

    rpass_info rpi{};
    rpi.id = FWD_RPASS;
    rpi.rpind = vkr_add_render_pass(&vk->inst.device, {});

    int ret = vkr_init_render_pass(vk, &rp_cfg, &vk->inst.device.render_passes[rpi.rpind]);
    if (ret == err_code::VKR_NO_ERROR) {
        hmap_set(&rndr->rpasses, rpi.id, rpi);
    }
    return ret;
}

intern int setup_pipeline(renderer *rndr)
{
    auto vk = rndr->vk;
    vkr_pipeline_cfg info{};

    arr_push_back(&info.dynamic_states, VK_DYNAMIC_STATE_VIEWPORT);
    arr_push_back(&info.dynamic_states, VK_DYNAMIC_STATE_SCISSOR);

    // Descripitor Set Layouts - Just one layout for the moment with a binding at 0 for uniforms and a binding at 1 for
    // image sampler
    info.set_layouts.size = 4;

    // Uniform buffer
    info.set_layouts[0].bindings[0].binding = 0;
    info.set_layouts[0].bindings[0].stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
    info.set_layouts[0].bindings[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    info.set_layouts[0].bindings[0].descriptorCount = 1;
    info.set_layouts[0].bindings.size = 1;

    info.set_layouts[1].bindings[0].binding = 0;
    info.set_layouts[1].bindings[0].stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
    info.set_layouts[1].bindings[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    info.set_layouts[1].bindings[0].descriptorCount = 1;
    info.set_layouts[1].bindings.size = 1;

    info.set_layouts[2].bindings[0].binding = 0;
    info.set_layouts[2].bindings[0].stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
    info.set_layouts[2].bindings[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    info.set_layouts[2].bindings[0].descriptorCount = 1;
    info.set_layouts[2].bindings.size = 1;

    info.set_layouts[3].bindings[0].binding = 0;
    info.set_layouts[3].bindings[0].stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
    info.set_layouts[3].bindings[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
    info.set_layouts[3].bindings[0].descriptorCount = 1;
    info.set_layouts[3].bindings.size = 1;

    // Image sampler
    // info.set_layouts[0].bindings[1].binding = 1;
    // info.set_layouts[0].bindings[1].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    // info.set_layouts[0].bindings[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    // info.set_layouts[0].bindings[1].descriptorCount = 1;

    // Setup our push constant
    ++info.push_constant_ranges.size;
    info.push_constant_ranges[0].offset = 0;
    info.push_constant_ranges[0].size = sizeof(push_constants);
    info.push_constant_ranges[0].stageFlags = VK_SHADER_STAGE_VERTEX_BIT;

    // Vertex binding:
    VkVertexInputBindingDescription binding_desc{};
    binding_desc.binding = 0;
    binding_desc.stride = sizeof(vertex);
    binding_desc.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
    arr_push_back(&info.vert_binding_desc, binding_desc);

    // Attribute Descriptions - so far we just have three
    VkVertexInputAttributeDescription attrib_desc{};
    attrib_desc.binding = 0;
    attrib_desc.location = 0;
    attrib_desc.format = VK_FORMAT_R32G32B32_SFLOAT;
    attrib_desc.offset = offsetof(vertex, pos);
    arr_push_back(&info.vert_attrib_desc, attrib_desc);

    attrib_desc.binding = 0;
    attrib_desc.location = 1;
    attrib_desc.format = VK_FORMAT_R32G32_SFLOAT;
    attrib_desc.offset = offsetof(vertex, tc);
    arr_push_back(&info.vert_attrib_desc, attrib_desc);

    attrib_desc.binding = 0;
    attrib_desc.location = 2;
    attrib_desc.format = VK_FORMAT_R8G8B8A8_UINT;
    attrib_desc.offset = offsetof(vertex, color);
    arr_push_back(&info.vert_attrib_desc, attrib_desc);

    // Viewports and scissors
    VkViewport viewport{};
    viewport.x = 0.0f;
    viewport.y = 0.0f;
    viewport.width = (float)vk->inst.device.swapchain.extent.width;
    viewport.height = (float)vk->inst.device.swapchain.extent.height;
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;
    arr_push_back(&info.viewports, viewport);

    VkRect2D scissor{};
    scissor.offset = {0, 0};
    scissor.extent = vk->inst.device.swapchain.extent;
    arr_push_back(&info.scissors, scissor);

    // Input Assembly
    info.input_assembly.primitive_topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    info.input_assembly.primitive_restart_enable = false;

    // Raster options
    info.raster.depth_clamp_enable = false;
    info.raster.rasterizer_discard_enable = false;
    info.raster.polygon_mode = VK_POLYGON_MODE_FILL;
    info.raster.line_width = 1.0f;
    info.raster.cull_mode = VK_CULL_MODE_BACK_BIT;
    info.raster.front_face = VK_FRONT_FACE_CLOCKWISE;
    info.raster.depth_bias_enable = false;
    info.raster.depth_bias_constant_factor = 0.0f;
    info.raster.depth_bias_clamp = 0.0f;
    info.raster.depth_bias_slope_factor = 0.0f;

    // Multisampling defaults are good

    // Color blending
    VkPipelineColorBlendAttachmentState col_blnd_att{};
    col_blnd_att.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    col_blnd_att.blendEnable = true;
    col_blnd_att.srcColorBlendFactor = VK_BLEND_FACTOR_ONE;
    col_blnd_att.dstColorBlendFactor = VK_BLEND_FACTOR_ZERO;
    col_blnd_att.colorBlendOp = VK_BLEND_OP_ADD;
    //////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    // This is to do alpha blending - though it seems it doesn't really matter about the src and dest alpha and alpha blend op //
    // col_blnd_att.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT |
    // VK_COLOR_COMPONENT_A_BIT; // col_blnd_att.blendEnable = true; // col_blnd_att.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA; //
    // VK_BLEND_FACTOR_ONE;  // Optional                             // col_blnd_att.dstColorBlendFactor =
    // VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA; // VK_BLEND_FACTOR_ZERO; // Optional                             // col_blnd_att.colorBlendOp =
    // VK_BLEND_OP_ADD;                            // Optional                                                      //
    // col_blnd_att.srcAlphaBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;           // VK_BLEND_FACTOR_ONE;  // Optional //
    // col_blnd_att.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA; // VK_BLEND_FACTOR_ZERO; // Optional //
    // col_blnd_att.alphaBlendOp = VK_BLEND_OP_ADD;                            // Optional //
    //////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

    arr_push_back(&info.col_blend.attachments, col_blnd_att);

    // Depth Stencil
    info.depth_stencil.depth_test_enable = true;
    info.depth_stencil.depth_write_enable = true;
    info.depth_stencil.depth_compare_op = VK_COMPARE_OP_LESS;
    info.depth_stencil.depth_bounds_test_enable = false;
    info.depth_stencil.min_depth_bounds = 0.0f;
    info.depth_stencil.max_depth_bounds = 1.0f;

    // Our basic shaders
    const char *fnames[] = {"./data/shaders/rdev.vert.spv", "./data/shaders/rdev.frag.spv"};
    for (int i = 0; i <= VKR_SHADER_STAGE_FRAG; ++i) {
        platform_file_err_desc err{};
        arr_init(&info.shader_stages[i].code, &rndr->vk_frame_linear);
        read_file(fnames[i], &info.shader_stages[i].code, 0, &err);
        if (err.code != err_code::PLATFORM_NO_ERROR) {
            wlog("Error reading file %s from disk (code %d): %s", fnames[i], err.code, err.str);
            return err_code::RENDER_LOAD_SHADERS_FAIL;
        }
        info.shader_stages[i].entry_point = "main";
    }

    auto rpass_fiter = hmap_find(&rndr->rpasses, FWD_RPASS);
    if (rpass_fiter) {
        info.rpass = &vk->inst.device.render_passes[rpass_fiter->val.rpind];
        pipeline_info plinfo{};
        plinfo.id = PLINE_FWD_RPASS_S0_OPAQUE;
        plinfo.plind = vkr_add_pipeline(&vk->inst.device, {});
        plinfo.rpass_id = rpass_fiter->val.id;
        int code = vkr_init_pipeline(vk, &info, &vk->inst.device.pipelines[plinfo.plind]);
        if (code == err_code::VKR_NO_ERROR) {
            hmap_set(&rndr->pipelines, plinfo.id, plinfo);

            // Set our global frame layout
            G_FRAME_PL_LAYOUT = vk->inst.device.pipelines[plinfo.plind].layout_hndl;
        }
        return code;
    }
    return err_code::RENDER_INIT_FAIL;
}

intern int setup_rmesh_info(renderer *rndr)
{
    auto vk = rndr->vk;
    auto dev = &vk->inst.device;

    // Create vertex buffer on GPU
    vkr_buffer_cfg b_cfg{};
    rndr->rmi.verts.buf_ind = vkr_add_buffer(dev, {});
    rndr->rmi.verts.min_free_block_size = MIN_VERT_FREE_BLOCK_SIZE;

    rndr->rmi.inds.buf_ind = vkr_add_buffer(dev, {});
    rndr->rmi.inds.min_free_block_size = MIN_IND_FREE_BLOCK_SIZE;

    // Common to all buffer options
    b_cfg.mem_usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;
    b_cfg.sharing_mode = VK_SHARING_MODE_EXCLUSIVE;
    b_cfg.vma_alloc = &dev->vma_alloc;

    // Vert buffer
    ilog("Creating vertex buffer");
    b_cfg.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    b_cfg.buffer_size = DEFAULT_VERT_BUFFER_SIZE * sizeof(vertex);
    int err = vkr_init_buffer(&dev->buffers[rndr->rmi.verts.buf_ind], &b_cfg);
    if (err != err_code::VKR_NO_ERROR) {
        return err;
    }

    // Ind buffer
    ilog("Creating index buffer");
    b_cfg.usage = VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    b_cfg.buffer_size = DEFAULT_IND_BUFFER_SIZE * sizeof(ind_t);
    err = vkr_init_buffer(&dev->buffers[rndr->rmi.inds.buf_ind], &b_cfg);
    if (err != err_code::VKR_NO_ERROR) {
        return err;
    }

    mem_init_pool_arena<sbuffer_entry_slnode>(&rndr->rmi.verts.node_pool, MAX_FREE_SBUFFER_NODE_COUNT, mem_global_stack_arena());
    mem_init_pool_arena<sbuffer_entry_slnode>(&rndr->rmi.inds.node_pool, MAX_FREE_SBUFFER_NODE_COUNT, mem_global_stack_arena());
    hashmap_init(&rndr->rmi.meshes, mem_global_arena());

    // Create the head nodes of our vert and index buffer free list - indice 0 and full buffer size
    auto vert_head = mem_alloc<sbuffer_entry_slnode>(&rndr->rmi.verts.node_pool);
    vert_head->next = nullptr;
    vert_head->data.size = DEFAULT_VERT_BUFFER_SIZE;
    vert_head->data.offset = 0;
    ll_push_back(&rndr->rmi.verts.fl, vert_head);

    auto ind_head = mem_alloc<sbuffer_entry_slnode>(&rndr->rmi.inds.node_pool);
    ind_head->next = nullptr;
    ind_head->data.size = DEFAULT_IND_BUFFER_SIZE;
    ind_head->data.offset = 0;
    ll_push_back(&rndr->rmi.inds.fl, ind_head);

    return err_code::VKR_NO_ERROR;
}

intern sbuffer_entry find_sbuffer_block(sbuffer_info *sbuf, sizet req_size)
{
    // Find the first node with enough available memory
    auto node = sbuf->fl.head;
    sbuffer_entry_slnode *prev_node{};
    while (node && req_size > node->data.size) {
        prev_node = node;
        node = node->next;
    }

    // Crash if we don't have enough memory spots left
    assert(node);
    auto ret_entry = node->data;

    // Remove the node we just used
    ll_remove(&sbuf->fl, prev_node, node);
    mem_free(node, &sbuf->node_pool);

    // If the remaining is enough to hold at least a quad we should insert a new free entry at the proper spot
    sizet remain = ret_entry.size - req_size;
    if (remain >= sbuf->min_free_block_size) {

        auto fnode = sbuf->fl.head;
        sbuffer_entry_slnode *prev_fnode{};
        while (fnode && fnode->data.size < remain) {
            prev_fnode = fnode;
            fnode = fnode->next;
        }

        auto new_node = mem_alloc<sbuffer_entry_slnode>(&sbuf->node_pool);
        ret_entry.size -= remain;
        new_node->next = nullptr;
        new_node->data.size = remain;
        new_node->data.offset = ret_entry.offset + ret_entry.size;
        ll_insert(&sbuf->fl, prev_fnode, new_node);
    }
    return ret_entry;
}

// Upload mesh data to GPU using the shared indice/vertex buffer, also "registers" the mesh with the renderer so it can
// be drawn
bool upload_to_gpu(mesh *msh, renderer *rndr)
{
    auto fiter = hashmap_find(&rndr->rmi.meshes, msh->id);
    if (fiter) {
        return false;
    }
    auto dev = &rndr->vk->inst.device;
    // Find the first available sbuffer entry in the free list
    rmesh_entry new_mentry{};
    // arr_init(&new_mentry.submesh_entrees, rndr->upstream_fl_arena);
    for (int subi = 0; subi < msh->submeshes.size; ++subi) {
        sizet req_vert_size = arr_len(msh->submeshes[subi].verts);
        sizet req_vert_byte_size = arr_sizeof(msh->submeshes[subi].verts);
        sizet req_inds_size = arr_len(msh->submeshes[subi].inds);
        sizet req_inds_byte_size = arr_sizeof(msh->submeshes[subi].inds);

        // Add an entry in the hashmap for our mesh to refer to this sbuffer entry we just got from the free list
        rsubmesh_entry new_smentry{};
        new_smentry.verts = find_sbuffer_block(&rndr->rmi.verts, req_vert_size);
        new_smentry.inds = find_sbuffer_block(&rndr->rmi.inds, req_inds_size);
        assert(new_smentry.verts.size > 0);
        assert(new_smentry.inds.size > 0);
        arr_emplace_back(&new_mentry.submesh_entrees, new_smentry);

        // Our required vert and ind size might be a little less than the avail block size, if a block was picked that
        // was big enough to fit our needs but the remaining available in the block was less than the required min block
        // size fot the sbuffer
        VkBufferCopy vert_region{}, ind_region{};
        vert_region.size = req_vert_byte_size;
        vert_region.dstOffset = new_smentry.verts.offset * sizeof(vertex);

        ind_region.size = req_inds_byte_size;
        ind_region.dstOffset = new_smentry.inds.offset * sizeof(ind_t);

        // TODO: Handle error conditions here - there are several reasons why a buffer upload might fail - for new we
        // just assert it worked

        // Upload our vert data to the GPU
        int ret = vkr_stage_and_upload_buffer_data(&dev->buffers[rndr->rmi.verts.buf_ind],
                                                   msh->submeshes[subi].verts.data,
                                                   req_vert_byte_size,
                                                   &vert_region,
                                                   &dev->qfams[VKR_QUEUE_FAM_TYPE_GFX],
                                                   rndr->vk);
        assert(ret == err_code::VKR_NO_ERROR);

        // Upload our ind data to the GPU
        ret = vkr_stage_and_upload_buffer_data(&dev->buffers[rndr->rmi.inds.buf_ind],
                                               msh->submeshes[subi].inds.data,
                                               req_inds_byte_size,
                                               &ind_region,
                                               &dev->qfams[VKR_QUEUE_FAM_TYPE_GFX],
                                               rndr->vk);
        assert(ret == err_code::VKR_NO_ERROR);
    }
    dlog("Should be adding mesh id %s %d submeshes", str_cstr(msh->id.str), new_mentry.submesh_entrees.size);
    for (int si = 0; si < new_mentry.submesh_entrees.size; ++si) {
        auto sub = &new_mentry.submesh_entrees[si];
        dlog("submesh %d:  vertsp(offset:%d  size:%d)  inds(offset:%d size:%d)",
             si,
             sub->verts.offset,
             sub->verts.size,
             sub->inds.offset,
             sub->inds.size);
    }
    // Add the smesh entry we just built to the renderer mesh entry map (stored by id)
    hashmap_set(&rndr->rmi.meshes, msh->id, new_mentry);

    return true;
}

intern void insert_node_to_free_list(sbuffer_info *sbuf, const sbuffer_entry *entry)
{
    // Iterate over the free list and find the lowest index position to insert the new node
    auto it = sbuf->fl.head;
    sbuffer_entry_slnode *it_prev{};
    while (it && entry->offset < it->data.offset) {
        it_prev = it;
        it = it->next;
    }
    auto new_node = mem_alloc<sbuffer_entry_slnode>(&sbuf->node_pool);
    new_node->next = nullptr;
    new_node->data = *entry;
    ll_insert(&sbuf->fl, it_prev, new_node);
}

bool remove_from_gpu(mesh *msh, renderer *rndr)
{
    // TODO: This code is basically completely untested
    auto minfo = hashmap_find(&rndr->rmi.meshes, msh->id);
    if (minfo) {
        for (int subi = 0; subi < minfo->value.submesh_entrees.size; ++subi) {
            // Insert the block to our free list
            auto cur_entry = &minfo->value.submesh_entrees[subi];
            insert_node_to_free_list(&rndr->rmi.verts, &cur_entry->verts);
            insert_node_to_free_list(&rndr->rmi.inds, &cur_entry->inds);
        }
        hashmap_remove(&rndr->rmi.meshes, msh->id);
    }
    return minfo;
}

intern int load_default_image_and_sampler(renderer *rndr)
{
    auto vk = rndr->vk;
    auto dev = &rndr->vk->inst.device;

    /////////////////////////////////
    // Load the image for sampling //
    /////////////////////////////////
    int tex_channels;
    ivec2 tex_dims;
    stbi_uc *pixels = stbi_load("data/textures/texture.jpg", &tex_dims.w, &tex_dims.h, &tex_channels, STBI_rgb_alpha);
    if (!pixels) {
        return err_code::PLATFORM_INIT_FAIL;
    }

    rndr->default_image_ind = vkr_add_image(dev, {});
    auto dest_image = &dev->images[rndr->default_image_ind];

    vkr_image_cfg cfg{};
    cfg.dims = {tex_dims, 1};
    cfg.format = VK_FORMAT_R8G8B8A8_SRGB;
    //    cfg.mip_levels = 1;vkr_get_required_mip_levels(tex_dims);

    // We have the src bit set here because we generate mipmaps for the image
    cfg.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
    cfg.mem_usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;
    cfg.vma_alloc = &dev->vma_alloc;

    int err = vkr_init_image(dest_image, &cfg);
    if (err != err_code::VKR_NO_ERROR) {
        stbi_image_free(pixels);
        return err;
    }

    sizet imsize = cfg.dims.x * cfg.dims.y * cfg.dims.z * 4;
    vkr_stage_and_upload_image_data(dest_image, pixels, imsize, &dev->qfams[VKR_QUEUE_FAM_TYPE_GFX], vk);

    stbi_image_free(pixels);

    ///////////////////////////////////////
    // Create the image view and sampler //
    ///////////////////////////////////////
    vkr_image_view_cfg iview_cfg{};
    iview_cfg.image = dest_image;

    rndr->default_image_view_ind = vkr_add_image_view(dev);
    auto iview_ptr = &dev->image_views[rndr->default_image_view_ind];
    err = vkr_init_image_view(iview_ptr, &iview_cfg, vk);
    if (err != err_code::VKR_NO_ERROR) {
        return err;
    }

    vkr_sampler_cfg samp_cfg{};
    for (int i = 0; i < 3; ++i) {
        samp_cfg.address_mode_uvw[i] = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    }
    samp_cfg.mag_filter = VK_FILTER_LINEAR;
    samp_cfg.min_filter = VK_FILTER_LINEAR;
    samp_cfg.anisotropy_enable = true;
    samp_cfg.max_anisotropy = vk->inst.pdev_info.props.limits.maxSamplerAnisotropy;
    samp_cfg.border_color = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
    samp_cfg.compare_op = VK_COMPARE_OP_ALWAYS;
    samp_cfg.mipmap_mode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    rndr->default_sampler_ind = vkr_add_sampler(dev);
    auto samp_ptr = &dev->samplers[rndr->default_sampler_ind];
    err = vkr_init_sampler(samp_ptr, &samp_cfg, vk);
    return err;
}

intern int init_swapchain_framebuffers(renderer *rndr)
{
    auto vk = rndr->vk;
    auto dev = &rndr->vk->inst.device;

    vkr_image_cfg im_cfg{};
    im_cfg.dims = {dev->swapchain.extent.width, dev->swapchain.extent.height, 1};
    im_cfg.format = VK_FORMAT_D32_SFLOAT_S8_UINT;
    im_cfg.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
    im_cfg.mem_usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;
    im_cfg.vma_alloc = &dev->vma_alloc;

    if (rndr->swapchain_fb_depth_stencil_im_ind == INVALID_IND) {
        rndr->swapchain_fb_depth_stencil_im_ind = vkr_add_image(dev);
    }
    int err = vkr_init_image(&dev->images[rndr->swapchain_fb_depth_stencil_im_ind], &im_cfg);
    if (err != err_code::VKR_NO_ERROR) {
        return err;
    }

    vkr_image_view_cfg imv_cfg{};
    imv_cfg.srange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT;
    imv_cfg.image = &dev->images[rndr->swapchain_fb_depth_stencil_im_ind];

    if (rndr->swapchain_fb_depth_stencil_iview_ind == INVALID_IND) {
        rndr->swapchain_fb_depth_stencil_iview_ind = vkr_add_image_view(dev);
    }
    err = vkr_init_image_view(&dev->image_views[rndr->swapchain_fb_depth_stencil_iview_ind], &imv_cfg, vk);
    if (err != err_code::VKR_NO_ERROR) {
        return err;
    }

    // We need the render pass associated with our main framebuffer
    auto rp_fiter = hmap_find(&rndr->rpasses, FWD_RPASS);
    assert(rp_fiter);
    if (rp_fiter) {
        vkr_init_swapchain_framebuffers(dev,
                                        vk,
                                        &dev->render_passes[rp_fiter->val.rpind],
                                        vkr_framebuffer_attachment{dev->image_views[rndr->swapchain_fb_depth_stencil_iview_ind]});
    }
    return err;
}

intern int setup_rendering(renderer *rndr)
{
    ilog("Setting up default rendering...");
    auto vk = rndr->vk;
    auto dev = &rndr->vk->inst.device;

    int err = setup_render_pass(rndr);
    if (err != err_code::VKR_NO_ERROR) {
        return err;
    }

    err = setup_pipeline(rndr);
    if (err != err_code::VKR_NO_ERROR) {
        return err;
    }

    err = init_swapchain_framebuffers(rndr);
    if (err != err_code::VKR_NO_ERROR) {
        return err;
    }

    err = setup_rmesh_info(rndr);
    if (err != err_code::VKR_NO_ERROR) {
        return err;
    }

    err = load_default_image_and_sampler(rndr);
    if (err != err_code::VKR_NO_ERROR) {
        return err;
    }

    ////////////////////////////////////////////////////////////////////////////////
    // Create uniform buffers and descriptor sets pointing to them for each frame //
    ////////////////////////////////////////////////////////////////////////////////
    auto pline = hmap_find(&rndr->pipelines, PLINE_FWD_RPASS_S0_OPAQUE);
    assert(pline);
    if (!pline) {
        return err_code::RENDER_INIT_FAIL;
    }
    for (int i = 0; i < dev->rframes.size; ++i) {
        // Create a uniform buffers for each frame

        // Frame uniform buffer - per frame data
        vkr_buffer_cfg buf_cfg{};
        buf_cfg.mem_usage = VMA_MEMORY_USAGE_AUTO_PREFER_HOST;
        buf_cfg.sharing_mode = VK_SHARING_MODE_EXCLUSIVE;
        buf_cfg.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
        buf_cfg.buffer_size = sizeof(frame_ubo_data);
        buf_cfg.alloc_flags = VMA_ALLOCATION_CREATE_MAPPED_BIT | VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;
        buf_cfg.vma_alloc = &dev->vma_alloc;

        vkr_buffer frame_uniform_buf{};
        int err = vkr_init_buffer(&frame_uniform_buf, &buf_cfg);
        if (err != err_code::VKR_NO_ERROR) {
            return err;
        }
        dev->rframes[i].frame_ubo_ind = vkr_add_buffer(dev, frame_uniform_buf);

        // Pipeline uniform buffer - per pipeline data
        buf_cfg.buffer_size = MAX_PIPELINE_COUNT * sizeof(pipeline_ubo_data);
        vkr_buffer pl_uniform_buf{};
        err = vkr_init_buffer(&pl_uniform_buf, &buf_cfg);
        if (err != err_code::VKR_NO_ERROR) {
            return err;
        }
        dev->rframes[i].pl_ubo_ind = vkr_add_buffer(dev, pl_uniform_buf);

        // Material uniform buffer - per material data
        buf_cfg.buffer_size = MAX_MATERIAL_COUNT * sizeof(material_ubo_data);
        vkr_buffer mat_uniform_buf{};
        err = vkr_init_buffer(&mat_uniform_buf, &buf_cfg);
        if (err != err_code::VKR_NO_ERROR) {
            return err;
        }
        dev->rframes[i].mat_ubo_ind = vkr_add_buffer(dev, mat_uniform_buf);

        // Object uniform buffer - per material data
        buf_cfg.buffer_size = MAX_OBJECT_COUNT * sizeof(obj_ubo_data);
        vkr_buffer obj_uniform_buf{};
        err = vkr_init_buffer(&obj_uniform_buf, &buf_cfg);
        if (err != err_code::VKR_NO_ERROR) {
            return err;
        }
        dev->rframes[i].obj_ubo_ind = vkr_add_buffer(dev, obj_uniform_buf);
    }
    return err_code::VKR_NO_ERROR;
}

intern void add_desc_write_update(renderer *rndr,
                                  vkr_frame *cur_frame,
                                  sizet offset,
                                  sizet range,
                                  sizet buf_ind,
                                  sizet set_ind,
                                  array<VkWriteDescriptorSet> *updates,
                                  VkDescriptorType type)
{
    // Add a write descriptor set to update our obj descriptor to point at this portion of our unifrom buffer
    auto buffer_info = mem_alloc<VkDescriptorBufferInfo>(&rndr->frame_fl);
    memset(buffer_info, 0, sizeof(VkDescriptorBufferInfo));
    buffer_info->offset = offset;
    buffer_info->range = range;
    buffer_info->buffer = rndr->vk->inst.device.buffers[buf_ind].hndl;

    VkWriteDescriptorSet desc_write{};
    desc_write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    desc_write.dstSet = cur_frame->desc_pool.desc_sets[set_ind].hndl;
    desc_write.dstBinding = 0;
    desc_write.dstArrayElement = 0;
    desc_write.descriptorType = type;
    desc_write.descriptorCount = 1;
    desc_write.pBufferInfo = buffer_info;
    arr_push_back(updates, desc_write);
}

intern int record_command_buffer(renderer *rndr, vkr_framebuffer *fb, vkr_frame *cur_frame, vkr_command_buffer *cmd_buf)
{
    auto dev = &rndr->vk->inst.device;
    auto vert_buf = &dev->buffers[rndr->rmi.verts.buf_ind];
    auto ind_buf = &dev->buffers[rndr->rmi.inds.buf_ind];

    int err = vkr_begin_cmd_buf(cmd_buf);
    if (err != err_code::VKR_NO_ERROR) {
        return err;
    }

    VkClearValue att_clear_vals[] = {{.color{{1.0f, 0.0f, 1.0f, 1.0f}}}, {.depthStencil{1.0f, 0}}};
    vkr_cmd_begin_rpass(cmd_buf, fb, att_clear_vals, 2);

    // Bind the global vertex/index buffer/s
    VkBuffer vert_bufs[] = {vert_buf->hndl};
    VkDeviceSize offsets[] = {0};
    vkCmdBindVertexBuffers(cmd_buf->hndl, 0, 1, vert_bufs, offsets);
    vkCmdBindIndexBuffer(cmd_buf->hndl, ind_buf->hndl, 0, VK_INDEX_TYPE_UINT16);

    sizet rpass_i{};
    while (auto rpass_iter = hashmap_iter(&rndr->dcs.rpasses, &rpass_i)) {
        // Bind frame rpass descriptor set
        auto ds = cur_frame->desc_pool.desc_sets[rpass_iter->value->fset].hndl;
        vkCmdBindDescriptorSets(
            cmd_buf->hndl, VK_PIPELINE_BIND_POINT_GRAPHICS, G_FRAME_PL_LAYOUT, DESCRIPTOR_SET_LAYOUT_FRAME, 1, &ds, 0, nullptr);

        // We could make our render pass have the vert/index buffer info.. ie
        sizet pl_i{};
        while (auto pl_iter = hashmap_iter(&rpass_iter->value->plines, &pl_i)) {
            // Grab the pipeline and set it, and set the viewport/scissor
            auto pipeline = &dev->pipelines[pl_iter->value->plinfo->plind];
            vkCmdBindPipeline(cmd_buf->hndl, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline->hndl);

            auto ds = cur_frame->desc_pool.desc_sets[pl_iter->value->set_ind].hndl;
            vkCmdBindDescriptorSets(
                cmd_buf->hndl, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline->layout_hndl, DESCRIPTOR_SET_LAYOUT_PIPELINE, 1, &ds, 0, nullptr);

            VkViewport viewport{};
            viewport.x = 0.0f;
            viewport.y = 0.0f;
            viewport.width = (float)fb->size.w;
            viewport.height = (float)fb->size.h;
            viewport.minDepth = 0.0f;
            viewport.maxDepth = 1.0f;
            vkCmdSetViewport(cmd_buf->hndl, 0, 1, &viewport);

            VkRect2D scissor{};
            scissor.offset = {0, 0};
            scissor.extent = {fb->size.w, fb->size.h};
            vkCmdSetScissor(cmd_buf->hndl, 0, 1, &scissor);

            sizet mat_i{};
            while (auto mat_iter = hashmap_iter(&pl_iter->value->mats, &mat_i)) {
                // Bind the material set
                auto ds = cur_frame->desc_pool.desc_sets[mat_iter->value->set_ind].hndl;
                vkCmdBindDescriptorSets(
                    cmd_buf->hndl, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline->layout_hndl, DESCRIPTOR_SET_LAYOUT_MATERIAL, 1, &ds, 0, nullptr);

                sizet obj_set_ind{INVALID_IND};
                for (u32 dci = 0; dci < mat_iter->value->dcs.size; ++dci) {
                    const draw_call *cur_dc = &mat_iter->value->dcs[dci];

                    auto ds = cur_frame->desc_pool.desc_sets[rpass_iter->value->oset].hndl;
                    u32 dyn_offset = dci * sizeof(obj_ubo_data);
                    vkCmdBindDescriptorSets(cmd_buf->hndl,
                                            VK_PIPELINE_BIND_POINT_GRAPHICS,
                                            pipeline->layout_hndl,
                                            DESCRIPTOR_SET_LAYOUT_OBJECT,
                                            1,
                                            &ds,
                                            1,
                                            &dyn_offset);

                    push_constants pc{3};
                    vkCmdPushConstants(cmd_buf->hndl, pipeline->layout_hndl, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(push_constants), &pc);
                    vkCmdDrawIndexed(cmd_buf->hndl,
                                     cur_dc->index_count,
                                     cur_dc->instance_count,
                                     cur_dc->first_index,
                                     cur_dc->vertex_offset,
                                     cur_dc->first_instance);
                }
            }
        }
    }

    vkr_cmd_end_rpass(cmd_buf);
    return vkr_end_cmd_buf(cmd_buf);
}

int init_renderer(renderer *rndr, robj_cache_group *cg, void *win_hndl, mem_arena *fl_arena)
{
    assert(fl_arena->alloc_type == mem_alloc_type::FREE_LIST);
    rndr->upstream_fl_arena = fl_arena;
    mem_init_fl_arena(&rndr->vk_free_list, 100 * MB_SIZE, fl_arena);
    mem_init_lin_arena(&rndr->vk_frame_linear, 10 * MB_SIZE, mem_global_stack_arena());
    mem_init_fl_arena(&rndr->frame_fl, 100 * MB_SIZE, fl_arena);

    rndr->vk = mem_alloc<vkr_context>(&rndr->vk_free_list, 8);
    memset(rndr->vk, 0, sizeof(vkr_context));

    hmap_init(&rndr->rpasses, hash_type, generate_rand_seed(), generate_rand_seed(), fl_arena);
    hmap_init(&rndr->pipelines, hash_type, generate_rand_seed(), generate_rand_seed(), fl_arena);

    // Set up our draw call list render pass hashmap with frame linear memory
    // hashmap_init(&rndr->dcs.rpasses, &rndr->frame_fl);
    vkr_descriptor_cfg desc_cfg{};
    // Frame + pl + mat + obj uob
    desc_cfg.max_sets = MAX_FRAMES_IN_FLIGHT * (MAX_RENDERPASS_COUNT * 2 + MAX_PIPELINE_COUNT + MAX_MATERIAL_COUNT);
    desc_cfg.max_desc_per_type[VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER] =
        MAX_FRAMES_IN_FLIGHT * (MAX_RENDERPASS_COUNT + MAX_PIPELINE_COUNT + MAX_MATERIAL_COUNT);
    desc_cfg.max_desc_per_type[VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC] = MAX_FRAMES_IN_FLIGHT * MAX_RENDERPASS_COUNT;
    desc_cfg.max_desc_per_type[VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER] = MAX_FRAMES_IN_FLIGHT * MAX_MATERIAL_COUNT * TEXTURE_SLOT_COUNT;
    vkr_cfg vkii{"rdev",
                 {1, 0, 0},
                 {.persistent_arena = &rndr->vk_free_list, .command_arena = &rndr->vk_frame_linear},
                 LOG_DEBUG,
                 win_hndl,
                 INST_CREATE_FLAGS,
                 desc_cfg,
                 ADDITIONAL_INST_EXTENSIONS,
                 ADDITIONAL_INST_EXTENSION_COUNT,
                 DEVICE_EXTENSIONS,
                 DEVICE_EXTENSION_COUNT,
                 VALIDATION_LAYERS,
                 VALIDATION_LAYER_COUNT};

    if (vkr_init(&vkii, rndr->vk) != err_code::VKR_NO_ERROR) {
        return err_code::RENDER_INIT_FAIL;
    }

    int err = setup_rendering(rndr);
    if (err != err_code::VKR_NO_ERROR) {
        elog("Failed to initialize renderer with code %d", err);
        return err;
    }

    // Create a default material for submeshes without materials
    auto mat_cache = get_cache<material>(cg);
    auto def_mat = add_robj(DEFAULT_MAT_ID, mat_cache);
    init_material(def_mat, rndr->upstream_fl_arena);
    def_mat->col = vec4{0.5f, 0.2f, 0.8f, 1.0f};
    hashset_set(&def_mat->pipelines, PLINE_FWD_RPASS_S0_OPAQUE);

    // Setup our indice and vert buffer sbuffer
    return err_code::RENDER_NO_ERROR;
}

intern void recreate_swapchain(renderer *rndr)
{
    // Recreating the swapchain will wait on all semaphores and fences before continuing
    auto dev = &rndr->vk->inst.device;
    vkr_recreate_swapchain(&rndr->vk->inst, rndr->vk);
    vkr_terminate_image_view(&dev->image_views[rndr->swapchain_fb_depth_stencil_iview_ind], rndr->vk);
    vkr_terminate_image(&dev->images[rndr->swapchain_fb_depth_stencil_im_ind]);
    init_swapchain_framebuffers(rndr);
}

intern i32 acquire_swapchain_image(renderer *rndr, vkr_frame *cur_frame, u32 *im_ind)
{
    auto dev = &rndr->vk->inst.device;

    // Acquire the image, signal the image_avail semaphore once the image has been acquired. We get the index back, but
    // that doesn't mean the image is ready. The image is only ready (on the GPU side) once the image avail semaphore is triggered
    VkResult result = vkAcquireNextImageKHR(dev->hndl, dev->swapchain.swapchain, UINT64_MAX, cur_frame->image_avail, VK_NULL_HANDLE, im_ind);
    if (result == VK_ERROR_OUT_OF_DATE_KHR) {
        recreate_swapchain(rndr);
    }
    else if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) {
        elog("Failed to acquire swapchain image");
        return err_code::RENDER_ACQUIRE_IMAGE_FAIL;
    }
    return err_code::RENDER_NO_ERROR;
}

intern i32 submit_command_buffer(renderer *rndr, vkr_frame *cur_frame, vkr_command_buffer *cmd_buf)
{
    auto dev = &rndr->vk->inst.device;
    // Get the info ready to submit our command buffer to the queue. We need to wait until the image avail semaphore has
    // signaled, and then we need to trigger the render finished signal once the the command buffer completes
    VkSubmitInfo submit_info{};
    VkPipelineStageFlags wait_stages[] = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
    submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submit_info.waitSemaphoreCount = 1;
    submit_info.pWaitSemaphores = &cur_frame->image_avail;
    submit_info.pWaitDstStageMask = wait_stages;
    submit_info.commandBufferCount = 1;
    submit_info.pCommandBuffers = &cmd_buf->hndl;
    submit_info.signalSemaphoreCount = 1;
    submit_info.pSignalSemaphores = &cur_frame->render_finished;
    if (vkQueueSubmit(dev->qfams[VKR_QUEUE_FAM_TYPE_GFX].qs[0].hndl, 1, &submit_info, cur_frame->in_flight) != VK_SUCCESS) {
        return err_code::RENDER_SUBMIT_QUEUE_FAIL;
    }
    return err_code::RENDER_NO_ERROR;
}

intern i32 present_image(renderer *rndr, vkr_frame *cur_frame, u32 image_ind)
{
    auto dev = &rndr->vk->inst.device;
    // Once the rendering signal has fired, present the image (show it on screen)
    VkPresentInfoKHR present_info{};
    present_info.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    present_info.waitSemaphoreCount = 1;
    present_info.pWaitSemaphores = &cur_frame->render_finished;
    present_info.swapchainCount = 1;
    present_info.pSwapchains = &dev->swapchain.swapchain;
    present_info.pImageIndices = &image_ind;
    present_info.pResults = nullptr; // Optional - check for individual swaps
    VkResult result = vkQueuePresentKHR(dev->qfams[VKR_QUEUE_FAM_TYPE_PRESENT].qs[0].hndl, &present_info);
    if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR) {
        recreate_swapchain(rndr);
    }
    else if (result != VK_SUCCESS) {
        elog("Failed to presenet KHR");
        return err_code::RENDER_PRESENT_KHR_FAIL;
    }
    return err_code::RENDER_NO_ERROR;
}

intern vkr_frame *get_current_frame(renderer *rndr)
{
    // Grab the current in flight frame and its command buffer
    int current_frame_ind = rndr->finished_frames % MAX_FRAMES_IN_FLIGHT;
    return &rndr->vk->inst.device.rframes[current_frame_ind];
}

int rpush_sm(renderer *rndr, const static_model *sm, const transform *tf, const mesh_cache *msh_cache, const material_cache *mat_cache)
{
    auto dev = &rndr->vk->inst.device;
    auto cur_frame = get_current_frame(rndr);

    auto rmesh = hashmap_find(&rndr->rmi.meshes, sm->mesh_id);
    static auto default_mat = get_robj(DEFAULT_MAT_ID, mat_cache);
    assert(rmesh);

    // First iterate through all submeshes and associated materials
    // TODO: Create a default material to fall back on in the case that a submesh does not have a material
    auto msh = get_robj(sm->mesh_id, msh_cache);
    assert(msh);
    assert(rmesh->value.submesh_entrees.size == msh->submeshes.size);
    sizet cur_desc_ind = cur_frame->desc_pool.desc_sets.size;

    for (int i = 0; i < msh->submeshes.size; ++i) {
        auto mat = default_mat;
        if (sm->mat_ids[i].id != 0) {
            auto mato = get_robj(sm->mat_ids[i], mat_cache);
            if (mato) {
                mat = mato;
            }
        }

        sizet pli{};
        // Go through all pipelines this material references
        while (auto pl_iter = hashset_iter(&mat->pipelines, &pli)) {
            auto pl_fiter = hmap_find(&rndr->pipelines, *pl_iter);
            assert(pl_fiter);
            auto pline = &dev->pipelines[pl_fiter->val.plind];

            // Get the pipelines render pass
            auto rp_fiter = hmap_find(&rndr->rpasses, pl_fiter->val.rpass_id);
            assert(rp_fiter);

            // If the render pass has not yet been added to our renderer's push list, add it now
            auto push_rp_fiter = hashmap_find(&rndr->dcs.rpasses, rp_fiter->key);
            if (!push_rp_fiter) {
                auto rpdg = mem_alloc<render_pass_draw_group>(&rndr->frame_fl);
                memset(rpdg, 0, sizeof(render_pass_draw_group));
                rpdg->rpinfo = &rp_fiter->val;
                arr_init(&rpdg->desc_updates, &rndr->frame_fl);
                arr_init(&rpdg->sets_to_make, &rndr->frame_fl);
                hashmap_init(&rpdg->plines, &rndr->frame_fl);

                // Add the frame rpass descriptor set
                arr_emplace_back(&rpdg->sets_to_make, pline->descriptor_layouts[DESCRIPTOR_SET_LAYOUT_FRAME]);
                rpdg->fset = cur_desc_ind;
                ++cur_desc_ind;

                // Add the obj rpass descriptor set
                arr_emplace_back(&rpdg->sets_to_make, pline->descriptor_layouts[DESCRIPTOR_SET_LAYOUT_OBJECT]);
                rpdg->oset = cur_desc_ind;
                ++cur_desc_ind;

                hashmap_set(&rndr->dcs.rpasses, rp_fiter->key, rpdg);
                push_rp_fiter = hashmap_find(&rndr->dcs.rpasses, rp_fiter->key);
            }
            assert(push_rp_fiter);

            // If the pipeline has not yey been added to our render pass' push list, add it now
            auto push_pl_fiter = hashmap_find(&push_rp_fiter->value->plines, pl_fiter->key);
            if (!push_pl_fiter) {
                auto pldg = mem_alloc<pipeline_draw_group>(&rndr->frame_fl);
                memset(pldg, 0, sizeof(pipeline_draw_group));

                pldg->plinfo = &pl_fiter->val;
                hashmap_init(&pldg->mats, &rndr->frame_fl);

                // Add the pipeline discriptor set
                arr_emplace_back(&push_rp_fiter->value->sets_to_make, pline->descriptor_layouts[DESCRIPTOR_SET_LAYOUT_PIPELINE]);
                pldg->set_ind = cur_desc_ind;
                ++cur_desc_ind;

                hashmap_set(&push_rp_fiter->value->plines, pl_fiter->key, pldg);
                push_pl_fiter = hashmap_find(&push_rp_fiter->value->plines, pl_fiter->key);
            }
            assert(push_pl_fiter);

            // Now create the material set, if it doesn't exist
            auto push_mat_fiter = hashmap_find(&push_pl_fiter->value->mats, mat->id);
            if (!push_mat_fiter) {
                auto matdg = mem_alloc<material_draw_group>(&rndr->frame_fl);
                memset(matdg, 0, sizeof(material_draw_group));

                matdg->mat = mat;
                arr_init(&matdg->dcs, &rndr->frame_fl);

                // Add the material discriptor set
                // Add the pipeline discriptor set
                arr_emplace_back(&push_rp_fiter->value->sets_to_make, pline->descriptor_layouts[DESCRIPTOR_SET_LAYOUT_MATERIAL]);
                matdg->set_ind = cur_desc_ind;
                ++cur_desc_ind;

                hashmap_set(&push_pl_fiter->value->mats, mat->id, matdg);
                push_mat_fiter = hashmap_find(&push_pl_fiter->value->mats, mat->id);
            }
            assert(push_mat_fiter);

            // Now we create the actual draw call, and add the tform to the tform uniform buffer and increase the cur index
            draw_call dc{.index_count = (u32)rmesh->value.submesh_entrees[i].inds.size,
                         .instance_count = 1,
                         .first_index = (u32)rmesh->value.submesh_entrees[i].inds.offset,
                         .vertex_offset = (u32)rmesh->value.submesh_entrees[i].verts.offset,
                         .first_instance = 0,
                         .tf = &tf->cached};

            arr_push_back(&push_mat_fiter->value->dcs, dc);
        }
    }
    return err_code::VKR_NO_ERROR;
}

int render_frame_begin(renderer *rndr, int finished_frame_count)
{
    auto dev = &rndr->vk->inst.device;

    // Clear our frame rendering entries
    hashmap_terminate(&rndr->dcs.rpasses);

    mem_reset_arena(&rndr->vk_frame_linear);
    mem_reset_arena(&rndr->frame_fl);

    hashmap_init(&rndr->dcs.rpasses, &rndr->frame_fl);

    // Update finished frames which is used to get the current frame
    rndr->finished_frames = finished_frame_count;
    auto cur_frame = get_current_frame(rndr);

    // We wait until this FIF's fence has been triggered before rendering the frame. FIF fences are created in a
    // triggered state so there will be no waiting on the first time. We then reset the fence (aka set it to
    // untriggered) and it is passed to the vkQueueSubmit call to trigger it again. So if not the first time rendering
    // this FIF, we are waiting for the vkQueueSubmit from the previous time this FIF was rendered to complete
    VkResult vk_err = vkWaitForFences(dev->hndl, 1, &cur_frame->in_flight, VK_TRUE, UINT64_MAX);
    if (vk_err != VK_SUCCESS) {
        elog("Failed to wait for fence");
        return err_code::RENDER_WAIT_FENCE_FAIL;
    }

    // Clear all prev desc sets
    vkr_reset_descriptor_pool(&cur_frame->desc_pool, rndr->vk);

    return err_code::VKR_NO_ERROR;
}

intern int update_uniform_descriptors(renderer *rndr, vkr_frame *cur_frame)
{
    auto dev = &rndr->vk->inst.device;
    // Update all descriptor sets
    sizet rpi{};
    sizet total_dci{};
    while (auto rp_iter = hashmap_iter(&rndr->dcs.rpasses, &rpi)) {
        vkr_add_result desc_ind =
            vkr_add_descriptor_sets(&cur_frame->desc_pool, rndr->vk, rp_iter->value->sets_to_make.data, rp_iter->value->sets_to_make.size);
        if (desc_ind.err_code != err_code::VKR_NO_ERROR) {
            return desc_ind.err_code;
        }

        auto updates = &rp_iter->value->desc_updates;
        // Now update our pipeline ubo with this pipeline data
        frame_ubo_data frame_ubo{};
        frame_ubo.frame_count = rndr->finished_frames;
        char *addr = (char *)rndr->vk->inst.device.buffers[cur_frame->frame_ubo_ind].mem_info.pMappedData;
        memcpy(addr, &frame_ubo, sizeof(frame_ubo_data));

        // Add the frame rpass desc write update
        add_desc_write_update(
            rndr, cur_frame, 0, sizeof(frame_ubo_data), cur_frame->frame_ubo_ind, rp_iter->value->fset, updates, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);

        // Add the dynamic per obj desc write update
        add_desc_write_update(rndr,
                              cur_frame,
                              0,
                              sizeof(obj_ubo_data),
                              cur_frame->obj_ubo_ind,
                              rp_iter->value->oset,
                              updates,
                              VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC);

        // Update material and pipeline UBOs and create the descriptor writes for them
        sizet pli{};
        while (auto pl_iter = hashmap_iter(&rp_iter->value->plines, &pli)) {
            // Now update our pipeline ubo with this pipeline data
            pipeline_ubo_data pl_ubo{};
            pl_ubo.proj_view = pl_iter->value->plinfo->proj_view;
            sizet byte_offset = (pli - 1) * sizeof(pipeline_ubo_data);
            char *adjusted_addr = (char *)dev->buffers[cur_frame->pl_ubo_ind].mem_info.pMappedData + byte_offset;
            memcpy(adjusted_addr, &pl_ubo, sizeof(pipeline_ubo_data));

            add_desc_write_update(rndr,
                                  cur_frame,
                                  byte_offset,
                                  sizeof(pipeline_ubo_data),
                                  cur_frame->pl_ubo_ind,
                                  pl_iter->value->set_ind,
                                  updates,
                                  VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);

            sizet mati{};
            while (auto mat_iter = hashmap_iter(&pl_iter->value->mats, &mati)) {
                // Now update our material ubo with this mat data
                material_ubo_data mat_ubo{};
                mat_ubo.color = mat_iter->value->mat->col;
                sizet byte_offset = (mati - 1) * sizeof(material_ubo_data);
                char *adjusted_addr = (char *)dev->buffers[cur_frame->mat_ubo_ind].mem_info.pMappedData + byte_offset;
                memcpy(adjusted_addr, &mat_ubo, sizeof(material_ubo_data));

                add_desc_write_update(rndr,
                                      cur_frame,
                                      byte_offset,
                                      sizeof(material_ubo_data),
                                      cur_frame->mat_ubo_ind,
                                      mat_iter->value->set_ind,
                                      updates,
                                      VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);

                for (int dci = 0; dci < mat_iter->value->dcs.size; ++dci) {
                    // Now update our object ubo with this dc data and update the total count
                    auto cur_dc = &mat_iter->value->dcs[dci];
                    obj_ubo_data oubo{};
                    oubo.transform = *cur_dc->tf;
                    sizet byte_offset = total_dci * sizeof(obj_ubo_data);
                    char *adjusted_addr = (char *)dev->buffers[cur_frame->obj_ubo_ind].mem_info.pMappedData + byte_offset;
                    memcpy(adjusted_addr, &oubo, sizeof(obj_ubo_data));
                    ++total_dci;
                }
            }
        }

        vkUpdateDescriptorSets(dev->hndl, rp_iter->value->desc_updates.size, rp_iter->value->desc_updates.data, 0, nullptr);
    }
    return err_code::VKR_NO_ERROR;
}

int render_frame_end(renderer *rndr, camera *cam)
{
    auto dev = &rndr->vk->inst.device;
    auto cur_frame = get_current_frame(rndr);

    // Get the next available swapchain image index
    u32 im_ind{};
    i32 err = acquire_swapchain_image(rndr, cur_frame, &im_ind);
    if (err != err_code::RENDER_NO_ERROR) {
        return err;
    }

    // Recreating the swapchain here before even acquiring images gives us the smoothest resizing, but we still need to
    // handle recreation for other things that might happen so keep the acquire image and present image recreations
    // based on return value
    if (platform_framebuffer_resized(rndr->vk->cfg.window)) {
        ivec2 sz = get_framebuffer_size(rndr->vk->cfg.window);
        if (cam) {
            cam->proj = (math::perspective(60.0f, (f32)sz.w / (f32)sz.h, 0.1f, 1000.0f));
        }
    }

    int vk_err = vkResetFences(dev->hndl, 1, &cur_frame->in_flight);
    if (vk_err != VK_SUCCESS) {
        elog("Failed to reset fence");
        return err_code::RENDER_RESET_FENCE_FAIL;
    }

    // Update our main pipeline view_proj with the cam transform update
    if (cam) {
        auto pline_to_update = hmap_find(&rndr->pipelines, PLINE_FWD_RPASS_S0_OPAQUE);
        assert(pline_to_update);
        pline_to_update->val.proj_view = cam->proj * cam->view;
    }

    // Update all uniform buffers and write the descriptor set updates for them
    err = update_uniform_descriptors(rndr, cur_frame);
    if (err != err_code::RENDER_NO_ERROR) {
        return err;
    }

    // The command buf index struct has an ind struct into the pool the cmd buf comes from, and then an ind into the buffer
    // The ind into the pool has an ind into the queue family (as that contains our array of command pools) and then and
    // ind to the command pool
    auto buf_ind = cur_frame->cmd_buf_ind;
    auto cmd_buf = &dev->qfams[buf_ind.pool_ind.qfam_ind].cmd_pools[buf_ind.pool_ind.pool_ind].buffers[buf_ind.buffer_ind];
    auto fb = &dev->framebuffers[im_ind];

    // We have the acquired image index, though we don't know when it will be ready to have ops submitted, we can record
    // the ops in the command buffer and submit once it is readyy
    err = record_command_buffer(rndr, fb, cur_frame, cmd_buf);
    if (err != err_code::RENDER_NO_ERROR) {
        return err;
    }

    // Submit command buffer to GPU
    err = submit_command_buffer(rndr, cur_frame, cmd_buf);
    if (err != err_code::RENDER_NO_ERROR) {
        return err;
    }

    return present_image(rndr, cur_frame, im_ind);
}

void terminate_renderer(renderer *rndr)
{
    hashmap_terminate(&rndr->dcs.rpasses);
    mem_reset_arena(&rndr->vk_frame_linear);
    mem_reset_arena(&rndr->frame_fl);

    // These are stack arenas so must go in this order
    hashmap_terminate(&rndr->rmi.meshes);
    mem_terminate_arena(&rndr->rmi.inds.node_pool);
    mem_terminate_arena(&rndr->rmi.verts.node_pool);

    vkr_terminate(rndr->vk);
    mem_free(rndr->vk, &rndr->vk_free_list);

    mem_terminate_arena(&rndr->vk_free_list);
    mem_terminate_arena(&rndr->vk_frame_linear);
    mem_terminate_arena(&rndr->frame_fl);

    hmap_terminate(&rndr->rpasses);
    hmap_terminate(&rndr->pipelines);
}

} // namespace nslib
