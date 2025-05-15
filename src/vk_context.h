#pragma once
#include "vk_mem_alloc.h"
#include "containers/array.h"
#include "rid.h"
#include "util.h"
#include "math/vector4.h"
#include "basic_types.h"

namespace nslib
{

namespace err_code
{
enum vkr
{
    VKR_NO_ERROR,
    VKR_CREATE_INSTANCE_FAIL,
    VKR_CREATE_SURFACE_FAIL,
    VKR_NO_PHYSICAL_DEVICES,
    VKR_ENUMERATE_PHYSICAL_DEVICES_FAIL,
    VKR_NO_SUITABLE_PHYSICAL_DEVICE,
    VKR_CREATE_VMA_ALLOCATOR_FAIL,
    VKR_CREATE_DEVICE_FAIL,
    VKR_CREATE_SEMAPHORE_FAIL,
    VKR_CREATE_FENCE_FAIL,
    VKR_CREATE_SWAPCHAIN_FAIL,
    VKR_GET_SWAPCHAIN_IMAGES_FAIL,
    VKR_CREATE_IMAGE_VIEW_FAIL,
    VKR_CREATE_SHADER_MODULE_FAIL,
    VKR_CREATE_PIPELINE_LAYOUT_FAIL,
    VKR_CREATE_RENDER_PASS_FAIL,
    VKR_CREATE_PIPELINE_FAIL,
    VKR_CREATE_FRAMEBUFFER_FAIL,
    VKR_CREATE_COMMAND_POOL_FAIL,
    VKR_CREATE_COMMAND_BUFFER_FAIL,
    VKR_CREATE_DESCRIPTOR_POOL_FAIL,
    VKR_CREATE_DESCRIPTOR_SETS_FAIL,
    VKR_CREATE_SAMPLER_FAIL,
    VKR_BEGIN_COMMAND_BUFFER_FAIL,
    VKR_END_COMMAND_BUFFER_FAIL,
    VKR_CREATE_BUFFER_FAIL,
    VKR_CREATE_IMAGE_FAIL,
    VKR_COPY_BUFFER_BEGIN_FAIL,
    VKR_COPY_BUFFER_SUBMIT_FAIL,
    VKR_COPY_BUFFER_WAIT_IDLE_FAIL,
    VKR_TRANSITION_IMAGE_UNSUPPORTED_LAYOUT
};
}

enum vkr_shader_stage_type
{
    VKR_SHADER_STAGE_VERT,
    VKR_SHADER_STAGE_FRAG,
    VKR_SHADER_STAGE_COUNT
};

enum vkr_queue_fam_type
{
    VKR_QUEUE_FAM_TYPE_GFX,
    VKR_QUEUE_FAM_TYPE_PRESENT,
    VKR_QUEUE_FAM_TYPE_COUNT
};

// For now, we will just use a single queue since some cards only have one
inline constexpr sizet VKR_RENDER_QUEUE = 0;
inline constexpr u32 VKR_DESCRIPTOR_TYPE_COUNT = 11;
inline constexpr u32 MAX_QUEUE_REQUEST_COUNT = 32;
inline constexpr u32 VKR_MAX_EXTENSION_STR_LEN = 128;
inline constexpr sizet MAX_FRAMES_IN_FLIGHT = 2;
inline constexpr u32 VKR_INVALID = (u32)-1;
inline constexpr u32 MEM_ALLOC_TYPE_COUNT = VK_SYSTEM_ALLOCATION_SCOPE_INSTANCE + 1;
inline constexpr u32 VKR_API_VERSION = VK_API_VERSION_1_3;

struct vk_mem_alloc_stats
{
    u32 alloc_count{};
    u32 free_count{};
    u32 realloc_count{};
    sizet req_alloc{};
    sizet actual_alloc{};
    sizet req_free{};
    sizet actual_free{};
};

struct vkr_gpu_allocator
{
    VmaAllocator hndl;
    VkDeviceSize total_size;
};

struct mem_arena;

struct vk_arenas
{
    vk_mem_alloc_stats stats[MEM_ALLOC_TYPE_COUNT]{};

