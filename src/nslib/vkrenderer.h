#pragma once
#include <vulkan/vulkan.h>
#include "basic_types.h"

namespace nslib
{
namespace err_code
{
enum vkr
{
    VKR_NO_ERROR,
    VKR_CREATE_INSTANCE_FAIL,
    VKR_NO_PHYSICAL_DEVICES,
    VKR_NO_QUEUE_FAMILIES,
    VKR_DEVICE_CREATION_FAILED
};
}

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

struct vkr_context
{
    VkInstance inst{};
    VkDebugUtilsMessengerEXT dbg_messenger{};
    VkAllocationCallbacks alloc_cbs{};
    VkPhysicalDevice pdevice{};
    VkDevice device{};
    
    vk_arenas arenas{};
    extension_funcs ext_funcs;
    int log_verbosity{};
};

struct version_info
{
    int major{};
    int minor{};
    int patch{};
};

struct vkr_init_info
{
    const char *app_name{};
    version_info vi{};
    vk_arenas arenas{};
    int log_verbosity{};
};

struct vkr_queue_family_info
{
    u32 index{VKR_INVALID};
    u32 available_count{0};
}; 

struct vkr_queue_families
{
    vkr_queue_family_info gfx;
}; 

const char *vkr_physical_device_type_str(VkPhysicalDeviceType type);
vkr_queue_families vkr_get_queue_families(VkPhysicalDevice pdevice);

// Log out the physical devices and set device to the best one based on very simple scoring (dedicated takes the cake)
int vkr_select_best_graphics_physical_device(VkInstance inst, VkPhysicalDevice *device);

// Enumerate (log) the available extensions - if an extension is included in the passed in array then it will be
// indicated as such
void vkr_enumerate_extensions(const char *const *enabled_extensions=nullptr, u32 enabled_extension_count=0);

// Enumerate (log) the available layers - if an extension is included in the passed in array then it will be
// indicated as such
void vkr_enumerate_validation_layers(const char *const *enabled_layers=nullptr, u32 enabled_layer_count=0);

int vkr_init_instance(const vkr_init_info *init_info, vkr_context *vk);
void vkr_terminate_instance(vkr_context *vk);

int vkr_init(const vkr_init_info *init_info, vkr_context *vk);
void vkr_terminate(vkr_context *vk);

} // namespace nslib
