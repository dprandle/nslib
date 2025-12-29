#include "sim_region.h"
#include "containers/string.h"
#include "containers/linked_list.h"
#include "stb_image.h"
#include "platform.h"
#include "vk_context.h"
#include "renderer.h"

#include "imgui/imgui.h"
#include "imgui/imgui_impl_sdl3.h"
#include "imgui/imgui_impl_vulkan.h"
#include "SDL3/SDL_events.h"

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
intern constexpr VkInstanceCreateFlags INST_CREATE_FLAGS = {};
intern constexpr u32 ADDITIONAL_INST_EXTENSION_COUNT = 1;
intern constexpr const char *ADDITIONAL_INST_EXTENSIONS[ADDITIONAL_INST_EXTENSION_COUNT] = {VK_EXT_DEBUG_UTILS_EXTENSION_NAME};
intern constexpr u32 DEVICE_EXTENSION_COUNT = 1;
intern constexpr const char *DEVICE_EXTENSIONS[DEVICE_EXTENSION_COUNT] = {VK_KHR_SWAPCHAIN_EXTENSION_NAME};
#endif

intern constexpr f64 RESIZE_DEBOUNCE_FRAME_COUNT = 0.15; // 100 ms
intern VkPipelineLayout G_FRAME_PL_LAYOUT{};

enum descriptor_set_layout
{
    DESCRIPTOR_SET_LAYOUT_FRAME,
    DESCRIPTOR_SET_LAYOUT_PIPELINE,
    DESCRIPTOR_SET_LAYOUT_MATERIAL,
    DESCRIPTOR_SET_LAYOUT_OBJECT,
};

enum descriptor_set_binding
{
    DESCRIPTOR_SET_BINDING_UNIFORM_BUFFER,
    DESCRIPTOR_SET_BINDING_IMAGE_SAMPLER,
};

enum draw_call_flags
{
    DRAW_CALL_FLAG_NONE = 0,
    DRAW_CALL_FLAG_HIDDEN = 1 << 0,
};

struct draw_call
{
    u32 index_count;
    u32 instance_count;
    u32 first_index;
    u32 vertex_offset;
    u32 first_instance;
    u32 flags{};
    sizet ubo_offset;
};

struct material_draw_bucket
{
    const material_info *mi;
    sizet set_layouti;
    array<draw_call> dcs;
};

struct pipeline_draw_bucket
{
    const pipeline_info *plinfo;
    // Not to be confused with the set index in the descriptor set layout, this is the index of the descriptor set struct
    // that should be bound during draw - ie
    sizet set_layouti;
    hmap<rid, material_draw_bucket *> mats;
};

struct render_pass_draw_bucket
{
    const rpass_info *rpinfo;
    sizet frame_set_layouti{INVALID_IND};
    sizet obj_set_layouti{INVALID_IND};
    array<VkDescriptorSetLayout> set_layouts;
    hmap<rid, pipeline_draw_bucket *> plines;
};

intern void imgui_mem_free(void *ptr, void *usr)
{
    mem_free(ptr, (mem_arena *)usr);
}

intern void *imgui_mem_alloc(sizet sz, void *usr)
{
    return mem_alloc(sz, (mem_arena *)usr, SIMD_MIN_ALIGNMENT);
}

intern void check_vk_result(VkResult err)
{
    if (err != VK_SUCCESS) {
        wlog("vulkan err: %d", err);
    }
    asrt(err >= 0);
}

bool sdl_event_func(void *sdl_event, void *)
{
    auto *ev = (SDL_Event *)sdl_event;
    auto io = ImGui::GetIO();
    ImGui_ImplSDL3_ProcessEvent(ev);
    if (ev->type == SDL_EVENT_MOUSE_BUTTON_DOWN || ev->type == SDL_EVENT_MOUSE_BUTTON_UP || ev->type == SDL_EVENT_MOUSE_WHEEL) {
        return io.WantCaptureMouse;
    }
    // NOTE: We might uncomment this in the future but for now we don't need to capture keyboard..
    if (ev->type == SDL_EVENT_KEY_DOWN || ev->type == SDL_EVENT_KEY_UP) {
        return io.WantCaptureKeyboard;
    }
    return false;
}

intern void init_imgui(renderer *rndr, void *win_hndl)
{
    auto dev = &rndr->vk.inst.device;
    // 263 KB seems to be about the min required - we'll give it a MB
    mem_init_fl_arena(&rndr->imgui.fl, MB_SIZE, rndr->upstream_fl_arena, "imgui");

    // Use the main forward pass for imgui.. this might only change if we use deferred shading.. but i think the imgui
    // created pipeling only requires a color attachment
    auto rpass_fiter = hmap_find(&rndr->rpasses, FWD_RPASS);
    asrt(rpass_fiter);
    rndr->imgui.rpass = &dev->render_passes[rpass_fiter->val.rpind];

    ImGui::SetAllocatorFunctions(imgui_mem_alloc, imgui_mem_free, &rndr->imgui.fl);
    rndr->imgui.ctxt = ImGui::CreateContext();
    ImGui::StyleColorsDark();
    auto &io = ImGui::GetIO();
    io.FontGlobalScale = get_window_display_scale(win_hndl);

    vkr_descriptor_cfg cfg{};
    cfg.max_desc_per_type[VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER] = IMGUI_IMPL_VULKAN_MINIMUM_IMAGE_SAMPLER_POOL_SIZE;
    cfg.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
    if (vkr_init_descriptor_pool(&rndr->imgui.pool, &rndr->vk, &cfg) != err_code::VKR_NO_ERROR) {
        wlog("Could not create imgui descriptor pool");
    }

    ImGui_ImplSDL3_InitForVulkan((SDL_Window *)rndr->vk.cfg.window);

    ImGui_ImplVulkan_InitInfo init_info = {};
    init_info.ApiVersion = VKR_API_VERSION;
    init_info.Instance = rndr->vk.inst.hndl;
    init_info.PhysicalDevice = rndr->vk.inst.pdev_info.hndl;
    init_info.Device = rndr->vk.inst.device.hndl;
    init_info.QueueFamily = rndr->vk.inst.device.qfams[VKR_QUEUE_FAM_TYPE_GFX].fam_ind;
    init_info.Queue = rndr->vk.inst.device.qfams[VKR_QUEUE_FAM_TYPE_GFX].qs[VKR_RENDER_QUEUE].hndl;
    init_info.PipelineCache = VK_NULL_HANDLE;
    init_info.DescriptorPool = rndr->imgui.pool.hndl;
    init_info.Allocator = &rndr->vk.alloc_cbs;
    init_info.MinImageCount = MAX_FRAMES_IN_FLIGHT;
    init_info.ImageCount = rndr->vk.inst.device.swapchain.images.size;
    init_info.RenderPass = rndr->imgui.rpass->hndl;
    init_info.Subpass = 0;
    init_info.CheckVkResultFn = check_vk_result;
    // init_info.MSAASamples
    ImGui_ImplVulkan_Init(&init_info);

    if (!ImGui_ImplVulkan_CreateFontsTexture()) {
        wlog("Could not create imgui vulkan font texture");
    }

    set_platform_sdl_event_hook(win_hndl, {.cb = sdl_event_func});
}

