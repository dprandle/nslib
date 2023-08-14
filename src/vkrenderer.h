#pragma once
#include <vulkan/vulkan.h>
#include "containers/array.h"
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
    VKR_NO_SUITABLE_PHYSICAL_DEVICE,
    VKR_CREATE_DEVICE_FAIL,
    VKR_CREATE_SWAPCHAIN_FAIL
};
}

enum vkr_queue_fam_type
{
    VKR_QUEUE_FAM_TYPE_GFX,
    VKR_QUEUE_FAM_TYPE_PRESENT,
    VKR_QUEUE_FAM_TYPE_COUNT
};

inline constexpr const u32 MAX_QUEUE_REQUEST_COUNT=32;

const u32 VKR_INVALID = (u32)-1;
inline constexpr const u32 MEM_ALLOC_TYPE_COUNT = VK_SYSTEM_ALLOCATION_SCOPE_INSTANCE+1;

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
    vk_mem_alloc_stats stats[MEM_ALLOC_TYPE_COUNT] {};

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

struct vkr_queue_family_info
{
    u32 index{VKR_INVALID};
    u32 available_count{0};
    u32 requested_count{1};
    float priorities[MAX_QUEUE_REQUEST_COUNT] = {1.0f};
    VkQueue qs[MAX_QUEUE_REQUEST_COUNT] {};
    
    u32 qoffset{0};
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

struct vkr_context
{
    VkInstance inst;
    VkDebugUtilsMessengerEXT dbg_messenger;
    VkAllocationCallbacks alloc_cbs;

    VkDevice device;
    VkSurfaceKHR surface;
    vkr_swapchain_info sw_info;
    vkr_physical_device_info pdev_info;

    VkQueue gfx_q;
    VkQueue present_q;

    vk_arenas arenas;
    extension_funcs ext_funcs;
    int log_verbosity;
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
};

const char *vkr_physical_device_type_str(VkPhysicalDeviceType type);
vkr_queue_families vkr_get_queue_families(VkPhysicalDevice pdevice, VkSurfaceKHR surface, vk_arenas *arenas);

// Log out the physical devices and set device to the best one based on very simple scoring (dedicated takes the cake)
int vkr_select_best_graphics_physical_device(VkInstance inst, VkSurfaceKHR surface, VkPhysicalDevice *device, vk_arenas *arenas);

// Enumerate (log) the available extensions - if an extension is included in the passed in array then it will be
// indicated as such
void vkr_enumerate_instance_extensions(const char *const *enabled_extensions, u32 enabled_extension_count, vk_arenas *arenas);

// Enumerate (log) the available layers - if an extension is included in the passed in array then it will be
// indicated as such
void vkr_enumerate_validation_layers(const char *const *enabled_layers, u32 enabled_layer_count, vk_arenas *arenas);

int vkr_init_swapchain(VkDevice device,
                       VkSurfaceKHR surface,
                       const vkr_physical_device_info *dev_info,
                       const VkAllocationCallbacks *alloc_cbs,
                       void *window,
                       vkr_swapchain_info *swinfo);

void vkr_init_swapchain_info(vkr_swapchain_info *sw_info, mem_arena*arena);
void vkr_terminate_swapchain_info(vkr_swapchain_info *sw_info);

void vkr_init_pdevice_swapchain_support(vkr_pdevice_swapchain_support *ssup, mem_arena *arena);
void vkr_fill_pdevice_swapchain_support(VkPhysicalDevice pdevice, VkSurfaceKHR surface, vkr_pdevice_swapchain_support *ssup);
void vkr_terminate_pdevice_swapchain_support(vkr_pdevice_swapchain_support *ssup);

int vkr_init_instance(const vkr_init_info *init_info, vkr_context *vk);
void vkr_terminate_instance(vkr_context *vk);

int vkr_init(const vkr_init_info *init_info, vkr_context *vk);
void vkr_terminate(vkr_context *vk);

} // namespace nslib
