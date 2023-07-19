#include <GLFW/glfw3.h>
#include <cstring>

#include "vkrenderer.h"
#include "logging.h"
#include "mem.h"

#define PRINT_MEM_DEBUG false
#define PRINT_MEM_INSTANCE_ONLY true

namespace nslib
{

intern const i32 VK_INTERNAL_MEM_HEADER_SIZE = 8;

#if defined(NDEBUG)
intern const u32 VALIDATION_LAYER_COUNT = 0;
intern const char **VALIDATION_LAYERS = nullptr;
#else
intern const u32 VALIDATION_LAYER_COUNT = 1;
intern const char *VALIDATION_LAYERS[VALIDATION_LAYER_COUNT] = {"VK_LAYER_KHRONOS_validation"};
#endif

intern const u32 ADDITIONAL_EXTENSION_COUNT = 1;
intern const char *ADDITIONAL_EXTENSIONS[ADDITIONAL_EXTENSION_COUNT] = {"VK_EXT_debug_utils"};
intern const u32 MAX_EXTENSION_STR_LEN = 128;

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
    auto arena = arenas->persistent_arena;
    if (scope == VK_SYSTEM_ALLOCATION_SCOPE_COMMAND) {
        arena = arenas->command_arena;
    }
    sizet used_before = arena->used;
    size += VK_INTERNAL_MEM_HEADER_SIZE;
    auto scope_store = (i8 *)mem_alloc(size, arena, alignment);
    *scope_store = scope;
    void *ret = scope_store + VK_INTERNAL_MEM_HEADER_SIZE;

#if PRINT_MEM_DEBUG
#if PRINT_MEM_INSTANCE_ONLY
    if (scope == VK_SYSTEM_ALLOCATION_SCOPE_INSTANCE) {
#endif
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
#if PRINT_MEM_INSTANCE_ONLY
    }
#endif
#endif
    return ret;
}

intern void vk_free(void *user, void *ptr)
{
    assert(user);
    if (!ptr) {
        return;
    }
    auto arenas = (vk_arenas *)user;
    auto arena = arenas->persistent_arena;
    auto scope_store = (i8 *)ptr - VK_INTERNAL_MEM_HEADER_SIZE;
    if (*scope_store == VK_SYSTEM_ALLOCATION_SCOPE_COMMAND) {
        arena = arenas->command_arena;
    }
    sizet used_before = arena->used;
    mem_free((void *)scope_store, arena);

#if PRINT_MEM_DEBUG
#if PRINT_MEM_INSTANCE_ONLY
    if (*scope_store == VK_SYSTEM_ALLOCATION_SCOPE_INSTANCE) {
#endif
        dlog("header_addr:%p ptr:%p scope:%s used_before:%d dealloc:%d used_after:%d",
             (void *)scope_store,
             ptr,
             alloc_scope_str((VkSystemAllocationScope)*scope_store),
             used_before,
             used_before - arena->used,
             arena->used);
#if PRINT_MEM_INSTANCE_ONLY
    }
#endif
#endif
}

intern void *vk_realloc(void *user, void *ptr, sizet size, sizet alignment, VkSystemAllocationScope scope)
{
    assert(user);
    if (!ptr) {
        return nullptr;
    }
    auto arenas = (vk_arenas *)user;
    auto arena = arenas->persistent_arena;
    auto scope_store = (i8 *)ptr - VK_INTERNAL_MEM_HEADER_SIZE;
    assert(*scope_store == scope);
    if (scope == VK_SYSTEM_ALLOCATION_SCOPE_COMMAND) {
        arena = arenas->command_arena;
    }

    scope_store = (i8 *)mem_realloc(scope_store, size + VK_INTERNAL_MEM_HEADER_SIZE, arena, alignment);
    *scope_store = scope;
    void *ret = scope_store + VK_INTERNAL_MEM_HEADER_SIZE;
    return ret;
}