    // Should persist through the lifetime of the program - only use free list arena
    mem_arena *persistent_arena{};
    // Should persist for the lifetime of a vulkan command
    mem_arena *command_arena{};
};

struct vkr_buffer_cfg
{
    sizet buffer_size;
    VkBufferUsageFlags usage;
    VkBufferCreateFlags buf_create_flags;
    VkSharingMode sharing_mode{VK_SHARING_MODE_EXCLUSIVE};
    VmaMemoryUsage mem_usage;
    VmaAllocationCreateFlags alloc_flags;
    VkMemoryPropertyFlags required_flags;
    VkMemoryPropertyFlags preferred_flags;
    const vkr_gpu_allocator *vma_alloc;
};

struct vkr_buffer
{
    VkBuffer hndl;
    VmaAllocation mem_hndl;
    VmaAllocationInfo mem_info;
};

struct vkr_image_cfg
{
    uvec3 dims;
    VkImageType type{VK_IMAGE_TYPE_2D};
    VkFormat format;
    VkImageTiling tiling{VK_IMAGE_TILING_OPTIMAL};
    VkImageUsageFlags usage;
    VkImageCreateFlags im_create_flags;
    VmaMemoryUsage mem_usage;
    VmaAllocationCreateFlags alloc_flags;
    VkSampleCountFlagBits samples{VK_SAMPLE_COUNT_1_BIT};
    VkImageLayout initial_layout{VK_IMAGE_LAYOUT_UNDEFINED};
    VkSharingMode sharing_mode{VK_SHARING_MODE_EXCLUSIVE};
    int mip_levels{1};
    int array_layers{1};
    VkMemoryPropertyFlags required_flags;
    VkMemoryPropertyFlags preferred_flags;
    const vkr_gpu_allocator *vma_alloc;
};

struct vkr_image
{
    VkImage hndl;
    VkFormat format;
    uvec3 dims;
    VmaAllocation mem_hndl;
    VmaAllocationInfo mem_info;
    const vkr_gpu_allocator *vma_alloc;
};

struct vkr_image_view_cfg
{
    VkImageSubresourceRange srange{VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
    VkImageViewType view_type{VK_IMAGE_VIEW_TYPE_2D};
    VkImageViewCreateFlags create_flags;
    VkComponentMapping components;
    const vkr_image *image;
};

struct vkr_image_view
{
    VkImageView hndl;
};

struct vkr_framebuffer_attachment
{
    vkr_image_view iview;
    VkClearValue cv;
};

struct vkr_sampler_cfg
{
    VkSamplerCreateFlags flags;
    VkFilter mag_filter;
    VkFilter min_filter;
    VkSamplerMipmapMode mipmap_mode;
    VkSamplerAddressMode address_mode_uvw[3];
    f32 mip_lod_bias;
    b32 anisotropy_enable;
    f32 max_anisotropy;
    b32 compare_enable;
    VkCompareOp compare_op;
    f32 min_lod;
    f32 max_lod;
    VkBorderColor border_color;
    b32 unnormalized_coords;
};

struct vkr_sampler
{
    VkSampler hndl;
};

struct vkr_add_result
{
    sizet begin;
    sizet end;
    int err_code;
};

struct vkr_debug_extension_funcs
{
    PFN_vkCreateDebugUtilsMessengerEXT create_debug_utils_messenger{};
    PFN_vkDestroyDebugUtilsMessengerEXT destroy_debug_utils_messenger{};
};

struct vkr_command_buffer
{
    VkCommandBuffer hndl;
};

struct vkr_command_pool
{
    VkCommandPool hndl;
    array<vkr_command_buffer> buffers{};
};

struct vkr_queue_family_info
{
    u32 index{VKR_INVALID};
    u32 available_count{0};