intern void terminate_imgui(renderer *rndr)
{
    ImGui_ImplVulkan_Shutdown();
    vkr_terminate_descriptor_pool(&rndr->imgui.pool, &rndr->vk);
    ImGui_ImplSDL3_Shutdown();
    ImGui::DestroyContext(rndr->imgui.ctxt);
    mem_terminate_arena(&rndr->imgui.fl);
}

intern int setup_render_pass(renderer *rndr)
{
    auto vk = &rndr->vk;
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

    att.format = vkr_find_best_depth_format(&rndr->vk.inst.pdev_info);
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

    int ret = vkr_init_render_pass(&vk->inst.device.render_passes[rpi.rpind], &rp_cfg, vk);
    if (ret == err_code::VKR_NO_ERROR) {
        ilog("Setting render pass %s", str_cstr(rpi.id.str));
        hmap_set(&rndr->rpasses, rpi.id, rpi);
    }
    return ret;
}

intern int setup_diffuse_mat_pipeline(renderer *rndr)
{
    auto vk = &rndr->vk;
    vkr_pipeline_cfg info{};

    arr_push_back(&info.dynamic_states, VK_DYNAMIC_STATE_VIEWPORT);
    arr_push_back(&info.dynamic_states, VK_DYNAMIC_STATE_SCISSOR);

    // Descripitor Set Layouts - Just one layout for the moment with a binding at 0 for uniforms and a binding at 1 for
    // image sampler
    info.set_layouts.size = 4;

    // Descriptor layouts
    // Single Uniform buffer
    VkDescriptorSetLayoutBinding b{};
    b.binding = DESCRIPTOR_SET_BINDING_UNIFORM_BUFFER;
    b.descriptorCount = 1;
    b.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
    b.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;

    // Add uniform buffer binding to each set, and image sampler to material set as well
    arr_push_back(&info.set_layouts[DESCRIPTOR_SET_LAYOUT_FRAME].bindings, b);
    arr_push_back(&info.set_layouts[DESCRIPTOR_SET_LAYOUT_PIPELINE].bindings, b);
    arr_push_back(&info.set_layouts[DESCRIPTOR_SET_LAYOUT_MATERIAL].bindings, b);

    // Change descriptor type to dynamic descriptor for object - we bind it with an offset for each object
    b.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
    arr_push_back(&info.set_layouts[DESCRIPTOR_SET_LAYOUT_OBJECT].bindings, b);

    // Add image sampler to material
    b.binding = DESCRIPTOR_SET_BINDING_IMAGE_SAMPLER;
    b.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    b.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    arr_push_back(&info.set_layouts[DESCRIPTOR_SET_LAYOUT_MATERIAL].bindings, b);

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
    info.raster.cull_mode = VK_CULL_MODE_NONE;
    info.raster.front_face = VK_FRONT_FACE_CLOCKWISE;
    info.raster.depth_bias_enable = false;
    info.raster.depth_bias_constant_factor = 0.0f;
    info.raster.depth_bias_clamp = 0.0f;
    info.raster.depth_bias_slope_factor = 0.0f;

    // Multisampling defaults are good

    // Color blending - none for this pipeline
    VkPipelineColorBlendAttachmentState col_blnd_att{};
    col_blnd_att.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    col_blnd_att.blendEnable = false;
    arr_push_back(&info.col_blend.attachments, col_blnd_att);

    // Depth Stencil
    info.depth_stencil.depth_test_enable = true;
    info.depth_stencil.depth_write_enable = true;
    info.depth_stencil.depth_compare_op = VK_COMPARE_OP_LESS;
    info.depth_stencil.depth_bounds_test_enable = false;
    info.depth_stencil.min_depth_bounds = 0.0f;
    info.depth_stencil.max_depth_bounds = 1.0f;

    // Our basic shaders
    const char *fnames[] = {"data/shaders/fwd-diffuse.vert.spv", "data/shaders/fwd-diffuse.frag.spv"};
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
        plinfo.id = PLINE_FWD_RPASS_S0_OPAQUE_DIFFUSE;
        plinfo.ubo_offset = rndr->pipelines.count;
        plinfo.plind = vkr_add_pipeline(&vk->inst.device, {});
        plinfo.rpass_id = rpass_fiter->val.id;
        int code = vkr_init_pipeline(&vk->inst.device.pipelines[plinfo.plind], &info, vk);
        if (code == err_code::VKR_NO_ERROR) {
            hmap_set(&rndr->pipelines, plinfo.id, plinfo);
            // Set our global frame layout
            G_FRAME_PL_LAYOUT = vk->inst.device.pipelines[plinfo.plind].layout_hndl;
        }
        return code;
    }
    else {
        elog("Failed to find render pass %s", str_cstr(FWD_RPASS.str));
    }
    return err_code::RENDER_INIT_FAIL;
}