intern void enumerate_extensions(char *const *enabled_extensions, u32 enabled_extension_count)
{
    u32 extension_count{0};
    ilog("Enumerating vulkan extensions...");
    vkEnumerateInstanceExtensionProperties(nullptr, &extension_count, nullptr);
    VkExtensionProperties *ext_array =
        (VkExtensionProperties *)mem_alloc(extension_count * sizeof(VkExtensionProperties), mem_global_frame_lin_arena());
    vkEnumerateInstanceExtensionProperties(nullptr, &extension_count, ext_array);
    for (int i = 0; i < extension_count; ++i) {
        bool ext_enabled{false};
        for (int j = 0; j < enabled_extension_count; ++j) {
            if (strncmp(enabled_extensions[j], ext_array[i].extensionName, MAX_EXTENSION_STR_LEN) == 0) {
                ext_enabled = true;
            }
        }
        ilog("Extension:%s  SpecVersion:%d  Enabled:%s", ext_array[i].extensionName, ext_array[i].specVersion, ext_enabled ? "true" : "false");
    }
}

intern void enumerate_validation_layers()
{
    u32 layer_count{0};
    ilog("Enumerating vulkan validation layers...");
    vkEnumerateInstanceLayerProperties(&layer_count, nullptr);
    VkLayerProperties *layer_array = (VkLayerProperties *)mem_alloc(layer_count * sizeof(VkLayerProperties), mem_global_frame_lin_arena());
    vkEnumerateInstanceLayerProperties(&layer_count, layer_array);
    for (int i = 0; i < layer_count; ++i) {
        bool enabled{false};
        for (int j = 0; j < VALIDATION_LAYER_COUNT; ++j) {
            if (strcmp(VALIDATION_LAYERS[j], layer_array[i].layerName) == 0) {
                enabled = true;
            }
        }
        ilog("Layer:%s  Desc:\"%s\"  ImplVersion:%d  SpecVersion:%d  Enabled:%s",
             layer_array[i].layerName,
             layer_array[i].description,
             layer_array[i].implementationVersion,
             layer_array[i].specVersion,
             enabled ? "true" : "false");
    }
}

intern VkBool32 VKAPI_PTR debug_message_callback(VkDebugUtilsMessageSeverityFlagBitsEXT severity,
                                          VkDebugUtilsMessageTypeFlagsEXT types,
                                          const VkDebugUtilsMessengerCallbackDataEXT *data,
                                          void *user)
{
    if (severity >= VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT) {
        elog("Vk: %s", data->pMessage);
    }
    else if (severity >= VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT) {
        wlog("Vk: %s", data->pMessage);
    }
    else if (severity >= VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT) {
        tlog("Vk: %s", data->pMessage);
    }
    else if (severity >= VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT) {
        dlog("Vk: %s", data->pMessage);
    }
    return VK_FALSE;
}