    // If the request count is set to a non zero number then that number will be requested for each queue family type in
    // the queue_fam_type enum
    u32 requested_count{1};
    float priorities[MAX_QUEUE_REQUEST_COUNT] = {1.0f};
    u32 create_ind{0};
};

struct vkr_queue_families
{
    vkr_queue_family_info qinfo[VKR_QUEUE_FAM_TYPE_COUNT];
};

struct vkr_pdevice_swapchain_support
{
    VkSurfaceCapabilitiesKHR capabilities;
    array<VkSurfaceFormatKHR> formats{};
    array<VkPresentModeKHR> present_modes{};
};

struct vkr_phys_device
{
    VkPhysicalDevice hndl{};
    VkPhysicalDeviceFeatures features{};
    VkPhysicalDeviceProperties props{};
    vkr_queue_families qfams{};
    vkr_pdevice_swapchain_support swap_support{};
    VkPhysicalDeviceMemoryProperties mem_properties{};
};

struct vkr_cmd_pool_ind
{
    sizet qfam_ind;
    sizet pool_ind;
};

struct vkr_cmd_buf_ind
{
    vkr_cmd_pool_ind pool_ind;
    sizet buffer_ind;
};

struct vkr_descriptor_set
{
    VkDescriptorSet hndl;
    VkDescriptorSetLayout layout;
};

struct vkr_descriptor_pool
{
    VkDescriptorPool hndl;
    array<vkr_descriptor_set> desc_sets;
};

struct vkr_frame
{
    vkr_cmd_buf_ind cmd_buf_ind;
    sizet frame_ubo_ind;
    sizet pl_ubo_ind;
    sizet mat_ubo_ind;
    sizet obj_ubo_ind;
    vkr_descriptor_pool desc_pool;
    VkSemaphore image_avail;
    VkSemaphore render_finished;
    VkFence in_flight;
};

struct vkr_swapchain
{
    array<vkr_image> images;
    array<vkr_image_view> image_views;
    array<array<vkr_framebuffer_attachment>> other_attachments;
    VkFormat format;
    VkExtent2D extent;
    VkSwapchainKHR swapchain;
};

struct vkr_rpass_cfg_subpass
{
    VkPipelineBindPoint pipeline_bind_point{};
    static_array<VkAttachmentReference, 16> color_attachments;
    static_array<VkAttachmentReference, 16> input_attachments;
    static_array<VkAttachmentReference, 16> resolve_attachments;
    static_array<u32, 16> preserve_attachments;
    const VkAttachmentReference *depth_stencil_attachment{};
};

struct vkr_rpass_cfg
{
    static_array<VkAttachmentDescription, 16> attachments;
    static_array<vkr_rpass_cfg_subpass, 16> subpasses;
    static_array<VkSubpassDependency, 16> subpass_dependencies;
};

struct vkr_rpass
{
    VkRenderPass hndl;
};

struct vkr_shader_stage
{
    byte_array code;
    const char *entry_point;
};

struct vkr_push_constant_range
{
    VkPushConstantRange hndl;
};

struct vkr_pipeline_cfg_raster
{
    bool depth_clamp_enable;
    bool rasterizer_discard_enable;
    VkPolygonMode polygon_mode;
    float line_width;
    VkCullModeFlags cull_mode;
    VkFrontFace front_face;
    bool depth_bias_enable;
    float depth_bias_constant_factor{0.0f};
    float depth_bias_clamp{0.0f};
    float depth_bias_slope_factor{0.0f};
};

struct vkr_pipeline_cfg_multisample
{
    bool sample_shading_enable{false};
    VkSampleCountFlagBits rasterization_samples{VK_SAMPLE_COUNT_1_BIT};
    float min_sample_shading{1.0f};
    const VkSampleMask *sample_masks{nullptr};
    bool alpha_to_coverage_enable{false};
    bool alpha_to_one_enable{false};
};

struct vkr_pipeline_cfg_input_assembly
{
    bool primitive_restart_enable{};
    VkPrimitiveTopology primitive_topology{};
};

struct vkr_pipeline_cfg_color_blending
{
    bool logic_op_enabled{false};
    VkLogicOp logic_op{};
    static_array<VkPipelineColorBlendAttachmentState, 16> attachments{};
    vec4 blend_constants{};
};

struct vkr_descriptor_set_layout_desc
{
    static_array<VkDescriptorSetLayoutBinding, 16> bindings;
};

struct vkr_pipeline_cfg_depth_stencil
{
    VkPipelineDepthStencilStateCreateFlags flags;
    b32 depth_test_enable;
    b32 depth_write_enable;
    VkCompareOp depth_compare_op;
    b32 depth_bounds_test_enable;
    b32 stencil_test_enable;
    VkStencilOpState front;
    VkStencilOpState back;
    f32 min_depth_bounds;
    f32 max_depth_bounds;
};

struct vkr_pipeline_cfg
{
    vkr_shader_stage shader_stages[VKR_SHADER_STAGE_COUNT];