intern int setup_color_mat_pipeline(renderer *rndr)
{
    auto vk = &rndr->vk;
    vkr_pipeline_cfg info{};

    arr_push_back(&info.dynamic_states, VK_DYNAMIC_STATE_VIEWPORT);
    arr_push_back(&info.dynamic_states, VK_DYNAMIC_STATE_SCISSOR);

    // Descripitor Set Layouts - Just one layout for the moment with a binding at 0 for uniforms and a binding at 1 for
    // image sampler
    info.set_layouts.size = 4;

    // Single Uniform buffer
    VkDescriptorSetLayoutBinding b{};
    b.binding = DESCRIPTOR_SET_BINDING_UNIFORM_BUFFER;
    b.descriptorCount = 1;
    b.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
    b.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;

    // Add uniform buffer binding to each set
    arr_push_back(&info.set_layouts[DESCRIPTOR_SET_LAYOUT_FRAME].bindings, b);
    arr_push_back(&info.set_layouts[DESCRIPTOR_SET_LAYOUT_PIPELINE].bindings, b);
    arr_push_back(&info.set_layouts[DESCRIPTOR_SET_LAYOUT_MATERIAL].bindings, b);

    // Change descriptor type to dynamic descriptor for object so we only are binding it once per frame
    b.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
    arr_push_back(&info.set_layouts[DESCRIPTOR_SET_LAYOUT_OBJECT].bindings, b);

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
    col_blnd_att.blendEnable = false;
    arr_push_back(&info.col_blend.attachments, col_blnd_att);

    // This is to do alpha blending - leaving here until we create a pipeline that uses it
    // col_blnd_att.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT |
    // VK_COLOR_COMPONENT_A_BIT; col_blnd_att.blendEnable = true; col_blnd_att.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
    // col_blnd_att.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    // col_blnd_att.colorBlendOp = VK_BLEND_OP_ADD;
    // col_blnd_att.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
    // col_blnd_att.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
    // col_blnd_att.alphaBlendOp = VK_BLEND_OP_ADD;

    // Depth Stencil
    info.depth_stencil.depth_test_enable = true;
    info.depth_stencil.depth_write_enable = true;
    info.depth_stencil.depth_compare_op = VK_COMPARE_OP_LESS;
    info.depth_stencil.depth_bounds_test_enable = false;
    info.depth_stencil.min_depth_bounds = 0.0f;
    info.depth_stencil.max_depth_bounds = 1.0f;

    // Our basic shaders
    const char *fnames[] = {"data/shaders/fwd-color.vert.spv", "data/shaders/fwd-color.frag.spv"};
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
        plinfo.id = PLINE_FWD_RPASS_S0_OPAQUE_COL;
        plinfo.ubo_offset = rndr->pipelines.count;
        plinfo.plind = vkr_add_pipeline(&vk->inst.device, {});
        plinfo.rpass_id = rpass_fiter->val.id;
        int code = vkr_init_pipeline(&vk->inst.device.pipelines[plinfo.plind], &info, vk);
        if (code == err_code::VKR_NO_ERROR) {
            hmap_set(&rndr->pipelines, plinfo.id, plinfo);
            // Set our global frame layout
            G_FRAME_PL_LAYOUT = vk->inst.device.pipelines[plinfo.plind].layout_hndl;
        }
        return code;
    }
    else {
        elog("Failed to find render pass %s", str_cstr(FWD_RPASS.str));
    }
    return err_code::RENDER_INIT_FAIL;
}

