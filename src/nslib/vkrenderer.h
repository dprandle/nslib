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
struct vk_arenas {
    // Should persist through the lifetime of the program - only use free list arena
    mem_arena *persitant_arena{};
    // Should persist for the lifetime of a vulkan command
    mem_arena *command_arena{};
};

struct vkr_context
{
    VkInstance inst{};
    VkAllocationCallbacks alloc{};
    vk_arenas arenas{};
};

struct version_info {
    int major{};
    int minor{};
    int patch{};
};

int vkr_init(const char *app_name, const version_info *app_version, const vk_arenas *arenas, vkr_context *vk);
void vkr_terminate(vkr_context *vk);

} // namespace nslib