    // Render pass info
    const vkr_rpass *rpass;
    u32 subpass{};

    // Dynamic states
    static_array<VkDynamicState, 32> dynamic_states;

    // Vertex Input
    static_array<VkVertexInputBindingDescription, 32> vert_binding_desc;
    static_array<VkVertexInputAttributeDescription, 32> vert_attrib_desc;

    // Default Scissor/Viewport
    static_array<VkViewport, 16> viewports;
    static_array<VkRect2D, 16> scissors;

    // Input assembly
    vkr_pipeline_cfg_input_assembly input_assembly;

    // Rasterization
    vkr_pipeline_cfg_raster raster;

    // Multisampling
    vkr_pipeline_cfg_multisample multisampling;

    // Color blending
    vkr_pipeline_cfg_color_blending col_blend;

    // Depth stencil state
    vkr_pipeline_cfg_depth_stencil depth_stencil;

    // Descriptor Sets and push constants
    static_array<vkr_descriptor_set_layout_desc, 4> set_layouts;
    static_array<VkPushConstantRange, 32> push_constant_ranges;
};

struct vkr_pipeline
{
    vkr_rpass rpass;
    VkPipelineLayout layout_hndl;
    static_array<VkDescriptorSetLayout, 4> descriptor_layouts;
    VkPipeline hndl;
};

struct vkr_framebuffer_cfg
{
    uvec2 size;
    u32 layers{1};
    const vkr_rpass *rpass;
    const vkr_framebuffer_attachment *attachments;
    u32 attachment_count;
};

struct vkr_framebuffer
{
    uvec2 size;
    u32 layers;
    vkr_rpass rpass;
    array<vkr_framebuffer_attachment> attachments;
    VkFramebuffer hndl;
};

struct vkr_queue
{
    VkQueue hndl;
};

struct vkr_device_queue_fam_info
{
    u32 fam_ind;
    sizet default_pool;
    sizet transient_pool;
    array<vkr_queue> qs;
    array<vkr_command_pool> cmd_pools;
};

struct vkr_device
{
    VkDevice hndl;
    vkr_device_queue_fam_info qfams[VKR_QUEUE_FAM_TYPE_COUNT];
    array<vkr_rpass> render_passes;
    array<vkr_pipeline> pipelines;
    array<vkr_framebuffer> framebuffers;
    array<vkr_buffer> buffers;
    array<vkr_image> images;
    array<vkr_image_view> image_views;
    array<vkr_sampler> samplers;

    vkr_swapchain swapchain;
    static_array<vkr_frame, MAX_FRAMES_IN_FLIGHT> rframes;
    vkr_gpu_allocator vma_alloc;
};

struct vkr_instance
{
    VkInstance hndl;
    VkDebugUtilsMessengerEXT dbg_messenger;
    vkr_debug_extension_funcs ext_funcs;

    VkSurfaceKHR surface;
    vkr_phys_device pdev_info;
    vkr_device device;
};

struct vkr_descriptor_cfg
{
    u32 max_desc_per_type[VKR_DESCRIPTOR_TYPE_COUNT] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
    // If this is left as invalid, then it will be set to the sum of all descriptors in max_desc_per_type
    u32 max_sets{INVALID_ID};
    u32 flags{0};
};

struct vkr_cfg
{
    const char *app_name;
    version_info vi;
    vk_arenas arenas;
    int log_verbosity;
    void *window;
    VkInstanceCreateFlags inst_create_flags;
    vkr_descriptor_cfg desc_cfg{};