intern int setup_rmesh_info(renderer *rndr)
{
    auto vk = &rndr->vk;
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
    b_cfg.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    b_cfg.buffer_size = DEFAULT_VERT_BUFFER_SIZE * sizeof(vertex);
    int err = vkr_init_buffer(&dev->buffers[rndr->rmi.verts.buf_ind], &b_cfg);
    if (err != err_code::VKR_NO_ERROR) {
        return err;
    }

    // Ind buffer
    b_cfg.usage = VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    b_cfg.buffer_size = DEFAULT_IND_BUFFER_SIZE * sizeof(ind_t);
    err = vkr_init_buffer(&dev->buffers[rndr->rmi.inds.buf_ind], &b_cfg);
    if (err != err_code::VKR_NO_ERROR) {
        return err;
    }

    mem_init_pool_arena<sbuffer_entry_slnode>(&rndr->rmi.verts.node_pool, MAX_FREE_SBUFFER_NODE_COUNT, mem_global_stack_arena(), "mesh-verts");
    mem_init_pool_arena<sbuffer_entry_slnode>(&rndr->rmi.inds.node_pool, MAX_FREE_SBUFFER_NODE_COUNT, mem_global_stack_arena(), "mesh-inds");
    hmap_init(&rndr->rmi.meshes, hash_type);

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
    asrt(node);
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

bool upload_to_gpu(const texture *tex, renderer *rndr)
{
    auto dev = &rndr->vk.inst.device;
    // Most default values are correct
    vkr_image_cfg cfg{};
    cfg.format = VK_FORMAT_R8G8B8A8_SRGB;
    cfg.usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    cfg.dims = {tex->size, 1};
    cfg.mem_usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;
    cfg.vma_alloc = &rndr->vk.inst.device.vma_alloc;

    // Create image
    vkr_image im{};
    int err = vkr_init_image(&im, &cfg);
    if (err != err_code::VKR_NO_ERROR) {
        wlog("Failed to initialize vk image for texture %s - err code: %d", str_cstr(tex->id.str), err);
        return err;
    }

    // Upload texture data to created image using staging buffer
    err = vkr_stage_and_upload_image_data(
        &im, tex->pixels.data, get_texture_memsize(tex), &dev->qfams[VKR_QUEUE_FAM_TYPE_GFX], VKR_RENDER_QUEUE, &rndr->vk);
    if (err != err_code::VKR_NO_ERROR) {
        wlog("Failed to upload texture data for %s with err code %d", to_cstr(tex->name), err);
        vkr_terminate_image(&im);
    }

    // Create image view
    vkr_image_view iview{};
    vkr_image_view_cfg iview_cfg{};
    iview_cfg.image = &im;

    err = vkr_init_image_view(&iview, &iview_cfg, &rndr->vk);
    if (err != err_code::VKR_NO_ERROR) {
        wlog("Failed to initialize image view for texture %s - err code: %d", to_cstr(tex->name), err);
        vkr_terminate_image(&im);
        return err;
    }

    // Create image sampler
    vkr_sampler sampler{};
    vkr_sampler_cfg samp_cfg{};
    for (int i = 0; i < 3; ++i) {
        samp_cfg.address_mode_uvw[i] = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    }
    samp_cfg.mag_filter = VK_FILTER_LINEAR;
    samp_cfg.min_filter = VK_FILTER_LINEAR;
    samp_cfg.anisotropy_enable = true;
    samp_cfg.max_anisotropy = rndr->vk.inst.pdev_info.props.limits.maxSamplerAnisotropy;
    samp_cfg.border_color = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
    samp_cfg.compare_op = VK_COMPARE_OP_ALWAYS;
    samp_cfg.mipmap_mode = VK_SAMPLER_MIPMAP_MODE_LINEAR;

    err = vkr_init_sampler(&sampler, &samp_cfg, &rndr->vk);
    if (err != err_code::VKR_NO_ERROR) {
        wlog("Failed to initialize sampler for texture %s - err code: %d", to_cstr(tex->name), err);
        vkr_terminate_image(&im);
        vkr_terminate_image_view(&iview);
        return err;
    }

    texture_info stex_info{};
    stex_info.im = vkr_add_image(dev, im);
    stex_info.im_view = vkr_add_image_view(dev, iview);
    stex_info.sampler = vkr_add_sampler(dev, sampler);

    auto ins = hmap_insert(&rndr->textures, tex->id, stex_info);
    if (!ins) {
        wlog("Failed to insert sampled texture %s into renderer sampled texture map", str_cstr(tex->name));
        vkr_terminate_image(&im);
        vkr_terminate_image_view(&iview);
        vkr_terminate_sampler(&sampler);
        return false;
    }
    return true;
}

// Upload mesh data to GPU using the shared indice/vertex buffer, also "registers" the mesh with the renderer so it can
// be drawn
bool upload_to_gpu(const mesh *msh, renderer *rndr)
{
    auto fiter = hmap_find(&rndr->rmi.meshes, msh->id);
    if (fiter) {
        return false;
    }
    auto dev = &rndr->vk.inst.device;
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
        asrt(new_smentry.verts.size > 0);
        asrt(new_smentry.inds.size > 0);
        arr_emplace_back(&new_mentry.submesh_entrees, new_smentry);

        // Our required vert and ind size might be a little less than the avail block size, if a block was picked that
        // was big enough to fit our needs but the remaining available in the block was less than the required min block
        // size for the sbuffer
        VkBufferCopy vert_region{}, ind_region{};
        vert_region.size = req_vert_byte_size;
        vert_region.dstOffset = new_smentry.verts.offset * sizeof(vertex);

        ind_region.size = req_inds_byte_size;
        ind_region.dstOffset = new_smentry.inds.offset * sizeof(ind_t);

        // Upload our vert data to the GPU
        int ret = vkr_stage_and_upload_buffer_data(&dev->buffers[rndr->rmi.verts.buf_ind],
                                                   msh->submeshes[subi].verts.data,
                                                   req_vert_byte_size,
                                                   &vert_region,
                                                   &dev->qfams[VKR_QUEUE_FAM_TYPE_GFX],
                                                   VKR_RENDER_QUEUE,
                                                   &rndr->vk);

        // TODO: Handle error conditions here - there are several reasons why a buffer upload might fail - for now we
        // just asrt it worked
        asrt(ret == err_code::VKR_NO_ERROR);

        // Upload our ind data to the GPU
        ret = vkr_stage_and_upload_buffer_data(&dev->buffers[rndr->rmi.inds.buf_ind],
                                               msh->submeshes[subi].inds.data,
                                               req_inds_byte_size,
                                               &ind_region,
                                               &dev->qfams[VKR_QUEUE_FAM_TYPE_GFX],
                                               VKR_RENDER_QUEUE,
                                               &rndr->vk);
        asrt(ret == err_code::VKR_NO_ERROR);
    }
    ilog("Adding mesh id %s %d submeshes", str_cstr(msh->id.str), new_mentry.submesh_entrees.size);
    for (int si = 0; si < new_mentry.submesh_entrees.size; ++si) {
        auto sub = &new_mentry.submesh_entrees[si];
        ilog("submesh %d:  vertsp(ubo_offset:%d  size:%d)  inds(ubo_offset:%d size:%d)",
             si,
             sub->verts.offset,
             sub->verts.size,
             sub->inds.offset,
             sub->inds.size);
    }
    // Add the smesh entry we just built to the renderer mesh entry map (stored by id)
    hmap_set(&rndr->rmi.meshes, msh->id, new_mentry);
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
    auto minfo = hmap_find(&rndr->rmi.meshes, msh->id);
    if (minfo) {
        for (int subi = 0; subi < minfo->val.submesh_entrees.size; ++subi) {
            // Insert the block to our free list
            auto cur_entry = &minfo->val.submesh_entrees[subi];
            insert_node_to_free_list(&rndr->rmi.verts, &cur_entry->verts);
            insert_node_to_free_list(&rndr->rmi.inds, &cur_entry->inds);
        }
        hmap_remove(&rndr->rmi.meshes, msh->id);
    }
    return minfo;
}

intern int init_swapchain_images_and_framebuffer(renderer *rndr)
{
    auto vk = &rndr->vk;
    auto dev = &rndr->vk.inst.device;

    vkr_image_cfg im_cfg{};
    im_cfg.dims = {dev->swapchain.extent.width, dev->swapchain.extent.height, 1};
    im_cfg.format = vkr_find_best_depth_format(&rndr->vk.inst.pdev_info);
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
    asrt(rp_fiter);
    vkr_init_swapchain_framebuffers(dev,
                                    vk,
                                    &dev->render_passes[rp_fiter->val.rpind],
                                    vkr_framebuffer_attachment{dev->image_views[rndr->swapchain_fb_depth_stencil_iview_ind]});

    return err;
}

