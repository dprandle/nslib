#pragma once
#include <vulkan/vulkan.h>
#include "containers/array.h"
#include "math/vector2.h"
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
    VKR_CREATE_DEVICE_FAIL,
    VKR_CREATE_SWAPCHAIN_FAIL,
    VKR_GET_SWAPCHAIN_IMAGES_FAIL,
    VKR_CREATE_IMAGE_VIEW_FAIL,
    VKR_CREATE_SHADER_MODULE_FAIL,
    VKR_CREATE_PIPELINE_LAYOUT_FAIL,
    VKR_CREATE_RENDER_PASS_FAIL,
    VKR_CREATE_PIPELINE_FAIL,
    VKR_CREATE_FRAMEBUFFER_FAIL,
    VKR_CREATE_COMMAND_POOL_FAIL,
    VKR_CREATE_COMMAND_BUFFER_FAIL
};
}

enum vkr_queue_fam_type
{
    VKR_QUEUE_FAM_TYPE_GFX,
    VKR_QUEUE_FAM_TYPE_PRESENT,
    VKR_QUEUE_FAM_TYPE_COUNT
};

inline constexpr const u32 MAX_QUEUE_REQUEST_COUNT = 32;
inline constexpr const u32 VKR_MAX_EXTENSION_STR_LEN = 128;

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

struct extension_funcs
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
    array<vkr_command_buffer> buffers;
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

struct vkr_physical_device_info
{
    VkPhysicalDevice pdevice{};
    vkr_queue_families qfams{};
    vkr_pdevice_swapchain_support swap_support{};
};

struct vkr_swapchain_info
{
    array<VkImage> images;
    array<VkImageView> image_views;
    VkFormat format;
    VkExtent2D extent;
    VkSwapchainKHR swapchain;
};

struct vkr_pipeline_info
{
    VkPipelineLayout layout;
    VkPipeline pipeline;
};

struct vkr_queue
{
    VkQueue hndl;
};

struct vkr_device_queue_fam_info
{
    u32 fam_ind;
    array<vkr_queue> qs;
    array<vkr_command_pool> cmd_pools;
};

struct vkr_device
{
    VkDevice hndl;
    vkr_device_queue_fam_info qfams[VKR_QUEUE_FAM_TYPE_COUNT];
    array<VkRenderPass> render_passes;
    array<VkFramebuffer> framebuffers;
    array<vkr_pipeline_info> pipelines;
    vkr_swapchain_info sw_info;    
};

struct vkr_instance
{
    VkInstance hndl;
    VkDebugUtilsMessengerEXT dbg_messenger;
    extension_funcs ext_funcs;

    VkSurfaceKHR surface;
    vkr_physical_device_info pdev_info;
    vkr_device device;
};

struct vkr_context
{
    vkr_instance inst;
    vk_arenas arenas;
    int log_verbosity;
    VkAllocationCallbacks alloc_cbs;
};

struct version_info
{
    int major;
    int minor;
    int patch;
};

struct vkr_init_info
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

struct vkr_pipeline_init_info
{
    byte_array frag_shader_data;
    byte_array vert_shader_data;
    VkRenderPass rpass;
};

const char *vkr_physical_device_type_str(VkPhysicalDeviceType type);
vkr_queue_families vkr_get_queue_families(const vkr_context *vk, VkPhysicalDevice dev);

// Log out the physical devices and set device to the best one based on very simple scoring (dedicated takes the cake)
int vkr_select_best_graphics_physical_device(const vkr_context *vk, vkr_physical_device_info *dev_info);

// NOTE: These enumerate functions are meant to be a convenience for not needing to use a tool to decide on which
// extensions and validation layers you need to use. They also print the layers as part of the init routine for vk

// Enumerate (log) the available extensions - if an extension is included in the passed in array then it will be
// indicated as such
void vkr_enumerate_instance_extensions(const char *const *enabled_extensions, u32 enabled_extension_count, const vk_arenas *arenas);

