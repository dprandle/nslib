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

void vkr_enumerate_extensions(const char *const *enabled_extensions, u32 enabled_extension_count)
{
    u32 extension_count{0};
    ilog("Enumerating vulkan extensions...");
    int res = vkEnumerateInstanceExtensionProperties(nullptr, &extension_count, nullptr);
    assert(res == VK_SUCCESS);
    VkExtensionProperties *ext_array =
        (VkExtensionProperties *)mem_alloc(extension_count * sizeof(VkExtensionProperties), mem_global_frame_lin_arena());
    res = vkEnumerateInstanceExtensionProperties(nullptr, &extension_count, ext_array);
    assert(res == VK_SUCCESS);
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

void vkr_enumerate_validation_layers(const char *const *enabled_layers, u32 enabled_layer_count)
{
    u32 layer_count{0};
    ilog("Enumerating vulkan validation layers...");
    int res = vkEnumerateInstanceLayerProperties(&layer_count, nullptr);
    assert(res == VK_SUCCESS);
    VkLayerProperties *layer_array = (VkLayerProperties *)mem_alloc(layer_count * sizeof(VkLayerProperties), mem_global_frame_lin_arena());
    res = vkEnumerateInstanceLayerProperties(&layer_count, layer_array);
    assert(res == VK_SUCCESS);

    for (int i = 0; i < layer_count; ++i) {
        bool enabled{false};
        for (int j = 0; j < enabled_layer_count; ++j) {
            if (strcmp(enabled_layers[j], layer_array[i].layerName) == 0) {
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
    int cur = log_get_level();
    log_set_level(((vkr_context *)user)->log_verbosity);
    if (severity >= VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT) {
        elog("Vk: %s", data->pMessage);
    }
    else if (severity >= VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT) {
        wlog("Vk: %s", data->pMessage);
    }
    else if (severity >= VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT) {
        dlog("Vk: %s", data->pMessage);
    }
    else if (severity >= VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT) {
        tlog("Vk: %s", data->pMessage);
    }
    log_set_level(cur);
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

intern void fill_debug_ext_create_info(VkDebugUtilsMessengerCreateInfoEXT *create_info, void *user_p)
{
    create_info->sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
    create_info->messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
                                   VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
    create_info->messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
                               VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
    create_info->pfnUserCallback = debug_message_callback;
    create_info->pUserData = user_p;
}

int vkr_init_instance(const vkr_init_info *init_info, vkr_context *vk)
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
    fill_debug_ext_create_info(&dbg_ci, vk);

    create_inf.pNext = &dbg_ci;
    create_inf.ppEnabledExtensionNames = ext;
    create_inf.enabledExtensionCount = ext_count + ADDITIONAL_EXTENSION_COUNT;
    create_inf.ppEnabledLayerNames = VALIDATION_LAYERS;
    create_inf.enabledLayerCount = VALIDATION_LAYER_COUNT;
    vkr_enumerate_extensions(ext, create_inf.enabledExtensionCount);
    vkr_enumerate_validation_layers(VALIDATION_LAYERS, VALIDATION_LAYER_COUNT);

    int err = vkCreateInstance(&create_inf, &vk->alloc_cbs, &vk->inst);
    if (err == VK_SUCCESS) {
        get_extension_funcs(&vk->ext_funcs, vk->inst);
        vk->ext_funcs.create_debug_utils_messenger(vk->inst, &dbg_ci, &vk->alloc_cbs, &vk->dbg_messenger);
    }
    return err;
}

const char *vkr_physical_device_type_str(VkPhysicalDeviceType type)
{
    switch (type) {
    case (VK_PHYSICAL_DEVICE_TYPE_OTHER):
        return "other";
    case (VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU):
        return "integrated_gpu";
    case (VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU):
        return "discrete_gpu";
    case (VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU):
        return "virtual_gpu";
    case (VK_PHYSICAL_DEVICE_TYPE_CPU):
        return "CPU";
    default:
        return "unknown";
    }
}

int vkr_enumerate_and_select_best_physical_device(VkInstance inst, VkPhysicalDevice *device)
{
    u32 count{};
    int ret = vkEnumeratePhysicalDevices(inst, &count, nullptr);
    assert(ret == VK_SUCCESS);
    if (count == 0) {
        elog("No physical devices found - cannot continue");
        return err_code::VKR_NO_PHYSICAL_DEVICES;
    }

    int sel_ind = 0;
    int high_score = 0;
    VkPhysicalDeviceProperties sel_dev{};

    ilog("Found %d physical devices", count);
    VkPhysicalDevice *pdevices = (VkPhysicalDevice *)mem_alloc(sizeof(VkPhysicalDevice) * count, mem_global_frame_lin_arena());
    ret = vkEnumeratePhysicalDevices(inst, &count, pdevices);
    assert(ret == VK_SUCCESS);
    for (int i = 0; i < count; ++i) {
        int cur_score = 0;

        VkPhysicalDeviceProperties props{};
        vkGetPhysicalDeviceProperties(pdevices[i], &props);
        if (i == 0) {
            sel_dev = props;
        }

        if (props.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) {
            cur_score += 10;
        }
        else if (props.deviceType == VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU) {
            cur_score += 5;
        }
        else if (props.deviceType == VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU) {
            cur_score += 2;
        }
        else if (props.deviceType == VK_PHYSICAL_DEVICE_TYPE_CPU) {
            cur_score += 1;
        }

        VkPhysicalDeviceFeatures features;
        vkGetPhysicalDeviceFeatures(pdevices[i], &features);

        if (features.geometryShader) {
            cur_score += 4;
        }
        if (features.tessellationShader) {
            cur_score += 3;
        }

        ilog("PhysDevice ID:%d Name:%s Type:%s VendorID:%d DriverVersion:%d GeomShader:%s TessShader:%s - total score:%d",
             props.deviceID,
             props.deviceName,
             vkr_physical_device_type_str(props.deviceType),
             props.vendorID,
             props.driverVersion,
             features.geometryShader ? "true" : "false",
             features.tessellationShader ? "true" : "false",
             cur_score);

        if (cur_score > high_score) {
            sel_ind = i;
            high_score = cur_score;
            sel_dev = props;
        }
    }

    *device = pdevices[sel_ind];
    ilog("Selected device id:%d  name:%s  type:%s", sel_dev.deviceID, sel_dev.deviceName, vkr_physical_device_type_str(sel_dev.deviceType));
    return err_code::VKR_NO_ERROR;
}

int vkr_init(const vkr_init_info *init_info, vkr_context *vk)
{
    ilog("Initializing vulkan");
    assert(init_info);
    vk->arenas = init_info->arenas;
    vk->log_verbosity = init_info->log_verbosity;
    if (!init_info->arenas.command_arena) {
        vk->arenas.command_arena = mem_global_frame_lin_arena();
        ilog("Using global frame linear arena %p", vk->arenas.command_arena);
    }

    if (!init_info->arenas.persistent_arena) {
        vk->arenas.persistent_arena = mem_global_arena();
        ilog("Using global persistent arena %p", vk->arenas.persistent_arena);
    }

    vk->alloc_cbs.pUserData = &vk->arenas;
    vk->alloc_cbs.pfnAllocation = vk_alloc;
    vk->alloc_cbs.pfnFree = vk_free;
    vk->alloc_cbs.pfnReallocation = vk_realloc;
    int err_code = vkr_init_instance(init_info, vk);
    if (err_code != VK_SUCCESS) {
        elog("Failed to create vulkan instance with vulkan err code: %d", err_code);
        return err_code::VKR_CREATE_INSTANCE_FAIL;
    }
    ilog("Successfully created vulkan instance");
    int code = vkr_enumerate_and_select_best_physical_device(vk->inst, &vk->physical_device);
    return code;
}

void vkr_terminate_instance(vkr_context *vk)
{
    vk->ext_funcs.destroy_debug_utils_messenger(vk->inst, vk->dbg_messenger, &vk->alloc_cbs);
    vkDestroyInstance(vk->inst, &vk->alloc_cbs);
}

void vkr_terminate(vkr_context *vk)
{
    ilog("Terminating vulkan");
    vkr_terminate_instance(vk);
}

} // namespace nslib
