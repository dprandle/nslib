#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <GLFW/glfw3native.h>
#include <cstring>

#include "math/algorithm.h"
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

intern const u32 ADDITIONAL_INST_EXTENSION_COUNT = 2;
intern const char *ADDITIONAL_INST_EXTENSIONS[ADDITIONAL_INST_EXTENSION_COUNT] = {VK_EXT_DEBUG_UTILS_EXTENSION_NAME,
                                                                                  VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME};

intern const u32 DEVICE_EXTENSION_COUNT = 1;
intern const char *DEVICE_EXTENSIONS[DEVICE_EXTENSION_COUNT] = {VK_KHR_SWAPCHAIN_EXTENSION_NAME};
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

void vkr_enumerate_device_extensions(VkPhysicalDevice pdevice,
                                     const char *const *enabled_extensions,
                                     u32 enabled_extension_count,
                                     vk_arenas *arenas)
{
    u32 extension_count{0};
    ilog("Enumerating device extensions...");
    int res = vkEnumerateDeviceExtensionProperties(pdevice, nullptr, &extension_count, nullptr);
    assert(res == VK_SUCCESS);
    VkExtensionProperties *ext_array =
        (VkExtensionProperties *)mem_alloc(extension_count * sizeof(VkExtensionProperties), arenas->command_arena);
    res = vkEnumerateDeviceExtensionProperties(pdevice, nullptr, &extension_count, ext_array);
    assert(res == VK_SUCCESS);
    for (u32 i = 0; i < extension_count; ++i) {
        bool ext_enabled{false};
        for (u32 j = 0; j < enabled_extension_count; ++j) {
            if (strncmp(enabled_extensions[j], ext_array[i].extensionName, MAX_EXTENSION_STR_LEN) == 0) {
                ext_enabled = true;
            }
        }
        ilog("Device Ext:%s  SpecVersion:%d  Enabled:%s", ext_array[i].extensionName, ext_array[i].specVersion, ext_enabled ? "true" : "false");
    }
}

void vkr_enumerate_instance_extensions(const char *const *enabled_extensions, u32 enabled_extension_count, vk_arenas *arenas)
{
    u32 extension_count{0};
    ilog("Enumerating instance extensions...");
    int res = vkEnumerateInstanceExtensionProperties(nullptr, &extension_count, nullptr);
    assert(res == VK_SUCCESS);
    VkExtensionProperties *ext_array =
        (VkExtensionProperties *)mem_alloc(extension_count * sizeof(VkExtensionProperties), arenas->command_arena);
    res = vkEnumerateInstanceExtensionProperties(nullptr, &extension_count, ext_array);
    assert(res == VK_SUCCESS);
    for (u32 i = 0; i < extension_count; ++i) {
        bool ext_enabled{false};
        for (u32 j = 0; j < enabled_extension_count; ++j) {
            if (strncmp(enabled_extensions[j], ext_array[i].extensionName, MAX_EXTENSION_STR_LEN) == 0) {
                ext_enabled = true;
            }
        }
        ilog("Inst Ext:%s  SpecVersion:%d  Enabled:%s", ext_array[i].extensionName, ext_array[i].specVersion, ext_enabled ? "true" : "false");
    }
}

