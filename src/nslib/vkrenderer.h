#pragma once
#include <vulkan/vulkan.h>

namespace nslib
{
namespace err_code
{
enum vkr
{
    VKR_NO_ERROR,
    VKR_CREATE_INSTANCE_FAIL
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
    VkAllocationCallbacks alloc{};
    vk_arenas arenas{};
    extension_funcs ext_funcs;
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
    
};

int vkr_init(const vkr_init_info *init_info, vkr_context *vk);
void vkr_terminate(vkr_context *vk);

} // namespace nslib