intern int setup_rendering(renderer *rndr)
{
    ilog("Setting up default rendering...");
    auto vk = &rndr->vk;
    auto dev = &rndr->vk.inst.device;

    int err = setup_render_pass(rndr);
    if (err != err_code::VKR_NO_ERROR) {
        elog("Failed to setup render pass");
        return err;
    }

    err = setup_diffuse_mat_pipeline(rndr);
    if (err != err_code::VKR_NO_ERROR) {
        elog("Failed to setup pipeline");
        return err;
    }

    err = setup_color_mat_pipeline(rndr);
    if (err != err_code::VKR_NO_ERROR) {
        elog("Failed to setup pipeline");
        return err;
    }

    err = init_swapchain_images_and_framebuffer(rndr);
    if (err != err_code::VKR_NO_ERROR) {
        elog("Failed to setup swapchain images/framebuffers");
        return err;
    }

    err = setup_rmesh_info(rndr);
    if (err != err_code::VKR_NO_ERROR) {
        elog("Failed to setup rmeshes");
        return err;
    }

    ////////////////////////////////////////////////////////////////////////////////
    // Create uniform buffers and descriptor sets pointing to them for each frame //
    ////////////////////////////////////////////////////////////////////////////////
    for (int i = 0; i < dev->rframes.size; ++i) {
        // Create a uniform buffers for each frame

        // Frame uniform buffer - per frame data
        vkr_buffer_cfg buf_cfg{};
        buf_cfg.mem_usage = VMA_MEMORY_USAGE_AUTO_PREFER_HOST;
        buf_cfg.sharing_mode = VK_SHARING_MODE_EXCLUSIVE;
        buf_cfg.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
        buf_cfg.buffer_size = vkr_uniform_buffer_offset_alignment(vk, sizeof(frame_ubo_data));
        buf_cfg.alloc_flags = VMA_ALLOCATION_CREATE_MAPPED_BIT | VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;
        buf_cfg.vma_alloc = &dev->vma_alloc;

        vkr_buffer frame_uniform_buf{};
        int err = vkr_init_buffer(&frame_uniform_buf, &buf_cfg);
        if (err != err_code::VKR_NO_ERROR) {
            return err;
        }
        dev->rframes[i].frame_ubo_ind = vkr_add_buffer(dev, frame_uniform_buf);

        // Pipeline uniform buffer - per pipeline data
        buf_cfg.buffer_size = MAX_PIPELINE_COUNT * vkr_uniform_buffer_offset_alignment(vk, sizeof(pipeline_ubo_data));
        vkr_buffer pl_uniform_buf{};
        err = vkr_init_buffer(&pl_uniform_buf, &buf_cfg);
        if (err != err_code::VKR_NO_ERROR) {
            return err;
        }
        dev->rframes[i].pl_ubo_ind = vkr_add_buffer(dev, pl_uniform_buf);

        // Material uniform buffer - per material data
        buf_cfg.buffer_size = MAX_MATERIAL_COUNT * vkr_uniform_buffer_offset_alignment(vk, sizeof(material_ubo_data));
        vkr_buffer mat_uniform_buf{};
        err = vkr_init_buffer(&mat_uniform_buf, &buf_cfg);
        if (err != err_code::VKR_NO_ERROR) {
            return err;
        }
        dev->rframes[i].mat_ubo_ind = vkr_add_buffer(dev, mat_uniform_buf);

        // Object uniform buffer - per material data
        buf_cfg.buffer_size = MAX_OBJECT_COUNT * vkr_uniform_buffer_offset_alignment(vk, sizeof(obj_ubo_data));
        vkr_buffer obj_uniform_buf{};
        err = vkr_init_buffer(&obj_uniform_buf, &buf_cfg);
        if (err != err_code::VKR_NO_ERROR) {
            return err;
        }
        dev->rframes[i].obj_ubo_ind = vkr_add_buffer(dev, obj_uniform_buf);
    }
    return err_code::VKR_NO_ERROR;
}

intern void add_image_sampler_desc_write_update(renderer *rndr,
                                                vkr_frame *cur_frame,
                                                sizet im_view,
                                                sizet sampler,
                                                sizet im_slot,
                                                sizet set_ind,
                                                array<VkWriteDescriptorSet> *updates,
                                                VkDescriptorType type)
{
    auto dev = &rndr->vk.inst.device;
    // Add a write descriptor set to update our obj descriptor to point at this portion of our unifrom buffer
    auto im_info = mem_calloc<VkDescriptorImageInfo>(1, &rndr->frame_linear);
    im_info->imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    im_info->imageView = dev->image_views[im_view].hndl;
    im_info->sampler = dev->samplers[sampler].hndl;

    VkWriteDescriptorSet desc_write{};
    desc_write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    desc_write.dstSet = cur_frame->desc_pool.desc_sets[set_ind].hndl;
    desc_write.dstBinding = DESCRIPTOR_SET_BINDING_IMAGE_SAMPLER + im_slot;
    desc_write.dstArrayElement = 0;
    desc_write.descriptorType = type;
    desc_write.descriptorCount = 1;
    desc_write.pImageInfo = im_info;
    arr_push_back(updates, desc_write);
}

intern void add_uniform_desc_write_update(renderer *rndr,
                                          vkr_frame *cur_frame,
                                          sizet ubo_offset,
                                          sizet range,
                                          sizet buf_ind,
                                          sizet set_ind,
                                          array<VkWriteDescriptorSet> *updates,
                                          VkDescriptorType type)
{
    // Add a write descriptor set to update our obj descriptor to point at this portion of our unifrom buffer
    auto buffer_info = mem_calloc<VkDescriptorBufferInfo>(1, &rndr->frame_linear);
    buffer_info->offset = ubo_offset;
    buffer_info->range = range;
    buffer_info->buffer = rndr->vk.inst.device.buffers[buf_ind].hndl;

    VkWriteDescriptorSet desc_write{};
    desc_write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    desc_write.dstSet = cur_frame->desc_pool.desc_sets[set_ind].hndl;
    desc_write.dstBinding = DESCRIPTOR_SET_BINDING_UNIFORM_BUFFER;
    desc_write.dstArrayElement = 0;
    desc_write.descriptorType = type;
    desc_write.descriptorCount = 1;
    desc_write.pBufferInfo = buffer_info;
    arr_push_back(updates, desc_write);
}

