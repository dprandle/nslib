#pragma once
#include <vulkan/vulkan.h>
#include "containers/array.h"
#include "util.h"
#include "math/vector4.h"
#include "basic_types.h"

namespace nslib
{

struct vertex
{
    vec2 pos;
    vec3 color;
};

const vertex verts[] = {{{0.0f, -0.5f}, {1.0f, 1.0f, 0.0f}}, {{0.5f, 0.5f}, {0.0f, 1.0f, 0.0f}}, {{-0.5f, 0.5f}, {1.0f, 0.0f, 0.0f}}};

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
    VKR_BEGIN_COMMAND_BUFFER_FAIL,
    VKR_END_COMMAND_BUFFER_FAIL,
    VKR_CREATE_BUFFER_FAIL
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

inline constexpr const u32 MAX_QUEUE_REQUEST_COUNT = 32;
inline constexpr const u32 VKR_MAX_EXTENSION_STR_LEN = 128;
inline constexpr const u32 VKR_RENDER_FRAME_COUNT = 3;

const u32 VKR_INVALID = (u32)-1;
inline constexpr const u32 MEM_ALLOC_TYPE_COUNT = VK_SYSTEM_ALLOCATION_SCOPE_INSTANCE + 1;

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

struct mem_arena;

struct vk_arenas
{
    vk_mem_alloc_stats stats[MEM_ALLOC_TYPE_COUNT]{};

