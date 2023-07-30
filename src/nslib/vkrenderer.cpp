#include <GLFW/glfw3.h>
#include <cstring>

#include "vkrenderer.h"
#include "logging.h"
#include "mem.h"

#define PRINT_MEM_DEBUG false
#define PRINT_MEM_INSTANCE_ONLY true

namespace nslib
{

struct internal_alloc_header
{
    u32 scope{};
    sizet req_size{};
};

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

intern const char *alloc_scope_str(int scope)
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
    ++arenas->stats[scope].alloc_count;
    arenas->stats[scope].req_alloc += size;

    auto arena = arenas->persistent_arena;
    if (scope == VK_SYSTEM_ALLOCATION_SCOPE_COMMAND) {
        arena = arenas->command_arena;
    }
    sizet used_before = arena->used;
    sizet header_size = sizeof(internal_alloc_header);

    auto header = (internal_alloc_header *)mem_alloc(size + header_size, arena, alignment);
    header->scope = scope;
    header->req_size = size;

    void *ret = (void *)((sizet)header + header_size);
    sizet used_actual = arena->used - used_before;

    arenas->stats[scope].actual_alloc += used_actual;
#if PRINT_MEM_DEBUG
#if PRINT_MEM_INSTANCE_ONLY
    if (scope == VK_SYSTEM_ALLOCATION_SCOPE_INSTANCE) {
#endif
        dlog("header_addr:%p ptr:%p requested_size:%d alignment:%d scope:%s used_before:%d alloc:%d used_after:%d",
             (void *)header,
             ret,
             size,
             alignment,
             alloc_scope_str(scope),
             used_before,
             used_actual,
             arena->used);
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

    sizet header_size = sizeof(internal_alloc_header);
    auto header = (internal_alloc_header *)((sizet)ptr - header_size);
    u32 scope = header->scope;
    sizet req_size = header->req_size;

    ++arenas->stats[scope].free_count;

    if (scope == VK_SYSTEM_ALLOCATION_SCOPE_COMMAND) {
        arena = arenas->command_arena;
    }
    sizet used_before = arena->used;
    arenas->stats[scope].req_free += req_size;

    mem_free(header, arena);
    sizet actual_freed = used_before - arena->used;
    arenas->stats[scope].actual_free += actual_freed;

#if PRINT_MEM_DEBUG
#if PRINT_MEM_INSTANCE_ONLY
    if (scope == VK_SYSTEM_ALLOCATION_SCOPE_INSTANCE) {
#endif
        dlog("header_addr:%p ptr:%p requested_size:%d scope:%s used_before:%d dealloc:%d used_after:%d",
             header,
             ptr,
             req_size,
             alloc_scope_str(scope),
             used_before,
             actual_freed,
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
    ++arenas->stats[scope].realloc_count;
    arenas->stats[scope].req_alloc += size;

    auto arena = arenas->persistent_arena;

    sizet header_size = sizeof(internal_alloc_header);
    auto old_header = (internal_alloc_header *)((sizet)ptr - header_size);
    assert(old_header->scope == scope);
    if (scope == VK_SYSTEM_ALLOCATION_SCOPE_COMMAND) {
        arena = arenas->command_arena;
    }

    sizet old_block_size = mem_block_size(old_header, arena);
    sizet old_req_size = old_header->req_size;
    arenas->stats[scope].actual_free += old_block_size;
    arenas->stats[scope].req_free += old_req_size;
    sizet used_before = arena->used;

    auto new_header = (internal_alloc_header *)mem_realloc(old_header, size + header_size, arena, alignment);
    sizet new_block_size = mem_block_size(new_header, arena);

    new_header->scope = scope;
    new_header->req_size = size;
    void *ret = (void *)((sizet)new_header + header_size);
    arenas->stats[scope].actual_alloc += new_block_size;
    sizet diff = arena->used - used_before;
    assert(diff == (new_block_size - old_block_size));

#if PRINT_MEM_DEBUG
#if PRINT_MEM_INSTANCE_ONLY
    if (scope == VK_SYSTEM_ALLOCATION_SCOPE_INSTANCE) {
#endif
        dlog("orig_header_addr:%p new_header_addr:%p orig_ptr:%p new_ptr:%p orig_req_size:%d new_req_size:%d scope:%s used_before:%d "
             "dealloc:%d alloc:%d used_after:%d diff:%d",
             old_header,
             new_header,
             ptr,
             ret,
             old_req_size,
             size,
             alloc_scope_str(scope),
             used_before,
             old_block_size,
             new_block_size,
             arena->used,
             diff);
#if PRINT_MEM_INSTANCE_ONLY
    }
#endif
#endif
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
    const char **glfw_ext = glfwGetRequiredInstanceExtensions(&ext_count);
    auto ext = (char **)mem_alloc((ext_count + ADDITIONAL_EXTENSION_COUNT) * sizeof(char *), mem_global_frame_lin_arena());

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

vkr_queue_families vkr_get_queue_families(VkPhysicalDevice pdevice)
{
    u32 count{};
    vkr_queue_families ret{};
    vkGetPhysicalDeviceQueueFamilyProperties(pdevice, &count, nullptr);
    auto qfams = (VkQueueFamilyProperties *)mem_alloc(sizeof(VkQueueFamilyProperties) * count, mem_global_frame_lin_arena());
    vkGetPhysicalDeviceQueueFamilyProperties(pdevice, &count, qfams);
    ilog("%d queue families available for selected device", count);
    for (u32 i = 0; i < count; ++i) {
        if (check_flags(qfams[i].queueFlags, VK_QUEUE_GRAPHICS_BIT) && ret.gfx.available_count == 0) {
            ret.gfx.index = i;
            ret.gfx.available_count = qfams[i].queueCount;
            ilog("Selected queue family at index %d for graphics", i);
        }
        ilog("Queue family ind %d has %d available queues with %#010x capabilities", i, qfams[i].queueCount, qfams[i].queueFlags);
    }
    return ret;
}

int vkr_create_device(VkPhysicalDevice pdevice, VkAllocationCallbacks *alloc_cbs, const char *const *layers, u32 layer_count, VkDevice *dev)
{
    vkr_queue_families qfams = vkr_get_queue_families(pdevice);
    if (qfams.gfx.available_count == 0) {
        return err_code::VKR_NO_QUEUE_FAMILIES;
    }

    VkDeviceQueueCreateInfo qinfo{};
    float priority{1.0f};
    qinfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    qinfo.queueCount = 1;
    qinfo.queueFamilyIndex = qfams.gfx.index;
    qinfo.pQueuePriorities = &priority;

    VkPhysicalDeviceFeatures features{};

    VkDeviceCreateInfo create_inf{};
    create_inf.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    create_inf.queueCreateInfoCount = 1;
    create_inf.pQueueCreateInfos = &qinfo;
    create_inf.enabledLayerCount = layer_count;
    create_inf.ppEnabledLayerNames = layers;
    create_inf.pEnabledFeatures = &features;
    create_inf.enabledExtensionCount = 0;
    int result = vkCreateDevice(pdevice, &create_inf, alloc_cbs, dev);
    if (result != VK_SUCCESS) {
        elog("Device creation failed - vk err:%d", result);
        return err_code::VKR_DEVICE_CREATION_FAILED;
    }
    return err_code::VKR_NO_ERROR;
}

int vkr_select_best_graphics_physical_device(VkInstance inst, VkPhysicalDevice *device)
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
    auto pdevices = (VkPhysicalDevice *)mem_alloc(sizeof(VkPhysicalDevice) * count, mem_global_frame_lin_arena());
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

    int code = vkr_select_best_graphics_physical_device(vk->inst, &vk->pdevice);
    if (code != err_code::VKR_NO_ERROR) {
        vkr_terminate_instance(vk);
        return code;
    }

    code = vkr_create_device(vk->pdevice, &vk->alloc_cbs, VALIDATION_LAYERS, VALIDATION_LAYER_COUNT, &vk->device);
    // if (code != err_code::VKR_NO_ERROR) {
    //     vkr_terminate_instance(vk);
    //     return code;
    // }
    return code;
}

void vkr_terminate_instance(vkr_context *vk)
{
    vk->ext_funcs.destroy_debug_utils_messenger(vk->inst, vk->dbg_messenger, &vk->alloc_cbs);
    vkDestroyInstance(vk->inst, &vk->alloc_cbs);
}

intern void log_mem_stats(const char *type, const vk_mem_alloc_stats *stats)
{
    ilog("%s stats:\n alloc_count:%d free_count:%d realloc_count:%d req_alloc:%d req_free:%d actual_alloc:%d actual_free:%d",
         type,
         stats->alloc_count,
         stats->free_count,
         stats->realloc_count,
         stats->req_alloc,
         stats->req_free,
         stats->actual_alloc,
         stats->actual_free);
}

void vkr_terminate(vkr_context *vk)
{
    ilog("Terminating vulkan");
    vkDestroyDevice(vk->device, &vk->alloc_cbs);
    vkr_terminate_instance(vk);
    for (int i = 0; i < MEM_ALLOC_TYPE_COUNT; ++i) {
        log_mem_stats(alloc_scope_str(i), &vk->arenas.stats[i]);
    }
}

} // namespace nslib