    // Array of additional instance extension names - besides defaults determined by window
    const char *const *extra_instance_extension_names;
    u32 extra_instance_extension_count;

    // Array of device extension names
    const char *const *device_extension_names;
    u32 device_extension_count;

    // Array of device validation layer names
    const char *const *validation_layer_names;
    u32 validation_layer_count;
};

struct vkr_context
{
    vkr_instance inst;
    vkr_cfg cfg;
    VkAllocationCallbacks alloc_cbs;
};

u32 vkr_find_mem_type(u32 type_flags, VkMemoryPropertyFlags property_flags, const vkr_phys_device *pdev);

VkShaderStageFlagBits vkr_shader_stage_type_bits(vkr_shader_stage_type st_type);
const char *vkr_shader_stage_type_str(vkr_shader_stage_type st_type);

const char *vkr_physical_device_type_str(VkPhysicalDeviceType type);
vkr_queue_families vkr_get_queue_families(const vkr_context *vk, VkPhysicalDevice dev);

// Log out the physical devices and set device to the best one based on very simple scoring (dedicated takes the cake)
int vkr_select_best_graphics_physical_device(const vkr_context *vk, vkr_phys_device *dev);

// NOTE: These enumerate functions are meant to be a convenience for not needing to use a tool to decide on which
// extensions and validation layers you need to use. They also print the layers as part of the init routine for vk

// Enumerate (log) the available extensions - if an extension is included in the passed in array then it will be
// indicated as such
void vkr_enumerate_instance_extensions(const char *const *enabled_extensions, u32 enabled_extension_count, const vk_arenas *arenas);

// Enumerate (log) the available device extensions
void vkr_enumerate_device_extensions(const vkr_phys_device *pdevice,
                                     const char *const *enabled_extensions,
                                     u32 enabled_extension_count,
                                     const vk_arenas *arenas);

// Enumerate (log) the available layers - if an extension is included in the passed in array then it will be
// indicated as such
void vkr_enumerate_validation_layers(const char *const *enabled_layers, u32 enabled_layer_count, const vk_arenas *arenas);

// Descriptors
int vkr_init_descriptor_pool(vkr_descriptor_pool *desc_pool, const vkr_context *vk, const vkr_descriptor_cfg *cfg);
void vkr_reset_descriptor_pool(vkr_descriptor_pool *desc_pool, const vkr_context *vk);
void vkr_terminate_descriptor_pool(vkr_descriptor_pool *desc_pool, const vkr_context *vk);
vkr_add_result vkr_add_descriptor_sets(vkr_descriptor_pool *pool, const vkr_context *vk, const VkDescriptorSetLayout *layouts, sizet count = 1);
void vkr_remove_descriptor_sets(vkr_descriptor_pool *pool, const vkr_context *vk, u32 ind, u32 count = 1);

// Command Buffers
sizet vkr_add_cmd_pool(vkr_device_queue_fam_info *qfam, const vkr_command_pool &cpool = {});
int vkr_init_cmd_pool(const vkr_context *vk, u32 fam_ind, VkCommandPoolCreateFlags flags, vkr_command_pool *cpool);
void vkr_terminate_cmd_pool(const vkr_context *vk, u32 fam_ind, vkr_command_pool *cpool);
vkr_add_result vkr_add_cmd_bufs(vkr_command_pool *pool, const vkr_context *vk, sizet count = 1);
void vkr_remove_cmd_bufs(vkr_command_pool *pool, const vkr_context *vk, sizet ind, sizet count = 1);

// Render passes
sizet vkr_add_render_pass(vkr_device *device, const vkr_rpass &copy = {});
int vkr_init_render_pass(const vkr_context *vk, const vkr_rpass_cfg *cfg, vkr_rpass *rpass);
void vkr_terminate_render_pass(const vkr_context *vk, const vkr_rpass *rpass);

// Pipelines
sizet vkr_add_pipeline(vkr_device *device, const vkr_pipeline &copy = {});
int vkr_init_pipeline(const vkr_context *vk, const vkr_pipeline_cfg *cfg, vkr_pipeline *pipe_info);
void vkr_terminate_pipeline(const vkr_context *vk_ctxt, const vkr_pipeline *pipe_info);
int vkr_init_shader_module(const vkr_context *vk, const byte_array *code, VkShaderModule *module);
void vkr_terminate_shader_module(const vkr_context *vk, VkShaderModule module);

// Framebuffers
sizet vkr_add_framebuffer(vkr_device *device, const vkr_framebuffer &copy = {});
int vkr_init_framebuffer(const vkr_context *vk, const vkr_framebuffer_cfg *cfg, vkr_framebuffer *framebuffer);
void vkr_terminate_framebuffer(const vkr_context *vk, vkr_framebuffer *fb);

// Buffers
sizet vkr_add_buffer(vkr_device *device, const vkr_buffer &copy = {});
int vkr_init_buffer(vkr_buffer *buffer, const vkr_buffer_cfg *cfg);
void vkr_terminate_buffer(vkr_buffer *buffer, const vkr_context *vk);
void *vkr_map_buffer(vkr_buffer *buf, const vkr_gpu_allocator *vma);
void vkr_unmap_buffer(vkr_buffer *buf, const vkr_gpu_allocator *vma);
int vkr_stage_and_upload_buffer_data(vkr_buffer *dest_buffer,
                                     const void *src_data,
                                     sizet src_data_size,
                                     const VkBufferCopy *region,
                                     vkr_device_queue_fam_info *cmd_q,
                                     sizet qind,
                                     const vkr_context *vk);
int vkr_stage_and_upload_buffer_data(vkr_buffer *dest_buffer,
                                     const void *src_data,
                                     sizet src_data_size,
                                     vkr_device_queue_fam_info *cmd_q,
                                     sizet qind,
                                     const vkr_context *vk);

// Images
sizet vkr_add_image(vkr_device *device, const vkr_image &copy = {});
int vkr_init_image(vkr_image *image, const vkr_image_cfg *cfg);
void vkr_terminate_image(vkr_image *image);
int vkr_stage_and_upload_image_data(vkr_image *dest_buffer,
                                    const void *src_data,
                                    sizet src_data_size,
                                    vkr_device_queue_fam_info *cmd_q,
                                    sizet qind,
                                    const vkr_context *vk);
int vkr_stage_and_upload_image_data(vkr_image *dest_buffer,
                                    const void *src_data,
                                    sizet src_data_size,
                                    const VkBufferImageCopy *region,
                                    vkr_device_queue_fam_info *cmd_q,
                                    sizet qind,
                                    const vkr_context *vk);
sizet vkr_add_image_view(vkr_device *device, const vkr_image_view &copy = {});
int vkr_init_image_view(vkr_image_view *iview, const vkr_image_view_cfg *cfg, const vkr_context *vk);
void vkr_terminate_image_view(vkr_image_view *iview, const vkr_context *vk);
sizet vkr_add_sampler(vkr_device *device, const vkr_sampler &copy = {});
int vkr_init_sampler(vkr_sampler *sampler, const vkr_sampler_cfg *cfg, const vkr_context *vk);
void vkr_terminate_sampler(vkr_sampler *sampler, const vkr_context *vk);

// Returns the index if the first swapchain framebuffer added
sizet vkr_add_swapchain_framebuffers(vkr_device *device);

// Initialize the swapchain framebuffers (there is one for each color image in the swapchain)
// The other_attachment image view will be added to each framebuffer (so make sure that is okay)
void vkr_init_swapchain_framebuffers(vkr_device *device,
                                     const vkr_context *vk,
                                     const vkr_rpass *rpass,
                                     const vkr_framebuffer_attachment &other_attachment,
                                     sizet fb_offset = 0);

// Initialize the swapchain framebuffers (there is one for each color image in the swapchain)
// The other_attachments image views will be added to each framebuffer (so make sure that is okay)
void vkr_init_swapchain_framebuffers(vkr_device *device,
                                     const vkr_context *vk,
                                     const vkr_rpass *rpass,
                                     const array<vkr_framebuffer_attachment> &other_attachments,
                                     sizet fb_offset = 0);

// Initialize the swapchain framebuffers (there is one for each color image in the swapchain)
// The other_attachments should be an array of image view arrays, where each of the image view arrays contain
// attachments for the corresponding framebuffer - IE the outer most array (other attachments) should be the same size
// as the swapchain images array.
void vkr_init_swapchain_framebuffers(vkr_device *device,
                                     const vkr_context *vk,
                                     const vkr_rpass *rpass,
                                     const array<array<vkr_framebuffer_attachment>> *other_attachments,
                                     sizet fb_offset = 0);

void vkr_terminate_swapchain_framebuffers(vkr_device *device, const vkr_context *vk, sizet fb_offset = 0);

// The device should be created before calling this
int vkr_init_swapchain(vkr_swapchain *sw_info, const vkr_context *vk);
void vkr_terminate_swapchain(vkr_swapchain *sw_info, const vkr_context *vk);

int vkr_init_render_frames(vkr_device *dev, const vkr_context *vk);
void vkr_terminate_render_frames(vkr_device *dev, const vkr_context *vk);

void vkr_init_pdevice_swapchain_support(vkr_pdevice_swapchain_support *ssup, mem_arena *arena);
void vkr_fill_pdevice_swapchain_support(VkPhysicalDevice pdevice, VkSurfaceKHR surface, vkr_pdevice_swapchain_support *ssup);
void vkr_terminate_pdevice_swapchain_support(vkr_pdevice_swapchain_support *ssup);

int vkr_init_device(vkr_device *dev,
                    const vkr_context *vk,
                    const char *const *layers,
                    u32 layer_count,
                    const char *const *device_extensions,
                    u32 dev_ext_count);
void vkr_terminate_device(vkr_device *dev, const vkr_context *vk);

// Initialize surface in the vk_context from the window - the instance must have been created already
int vkr_init_surface(const vkr_context *vk, VkSurfaceKHR *surface);
void vkr_terminate_surface(const vkr_context *vk, VkSurfaceKHR surface);

int vkr_init_instance(const vkr_context *vk, vkr_instance *inst);
void vkr_terminate_instance(const vkr_context *vk, vkr_instance *inst);

int vkr_init(const vkr_cfg *cfg, vkr_context *vk);
void vkr_terminate(vkr_context *vk);

int vkr_begin_cmd_buf(const vkr_command_buffer *buf);
int vkr_end_cmd_buf(const vkr_command_buffer *buf);

void vkr_cmd_begin_rpass(const vkr_command_buffer *cmd_buf, const vkr_framebuffer *fb, const VkClearValue *att_clear_vals, sizet clear_val_size);
void vkr_cmd_end_rpass(const vkr_command_buffer *cmd_buf);

sizet vkr_min_uniform_buffer_offset_alignment(vkr_context *vk);
sizet vkr_uniform_buffer_offset_alignment(vkr_context *vk, sizet uniform_block_size);

void vkr_device_wait_idle(vkr_device *dev);

struct vkr_image_transition_cfg
{
    VkImageLayout old_layout;
    VkImageLayout new_layout;
    u32 src_fam_index{VK_QUEUE_FAMILY_IGNORED};
    u32 dest_fam_index{VK_QUEUE_FAMILY_IGNORED};
    VkImageSubresourceRange srange;
};

int vkr_copy_buffer(vkr_buffer *dest,
                    const vkr_buffer *src,
                    const VkBufferCopy *region,
                    vkr_device_queue_fam_info *cmd_q,
                    sizet qind,
                    const vkr_context *vk);
int vkr_copy_buffer_to_image(vkr_image *dest,
                             const vkr_buffer *src,
                             const VkBufferImageCopy *region,
                             vkr_device_queue_fam_info *cmd_q,
                             sizet qind,
                             const vkr_context *vk);

int vkr_transition_image_layout(const vkr_image *image,
                                const vkr_image_transition_cfg *cfg,
                                vkr_device_queue_fam_info *cmd_q,
                                sizet qind,
                                const vkr_context *vk);

} // namespace nslib