    // Should persist through the lifetime of the program - only use free list arena
    mem_arena *persistent_arena{};
    // Should persist for the lifetime of a vulkan command
    mem_arena *command_arena{};
};

struct vkr_buffer
{
    VkBuffer hndl;
    VkDeviceMemory mem_hndl;
    sizet size;
};

struct vkr_cmd_buf_add_result
{
    u32 begin;
    u32 end;
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
    vkr_queue_families qfams{};
    vkr_pdevice_swapchain_support swap_support{};
    VkPhysicalDeviceMemoryProperties mem_properties{};
};

struct vkr_cmd_buf_ind
{
    u32 qfam_ind;
    u32 pool_ind;
    u32 buffer_ind;
};

struct vkr_frame
{
    vkr_cmd_buf_ind cmd_buf_ind;
    VkSemaphore image_avail;
    VkSemaphore render_finished;
    VkFence in_flight;
};

struct vkr_swapchain
{
    array<VkImage> images;
    array<VkImageView> image_views;
    VkFormat format;
    VkExtent2D extent;
    VkSwapchainKHR swapchain;
};

struct vkr_rpass_cfg
{};

struct vkr_rpass
{
    VkRenderPass hndl;
    vkr_rpass_cfg cfg;
};

struct vkr_shader_stage
{
    byte_array code;
    const char *entry_point;
};

struct vkr_pipeline_cfg
{
    vkr_shader_stage shader_stages[VKR_SHADER_STAGE_COUNT];
    const vkr_rpass *rpass;
};

struct vkr_pipeline
{
    vkr_rpass rpass;
    VkPipelineLayout layout_hndl;
    VkPipeline hndl;
};

struct vkr_framebuffer_cfg
{
    uvec2 size;
    u32 layers{1};
    const vkr_rpass *rpass;
    const VkImageView *attachments;
    u32 attachment_count;
};

struct vkr_framebuffer
{
    uvec2 size;
    u32 layers;
    vkr_rpass rpass;
    array<VkImageView> attachments;
    VkFramebuffer hndl;
};

struct vkr_queue
{
    VkQueue hndl;
};

struct vkr_device_queue_fam_info
{
    u32 fam_ind;
    u32 default_pool;
    array<vkr_queue> qs;
    array<vkr_command_pool> cmd_pools;
};

struct vkr_device
{
    VkDevice hndl;
    vkr_device_queue_fam_info qfams[VKR_QUEUE_FAM_TYPE_COUNT];
    array<vkr_rpass> render_passes;
    array<vkr_framebuffer> framebuffers;
    array<vkr_pipeline> pipelines;
    array<vkr_buffer> buffers;
    vkr_swapchain swapchain;
    vkr_frame rframes[VKR_RENDER_FRAME_COUNT];
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

struct vkr_cfg
{
    const char *app_name;
    version_info vi;
    vk_arenas arenas;
    int log_verbosity;
    void *window;

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

vkr_cmd_buf_add_result vkr_add_cmd_bufs(const vkr_context *vk, vkr_command_pool *pool, u32 count = 1);

u32 vkr_find_mem_type(u32 type_mask, u32 property_mask, const vkr_phys_device *pdev);

sizet vkr_add_cmd_pool(vkr_device_queue_fam_info *qfam, const vkr_command_pool &cpool);
int vkr_init_cmd_pool(const vkr_context *vk, u32 fam_ind, vkr_command_pool *cpool);
void vkr_terminate_cmd_pool(const vkr_context *vk, u32 fam_ind, vkr_command_pool *cpool);

// Create a shader module from bytecode
int vkr_init_shader_module(const vkr_context *vk, const byte_array *code, VkShaderModule *module);
void vkr_terminate_shader_module(const vkr_context *vk, VkShaderModule module);

sizet vkr_add_render_pass(vkr_device *device, const vkr_rpass &copy);
int vkr_init_render_pass(const vkr_context *vk, vkr_rpass *rpass);
void vkr_terminate_render_pass(const vkr_context *vk, const vkr_rpass *rpass);

sizet vkr_add_pipeline(vkr_device *device, const vkr_pipeline &copy);
int vkr_init_pipeline(const vkr_context *vk, const vkr_pipeline_cfg *cfg, vkr_pipeline *pipe_info);
void vkr_terminate_pipeline(const vkr_context *vk_ctxt, const vkr_pipeline *pipe_info);

sizet vkr_add_framebuffer(vkr_device *device, const vkr_framebuffer &copy);
int vkr_init_framebuffer(const vkr_context *vk, const vkr_framebuffer_cfg *cfg, vkr_framebuffer *framebuffer);
void vkr_terminate_framebuffer(const vkr_context *vk, const vkr_framebuffer *fb);

sizet vkr_add_buffer(vkr_device *device, const vkr_buffer &copy);
int vkr_init_buffer(vkr_buffer *buffer, const vkr_context *vk);
void vkr_terminate_buffer(const vkr_buffer *buffer, const vkr_context *vk);

// Returns the index if the first swapchain framebuffer added
sizet vkr_add_swapchain_framebuffers(vkr_device *device);
void vkr_init_swapchain_framebuffers(vkr_device *device,
                                     const vkr_context *vk,
                                     const vkr_rpass *rpass,
                                     const array<array<VkImageView>> *other_attachments,
                                     sizet fb_offset = 0);
void vkr_terminate_swapchain_framebuffers(vkr_device *device, const vkr_context *vk, sizet fb_offset = 0);

// The device should be created before calling this
int vkr_init_swapchain(vkr_swapchain *sw_info, const vkr_context *vk, void *window);
void vkr_recreate_swapchain(vkr_instance *inst, const vkr_context *vk, void *window, sizet rpass_ind);
void vkr_terminate_swapchain(vkr_swapchain *sw_info, const vkr_context *vk);

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
int vkr_init_surface(const vkr_context *vk, void *window, VkSurfaceKHR *surface);
void vkr_terminate_surface(const vkr_context *vk, VkSurfaceKHR surface);

int vkr_init_instance(const vkr_context *vk, vkr_instance *inst);
void vkr_terminate_instance(const vkr_context *vk, vkr_instance *inst);

int vkr_init(const vkr_cfg *cfg, vkr_context *vk);
void vkr_terminate(vkr_context *vk);

int vkr_begin_cmd_buf(const vkr_command_buffer *buf);
int vkr_end_cmd_buf(const vkr_command_buffer *buf);

void vkr_cmd_begin_rpass(const vkr_command_buffer *cmd_buf, const vkr_framebuffer *fb);
void vkr_cmd_end_rpass(const vkr_command_buffer *cmd_buf);

} // namespace nslib