// Enumerate (log) the available device extensions
void vkr_enumerate_device_extensions(VkPhysicalDevice pdevice,
                                     const char *const *enabled_extensions,
                                     u32 enabled_extension_count,
                                     const vk_arenas *arenas);

// Enumerate (log) the available layers - if an extension is included in the passed in array then it will be
// indicated as such
void vkr_enumerate_validation_layers(const char *const *enabled_layers, u32 enabled_layer_count, const vk_arenas *arenas);


int vkr_add_command_buffer(const vkr_device *device, vkr_command_pool *pool);

int vkr_init_command_pool(const vkr_context *vk, u32 fam_ind, vkr_command_pool *cpool);
sizet vkr_add_command_pool(vkr_device_queue_fam_info *qfam, const vkr_command_pool *cpool);
void vkr_terminate_command_pool(const vkr_context *vk, u32 fam_ind, vkr_command_pool *cpool);

// Create a shader module from bytecode
int vkr_init_shader_module(const vkr_context *vk, const byte_array *code, VkShaderModule *module);
void vkr_terminate_shader_module(const vkr_context *vk, VkShaderModule module);

int vkr_init_render_pass(const vkr_context *vk, VkRenderPass *rpass);
sizet vkr_add_render_pass(vkr_device *device, VkRenderPass rpass);
void vkr_terminate_render_pass(const vkr_context *vk, VkRenderPass rpass);

int vkr_init_pipeline(const vkr_pipeline_init_info *init_info, const vkr_context *vk_ctxt, vkr_pipeline_info *pipe_info);
sizet vkr_add_pipeline(vkr_device *device, const vkr_pipeline_info *pipeline);
void vkr_terminate_pipeline(const vkr_context *vk_ctxt, const vkr_pipeline_info *pipe_info);

int vkr_init_framebuffer(const vkr_context *vk,
                         VkRenderPass render_pass,
                         uvec2 size,
                         const VkImageView *attachments,
                         u32 attachment_count,
                         VkFramebuffer *framebuffer);
sizet vkr_add_framebuffer(vkr_device *device, VkFramebuffer fb);
void vkr_terminate_framebuffer(const vkr_context *vk, VkFramebuffer framebuffer);

// The device should be created before calling this
int vkr_init_swapchain(const vkr_context *vk,
                        void *window,
                        vkr_swapchain_info *sw_info);
void vkr_terminate_swapchain(const vkr_context *vk, vkr_swapchain_info *sw_info);

void vkr_init_pdevice_swapchain_support(vkr_pdevice_swapchain_support *ssup, mem_arena *arena);
void vkr_fill_pdevice_swapchain_support(VkPhysicalDevice pdevice, VkSurfaceKHR surface, vkr_pdevice_swapchain_support *ssup);
void vkr_terminate_pdevice_swapchain_support(vkr_pdevice_swapchain_support *ssup);

int vkr_init_device(const vkr_context *vk,
                    const char *const *layers,
                    u32 layer_count,
                    const char *const *device_extensions,
                    u32 dev_ext_count,
                    vkr_device *dev);
void vkr_terminate_device(const vkr_context *vk,
                         vkr_device *dev);


// Initialize surface in the vk_context from the window - the instance must have been created already
int vkr_init_surface(const vkr_context *vk, void *window, VkSurfaceKHR *surface);
void vkr_terminate_surface(const vkr_context *vk, VkSurfaceKHR surface);

int vkr_init_instance(const vkr_init_info *init_info, const vkr_context *vk, vkr_instance *inst);
void vkr_terminate_instance(const vkr_context *vk, vkr_instance *inst);

int vkr_init(const vkr_init_info *init_info, vkr_context *vk);
void vkr_terminate(vkr_context *vk);

void vkr_setup_default_rendering(vkr_context *vk);

} // namespace nslib
