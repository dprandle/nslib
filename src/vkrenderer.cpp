#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <GLFW/glfw3native.h>
#include <cstring>

#include "math/algorithm.h"
#include "vkrenderer.h"
#include "logging.h"
#include "memory.h"

#include "platform.h"

#define PRINT_MEM_DEBUG false
#define PRINT_MEM_INSTANCE_ONLY true

namespace nslib
{

struct internal_alloc_header
{
    u32 scope{};
    sizet req_size{};
};

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

void vkr_enumerate_device_extensions(const vkr_phys_device *pdevice,
                                     const char *const *enabled_extensions,
                                     u32 enabled_extension_count,
                                     const vk_arenas *arenas)
{
    u32 extension_count{0};
    ilog("Enumerating device extensions...");
    int res = vkEnumerateDeviceExtensionProperties(pdevice->hndl, nullptr, &extension_count, nullptr);
    assert(res == VK_SUCCESS);
    VkExtensionProperties *ext_array =
        (VkExtensionProperties *)mem_alloc(extension_count * sizeof(VkExtensionProperties), arenas->command_arena);
    res = vkEnumerateDeviceExtensionProperties(pdevice->hndl, nullptr, &extension_count, ext_array);
    assert(res == VK_SUCCESS);
    for (u32 i = 0; i < extension_count; ++i) {
        bool ext_enabled{false};
        for (u32 j = 0; j < enabled_extension_count; ++j) {
            if (strncmp(enabled_extensions[j], ext_array[i].extensionName, VKR_MAX_EXTENSION_STR_LEN) == 0) {
                ext_enabled = true;
            }
        }
        ilog("Device Ext:%s  SpecVersion:%d  Enabled:%s", ext_array[i].extensionName, ext_array[i].specVersion, ext_enabled ? "true" : "false");
    }
}

void vkr_enumerate_instance_extensions(const char *const *enabled_extensions, u32 enabled_extension_count, const vk_arenas *arenas)
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
            if (strncmp(enabled_extensions[j], ext_array[i].extensionName, VKR_MAX_EXTENSION_STR_LEN) == 0) {
                ext_enabled = true;
            }
        }
        ilog("Inst Ext:%s  SpecVersion:%d  Enabled:%s", ext_array[i].extensionName, ext_array[i].specVersion, ext_enabled ? "true" : "false");
    }
}

void vkr_enumerate_validation_layers(const char *const *enabled_layers, u32 enabled_layer_count, const vk_arenas *arenas)
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
    log_set_level(*((int *)user));
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