void vkr_enumerate_validation_layers(const char *const *enabled_layers, u32 enabled_layer_count, vk_arenas *arenas)
{
    u32 layer_count{0};
    ilog("Enumerating vulkan validation layers...");
    int res = vkEnumerateInstanceLayerProperties(&layer_count, nullptr);
    assert(res == VK_SUCCESS);
    VkLayerProperties *layer_array = (VkLayerProperties *)mem_alloc(layer_count * sizeof(VkLayerProperties), arenas->command_arena);
    res = vkEnumerateInstanceLayerProperties(&layer_count, layer_array);
    assert(res == VK_SUCCESS);

    for (u32 i = 0; i < layer_count; ++i) {
        bool enabled{false};
        for (u32 j = 0; j < enabled_layer_count; ++j) {
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
    auto ext = (char **)mem_alloc((ext_count + ADDITIONAL_INST_EXTENSION_COUNT) * sizeof(char *), vk->arenas.command_arena);

    u32 copy_ind = 0;
    for (u32 i = 0; i < ext_count; ++i) {
        ext[copy_ind] = (char *)mem_alloc(strlen(glfw_ext[i]) + 1, vk->arenas.command_arena);
        strcpy(ext[copy_ind], glfw_ext[i]);
        ++copy_ind;
    }
    for (u32 i = 0; i < ADDITIONAL_INST_EXTENSION_COUNT; ++i) {
        ext[copy_ind] = (char *)mem_alloc(strlen(ADDITIONAL_INST_EXTENSIONS[i]) + 1, vk->arenas.command_arena);
        strcpy(ext[copy_ind], ADDITIONAL_INST_EXTENSIONS[i]);
        ilog("Got extension %s", ext[copy_ind]);
        ++copy_ind;
    }

    VkDebugUtilsMessengerCreateInfoEXT dbg_ci{};
    fill_debug_ext_create_info(&dbg_ci, vk);

    create_inf.pNext = &dbg_ci;
    create_inf.ppEnabledExtensionNames = ext;
    create_inf.enabledExtensionCount = ext_count + ADDITIONAL_INST_EXTENSION_COUNT;
    create_inf.ppEnabledLayerNames = VALIDATION_LAYERS;
    create_inf.flags = VK_INSTANCE_CREATE_ENUMERATE_PORTABILITY_BIT_KHR;
    create_inf.enabledLayerCount = VALIDATION_LAYER_COUNT;
    vkr_enumerate_instance_extensions(ext, create_inf.enabledExtensionCount, &vk->arenas);
    vkr_enumerate_validation_layers(VALIDATION_LAYERS, VALIDATION_LAYER_COUNT, &vk->arenas);

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

struct queue_create_info
{
    u32 qoffset;
    u32 create_ind;
};

// Check if any other queue families have the same index other than the family passed in at fam_ind. Fam ind is the
// index into our fam->qinfo array, and index is the actual vulkan queue family index
intern void fill_queue_offsets_and_create_inds(vkr_queue_families *qfams, u32 fam_ind)
{
    bool found_match = false;
    u32 highest_ind = 0;
    for (u32 i = 0; i < fam_ind; ++i) {
        if (qfams->qinfo[i].index == qfams->qinfo[fam_ind].index) {
            found_match = true;
            qfams->qinfo[fam_ind].qoffset += qfams->qinfo[i].requested_count;
            qfams->qinfo[fam_ind].create_ind = qfams->qinfo[i].create_ind;
        }
        if (!found_match && qfams->qinfo[i].create_ind >= highest_ind) {
            highest_ind = qfams->qinfo[i].create_ind + 1;
        }
    }
    if (!found_match) {
        qfams->qinfo[fam_ind].create_ind = highest_ind;
    }
}

vkr_queue_families vkr_get_queue_families(VkPhysicalDevice pdevice, VkSurfaceKHR surface, vk_arenas *arenas)
{
    u32 count{};
    vkr_queue_families ret{};
    vkGetPhysicalDeviceQueueFamilyProperties(pdevice, &count, nullptr);
    auto qfams = (VkQueueFamilyProperties *)mem_alloc(sizeof(VkQueueFamilyProperties) * count, arenas->command_arena);
    vkGetPhysicalDeviceQueueFamilyProperties(pdevice, &count, qfams);
    ilog("%d queue families available for selected device", count);

    // Crash if we ever get a higher count than our max queue fam allotment
    assert(count <= MAX_QUEUE_REQUEST_COUNT);
    for (u32 i = 0; i < count; ++i) {
        bool has_flag = test_flags(qfams[i].queueFlags, VK_QUEUE_GRAPHICS_BIT);
        bool nothing_set_yet = ret.qinfo[VKR_QUEUE_FAM_TYPE_GFX].available_count == 0;
        if (has_flag && nothing_set_yet) {
            ret.qinfo[VKR_QUEUE_FAM_TYPE_GFX].index = i;
            ret.qinfo[VKR_QUEUE_FAM_TYPE_GFX].available_count = qfams[i].queueCount;
            ilog("Selected queue family at index %d for graphics", i);
        }

        VkBool32 supported{false};
        vkGetPhysicalDeviceSurfaceSupportKHR(pdevice, i, surface, &supported);
        if (supported && (ret.qinfo[VKR_QUEUE_FAM_TYPE_PRESENT].available_count == 0 ||
                          ret.qinfo[VKR_QUEUE_FAM_TYPE_PRESENT].index == ret.qinfo[VKR_QUEUE_FAM_TYPE_GFX].index)) {
            ret.qinfo[VKR_QUEUE_FAM_TYPE_PRESENT].index = i;
            ret.qinfo[VKR_QUEUE_FAM_TYPE_PRESENT].available_count = qfams[i].queueCount;
            ilog("Selected queue family at index %d for presentation", i);
        }

        ilog("Queue family ind %d has %d available queues with %#010x capabilities", i, qfams[i].queueCount, qfams[i].queueFlags);
    }
    return ret;
}

int vkr_create_device_and_queues(VkPhysicalDevice pdevice,
                                 VkAllocationCallbacks *alloc_cbs,
                                 vkr_queue_families *qfams,
                                 const char *const *layers,
                                 u32 layer_count,
                                 VkDevice *dev)
{
    // Fill in the queue index offsets based on the fam index from vulkan - if our queue fams have the same index then
    // the queues coming from the later fams need to have an offset index set
    u32 highest_ind = 0;
    for (u32 i = 0; i < VKR_QUEUE_FAM_TYPE_COUNT; ++i) {
        fill_queue_offsets_and_create_inds(qfams, i);
        if (qfams->qinfo[i].create_ind > highest_ind) {
            highest_ind = qfams->qinfo[i].create_ind;
        }
    }
    u32 create_size = highest_ind + 1;

    // NOTE: Make sure you init all vulkan structs to zero - undefined behavior including crashes result otherwise
    VkDeviceQueueCreateInfo qinfo[VKR_QUEUE_FAM_TYPE_COUNT]{};
    float qinfo_f[VKR_QUEUE_FAM_TYPE_COUNT][MAX_QUEUE_REQUEST_COUNT]{};

    // Here we gather how many queues we want for each queue fam type and populate the queue create infos.
    // Its highly likely that the different queue_fam_types will actually have the same vulkan queue family index -
    // thats fine
    for (int i = 0; i < VKR_QUEUE_FAM_TYPE_COUNT; ++i) {
        vkr_queue_family_info *cq = &qfams->qinfo[i];
        u32 ind = cq->create_ind;
        qinfo[ind].sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;

        // This handles the case when the present and graphics must be in the same queue in the same family which can
        // happen if a gfx card has only 1 queue family with 1 available queue
        int req_offset = std::min((int)cq->available_count - (int)(qinfo[ind].queueCount + cq->requested_count), 0);
        cq->qoffset += req_offset;

        qinfo[ind].queueCount += (cq->requested_count + req_offset);
        qinfo[ind].queueFamilyIndex = cq->index;
        qinfo[ind].pQueuePriorities = qinfo_f[cq->create_ind];
        ilog("Setting qind:%d to queue family index:%d with %d queues requested", ind, qinfo[ind].queueFamilyIndex, qinfo[ind].queueCount);
    }

    // For now we will leave this blank - but probably enable geometry shaders later
    VkPhysicalDeviceFeatures features{};
    dlog("Creating %d queues", create_size);

    VkDeviceCreateInfo create_inf{};
    create_inf.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    create_inf.queueCreateInfoCount = create_size;
    create_inf.pQueueCreateInfos = qinfo;
    create_inf.enabledLayerCount = layer_count;
    create_inf.ppEnabledLayerNames = layers;
    create_inf.pEnabledFeatures = &features;
    create_inf.enabledExtensionCount = DEVICE_EXTENSION_COUNT;
    create_inf.ppEnabledExtensionNames = DEVICE_EXTENSIONS;

    int result = vkCreateDevice(pdevice, &create_inf, alloc_cbs, dev);
    if (result != VK_SUCCESS) {
        elog("Device creation failed - vk err:%d", result);
        return err_code::VKR_CREATE_DEVICE_FAIL;
    }

    for (int i = 0; i < VKR_QUEUE_FAM_TYPE_COUNT; ++i) {
        for (u32 qind = 0; qind < qfams->qinfo[i].requested_count; ++qind) {
            u32 adjusted_ind = qind + qfams->qinfo[i].qoffset;
            vkGetDeviceQueue(*dev, qfams->qinfo[i].index, adjusted_ind, &(qfams->qinfo[i].qs[qind]));
            ilog("Getting queue %d from queue family %d: %p", adjusted_ind, qfams->qinfo[i].index, qfams->qinfo[i].qs[qind]);
        }
    }

    return err_code::VKR_NO_ERROR;
}

int vkr_select_best_graphics_physical_device(VkInstance inst, VkSurfaceKHR surface, vkr_physical_device_info *dev_info, vk_arenas *arenas)
{
    u32 count{};
    int ret = vkEnumeratePhysicalDevices(inst, &count, nullptr);
    assert(ret == VK_SUCCESS);
    if (count == 0) {
        elog("No physical devices found - cannot continue");
        return err_code::VKR_NO_PHYSICAL_DEVICES;
    }

    // This is used just so we can log which device we select without having to grab the info again
    VkPhysicalDeviceProperties sel_dev{};

    // These are cached so we don't have to retreive the count again
    int high_score = -1;

    ilog("Found %d physical devices", count);
    auto pdevices = (VkPhysicalDevice *)mem_alloc(sizeof(VkPhysicalDevice) * count, arenas->command_arena);
    ret = vkEnumeratePhysicalDevices(inst, &count, pdevices);
    assert(ret == VK_SUCCESS);
    for (u32 i = 0; i < count; ++i) {
        int cur_score = 0;
        vkr_queue_families fams = vkr_get_queue_families(pdevices[i], surface, arenas);

        // We dont want to select a device unless we can present with it.
        if (fams.qinfo[VKR_QUEUE_FAM_TYPE_PRESENT].index == VKR_INVALID || fams.qinfo[VKR_QUEUE_FAM_TYPE_GFX].index == VKR_INVALID) {
            continue;
        }

        // We need at least one supported format and present mode
        u32 format_count{0};
        vkGetPhysicalDeviceSurfaceFormatsKHR(pdevices[i], surface, &format_count, nullptr);
        if (format_count == 0) {
            continue;
        }

        u32 present_mode_count{0};
        vkGetPhysicalDeviceSurfacePresentModesKHR(pdevices[i], surface, &present_mode_count, nullptr);
        if (present_mode_count == 0) {
            continue;
        }

        VkPhysicalDeviceProperties props{};
        vkGetPhysicalDeviceProperties(pdevices[i], &props);

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

        // Save the device and cache the props so we can easily log them after selecting the device
        if (cur_score > high_score) {
            dev_info->pdevice = pdevices[i];
            dev_info->qfams = fams;
            vkr_fill_pdevice_swapchain_support(dev_info->pdevice, surface, &dev_info->swap_support);
            high_score = cur_score;
            sel_dev = props;
        }
    }
    ilog("Selected device id:%d  name:%s  type:%s", sel_dev.deviceID, sel_dev.deviceName, vkr_physical_device_type_str(sel_dev.deviceType));
    if (high_score == -1) {
        return err_code::VKR_NO_SUITABLE_PHYSICAL_DEVICE;
    }

    return err_code::VKR_NO_ERROR;
}

void vkr_fill_pdevice_swapchain_support(VkPhysicalDevice pdevice, VkSurfaceKHR surface, vkr_pdevice_swapchain_support *ssup)
{
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(pdevice, surface, &ssup->capabilities);

    u32 format_count{0};
    vkGetPhysicalDeviceSurfaceFormatsKHR(pdevice, surface, &format_count, nullptr);
    arr_resize(&ssup->formats, format_count);
    vkGetPhysicalDeviceSurfaceFormatsKHR(pdevice, surface, &format_count, ssup->formats.data);

    u32 present_mode_count{0};
    vkGetPhysicalDeviceSurfacePresentModesKHR(pdevice, surface, &present_mode_count, nullptr);
    arr_resize(&ssup->present_modes, present_mode_count);
    vkGetPhysicalDeviceSurfacePresentModesKHR(pdevice, surface, &present_mode_count, ssup->present_modes.data);
}

int vkr_init_swapchain(VkDevice device,
                       VkSurfaceKHR surface,
                       const vkr_physical_device_info *dev_info,
                       const VkAllocationCallbacks *alloc_cbs,
                       void *window,
                       vkr_swapchain_info *sw_info)
{
    // I no like typing too much
    auto caps = &dev_info->swap_support.capabilities;
    auto qfams = &dev_info->qfams;
    auto formats = &dev_info->swap_support.formats;
    auto pmodes = &dev_info->swap_support.present_modes;

    VkSwapchainCreateInfoKHR swap_create{};
    swap_create.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    swap_create.surface = surface;
    swap_create.imageArrayLayers = 1;
    swap_create.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    swap_create.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    swap_create.preTransform = dev_info->swap_support.capabilities.currentTransform;
    swap_create.clipped = VK_TRUE;
    swap_create.oldSwapchain = VK_NULL_HANDLE;
    swap_create.minImageCount = caps->minImageCount + 1;
    if (caps->maxImageCount != 0 && caps->maxImageCount < swap_create.minImageCount) {
        swap_create.minImageCount = caps->maxImageCount;
    }

    // Basically if our present queue is different than our graphics queue then we enable concurrent sharing mode..
    // honestly not sure about that yet
    uint32_t queue_fam_inds[] = {qfams->qinfo[VKR_QUEUE_FAM_TYPE_GFX].index, qfams->qinfo[VKR_QUEUE_FAM_TYPE_PRESENT].index};
    swap_create.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    swap_create.queueFamilyIndexCount = 0;
    swap_create.pQueueFamilyIndices = nullptr;
    if (queue_fam_inds[0] != queue_fam_inds[1]) {
        swap_create.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
        swap_create.queueFamilyIndexCount = 2;
        swap_create.pQueueFamilyIndices = queue_fam_inds;
    }

    // Set the format to our ideal format, but just get the first one if thats not found
    int desired_format_ind{0};
    for (int i = 0; i < formats->size; ++i) {
        if ((*formats)[i].format == VK_FORMAT_B8G8R8A8_SRGB && (*formats)[i].colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
            desired_format_ind = i;
            break;
        }
    }
    swap_create.imageFormat = (*formats)[desired_format_ind].format;
    swap_create.imageColorSpace = (*formats)[desired_format_ind].colorSpace;

    // If mailbox is available then use it, otherwise use fifo
    // TODO: This is arbitray - investigate these different modes to actually chose which one we want based off of
    // something other than this tutorial
    swap_create.presentMode = VK_PRESENT_MODE_FIFO_KHR;
    for (int i = 0; i < pmodes->size; ++i) {
        if ((*pmodes)[i] == VK_PRESENT_MODE_MAILBOX_KHR) {
            swap_create.presentMode = (*pmodes)[i];
            break;
        }
    }

    // Handle the extents - if the screen coords don't match the pixel coords then we gotta set this from the frame
    // buffer as by default the extents are screen coords - this is really only for retina displays
    swap_create.imageExtent = caps->currentExtent;
    if (caps->currentExtent.width == VKR_INVALID) {
        int width, height;
        glfwGetFramebufferSize((GLFWwindow *)window, &width, &height);
        swap_create.imageExtent = {(u32)width, (u32)height};
        swap_create.imageExtent.width = std::clamp(swap_create.imageExtent.width, caps->minImageExtent.width, caps->maxImageExtent.width);
        swap_create.imageExtent.height = std::clamp(swap_create.imageExtent.height, caps->minImageExtent.height, caps->maxImageExtent.height);
    }

    // Create this baby boo
    if (vkCreateSwapchainKHR(device, &swap_create, alloc_cbs, &sw_info->swapchain) != VK_SUCCESS) {
        return err_code::VKR_CREATE_SWAPCHAIN_FAIL;
    }
    sw_info->extent = swap_create.imageExtent;
    sw_info->format = swap_create.imageFormat;
    u32 image_count{};
    VkResult res = vkGetSwapchainImagesKHR(device, sw_info->swapchain, &image_count, nullptr);
    assert(res == VK_SUCCESS);
    arr_resize(&sw_info->images, image_count);
    res = vkGetSwapchainImagesKHR(device, sw_info->swapchain, &image_count, sw_info->images.data);
    assert(res == VK_SUCCESS);

    arr_resize(&sw_info->image_views, image_count);
    for (int i = 0; i < sw_info->images.size; ++i) {
        VkImageViewCreateInfo iview_create{};
        iview_create.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        iview_create.image = sw_info->images[i];
        iview_create.viewType = VK_IMAGE_VIEW_TYPE_2D;
        iview_create.format = sw_info->format;

        iview_create.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
        iview_create.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
        iview_create.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
        iview_create.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;

        iview_create.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        iview_create.subresourceRange.baseMipLevel = 0;
        iview_create.subresourceRange.levelCount = 1;
        iview_create.subresourceRange.baseArrayLayer = 0;
        iview_create.subresourceRange.layerCount = 1;
        VkResult res = vkCreateImageView(device, &iview_create, alloc_cbs, &sw_info->image_views[i]);
        assert(res == VK_SUCCESS);
    }

    return err_code::VKR_NO_ERROR;
}

void vkr_init_swapchain_info(vkr_swapchain_info *sw_info, mem_arena *arena)
{
    *sw_info = {};
    arr_init(&sw_info->images, arena);
    arr_init(&sw_info->image_views, arena);
}

void vkr_terminate_swapchain_info(vkr_swapchain_info *sw_info)
{
    arr_terminate(&sw_info->images);
    arr_terminate(&sw_info->image_views);
}

void vkr_init_pipeline(const vkr_pipeline_init_info *init_info, const vk_arenas *arenas, vkr_pipeline_info *pipe_info)
{
    byte_array loaded_data{};
    arr_init(&loaded_data, arenas->command_arena);
}

void vkr_terminate_pipeline(const vk_arenas *arenas, vkr_pipeline_info *pipe_info)
{
    
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

    // Create surface
    if (init_info->window) {
        int ret = glfwCreateWindowSurface(vk->inst, (GLFWwindow *)init_info->window, &vk->alloc_cbs, &vk->surface);
        if (ret != VK_SUCCESS) {
            elog("Failed to create surface with err code %d", ret);
            vkr_terminate(vk);
            return err_code::VKR_CREATE_SURFACE_FAIL;
        }
        ilog("Successfully created window surface");
    }
    else {
        wlog("No window provided - rendering to window surface disabled");
    }

    vkr_init_pdevice_swapchain_support(&vk->pdev_info.swap_support, vk->arenas.persistent_arena);

    int code = vkr_select_best_graphics_physical_device(vk->inst, vk->surface, &vk->pdev_info, &vk->arenas);
    if (code != err_code::VKR_NO_ERROR) {
        elog("Failed to select physical device code:%d", code);
        vkr_terminate(vk);
        return code;
    }

    vkr_enumerate_device_extensions(vk->pdev_info.pdevice, DEVICE_EXTENSIONS, DEVICE_EXTENSION_COUNT, &vk->arenas);
    code = vkr_create_device_and_queues(
        vk->pdev_info.pdevice, &vk->alloc_cbs, &vk->pdev_info.qfams, VALIDATION_LAYERS, VALIDATION_LAYER_COUNT, &vk->device);

    if (code != err_code::VKR_NO_ERROR) {
        elog("Device creation failed");
        vkr_terminate(vk);
        return code;
    }

    vkr_init_swapchain_info(&vk->sw_info, vk->arenas.persistent_arena);
    code = vkr_init_swapchain(vk->device, vk->surface, &vk->pdev_info, &vk->alloc_cbs, init_info->window, &vk->sw_info);
    if (code != err_code::VKR_NO_ERROR) {
        elog("Failed to initialize swapchain");
        vkr_terminate(vk);
        return code;
    }

    return err_code::VKR_NO_ERROR;
}

void vkr_init_pdevice_swapchain_support(vkr_pdevice_swapchain_support *ssup, mem_arena *arena)
{
    arr_init(&ssup->formats, arena);
    arr_init(&ssup->present_modes, arena);
}

void vkr_terminate_pdevice_swapchain_support(vkr_pdevice_swapchain_support *ssup)
{
    arr_terminate(&ssup->formats);
    arr_terminate(&ssup->present_modes);
    ssup->capabilities = {};
}

void vkr_terminate_instance(vkr_context *vk)
{
    vk->ext_funcs.destroy_debug_utils_messenger(vk->inst, vk->dbg_messenger, &vk->alloc_cbs);

    // Destroying the instance calls more vk alloc calls
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
    for (int i = 0; i < vk->sw_info.image_views.size; ++i) {
        vkDestroyImageView(vk->device, vk->sw_info.image_views[i], &vk->alloc_cbs);
    }
    vkDestroySwapchainKHR(vk->device, vk->sw_info.swapchain, &vk->alloc_cbs);
    vkr_terminate_swapchain_info(&vk->sw_info);
    vkr_terminate_pdevice_swapchain_support(&vk->pdev_info.swap_support);
    vkDestroySurfaceKHR(vk->inst, vk->surface, &vk->alloc_cbs);
    vkDestroyDevice(vk->device, &vk->alloc_cbs);
    vkr_terminate_instance(vk);
    for (int i = 0; i < MEM_ALLOC_TYPE_COUNT; ++i) {
        log_mem_stats(alloc_scope_str(i), &vk->arenas.stats[i]);
    }
}

} // namespace nslib
