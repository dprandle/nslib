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
    VKR_NO_PHYSICAL_DEVICES
};
}

struct mem_arena;
struct vk_arenas
{
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
    VkPhysicalDevice physical_device{};
    
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

const char *vkr_physical_device_type_str(VkPhysicalDeviceType type);

// Log out the physical devices and set device to the best one based on very simple scoring (dedicated takes the cake)
int vkr_enumerate_and_select_best_physical_device(VkInstance inst, VkPhysicalDevice *device);

// Enumerate (log) the available extensions - if an extension is included in the passed in array then it will be
// indicated as such
void vkr_enumerate_extensions(const char *const *enabled_extensions=nullptr, u32 enabled_extension_count=0);

// Enumerate (log) the available layers - if an extension is included in the passed in array then it will be
// indicated as such
void vkr_enumerate_validation_layers(const char *const *enabled_layers=nullptr, u32 enabled_layer_count=0);

int vkr_init_instance(const vkr_init_info *init_info, vkr_context *vk);
void vkr_terminate_instance(const vkr_init_info *init_info, vkr_context *vk);

int vkr_init(const vkr_init_info *init_info, vkr_context *vk);
void vkr_terminate(vkr_context *vk);

} // namespace nslib