intern void fill_extension_funcs(vkr_debug_extension_funcs *funcs, VkInstance hndl)
{
    funcs->create_debug_utils_messenger = (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(hndl, "vkCreateDebugUtilsMessengerEXT");
    funcs->destroy_debug_utils_messenger =
        (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(hndl, "vkDestroyDebugUtilsMessengerEXT");
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

int vkr_init_instance(const vkr_context *vk, vkr_instance *inst)
{
    ilog("Trying to create vulkan instance...");
    VkApplicationInfo app_info{};
    app_info.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    app_info.pApplicationName = vk->cfg.app_name;
    app_info.applicationVersion = VK_MAKE_VERSION(vk->cfg.vi.major, vk->cfg.vi.minor, vk->cfg.vi.patch);
    app_info.engineVersion = VK_MAKE_VERSION(1, 0, 0);

    app_info.pEngineName = "Noble Steed";
    app_info.apiVersion = VK_API_VERSION_1_3;

    VkInstanceCreateInfo create_inf{};
    create_inf.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    create_inf.pApplicationInfo = &app_info;

    // This is for clarity.. we could just directly pass the enabled extension count
    u32 ext_count{0};
    const char **glfw_ext = glfwGetRequiredInstanceExtensions(&ext_count);
    auto ext = (char **)mem_alloc((ext_count + vk->cfg.extra_instance_extension_count) * sizeof(char *), vk->cfg.arenas.command_arena);

    u32 copy_ind = 0;
    for (u32 i = 0; i < ext_count; ++i) {
        ext[copy_ind] = (char *)mem_alloc(strlen(glfw_ext[i]) + 1, vk->cfg.arenas.command_arena);
        strcpy(ext[copy_ind], glfw_ext[i]);
        ++copy_ind;
    }
    for (u32 i = 0; i < vk->cfg.extra_instance_extension_count; ++i) {
        ext[copy_ind] = (char *)mem_alloc(strlen(vk->cfg.extra_instance_extension_names[i]) + 1, vk->cfg.arenas.command_arena);
        strcpy(ext[copy_ind], vk->cfg.extra_instance_extension_names[i]);
        ilog("Got extension %s", ext[copy_ind]);
        ++copy_ind;
    }

    // Setting this as the next struct in create_inf allows us to have debug printing for creation of instance errors
    VkDebugUtilsMessengerCreateInfoEXT dbg_ci{};
    fill_debug_ext_create_info(&dbg_ci, (void *)&vk->cfg.log_verbosity);

    u32 total_exts = ext_count + vk->cfg.extra_instance_extension_count;
    vkr_enumerate_instance_extensions(ext, total_exts, &vk->cfg.arenas);
    vkr_enumerate_validation_layers(vk->cfg.validation_layer_names, vk->cfg.validation_layer_count, &vk->cfg.arenas);

    create_inf.pNext = &dbg_ci;
    create_inf.ppEnabledExtensionNames = ext;
    create_inf.enabledExtensionCount = total_exts;
    create_inf.ppEnabledLayerNames = vk->cfg.validation_layer_names;
    create_inf.enabledLayerCount = vk->cfg.validation_layer_count;
    create_inf.flags = VK_INSTANCE_CREATE_ENUMERATE_PORTABILITY_BIT_KHR;

    int err = vkCreateInstance(&create_inf, &vk->alloc_cbs, &inst->hndl);
    if (err == VK_SUCCESS) {
        ilog("Successfully created vulkan instance");

        // Setup debugging callbacks (for logging/printing)
        fill_extension_funcs(&inst->ext_funcs, vk->inst.hndl);
        inst->ext_funcs.create_debug_utils_messenger(inst->hndl, &dbg_ci, &vk->alloc_cbs, &inst->dbg_messenger);

        return err_code::VKR_NO_ERROR;
    }
    else {
        elog("Failed to create vulkan instance with err code: %d", err);
        return err_code::VKR_CREATE_INSTANCE_FAIL;
    }
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

// Check if any other queue families have the same index other than the family passed in at fam_ind. Fam ind is the
// index into our fam->qinfo array, and index is the actual vulkan queue family index
intern void fill_queue_offsets_and_create_inds(vkr_queue_families *qfams, u32 fam_ind)
{
    bool found_match = false;
    u32 highest_ind = 0;
    for (u32 i = 0; i < fam_ind; ++i) {
        if (qfams->qinfo[i].index == qfams->qinfo[fam_ind].index) {
            found_match = true;
            // qfams->qinfo[fam_ind].qoffset += qfams->qinfo[i].requested_count;
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

vkr_queue_families vkr_get_queue_families(const vkr_context *vk, VkPhysicalDevice pdevice)
{
    u32 count{};
    vkr_queue_families ret{};
    vkGetPhysicalDeviceQueueFamilyProperties(pdevice, &count, nullptr);
    auto qfams = (VkQueueFamilyProperties *)mem_alloc(sizeof(VkQueueFamilyProperties) * count, vk->cfg.arenas.command_arena);
    vkGetPhysicalDeviceQueueFamilyProperties(pdevice, &count, qfams);
    ilog("%d queue families available for selected device", count);

    // Crash if we ever get a higher count than our max queue fam allotment
    assert(count <= MAX_QUEUE_REQUEST_COUNT);
    for (u32 i = 0; i < count; ++i) {
        bool has_flag = test_flags(qfams[i].queueFlags, VK_QUEUE_GRAPHICS_BIT);
        bool nothing_set_yet = ret.qinfo[VKR_QUEUE_FAM_TYPE_GFX].available_count == 0;
        if (has_flag && nothing_set_yet) {
            ret.qinfo[VKR_QUEUE_FAM_TYPE_GFX].index = i;
            if (ret.qinfo[VKR_QUEUE_FAM_TYPE_GFX].requested_count == 0) {
                ret.qinfo[VKR_QUEUE_FAM_TYPE_GFX].requested_count = qfams[i].queueCount;
            }
            ret.qinfo[VKR_QUEUE_FAM_TYPE_GFX].requested_count = qfams[i].queueCount;
            ret.qinfo[VKR_QUEUE_FAM_TYPE_GFX].available_count = qfams[i].queueCount;
            ilog("Selected queue family at index %d for graphics (%d available)", i, qfams[i].queueCount);
        }

        VkBool32 supported{false};
        vkGetPhysicalDeviceSurfaceSupportKHR(pdevice, i, vk->inst.surface, &supported);
        if (supported && (ret.qinfo[VKR_QUEUE_FAM_TYPE_PRESENT].available_count == 0 ||
                          ret.qinfo[VKR_QUEUE_FAM_TYPE_PRESENT].index == ret.qinfo[VKR_QUEUE_FAM_TYPE_GFX].index)) {
            ret.qinfo[VKR_QUEUE_FAM_TYPE_PRESENT].index = i;
            if (ret.qinfo[VKR_QUEUE_FAM_TYPE_PRESENT].requested_count == 0) {
                ret.qinfo[VKR_QUEUE_FAM_TYPE_PRESENT].requested_count = qfams[i].queueCount;
            }
            ret.qinfo[VKR_QUEUE_FAM_TYPE_PRESENT].available_count = qfams[i].queueCount;
            ilog("Selected queue family at index %d for presentation (%d available)", i, qfams[i].queueCount);
        }

        ilog("Queue family ind %d has %d available queues with %#010x capabilities", i, qfams[i].queueCount, qfams[i].queueFlags);
    }
    return ret;
}

int vkr_init_device(const vkr_context *vk,
                    const char *const *layers,
                    u32 layer_count,
                    const char *const *device_extensions,
                    u32 dev_ext_count,
                    vkr_device *dev)
{
    ilog("Creating vk device and queues");
    const vkr_queue_families *qfams = &vk->inst.pdev_info.qfams;

    // Get our highest create ind - to determine the size
    u32 highest_ind = 0;
    for (u32 i = 0; i < VKR_QUEUE_FAM_TYPE_COUNT; ++i) {
        if (qfams->qinfo[i].create_ind > highest_ind) {
            highest_ind = qfams->qinfo[i].create_ind;
        }
    }
    u32 create_size = highest_ind + 1;

    // NOTE: Make sure you init all vulkan structs to zero - undefined behavior including crashes result otherwise
    VkDeviceQueueCreateInfo qinfo[VKR_QUEUE_FAM_TYPE_COUNT]{};
    float qinfo_f[VKR_QUEUE_FAM_TYPE_COUNT][MAX_QUEUE_REQUEST_COUNT]{};

    u32 offsets[VKR_QUEUE_FAM_TYPE_COUNT]{};

    // Here we gather how many queues we want for each queue fam type and populate the queue create infos.
    // Its highly likely that the different queue_fam_types will actually have the same vulkan queue family index -
    // thats fine
    for (int i = 0; i < VKR_QUEUE_FAM_TYPE_COUNT; ++i) {
        const vkr_queue_family_info *cq = &qfams->qinfo[i];
        u32 ind = cq->create_ind;
        qinfo[ind].sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;

        // This handles the case when the present and graphics must be in the same queue in the same family which can
        // happen if a gfx card has only 1 queue family with 1 available queue
        int req_offset = std::min((int)cq->available_count - (int)(qinfo[ind].queueCount + cq->requested_count), 0);
        offsets[i] += req_offset;

        qinfo[ind].queueCount += (cq->requested_count + req_offset);
        qinfo[ind].queueFamilyIndex = cq->index;
        qinfo[ind].pQueuePriorities = qinfo_f[cq->create_ind];
        ilog("Setting qind:%d to queue family index:%d with %d queues requested", ind, qinfo[ind].queueFamilyIndex, qinfo[ind].queueCount);
    }

    // For now we will leave this blank - but probably enable geometry shaders later
    VkPhysicalDeviceFeatures features{};
    ilog("Creating %d queues", create_size);

    VkDeviceCreateInfo create_inf{};
    create_inf.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    create_inf.queueCreateInfoCount = create_size;
    create_inf.pQueueCreateInfos = qinfo;
    create_inf.enabledLayerCount = layer_count;
    create_inf.ppEnabledLayerNames = layers;
    create_inf.pEnabledFeatures = &features;
    create_inf.ppEnabledExtensionNames = device_extensions;
    create_inf.enabledExtensionCount = dev_ext_count;

    int result = vkCreateDevice(vk->inst.pdev_info.hndl, &create_inf, &vk->alloc_cbs, &dev->hndl);
    if (result != VK_SUCCESS) {
        elog("Device creation failed - vk err:%d", result);
        return err_code::VKR_CREATE_DEVICE_FAIL;
    }

    for (int i = 0; i < VKR_QUEUE_FAM_TYPE_COUNT; ++i) {
        arr_resize(&dev->qfams[i].qs, qfams->qinfo[i].requested_count);
        dev->qfams[i].fam_ind = qfams->qinfo[i].index;
        for (u32 qind = 0; qind < qfams->qinfo[i].requested_count; ++qind) {
            u32 adjusted_ind = qind + offsets[i];
            vkGetDeviceQueue(dev->hndl, qfams->qinfo[i].index, adjusted_ind, &dev->qfams[i].qs[qind].hndl);
            ilog("Getting queue %d from queue family %d: %p", adjusted_ind, qfams->qinfo[i].index, dev->qfams[i].qs[qind]);
        }
    }

    VkSemaphoreCreateInfo sem_info{};
    sem_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

    result = vkCreateSemaphore(dev->hndl, &sem_info, &vk->alloc_cbs, &dev->image_avail);
    if (result != VK_SUCCESS) {
        elog("Failed to create image semaphore with vk err code %d", result);
        return err_code::VKR_CREATE_SEMAPHORE_FAIL;
    }

    result = vkCreateSemaphore(dev->hndl, &sem_info, &vk->alloc_cbs, &dev->render_finished);
    if (result != VK_SUCCESS) {
        elog("Failed to create render semaphore with vk err code %d", result);
        return err_code::VKR_CREATE_SEMAPHORE_FAIL;
    }

    VkFenceCreateInfo fence_info{};
    fence_info.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fence_info.flags = VK_FENCE_CREATE_SIGNALED_BIT;

    result = vkCreateFence(dev->hndl, &fence_info, &vk->alloc_cbs, &dev->in_flight);
    if (result != VK_SUCCESS) {
        elog("Failed to create flight fence with vk err code %d", result);
        return err_code::VKR_CREATE_FENCE_FAIL;
    }
    
    ilog("Successfully created vk device and queues");
    return err_code::VKR_NO_ERROR;
}

int vkr_select_best_graphics_physical_device(const vkr_context *vk, vkr_phys_device *dev_info)
{
    u32 count{};
    int ret = vkEnumeratePhysicalDevices(vk->inst.hndl, &count, nullptr);
    if (ret != VK_SUCCESS) {
        elog("Failed to enumerate physical devices (with nullptr) with code %d", ret);
        return err_code::VKR_ENUMERATE_PHYSICAL_DEVICES_FAIL;
    }
    if (count == 0) {
        elog("No physical devices found - cannot continue");
        return err_code::VKR_NO_PHYSICAL_DEVICES;
    }

    // This is used just so we can log which device we select without having to grab the info again
    VkPhysicalDeviceProperties sel_dev{};

    // These are cached so we don't have to retreive the count again
    int high_score = -1;

    ilog("Selecting physical device - found %d physical devices", count);
    auto pdevices = (VkPhysicalDevice *)mem_alloc(sizeof(VkPhysicalDevice) * count, vk->cfg.arenas.command_arena);
    ret = vkEnumeratePhysicalDevices(vk->inst.hndl, &count, pdevices);
    if (ret != VK_SUCCESS) {
        elog("Failed to enumerate physical devices with code %d", ret);
        return err_code::VKR_ENUMERATE_PHYSICAL_DEVICES_FAIL;
    }
    for (u32 i = 0; i < count; ++i) {
        int cur_score = 0;
        vkr_queue_families fams = vkr_get_queue_families(vk, pdevices[i]);

        // We dont want to select a device unless we can present with it.
        if (fams.qinfo[VKR_QUEUE_FAM_TYPE_PRESENT].index == VKR_INVALID || fams.qinfo[VKR_QUEUE_FAM_TYPE_GFX].index == VKR_INVALID) {
            continue;
        }

        // We need at least one supported format and present mode
        u32 format_count{0};
        vkGetPhysicalDeviceSurfaceFormatsKHR(pdevices[i], vk->inst.surface, &format_count, nullptr);
        if (format_count == 0) {
            continue;
        }

        u32 present_mode_count{0};
        vkGetPhysicalDeviceSurfacePresentModesKHR(pdevices[i], vk->inst.surface, &present_mode_count, nullptr);
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
            dev_info->hndl = pdevices[i];
            dev_info->qfams = fams;
            vkr_fill_pdevice_swapchain_support(dev_info->hndl, vk->inst.surface, &dev_info->swap_support);
            high_score = cur_score;
            sel_dev = props;
        }
    }
    ilog("Selected device id:%d  name:%s  type:%s", sel_dev.deviceID, sel_dev.deviceName, vkr_physical_device_type_str(sel_dev.deviceType));
    if (high_score == -1) {
        return err_code::VKR_NO_SUITABLE_PHYSICAL_DEVICE;
    }

    // Fill in the queue index offsets based on the fam index from vulkan - if our queue fams have the same index then
    // the queues coming from the later fams need to have an offset index set
    for (u32 i = 0; i < VKR_QUEUE_FAM_TYPE_COUNT; ++i) {
        fill_queue_offsets_and_create_inds(&dev_info->qfams, i);
    }

    return err_code::VKR_NO_ERROR;
}

void vkr_fill_pdevice_swapchain_support(VkPhysicalDevice pdevice, VkSurfaceKHR surface, vkr_pdevice_swapchain_support *ssup)
{
    ilog("Getting physical device swapchain support");
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

int vkr_init_swapchain(const vkr_context *vk, void *window, vkr_swapchain *sw_info)
{
    ilog("Setting up swapchain");
    arr_init(&sw_info->images, vk->cfg.arenas.persistent_arena);
    arr_init(&sw_info->image_views, vk->cfg.arenas.persistent_arena);

    // I no like typing too much
    auto caps = &vk->inst.pdev_info.swap_support.capabilities;
    auto qfams = &vk->inst.pdev_info.qfams;
    auto formats = &vk->inst.pdev_info.swap_support.formats;
    auto pmodes = &vk->inst.pdev_info.swap_support.present_modes;

    VkSwapchainCreateInfoKHR swap_create{};
    swap_create.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    swap_create.surface = vk->inst.surface;
    swap_create.imageArrayLayers = 1;
    swap_create.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    swap_create.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    swap_create.preTransform = vk->inst.pdev_info.swap_support.capabilities.currentTransform;
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
    if (vkCreateSwapchainKHR(vk->inst.device.hndl, &swap_create, &vk->alloc_cbs, &sw_info->swapchain) != VK_SUCCESS) {
        arr_terminate(&sw_info->images);
        arr_terminate(&sw_info->image_views);
        return err_code::VKR_CREATE_SWAPCHAIN_FAIL;
    }
    sw_info->extent = swap_create.imageExtent;
    sw_info->format = swap_create.imageFormat;
    u32 image_count{};
    VkResult res = vkGetSwapchainImagesKHR(vk->inst.device.hndl, sw_info->swapchain, &image_count, nullptr);
    if (res != VK_SUCCESS) {
        elog("Failed to get swapchain images count with code %d", res);
        arr_terminate(&sw_info->images);
        arr_terminate(&sw_info->image_views);
        return err_code::VKR_GET_SWAPCHAIN_IMAGES_FAIL;
    }

    arr_resize(&sw_info->images, image_count);

    res = vkGetSwapchainImagesKHR(vk->inst.device.hndl, sw_info->swapchain, &image_count, sw_info->images.data);
    if (res != VK_SUCCESS) {
        elog("Failed to get swapchain images with code %d", res);
        return err_code::VKR_GET_SWAPCHAIN_IMAGES_FAIL;
    }

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
        VkResult res = vkCreateImageView(vk->inst.device.hndl, &iview_create, &vk->alloc_cbs, &sw_info->image_views[i]);
        if (res != VK_SUCCESS) {
            elog("Failed to create image view at index %d - vk err code %d", i, res);
            arr_terminate(&sw_info->images);
            arr_terminate(&sw_info->image_views);
            return err_code::VKR_CREATE_IMAGE_VIEW_FAIL;
        }
    }
    ilog("Successfully set up swapchain");
    return err_code::VKR_NO_ERROR;
}

int vkr_add_cmd_buf(const vkr_device *device, vkr_command_pool *pool)
{
    ilog("Adding command buffer");
    vkr_command_buffer buf{};
    VkCommandBufferAllocateInfo info{};
    info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    info.commandPool = pool->hndl;
    info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    info.commandBufferCount = 1;
    int err = vkAllocateCommandBuffers(device->hndl, &info, &buf.hndl);
    if (err != VK_SUCCESS) {
        elog("Failed to create command buffer with code %d", err);
        return err_code::VKR_CREATE_COMMAND_BUFFER_FAIL;
    }
    arr_push_back(&pool->buffers, buf);
    ilog("Successfully added command buffer");
    return err_code::VKR_NO_ERROR;
}

int vkr_init_cmd_pool(const vkr_context *vk, u32 fam_ind, vkr_command_pool *cpool)
{
    ilog("Initializing command pool");
    VkCommandPoolCreateInfo pool_info{};
    pool_info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    pool_info.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    pool_info.queueFamilyIndex = fam_ind;
    int err = vkCreateCommandPool(vk->inst.device.hndl, &pool_info, &vk->alloc_cbs, &cpool->hndl);
    if (err != VK_SUCCESS) {
        elog("Failed creating vulkan command pool with code %d", err);
        return err_code::VKR_CREATE_COMMAND_POOL_FAIL;
    }
    ilog("Successfully initialized command pool");
    return err_code::VKR_NO_ERROR;
}

sizet vkr_add_cmd_pool(vkr_device_queue_fam_info *qfam, const vkr_command_pool *cpool)
{
    ilog("Adding command pool to qfamily");
    sizet ind = qfam->cmd_pools.size;
    arr_push_back(&qfam->cmd_pools, *cpool);
    return ind;
}

void vkr_terminate_cmd_pool(const vkr_context *vk, u32 fam_ind, vkr_command_pool *cpool)
{
    ilog("Terminating command pool");
    vkDestroyCommandPool(vk->inst.device.hndl, cpool->hndl, &vk->alloc_cbs);
}

int vkr_init_shader_module(const vkr_context *vk, const byte_array *code, VkShaderModule *module)
{
    ilog("Initializing shader module");
    VkShaderModuleCreateInfo create_info{};
    create_info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    ilog("Code size:%d", code->size);
    create_info.codeSize = code->size;
    create_info.pCode = (const u32 *)code->data;
    if (vkCreateShaderModule(vk->inst.device.hndl, &create_info, &vk->alloc_cbs, module) != VK_SUCCESS) {
        return err_code::VKR_CREATE_SHADER_MODULE_FAIL;
    }
    ilog("Successfully initialized shader module");
    return err_code::VKR_NO_ERROR;
}
void vkr_terminate_shader_module(const vkr_context *vk, VkShaderModule module)
{
    ilog("Terminating shader module");
    vkDestroyShaderModule(vk->inst.device.hndl, module, &vk->alloc_cbs);
}

int vkr_init_render_pass(const vkr_context *vk, vkr_rpass *rpass)
{
    ilog("Initializing render pass");
    // Create Render Pass
    VkAttachmentDescription col_att{};
    col_att.format = vk->inst.device.sw_info.format;
    col_att.samples = VK_SAMPLE_COUNT_1_BIT;
    col_att.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    col_att.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    col_att.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    col_att.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    col_att.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    col_att.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

    VkAttachmentReference col_att_ref{};
    col_att_ref.attachment = 0;
    col_att_ref.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    // Create just one subpass for now
    VkSubpassDescription subpass{};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments = &col_att_ref;

    VkSubpassDependency dependency{};
    dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
    dependency.dstSubpass = 0;    
    dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependency.srcAccessMask = 0;
    dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

    VkRenderPassCreateInfo rpass_info{};
    rpass_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    rpass_info.attachmentCount = 1;
    rpass_info.pAttachments = &col_att;
    rpass_info.subpassCount = 1;
    rpass_info.pSubpasses = &subpass;
    rpass_info.dependencyCount = 1;
    rpass_info.pDependencies = &dependency;
    
    if (vkCreateRenderPass(vk->inst.device.hndl, &rpass_info, &vk->alloc_cbs, &rpass->hndl) != VK_SUCCESS) {
        elog("Failed to create render pass");
        return err_code::VKR_CREATE_RENDER_PASS_FAIL;
    }
    ilog("Successfully initialized render pass");
    return err_code::VKR_NO_ERROR;
}

sizet vkr_add_render_pass(vkr_device *device, const vkr_rpass *rpass)
{
    ilog("Adding render_pass to device");
    sizet ind = device->render_passes.size;
    arr_push_back(&device->render_passes, *rpass);
    return ind;
}

void vkr_terminate_render_pass(const vkr_context *vk, const vkr_rpass *rpass)
{
    ilog("Terminating render pass");
    vkDestroyRenderPass(vk->inst.device.hndl, rpass->hndl, &vk->alloc_cbs);
}

int vkr_init_pipeline(const vkr_context *vk, const vkr_pipeline_cfg *cfg, vkr_pipeline *pipe_info)
{
    ilog("Initializing pipeline");
    // Take care of the shaders
    VkShaderModule vert, frag;
    int err = vkr_init_shader_module(vk, &cfg->vert_shader_data, &vert);
    if (err != err_code::VKR_NO_ERROR) {
        elog("Could not initialized vert shader module ");
        return err;
    }
    err = vkr_init_shader_module(vk, &cfg->frag_shader_data, &frag);
    if (err != err_code::VKR_NO_ERROR) {
        elog("Could not initialized frag shader module ");
        vkr_terminate_shader_module(vk, vert);
        return err;
    }
    pipe_info->rpass = *cfg->rpass;

    VkPipelineShaderStageCreateInfo vert_stage_info{};
    vert_stage_info.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    vert_stage_info.stage = VK_SHADER_STAGE_VERTEX_BIT;
    vert_stage_info.module = vert;
    vert_stage_info.pName = "main";

    VkPipelineShaderStageCreateInfo frag_stage_info{};
    frag_stage_info.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    frag_stage_info.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    frag_stage_info.module = frag;
    frag_stage_info.pName = "main";

    VkPipelineShaderStageCreateInfo shader_stages[] = {vert_stage_info, frag_stage_info};

    // Dynamic state
    VkDynamicState states[] = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
    VkPipelineDynamicStateCreateInfo dyn_state{};
    dyn_state.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dyn_state.dynamicStateCount = 2;
    dyn_state.pDynamicStates = states;

    // Vertex Input
    VkPipelineVertexInputStateCreateInfo vertex_input_info{};
    vertex_input_info.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertex_input_info.vertexBindingDescriptionCount = 0;
    vertex_input_info.pVertexBindingDescriptions = nullptr; // Optional
    vertex_input_info.vertexAttributeDescriptionCount = 0;
    vertex_input_info.pVertexAttributeDescriptions = nullptr; // Optional

    // Input assembly
    VkPipelineInputAssemblyStateCreateInfo input_assembly{};
    input_assembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    input_assembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    input_assembly.primitiveRestartEnable = VK_FALSE;

    // Viewport and Scissors
    VkViewport viewport{};
    viewport.x = 0.0f;
    viewport.y = 0.0f;
    viewport.width = (float)vk->inst.device.sw_info.extent.width;
    viewport.height = (float)vk->inst.device.sw_info.extent.height;
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;

    VkRect2D scissor{};
    scissor.offset = {0, 0};
    scissor.extent = vk->inst.device.sw_info.extent;

    VkPipelineViewportStateCreateInfo viewport_state{};
    viewport_state.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewport_state.viewportCount = 1;
    viewport_state.pViewports = &viewport;
    viewport_state.scissorCount = 1;
    viewport_state.pScissors = &scissor;

    // Rasterizer
    VkPipelineRasterizationStateCreateInfo rstr{};
    rstr.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rstr.depthClampEnable = VK_FALSE;
    rstr.rasterizerDiscardEnable = VK_FALSE;
    rstr.polygonMode = VK_POLYGON_MODE_FILL;
    rstr.lineWidth = 1.0f;
    rstr.cullMode = VK_CULL_MODE_BACK_BIT;
    rstr.frontFace = VK_FRONT_FACE_CLOCKWISE;
    rstr.depthBiasEnable = VK_FALSE;
    rstr.depthBiasConstantFactor = 0.0f;
    rstr.depthBiasClamp = 0.0f;
    rstr.depthBiasSlopeFactor = 0.0f;

    // Multisampling
    VkPipelineMultisampleStateCreateInfo multisampling{};
    multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisampling.sampleShadingEnable = VK_FALSE;
    multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
    multisampling.minSampleShading = 1.0f;          // Optional
    multisampling.pSampleMask = nullptr;            // Optional
    multisampling.alphaToCoverageEnable = VK_FALSE; // Optional
    multisampling.alphaToOneEnable = VK_FALSE;      // Optional

    // Depth/Stencil - Skip for now

    // Color blending
    VkPipelineColorBlendAttachmentState col_blnd_att{};
    col_blnd_att.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    col_blnd_att.blendEnable = VK_FALSE;
    col_blnd_att.srcColorBlendFactor = VK_BLEND_FACTOR_ONE;  // Optional
    col_blnd_att.dstColorBlendFactor = VK_BLEND_FACTOR_ZERO; // Optional
    col_blnd_att.colorBlendOp = VK_BLEND_OP_ADD;             // Optional
    col_blnd_att.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;  // Optional
    col_blnd_att.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO; // Optional
    col_blnd_att.alphaBlendOp = VK_BLEND_OP_ADD;             // Optional

    VkPipelineColorBlendStateCreateInfo col_blend_state{};
    col_blend_state.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    col_blend_state.logicOpEnable = VK_FALSE;
    col_blend_state.logicOp = VK_LOGIC_OP_COPY; // Optional
    col_blend_state.attachmentCount = 1;
    col_blend_state.pAttachments = &col_blnd_att;
    col_blend_state.blendConstants[0] = 0.0f; // Optional
    col_blend_state.blendConstants[1] = 0.0f; // Optional
    col_blend_state.blendConstants[2] = 0.0f; // Optional
    col_blend_state.blendConstants[3] = 0.0f; // Optional

    // Pipeline layout - where we would bind uniforms and such
    VkPipelineLayoutCreateInfo pipeline_layout_create_inf{};
    pipeline_layout_create_inf.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipeline_layout_create_inf.setLayoutCount = 0;            // Optional
    pipeline_layout_create_inf.pSetLayouts = nullptr;         // Optional
    pipeline_layout_create_inf.pushConstantRangeCount = 0;    // Optional
    pipeline_layout_create_inf.pPushConstantRanges = nullptr; // Optional

    if (vkCreatePipelineLayout(vk->inst.device.hndl, &pipeline_layout_create_inf, &vk->alloc_cbs, &pipe_info->layout_hndl) != VK_SUCCESS) {
        elog("Failed to create pileline layout");
        vkr_terminate_shader_module(vk, vert);
        vkr_terminate_shader_module(vk, frag);
        return err_code::VKR_CREATE_PIPELINE_LAYOUT_FAIL;
    }

    VkGraphicsPipelineCreateInfo pipeline_info{};
    pipeline_info.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipeline_info.stageCount = 2;
    pipeline_info.pStages = shader_stages;
    pipeline_info.pVertexInputState = &vertex_input_info;
    pipeline_info.pInputAssemblyState = &input_assembly;
    pipeline_info.pViewportState = &viewport_state;
    pipeline_info.pRasterizationState = &rstr;
    pipeline_info.pMultisampleState = &multisampling;
    pipeline_info.pDepthStencilState = nullptr; // Optional
    pipeline_info.pColorBlendState = &col_blend_state;
    pipeline_info.pDynamicState = &dyn_state;
    pipeline_info.layout = pipe_info->layout_hndl;
    pipeline_info.renderPass = cfg->rpass->hndl;
    pipeline_info.subpass = 0;

    // Optional
    pipeline_info.basePipelineHandle = VK_NULL_HANDLE;
    pipeline_info.basePipelineIndex = -1;
    int err_ret = vkCreateGraphicsPipelines(vk->inst.device.hndl, VK_NULL_HANDLE, 1, &pipeline_info, &vk->alloc_cbs, &pipe_info->hndl);
    if (err_ret != VK_SUCCESS) {
        vkr_terminate_shader_module(vk, vert);
        vkr_terminate_shader_module(vk, frag);
        vkDestroyPipelineLayout(vk->inst.device.hndl, pipe_info->layout_hndl, &vk->alloc_cbs);
        err_ret = err_code::VKR_CREATE_PIPELINE_FAIL;
    }
    vkr_terminate_shader_module(vk, vert);
    vkr_terminate_shader_module(vk, frag);
    ilog("Successfully initialized pipeline");
    err_ret = err_code::VKR_NO_ERROR;
    return err_ret;
}

sizet vkr_add_pipeline(vkr_device *device, const vkr_pipeline *pipeline)
{
    ilog("Adding pipeline to device");
    sizet ind = device->pipelines.size;
    arr_push_back(&device->pipelines, *pipeline);
    return ind;
}

void vkr_terminate_pipeline(const vkr_context *vk, const vkr_pipeline *pipe_info)
{
    ilog("Terminating pipeline");
    vkDestroyPipeline(vk->inst.device.hndl, pipe_info->hndl, &vk->alloc_cbs);
    vkDestroyPipelineLayout(vk->inst.device.hndl, pipe_info->layout_hndl, &vk->alloc_cbs);
}

int vkr_init_framebuffer(const vkr_context *vk, const vkr_framebuffer_cfg *cfg, vkr_framebuffer *fb)
{
    ilog("Initializing framebuffer");
    fb->size = cfg->size;
    fb->rpass = *cfg->rpass;
    
    VkFramebufferCreateInfo create_info{};
    create_info.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
    create_info.renderPass = cfg->rpass->hndl;
    create_info.pAttachments = cfg->attachments;
    create_info.attachmentCount = cfg->attachment_count;
    create_info.width = cfg->size.x;
    create_info.height = cfg->size.y;
    create_info.layers = cfg->layers;
    int res = vkCreateFramebuffer(vk->inst.device.hndl, &create_info, &vk->alloc_cbs, &fb->hndl);
    if (res != VK_SUCCESS) {
        elog("Failed to create framebuffer with vk err %d", res);
        return err_code::VKR_CREATE_FRAMEBUFFER_FAIL;
    }
    ilog("Successfully initialized framebuffer");
    return err_code::VKR_NO_ERROR;
}

sizet vkr_add_framebuffer(vkr_device *device, const vkr_framebuffer *fb)
{
    ilog("Adding framebuffer to device");
    sizet ind = device->framebuffers.size;
    arr_push_back(&device->framebuffers, *fb);
    return ind;
}

void vkr_terminate_framebuffer(const vkr_context *vk, const vkr_framebuffer *fb)
{
    ilog("Terminating framebuffer");
    vkDestroyFramebuffer(vk->inst.device.hndl, fb->hndl, &vk->alloc_cbs);
}

// Initialize surface in the vk_context from the window - the instance must have been created already
int vkr_init_surface(const vkr_context *vk, void *window, VkSurfaceKHR *surface)
{
    ilog("Initializing window surface");
    assert(window);
    // Create surface
    int ret = glfwCreateWindowSurface(vk->inst.hndl, (GLFWwindow *)window, &vk->alloc_cbs, surface);
    if (ret != VK_SUCCESS) {
        elog("Failed to create surface with err code %d", ret);
        return err_code::VKR_CREATE_SURFACE_FAIL;
    }
    else {
        ilog("Successfully initialized window surface");
        return err_code::VKR_NO_ERROR;
    }
}

void vkr_terminate_surface(const vkr_context *vk, VkSurfaceKHR surface)
{
    ilog("Terminating window surface");
    vkDestroySurfaceKHR(vk->inst.hndl, surface, &vk->alloc_cbs);
}

int vkr_init(const vkr_cfg *cfg, vkr_context *vk)
{
    ilog("Initializing vulkan");
    assert(cfg);
    vk->cfg = *cfg;
    if (!cfg->arenas.command_arena) {
        vk->cfg.arenas.command_arena = mem_global_frame_lin_arena();
        ilog("Using global frame linear arena %p", vk->cfg.arenas.command_arena);
    }

    if (!cfg->arenas.persistent_arena) {
        vk->cfg.arenas.persistent_arena = mem_global_arena();
        ilog("Using global persistent arena %p", vk->cfg.arenas.persistent_arena);
    }

    vk->alloc_cbs.pUserData = &vk->cfg.arenas;
    vk->alloc_cbs.pfnAllocation = vk_alloc;
    vk->alloc_cbs.pfnFree = vk_free;
    vk->alloc_cbs.pfnReallocation = vk_realloc;

    int code = vkr_init_instance(vk, &vk->inst);
    if (code != err_code::VKR_NO_ERROR) {
        return code;
    }

    if (cfg->window) {
        code = vkr_init_surface(vk, cfg->window, &vk->inst.surface);
        if (code != err_code::VKR_NO_ERROR) {
            vkr_terminate(vk);
            return code;
        }
    }

    vkr_init_pdevice_swapchain_support(&vk->inst.pdev_info.swap_support, vk->cfg.arenas.persistent_arena);

    // Select the physical device
    code = vkr_select_best_graphics_physical_device(vk, &vk->inst.pdev_info);
    if (code != err_code::VKR_NO_ERROR) {
        vkr_terminate(vk);
        return code;
    }

    // Log out the device extensions
    vkr_enumerate_device_extensions(&vk->inst.pdev_info, cfg->device_extension_names, cfg->device_extension_count, &vk->cfg.arenas);
    code = vkr_init_device(vk,
                           cfg->validation_layer_names,
                           cfg->validation_layer_count,
                           cfg->device_extension_names,
                           cfg->device_extension_count,
                           &vk->inst.device);
    if (code != err_code::VKR_NO_ERROR) {
        vkr_terminate(vk);
        return code;
    }

    code = vkr_init_swapchain(vk, cfg->window, &vk->inst.device.sw_info);
    if (code != err_code::VKR_NO_ERROR) {
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

void vkr_terminate_swapchain(const vkr_context *vk, vkr_swapchain *sw_info)
{
    ilog("Terminating swapchain");
    for (int i = 0; i < sw_info->image_views.size; ++i) {
        vkDestroyImageView(vk->inst.device.hndl, sw_info->image_views[i], &vk->alloc_cbs);
    }
    vkDestroySwapchainKHR(vk->inst.device.hndl, sw_info->swapchain, &vk->alloc_cbs);
    arr_terminate(&sw_info->images);
    arr_terminate(&sw_info->image_views);
}

void vkr_terminate_device(const vkr_context *vk, vkr_device *dev)
{
    ilog("Terminating vkr device");
    for (int i = 0; i < dev->pipelines.size; ++i) {
        vkr_terminate_pipeline(vk, &dev->pipelines[i]);
    }
    for (int i = 0; i < dev->framebuffers.size; ++i) {
        vkr_terminate_framebuffer(vk, &dev->framebuffers[i]);
    }
    for (int i = 0; i < dev->render_passes.size; ++i) {
        vkr_terminate_render_pass(vk, &dev->render_passes[i]);
    }
    arr_terminate(&dev->render_passes);
    arr_terminate(&dev->pipelines);
    arr_terminate(&dev->framebuffers);

    for (sizet qfam_i = 0; qfam_i < VKR_QUEUE_FAM_TYPE_COUNT; ++qfam_i) {
        auto cur_fam = &dev->qfams[qfam_i];
        arr_terminate(&cur_fam->qs);
        for (sizet pool_i = 0; pool_i < cur_fam->cmd_pools.size; ++pool_i) {
            auto cur_pool = &cur_fam->cmd_pools[pool_i];
            arr_terminate(&cur_pool->buffers);
            vkr_terminate_cmd_pool(vk, cur_fam->fam_ind, cur_pool);
        }
        arr_terminate(&cur_fam->cmd_pools);
    }

    vkr_terminate_swapchain(vk, &dev->sw_info);

    vkDestroyFence(dev->hndl, dev->in_flight, &vk->alloc_cbs);
    vkDestroySemaphore(dev->hndl, dev->render_finished, &vk->alloc_cbs);
    vkDestroySemaphore(dev->hndl, dev->image_avail, &vk->alloc_cbs);
    vkDestroyDevice(dev->hndl, &vk->alloc_cbs);
}

void vkr_terminate_instance(const vkr_context *vk, vkr_instance *inst)
{
    ilog("Terminating vkr instance");
    vkr_terminate_device(vk, &inst->device);
    vkr_terminate_pdevice_swapchain_support(&inst->pdev_info.swap_support);
    vkr_terminate_surface(vk, inst->surface);
    inst->ext_funcs.destroy_debug_utils_messenger(inst->hndl, inst->dbg_messenger, &vk->alloc_cbs);

    // Destroying the instance calls more vk alloc calls
    vkDestroyInstance(inst->hndl, &vk->alloc_cbs);
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
    vkr_terminate_instance(vk, &vk->inst);
    for (int i = 0; i < MEM_ALLOC_TYPE_COUNT; ++i) {
        log_mem_stats(alloc_scope_str(i), &vk->cfg.arenas.stats[i]);
    }
}

int vkr_begin_cmd_buf(const vkr_command_buffer *buf)
{
    VkCommandBufferBeginInfo info{};
    info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    info.flags = 0;                  // Optional
    info.pInheritanceInfo = nullptr; // Optional

    int err = vkBeginCommandBuffer(buf->hndl, &info);
    if (err != VK_SUCCESS) {
        elog("Failed to begin command buffer with Vk code %d", err);
        return err_code::VKR_BEGIN_COMMAND_BUFFER_FAIL;
    }
    return err_code::VKR_NO_ERROR;
}

int vkr_end_cmd_buf(const vkr_command_buffer *buf)
{
    int err = vkEndCommandBuffer(buf->hndl);
    if (err != VK_SUCCESS) {
        elog("Failed to end command buffer with vk code %d", err);
        return err_code::VKR_END_COMMAND_BUFFER_FAIL;
    }
    return err_code::VKR_NO_ERROR;
}

void vkr_cmd_begin_rpass(const vkr_command_buffer *cmd_buf, vkr_framebuffer *fb)
{
    VkRenderPassBeginInfo info{};
    info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    info.renderPass = fb->rpass.hndl;
    info.framebuffer = fb->hndl;
    info.renderArea.extent = {fb->size.w, fb->size.h};

    // TODO: Figure out what all these different things are with regards to screen size...
    // Viewport.. ImageView extent.. framebuffer size.. render area.. 
    info.renderArea.offset = {0, 0};

    // TODO: Move this in to the render pass - the attachment clearing values
    VkClearValue v = {{{0.0f, 0.0f, 1.0f, 1.0}}};
    info.clearValueCount = 1;
    info.pClearValues = &v;
    
    vkCmdBeginRenderPass(cmd_buf->hndl, &info, VK_SUBPASS_CONTENTS_INLINE);        
}

void vkr_cmd_end_rpass(const vkr_command_buffer *cmd_buf)
{
    vkCmdEndRenderPass(cmd_buf->hndl);
}


void vkr_setup_default_rendering(vkr_context *vk)
{
    ilog("Setting up default rendering...");
    vkr_rpass rpass{};
    vkr_init_render_pass(vk, &rpass);
    vkr_add_render_pass(&vk->inst.device, &rpass);

    vkr_pipeline_cfg info{};

    platform_file_err_desc err{};
    const char *frag_fname = "shaders/triangle.frag.spv";
    const char *vert_fname = "shaders/triangle.vert.spv";

    platform_read_file(frag_fname, &info.frag_shader_data, 0, &err);
    if (err.code != err_code::PLATFORM_NO_ERROR) {
        wlog("Error reading file %s from disk (code %d): %s", frag_fname, err.code, err.str);
        return;
    }

    err = {};
    platform_read_file(vert_fname, &info.vert_shader_data, 0, &err);
    if (err.code != err_code::PLATFORM_NO_ERROR) {
        wlog("Error reading file %s from disk (code %d): %s", vert_fname, err.code, err.str);
        return;
    }

    info.rpass = &rpass;
    vkr_pipeline pinfo{};
    vkr_init_pipeline(vk, &info, &pinfo);
    vkr_add_pipeline(&vk->inst.device, &pinfo);

    for (int i = 0; i < vk->inst.device.sw_info.image_views.size; ++i) {
        vkr_framebuffer fb{};
        vkr_framebuffer_cfg cfg{};
        cfg.size = {vk->inst.device.sw_info.extent.width, vk->inst.device.sw_info.extent.height};
        cfg.rpass = &rpass;
        cfg.attachment_count = 1;
        cfg.attachments = &vk->inst.device.sw_info.image_views[i];
        vkr_init_framebuffer(vk, &cfg, &fb);
        vkr_add_framebuffer(&vk->inst.device, &fb);
    }

    vkr_command_pool pool{};
    vkr_device_queue_fam_info *qfam = &vk->inst.device.qfams[VKR_QUEUE_FAM_TYPE_GFX];
    if (vkr_init_cmd_pool(vk, qfam->fam_ind, &pool) != err_code::VKR_NO_ERROR) {
        return;
    }
    int pool_ind = vkr_add_cmd_pool(qfam, &pool);
    auto added_pool = &vk->inst.device.qfams[VKR_QUEUE_FAM_TYPE_GFX].cmd_pools[pool_ind];
    if (vkr_add_cmd_buf(&vk->inst.device, added_pool) != err_code::VKR_NO_ERROR) {
        return;
    }    
}

} // namespace nslib