intern int record_command_buffer(renderer *rndr, vkr_framebuffer *fb, vkr_frame *cur_frame, vkr_command_buffer *cmd_buf)
{
    auto dev = &rndr->vk.inst.device;
    auto vert_buf = &dev->buffers[rndr->rmi.verts.buf_ind];
    auto ind_buf = &dev->buffers[rndr->rmi.inds.buf_ind];

    int err = vkr_begin_cmd_buf(cmd_buf);
    if (err != err_code::VKR_NO_ERROR) {
        return err;
    }

    VkClearValue att_clear_vals[] = {{.color{{0.05f, 0.05f, 0.05f, 1.0f}}}, {.depthStencil{1.0f, 0}}};

    // Bind the global vertex/index buffer/s
    VkBuffer vert_bufs[] = {vert_buf->hndl};
    VkDeviceSize offsets[] = {0};
    vkCmdBindVertexBuffers(cmd_buf->hndl, 0, 1, vert_bufs, offsets);
    vkCmdBindIndexBuffer(cmd_buf->hndl, ind_buf->hndl, 0, VK_INDEX_TYPE_UINT16);

    // TODO: This really can't go in any order we want for render passes.. We might have dependency ordered between
    // them.. for now we just have the one.. I'm not sure if we need to do a
    auto rpass_iter = hmap_begin(&rndr->dcs.rpasses);
    while (rpass_iter) {
        auto rpass = &dev->render_passes[rpass_iter->val->rpinfo->rpind];
        vkr_cmd_begin_rpass(cmd_buf, fb, rpass, att_clear_vals, 2);

        // Bind frame rpass descriptor set
        auto ds = cur_frame->desc_pool.desc_sets[rpass_iter->val->frame_set_layouti].hndl;
        vkCmdBindDescriptorSets(
            cmd_buf->hndl, VK_PIPELINE_BIND_POINT_GRAPHICS, G_FRAME_PL_LAYOUT, DESCRIPTOR_SET_LAYOUT_FRAME, 1, &ds, 0, nullptr);

        // We could make our render pass have the vert/index buffer info.. ie
        auto pl_iter = hmap_begin(&rpass_iter->val->plines);
        while (pl_iter) {
            // Grab the pipeline and set it, and set the viewport/scissor
            auto pipeline = &dev->pipelines[pl_iter->val->plinfo->plind];
            vkCmdBindPipeline(cmd_buf->hndl, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline->hndl);

            auto ds = cur_frame->desc_pool.desc_sets[pl_iter->val->set_layouti].hndl;
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

            auto mat_iter = hmap_begin(&pl_iter->val->mats);
            while (mat_iter) {
                // Bind the material set
                auto ds = cur_frame->desc_pool.desc_sets[mat_iter->val->set_layouti].hndl;
                vkCmdBindDescriptorSets(
                    cmd_buf->hndl, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline->layout_hndl, DESCRIPTOR_SET_LAYOUT_MATERIAL, 1, &ds, 0, nullptr);

                for (u32 dci = 0; dci < mat_iter->val->dcs.size; ++dci) {
                    const draw_call *cur_dc = &mat_iter->val->dcs[dci];
                    auto ds = cur_frame->desc_pool.desc_sets[rpass_iter->val->obj_set_layouti].hndl;
                    sizet obj_ubo_item_size = vkr_uniform_buffer_offset_alignment(&rndr->vk, sizeof(obj_ubo_data));
                    // Our dynamic ubo_offset in to our singlestoring all of our transforms is computed by adding
                    // the material base draw call ubo_offset (computed each frame).
                    u32 dyn_offset = (u32)(obj_ubo_item_size * cur_dc->ubo_offset);
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
                mat_iter = hmap_next(&pl_iter->val->mats, mat_iter);
            }
            pl_iter = hmap_next(&rpass_iter->val->plines, pl_iter);
        }

        // If we are on the imgui rpass, render its stuff. It has it's own pipeling, vertex/index buffers, etc
        if (rpass == rndr->imgui.rpass) {
            auto img_data = ImGui::GetDrawData();
            ImGui_ImplVulkan_RenderDrawData(img_data, cmd_buf->hndl);
        }

        vkr_cmd_end_rpass(cmd_buf);
        rpass_iter = hmap_next(&rndr->dcs.rpasses, rpass_iter);
    }

    return vkr_end_cmd_buf(cmd_buf);
}

int init_renderer(renderer *rndr, const handle<material> &default_mat, void *win_hndl, mem_arena *fl_arena)
{
    asrt(fl_arena->alloc_type == mem_alloc_type::FREE_LIST);
    rndr->upstream_fl_arena = fl_arena;
    mem_init_fl_arena(&rndr->vk_free_list, 500 * MB_SIZE, fl_arena, "vk");
    mem_init_lin_arena(&rndr->frame_linear, 10 * KB_SIZE, fl_arena, "frame");
    mem_init_lin_arena(&rndr->vk_frame_linear, 10 * MB_SIZE, mem_global_stack_arena(), "vk-frame");

    hmap_init(&rndr->rpasses, hash_type, fl_arena);
    hmap_init(&rndr->pipelines, hash_type, fl_arena);
    hmap_init(&rndr->materials, hash_type, fl_arena);
    hmap_init(&rndr->textures, hash_type, fl_arena);

    mem_init_lin_arena(&rndr->dcs.dc_linear, 100 * MB_SIZE, fl_arena, "dcs");
    hmap_init(&rndr->dcs.rpasses);

    rndr->default_mat = default_mat;

    // Set up our draw call list render pass hashmap with frame linear memory
    vkr_descriptor_cfg desc_cfg{};
    // Frame + pl + mat + obj uob
    desc_cfg.max_sets = (MAX_RENDERPASS_COUNT * 2 + MAX_PIPELINE_COUNT + MAX_MATERIAL_COUNT);
    desc_cfg.max_desc_per_type[VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER] = (MAX_RENDERPASS_COUNT + MAX_PIPELINE_COUNT + MAX_MATERIAL_COUNT);
    desc_cfg.max_desc_per_type[VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC] = MAX_RENDERPASS_COUNT;
    desc_cfg.max_desc_per_type[VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER] = MAX_MATERIAL_COUNT * MAT_SAMPLER_SLOT_COUNT;
    vkr_cfg vkii{.app_name = "rdev",
                 .vi{1, 0, 0},
                 .arenas{.persistent_arena = &rndr->vk_free_list, .command_arena = &rndr->vk_frame_linear},
                 .log_verbosity = LOG_DEBUG,
                 .window = win_hndl,
                 .inst_create_flags = INST_CREATE_FLAGS,
                 .desc_cfg{desc_cfg},
                 .extra_instance_extension_names = ADDITIONAL_INST_EXTENSIONS,
                 .extra_instance_extension_count = ADDITIONAL_INST_EXTENSION_COUNT,
                 .device_extension_names = DEVICE_EXTENSIONS,
                 .device_extension_count = DEVICE_EXTENSION_COUNT,
                 .validation_layer_names = VALIDATION_LAYERS,
                 .validation_layer_count = VALIDATION_LAYER_COUNT};

    if (vkr_init(&vkii, &rndr->vk) != err_code::VKR_NO_ERROR) {
        return err_code::RENDER_INIT_FAIL;
    }

    // Set up our per frame data
    for (int fif_ind = 0; fif_ind < rndr->per_frame_data.size; ++fif_ind) {
        arr_init(&rndr->per_frame_data[fif_ind].buffer_updates, fl_arena);
        rndr->per_frame_data[fif_ind].vkf = &rndr->vk.inst.device.rframes[fif_ind];
    }

    int err = setup_rendering(rndr);
    if (err != err_code::VKR_NO_ERROR) {
        elog("Failed to initialize renderer with code %d", err);
        return err;
    }

    init_imgui(rndr, win_hndl);

    // Setup our indice and vert buffer sbuffer
    return err_code::RENDER_NO_ERROR;
}

intern void terminate_swapchain_images_and_framebuffer(renderer *rndr)
{
    auto dev = &rndr->vk.inst.device;
    vkr_terminate_image_view(&dev->image_views[rndr->swapchain_fb_depth_stencil_iview_ind]);
    vkr_terminate_image(&dev->images[rndr->swapchain_fb_depth_stencil_im_ind]);
    vkr_terminate_swapchain_framebuffers(dev, &rndr->vk);
}

intern void recreate_swapchain(renderer *rndr)
{
    ilog("Recreating swapchain");
    // Recreating the swapchain will wait on all semaphores and fences before continuing
    auto dev = &rndr->vk.inst.device;
    vkr_device_wait_idle(dev);
    terminate_swapchain_images_and_framebuffer(rndr);
    vkr_terminate_swapchain(&dev->swapchain, &rndr->vk);
    vkr_terminate_surface(&rndr->vk, rndr->vk.inst.surface);
    vkr_init_surface(&rndr->vk, &rndr->vk.inst.surface);
    vkr_init_swapchain(&dev->swapchain, &rndr->vk);
    init_swapchain_images_and_framebuffer(rndr);
}

int begin_render_frame(renderer *rndr, int finished_frames)
{
    auto dev = &rndr->vk.inst.device;

    mem_reset_arena(&rndr->vk_frame_linear);
    mem_reset_arena(&rndr->frame_linear);

    // Finalize IM GUI data - not dependent on our render stuff currently
    ImGui::Render();

    // Update finished frames which is used to get the current frame
    rndr->finished_frames = finished_frames;
    int current_frame_ind = rndr->finished_frames % MAX_FRAMES_IN_FLIGHT;
    vkr_frame *cur_frame = &dev->rframes[current_frame_ind];

    // We wait until this FIF's fence has been triggered before rendering the frame. FIF fences are created in a
    // triggered state so there will be no waiting on the first time. We then reset the fence (aka set it to
    // untriggered) and it is passed to the vkQueueSubmit call to trigger it again. So if not the first time rendering
    // this FIF, we are waiting for the vkQueueSubmit from the previous time this FIF was rendered to complete
    int vk_res = vkWaitForFences(dev->hndl, 1, &cur_frame->in_flight, VK_TRUE, UINT64_MAX);
    if (vk_res != VK_SUCCESS) {
        elog("Failed to wait for fence");
        return err_code::RENDER_WAIT_FENCE_FAIL;
    }

    // Clear all prev desc sets
    vkr_reset_descriptor_pool(&cur_frame->desc_pool, &rndr->vk);
    
    return err_code::RENDER_NO_ERROR;
}


int end_render_frame(renderer *rndr, camera *cam, f64 dt)
{
    auto dev = &rndr->vk.inst.device;
    int current_frame_ind = rndr->finished_frames % MAX_FRAMES_IN_FLIGHT;
    vkr_frame *cur_frame = &dev->rframes[current_frame_ind];

    if (window_resized_this_frame(rndr->vk.cfg.window)) {
        rndr->no_resize_frames = 0.0;
    }
    else {
        rndr->no_resize_frames += dt;
    }

    /////////////////////////////////
    // Acquire Swapchain Image Ind //
    /////////////////////////////////
    // Acquire the image, signal the image_avail semaphore once the image has been acquired. We get the index back, but
    // that doesn't mean the image is ready. The image is only ready (on the GPU side) once the image avail semaphore is triggered
    u32 im_ind{};
    int vk_res = vkAcquireNextImageKHR(dev->hndl, dev->swapchain.swapchain, UINT64_MAX, cur_frame->image_avail, VK_NULL_HANDLE, &im_ind);

    // If the image is out of date/suboptimal we need to recreate the swapchain and our caller needs to exit early as
    // well. It seems that on some platforms, if the result from above is out of date or suboptimal, the semaphore
    // associated with it will never get triggered. So if we were to continue and just resize at the end of frame it
    // wouldn't work because the queue submit would never fire as it depends on this image available semaphore.
    // At least.. i think?
    if (vk_res == VK_ERROR_OUT_OF_DATE_KHR) {
        if (rndr->no_resize_frames > RESIZE_DEBOUNCE_FRAME_COUNT) {
            recreate_swapchain(rndr);
        }
        return vk_res;
    }
    else if (vk_res != VK_SUCCESS && vk_res != VK_SUBOPTIMAL_KHR) {
        elog("Failed to acquire swapchain image");
        return err_code::RENDER_ACQUIRE_IMAGE_FAIL;
    }
    
    
    if (cam) {
        ivec2 sz = get_window_pixel_size(rndr->vk.cfg.window);
        if (cam->vp_size != sz) {
            rndr->no_resize_frames = 0;
            cam->vp_size = sz;
            cam->proj = (math::perspective(cam->fov, (f32)cam->vp_size.w / (f32)cam->vp_size.h, cam->near_far.x, cam->near_far.y));
        }

        // Update our main pipeline view_proj with the cam transform update. This should be done every frame because the
        // camera is dynamic
        auto pl_iter = hmap_begin(&rndr->pipelines);
        while (pl_iter) {
            pl_iter->val.proj_view = cam->proj * cam->view;
            pl_iter = hmap_next(&rndr->pipelines, pl_iter);
        }
    }


    // The command buf index struct has an ind struct into the pool the cmd buf comes from, and then an ind into the buffer
    // The ind into the pool has an ind into the queue family (as that contains our array of command pools) and then and
    // ind to the command pool
    auto buf_ind = cur_frame->cmd_buf_ind;
    auto cmd_buf = &dev->qfams[buf_ind.pool_ind.qfam_ind].cmd_pools[buf_ind.pool_ind.pool_ind].buffers[buf_ind.buffer_ind];
    auto fb = &dev->framebuffers[im_ind];

    ///////////////////////////
    // Record Command Buffer //
    ///////////////////////////
    // We have the acquired image index, though we don't know when it will be ready to have ops submitted, we can record
    // the ops in the command buffer and submit once it is ready
    // This takes about %80 of the run frame
    vk_res = record_command_buffer(rndr, fb, cur_frame, cmd_buf);
    if (vk_res != err_code::RENDER_NO_ERROR) {
        return vk_res;
    }

    /////////////////////
    // Reset FIF Fence //
    /////////////////////
    // Here we reset the fence for the current frame fence as we know we are going to call queue submit which is the
    // only thing that will trigger the fence - so this is why this reset needs to come here (rather than right after waiting)
    vk_res = vkResetFences(dev->hndl, 1, &cur_frame->in_flight);
    if (vk_res != VK_SUCCESS) {
        elog("Failed to reset fence");
        return err_code::RENDER_RESET_FENCE_FAIL;
    }
    

    //////////////////////////////////
    // Submit command buffer to GPU //
    //////////////////////////////////
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
    submit_info.pSignalSemaphores = &rndr->vk.inst.device.swapchain.renders_finished[im_ind];
    if (vkQueueSubmit(dev->qfams[VKR_QUEUE_FAM_TYPE_GFX].qs[VKR_RENDER_QUEUE].hndl, 1, &submit_info, cur_frame->in_flight) != VK_SUCCESS) {
        return err_code::RENDER_SUBMIT_QUEUE_FAIL;
    }

    ///////////////////
    // Present Image //
    ///////////////////
    // Once the rendering signal has fired, present the image (show it on screen)
    VkPresentInfoKHR present_info{};
    present_info.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    present_info.waitSemaphoreCount = 1;
    present_info.pWaitSemaphores = &rndr->vk.inst.device.swapchain.renders_finished[im_ind];
    present_info.swapchainCount = 1;
    present_info.pSwapchains = &dev->swapchain.swapchain;
    present_info.pImageIndices = &im_ind;
    present_info.pResults = nullptr; // Optional - check for individual swaps
    vk_res = vkQueuePresentKHR(dev->qfams[VKR_QUEUE_FAM_TYPE_PRESENT].qs[VKR_RENDER_QUEUE].hndl, &present_info);

    // This purely helps with smoothness - it works fine without recreating the swapchain here and instead doing it on
    // the next frame, but it seems to resize more smoothly doing it here
    if (vk_res == VK_ERROR_OUT_OF_DATE_KHR || vk_res == VK_SUBOPTIMAL_KHR) {
        if (rndr->no_resize_frames > RESIZE_DEBOUNCE_FRAME_COUNT) {
            recreate_swapchain(rndr);
        }
    }
    else if (vk_res != VK_SUCCESS) {
        elog("Failed to presenet KHR");
        return err_code::RENDER_PRESENT_KHR_FAIL;
    }
    
    return err_code::RENDER_NO_ERROR;
}

int end_render_frame(renderer *rndr, camera *cam, f64 dt)
{
}

void terminate_renderer(renderer *rndr)
{
    rndr->default_mat = {};
    for (int i = 0; i < rndr->per_frame_data.size; ++i) {
        arr_terminate(&rndr->per_frame_data[i].buffer_updates);
    }
    hmap_terminate(&rndr->dcs.rpasses);
    mem_reset_arena(&rndr->vk_frame_linear);
    mem_reset_arena(&rndr->frame_linear);
    mem_reset_arena(&rndr->dcs.dc_linear);

    // These are stack arenas so must go in this order
    hmap_terminate(&rndr->rmi.meshes);
    mem_terminate_arena(&rndr->rmi.inds.node_pool);
    mem_terminate_arena(&rndr->rmi.verts.node_pool);

    vkr_device_wait_idle(&rndr->vk.inst.device);
    terminate_imgui(rndr);
    vkr_terminate(&rndr->vk);
    mem_terminate_arena(&rndr->dcs.dc_linear);
    mem_terminate_arena(&rndr->vk_free_list);
    mem_terminate_arena(&rndr->vk_frame_linear);
    mem_terminate_arena(&rndr->frame_linear);

    hmap_terminate(&rndr->rpasses);
    hmap_terminate(&rndr->pipelines);
    hmap_terminate(&rndr->materials);
    hmap_terminate(&rndr->textures);
}

} // namespace nslib