intern void get_extension_funcs(extension_funcs *funcs, VkInstance inst)
{
    funcs->create_debug_utils_messenger = (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(inst, "vkCreateDebugUtilsMessengerEXT");
    funcs->destroy_debug_utils_messenger =
        (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(inst, "vkDestroyDebugUtilsMessengerEXT");
    assert(funcs->create_debug_utils_messenger);
    assert(funcs->destroy_debug_utils_messenger);
}

intern void fill_debug_ext_create_info(VkDebugUtilsMessengerCreateInfoEXT *create_info)
{
    create_info->sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
    create_info->messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
                                   VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
    create_info->messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
                               VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
    create_info->pfnUserCallback = debug_message_callback;
    create_info->pUserData = nullptr;
}

intern int create_instance(const vkr_init_info *init_info, vkr_context *vk)
{
    VkApplicationInfo app_info{};
    app_info.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    app_info.pApplicationName = init_info->app_name;
    app_info.applicationVersion = VK_MAKE_VERSION(init_info->vi.major, init_info->vi.minor, init_info->vi.patch);
    app_info.engineVersion = VK_MAKE_VERSION(1, 0, 0);
    app_info.pEngineName = "Noble Steed";
    app_info.apiVersion = VK_API_VERSION_1_3;

    VkInstanceCreateInfo create_inf{};
    create_inf.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    create_inf.pApplicationInfo = &app_info;

    // This is for clarity.. we could just directly pass the enabled extension count
    u32 ext_count{0};
    const char *const *glfw_ext = glfwGetRequiredInstanceExtensions(&ext_count);
    char **ext = (char **)mem_alloc((ext_count + ADDITIONAL_EXTENSION_COUNT) * sizeof(char *), mem_global_frame_lin_arena());

    u32 copy_ind = 0;
    for (u32 i = 0; i < ext_count; ++i) {
        ext[copy_ind] = (char *)mem_alloc(strlen(glfw_ext[i]), mem_global_frame_lin_arena());
        strcpy(ext[copy_ind], glfw_ext[i]);
        ++copy_ind;
    }
    for (u32 i = 0; i < ADDITIONAL_EXTENSION_COUNT; ++i) {
        ext[copy_ind] = (char *)mem_alloc(strlen(ADDITIONAL_EXTENSIONS[i]), mem_global_frame_lin_arena());
        strcpy(ext[copy_ind], ADDITIONAL_EXTENSIONS[i]);
        ++copy_ind;
    }

    VkDebugUtilsMessengerCreateInfoEXT dbg_ci{};
    fill_debug_ext_create_info(&dbg_ci);

    create_inf.pNext = &dbg_ci;
    create_inf.ppEnabledExtensionNames = ext;
    create_inf.enabledExtensionCount = ext_count + ADDITIONAL_EXTENSION_COUNT;
    create_inf.ppEnabledLayerNames = VALIDATION_LAYERS;
    create_inf.enabledLayerCount = VALIDATION_LAYER_COUNT;
    enumerate_extensions(ext, create_inf.enabledExtensionCount);
    enumerate_validation_layers();

    int err = vkCreateInstance(&create_inf, &vk->alloc, &vk->inst);
    if (err != VK_SUCCESS) {
        elog("Failed to create vulkan instance with vulkan err code: %d", err);
        return err_code::VKR_CREATE_INSTANCE_FAIL;
    }
    ilog("Successfully created vulkan instance");
    get_extension_funcs(&vk->ext_funcs, vk->inst);
    vk->ext_funcs.create_debug_utils_messenger(vk->inst, &dbg_ci, &vk->alloc, &vk->dbg_messenger);

    return err_code::VKR_NO_ERROR;
}

int vkr_init(const vkr_init_info *init_info, vkr_context *vk)
{
    ilog("Initializing vulkan");
    assert(init_info);
    vk->arenas = init_info->arenas;
    if (!init_info->arenas.command_arena) {
        vk->arenas.command_arena = mem_global_frame_lin_arena();
        ilog("Using global frame linear arena %p", vk->arenas.command_arena);
    }

    if (!init_info->arenas.persistent_arena) {
        vk->arenas.persistent_arena = mem_global_arena();
        ilog("Using global persistent arena %p", vk->arenas.persistent_arena);
    }

    vk->alloc.pUserData = &vk->arenas;
    vk->alloc.pfnAllocation = vk_alloc;
    vk->alloc.pfnFree = vk_free;
    vk->alloc.pfnReallocation = vk_realloc;

    return create_instance(init_info, vk);
}
void vkr_terminate(vkr_context *vk)
{
    ilog("Terminating vulkan");
    VkAllocationCallbacks c;
    vk->ext_funcs.destroy_debug_utils_messenger(vk->inst, vk->dbg_messenger, &vk->alloc);
    vkDestroyInstance(vk->inst, &vk->alloc);
}

} // namespace nslib
