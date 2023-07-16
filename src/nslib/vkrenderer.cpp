#include <GLFW/glfw3.h>

#include "vkrenderer.h"
#include "logging.h"
#include "mem.h"

namespace nslib
{

intern const i32 VK_INTERNAL_MEM_HEADER_SIZE = 8;

intern const char *alloc_scope_str(VkSystemAllocationScope scope)
{
    switch (scope) {
    case (VK_SYSTEM_ALLOCATION_SCOPE_COMMAND):
        return "command";
    case (VK_SYSTEM_ALLOCATION_SCOPE_OBJECT):
        return "object";
    case (VK_SYSTEM_ALLOCATION_SCOPE_CACHE):
        return "cache";
    case (VK_SYSTEM_ALLOCATION_SCOPE_DEVICE):
        return "device";
    case (VK_SYSTEM_ALLOCATION_SCOPE_INSTANCE):
        return "instance";
    default:
        return "unknown";
    }
}

intern void *vk_alloc(void *user, sizet size, sizet alignment, VkSystemAllocationScope scope)
{
    assert(user);
    auto arenas = (vk_arenas *)user;
    auto arena = arenas->persitant_arena;
    if (scope == VK_SYSTEM_ALLOCATION_SCOPE_COMMAND) {
        arena = arenas->command_arena;
    }
    sizet used_before = arena->used;
    size += VK_INTERNAL_MEM_HEADER_SIZE;
    auto scope_store = (i8 *)ns_alloc(size, arena, alignment);
    *scope_store = scope;
    void *ret = scope_store + VK_INTERNAL_MEM_HEADER_SIZE;
    if (scope != VK_SYSTEM_ALLOCATION_SCOPE_COMMAND) {
        dlog("header_addr:%p ptr:%p size:%d alignment:%d scope:%s used_before:%d alloc:%d used_after:%d",
             (void *)scope_store,
             ret,
             size,
             alignment,
             alloc_scope_str(scope),
             used_before,
             arena->used - used_before,
             arena->used,
             (arena->used - used_before) - size);
    }
    return ret;
}

intern void vk_free(void *user, void *ptr)
{
    assert(user);
    if (!ptr) {
        return;
    }
    auto arenas = (vk_arenas *)user;
    auto arena = arenas->persitant_arena;
    auto scope_store = (i8 *)ptr - VK_INTERNAL_MEM_HEADER_SIZE;
    if (*scope_store == VK_SYSTEM_ALLOCATION_SCOPE_COMMAND) {
        arena = arenas->command_arena;
    }
    sizet used_before = arena->used;
    ns_free((void *)scope_store, arena);
    if (*scope_store != VK_SYSTEM_ALLOCATION_SCOPE_COMMAND) {
        dlog("header_addr:%p ptr:%p scope:%s used_before:%d dealloc:%d used_after:%d",
             (void *)scope_store,
             ptr,
             alloc_scope_str((VkSystemAllocationScope)*scope_store),
             used_before,
             used_before - arena->used,
             arena->used);
    }
}

intern void *vk_realloc(void *user, void *ptr, sizet size, sizet alignment, VkSystemAllocationScope scope)
{
    assert(user);
    if (!ptr) {
        return nullptr;
    }
    auto arenas = (vk_arenas *)user;
    auto arena = arenas->persitant_arena;
    auto scope_store = (i8 *)ptr - VK_INTERNAL_MEM_HEADER_SIZE;
    assert(*scope_store == scope);
    if (scope == VK_SYSTEM_ALLOCATION_SCOPE_COMMAND) {
        arena = arenas->command_arena;
    }

    scope_store = (i8 *)ns_realloc(scope_store, size + VK_INTERNAL_MEM_HEADER_SIZE, arena, alignment);
    *scope_store = scope;
    void *ret = scope_store + VK_INTERNAL_MEM_HEADER_SIZE;
    return ret;
}

int vkr_init(const char *app_name, const version_info *app_version, const vk_arenas *arenas, vkr_context *vk)
{
    ilog("Initializing vulkan");
    if (arenas) {
        vk->arenas = *arenas;
    }
    else {
        vk->arenas.persitant_arena = get_global_arena();
        vk->arenas.command_arena = get_global_frame_linear_arena();
    }
    assert(vk->arenas.persitant_arena);
    assert(vk->arenas.command_arena);

    vk->alloc.pUserData = &vk->arenas;
    vk->alloc.pfnAllocation = vk_alloc;
    vk->alloc.pfnFree = vk_free;
    vk->alloc.pfnReallocation = vk_realloc;

    VkApplicationInfo app_info{};
    app_info.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    app_info.pApplicationName = app_name;
    app_info.applicationVersion = VK_MAKE_VERSION(app_version->major, app_version->minor, app_version->patch);
    app_info.engineVersion = VK_MAKE_VERSION(1, 0, 0);
    app_info.pEngineName = "Noble Steed";
    app_info.apiVersion = VK_API_VERSION_1_3;

    VkInstanceCreateInfo create_inf{};
    create_inf.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    create_inf.pApplicationInfo = &app_info;
    create_inf.ppEnabledExtensionNames = glfwGetRequiredInstanceExtensions(&create_inf.enabledExtensionCount);
    int err = vkCreateInstance(&create_inf, &vk->alloc, &vk->inst);
    if (err != VK_SUCCESS) {
        elog("Failed to create vulkan instance with vulkan err code: %d", err);
        return err_code::VKR_CREATE_INSTANCE_FAIL;
    }
    ilog("Successfully created vulkan instance");
    return err_code::VKR_NO_ERROR;
}
void vkr_terminate(vkr_context *vk)
{
    ilog("Terminating vulkan");
    VkAllocationCallbacks c;
    vkDestroyInstance(vk->inst, &vk->alloc);
}

} // namespace nslib
