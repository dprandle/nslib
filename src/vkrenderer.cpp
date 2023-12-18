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

#define PRINT_MEM_GPU_ALLOC true

namespace nslib
{

// If this is less than 16 bytes we can get crashes... not totally sure why but probably has to do with... aliasing...?
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

intern void vk_gpu_alloc_cb(VmaAllocator allocator, uint32_t memory_type, VkDeviceMemory memory, VkDeviceSize size, void *devp)
{
    vkr_device *dev = (vkr_device *)devp;
    dev->vma_alloc.total_size += size;
#if PRINT_MEM_GPU_ALLOC
    dlog("Allocator %p with mem type %d allocated ptr %p of size %lu - new total size %lu",
         allocator,
         memory_type,
         memory,
         size,
         dev->vma_alloc.total_size);
#endif
}

intern void vk_gpu_free_cb(VmaAllocator allocator, uint32_t memory_type, VkDeviceMemory memory, VkDeviceSize size, void *devp)
{
    vkr_device *dev = (vkr_device *)devp;
    dev->vma_alloc.total_size -= size;
#if PRINT_MEM_GPU_ALLOC
    dlog("Allocator %p with mem type %d freeing ptr %p of size %lu - new total size %lu",
         allocator,
         memory_type,
         memory,
         size,
         dev->vma_alloc.total_size);
#endif
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
        dlog("hs:%lu, header_addr:%p ptr:%p requested_size:%lu alignment:%lu scope:%s used_before:%lu alloc:%lu used_after:%lu",
             header_size,
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
        dlog("hs:%lu, header_addr:%p ptr:%p requested_size:%lu scope:%s used_before:%lu dealloc:%lu used_after:%lu",
             header_size,
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
    
    // TODO: Need to make the ability to pass in flags on creatnig instance - mac has extra layers required
    //create_inf.flags = VK_INSTANCE_CREATE_ENUMERATE_PORTABILITY_BIT_KHR;

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

int vkr_init_device(vkr_device *dev,
                    const vkr_context *vk,
                    const char *const *layers,
                    u32 layer_count,
                    const char *const *device_extensions,
                    u32 dev_ext_count)
{
    arr_init(&dev->render_passes, vk->cfg.arenas.persistent_arena);
    arr_init(&dev->framebuffers, vk->cfg.arenas.persistent_arena);
    arr_init(&dev->pipelines, vk->cfg.arenas.persistent_arena);
    arr_init(&dev->buffers, vk->cfg.arenas.persistent_arena);

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

    // Create command pools for each family
    for (int i = 0; i < VKR_QUEUE_FAM_TYPE_COUNT; ++i) {
        arr_init(&dev->qfams[i].qs, vk->cfg.arenas.persistent_arena);
        arr_init(&dev->qfams[i].cmd_pools, vk->cfg.arenas.persistent_arena);

        arr_resize(&dev->qfams[i].qs, qfams->qinfo[i].requested_count);
        dev->qfams[i].fam_ind = qfams->qinfo[i].index;
        for (u32 qind = 0; qind < qfams->qinfo[i].requested_count; ++qind) {
            u32 adjusted_ind = qind + offsets[i];
            vkGetDeviceQueue(dev->hndl, qfams->qinfo[i].index, adjusted_ind, &dev->qfams[i].qs[qind].hndl);
            ilog("Getting queue %d from queue family %d: %p", adjusted_ind, qfams->qinfo[i].index, dev->qfams[i].qs[qind]);
        }

        vkr_command_pool qpool{};
        if (vkr_init_cmd_pool(vk, dev->qfams[i].fam_ind, VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT, &qpool) == err_code::VKR_NO_ERROR) {
            u32 dpool = vkr_add_cmd_pool(&dev->qfams[i], qpool);
            dev->qfams[i].default_pool = dpool;
        }

        vkr_command_pool qpool_transient{};
        if (vkr_init_cmd_pool(vk, dev->qfams[i].fam_ind, VK_COMMAND_POOL_CREATE_TRANSIENT_BIT, &qpool_transient) == err_code::VKR_NO_ERROR) {
            u32 pool_transient_ind = vkr_add_cmd_pool(&dev->qfams[i], qpool_transient);
            dev->qfams[i].transient_pool = pool_transient_ind;
        }
    }

    // Create our rframes command buffers
    auto gfx_fam = &dev->qfams[VKR_QUEUE_FAM_TYPE_GFX];
    auto buf_res = vkr_add_cmd_bufs(&gfx_fam->cmd_pools[gfx_fam->default_pool], vk, VKR_RENDER_FRAME_COUNT);
    if (buf_res.err_code != err_code::VKR_NO_ERROR) {
        vkr_terminate_device(dev, vk);
        return buf_res.err_code;
    }

    // Create frame synchronization objects - start all fences as signalled already
    for (u32 framei = 0; framei < VKR_RENDER_FRAME_COUNT; ++framei) {
        VkSemaphoreCreateInfo sem_info{};
        sem_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

        result = vkCreateSemaphore(dev->hndl, &sem_info, &vk->alloc_cbs, &dev->rframes[framei].image_avail);
        if (result != VK_SUCCESS) {
            elog("Failed to create image semaphore for frame %d with vk err code %d", framei, result);
            vkr_terminate_device(dev, vk);
            return err_code::VKR_CREATE_SEMAPHORE_FAIL;
        }

        result = vkCreateSemaphore(dev->hndl, &sem_info, &vk->alloc_cbs, &dev->rframes[framei].render_finished);
        if (result != VK_SUCCESS) {
            elog("Failed to create render semaphore for frame %d with vk err code %d", framei, result);
            vkr_terminate_device(dev, vk);
            return err_code::VKR_CREATE_SEMAPHORE_FAIL;
        }

        VkFenceCreateInfo fence_info{};
        fence_info.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
        fence_info.flags = VK_FENCE_CREATE_SIGNALED_BIT;

        result = vkCreateFence(dev->hndl, &fence_info, &vk->alloc_cbs, &dev->rframes[framei].in_flight);
        if (result != VK_SUCCESS) {
            elog("Failed to create flight fence for frame %d with vk err code %d", framei, result);
            vkr_terminate_device(dev, vk);
            return err_code::VKR_CREATE_FENCE_FAIL;
        }

        // Assign the command buffer created earlier to this frame
        dev->rframes[framei].cmd_buf_ind = {VKR_QUEUE_FAM_TYPE_GFX, gfx_fam->default_pool, buf_res.begin + framei};

        // Get a count of the number of descriptors we are making avaialable for each desc type
        result = vkr_init_descriptor_pool(&dev->rframes[framei].desc_pool, vk, vk->cfg.max_desc_sets_per_pool);
        if (result != VK_SUCCESS) {
            elog("Failed to create descriptor pool for frame %d - aborting init", framei);
            vkr_terminate_device(dev, vk);
            return result;
        }
    }
    ilog("Successfully created vk device and queues");

    VmaDeviceMemoryCallbacks cb{};
    cb.pUserData = dev;
    cb.pfnAllocate = vk_gpu_alloc_cb;
    cb.pfnFree = vk_gpu_free_cb;

    // Create the vk mem allocator
    VmaAllocatorCreateInfo cr_info{};
    cr_info.device = dev->hndl;
    cr_info.physicalDevice = vk->inst.pdev_info.hndl;
    cr_info.instance = vk->inst.hndl;
    cr_info.pDeviceMemoryCallbacks = &cb;
    cr_info.pAllocationCallbacks = &vk->alloc_cbs;
    cr_info.vulkanApiVersion = VK_API_VERSION_1_3;
    cr_info.preferredLargeHeapBlockSize = 0; // This defaults to 256 MB
    vmaCreateAllocator(&cr_info, &dev->vma_alloc.hndl);

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

    // Fill mem properties
    vkGetPhysicalDeviceMemoryProperties(dev_info->hndl, &dev_info->mem_properties);

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

int vkr_init_swapchain(vkr_swapchain *sw_info, const vkr_context *vk, void *window)
{
    ilog("Setting up swapchain");
    arr_init(&sw_info->image_views, vk->cfg.arenas.persistent_arena);
    arr_init(&sw_info->images, vk->cfg.arenas.persistent_arena);

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
    // swap_create.imageExtent = caps->currentExtent;
    // if (caps->currentExtent.width == VKR_INVALID) {
    int width, height;
    glfwGetFramebufferSize((GLFWwindow *)window, &width, &height);
    swap_create.imageExtent = {(u32)width, (u32)height};
    // swap_create.imageExtent.width = std::clamp(swap_create.imageExtent.width, caps->minImageExtent.width, caps->maxImageExtent.width);
    // swap_create.imageExtent.height = std::clamp(swap_create.imageExtent.height, caps->minImageExtent.height,
    // caps->maxImageExtent.height);
    ilog("Should be setting extent to {%d %d}", swap_create.imageExtent.width, swap_create.imageExtent.height);
    //    }

    // Create this baby boo
    if (vkCreateSwapchainKHR(vk->inst.device.hndl, &swap_create, &vk->alloc_cbs, &sw_info->swapchain) != VK_SUCCESS) {
        arr_terminate(&sw_info->image_views);
        arr_terminate(&sw_info->images);
        return err_code::VKR_CREATE_SWAPCHAIN_FAIL;
    }
    sw_info->extent = swap_create.imageExtent;
    sw_info->format = swap_create.imageFormat;
    u32 image_count{};
    VkResult res = vkGetSwapchainImagesKHR(vk->inst.device.hndl, sw_info->swapchain, &image_count, nullptr);
    if (res != VK_SUCCESS) {
        elog("Failed to get swapchain images count with code %d", res);
        arr_terminate(&sw_info->image_views);
        arr_terminate(&sw_info->images);
        return err_code::VKR_GET_SWAPCHAIN_IMAGES_FAIL;
    }

    arr_resize(&sw_info->images, image_count);
    res = vkGetSwapchainImagesKHR(vk->inst.device.hndl, sw_info->swapchain, &image_count, sw_info->images.data);
    if (res != VK_SUCCESS) {
        elog("Failed to get swapchain images with code %d", res);
        return err_code::VKR_GET_SWAPCHAIN_IMAGES_FAIL;
    }

    arr_resize(&sw_info->image_views, image_count);
    for (int i = 0; i < sw_info->image_views.size; ++i) {
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

void vkr_recreate_swapchain(vkr_instance *inst, const vkr_context *vk, void *window, sizet rpass_ind)
{
    vkDeviceWaitIdle(inst->device.hndl);
    vkr_terminate_swapchain_framebuffers(&inst->device, vk);
    vkr_terminate_swapchain(&inst->device.swapchain, vk);
    vkr_terminate_surface(vk, inst->surface);
    vkr_init_surface(vk, window, &inst->surface);
    vkr_init_swapchain(&inst->device.swapchain, vk, window);
    vkr_init_swapchain_framebuffers(&inst->device, vk, &inst->device.render_passes[rpass_ind], nullptr);
}

vkr_add_result vkr_add_cmd_bufs(vkr_command_pool *pool, const vkr_context *vk, sizet count)
{
    ilog("Adding %lu command buffer/s", count);
    vkr_add_result ret{};
    array<VkCommandBuffer> hndls;
    arr_init(&hndls, vk->cfg.arenas.command_arena);
    arr_resize(&hndls, count);

    VkCommandBufferAllocateInfo info{};
    info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    info.commandPool = pool->hndl;
    info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    info.commandBufferCount = count;
    int err = vkAllocateCommandBuffers(vk->inst.device.hndl, &info, hndls.data);
    if (err != VK_SUCCESS) {
        elog("Failed to create command buffer/s with code %d", err);
        arr_terminate(&hndls);
        ret.err_code = err_code::VKR_CREATE_COMMAND_BUFFER_FAIL;
        return ret;
    }
    ret.begin = pool->buffers.size;
    ret.end = ret.begin + count;
    arr_resize(&pool->buffers, ret.end);
    for (sizet i = 0; i < count; ++i) {
        pool->buffers[ret.begin + i].hndl = hndls[i];
        ilog("Setting cmd buffer at index %d in pool %p to hndl %lu", ret.begin + i, pool, hndls[i]);
    }
    ilog("Successfully added command buffer/s");
    arr_terminate(&hndls);
    return ret;
}

void vkr_remove_cmd_bufs(vkr_command_pool *pool, const vkr_context *vk, sizet ind, sizet count)
{
    ilog("Removing %lu command buffers starting at index %lu", count, ind);
    array<VkCommandBuffer> hndls{};
    arr_init(&hndls, vk->cfg.arenas.command_arena);
    arr_resize(&hndls, count);
    for (u32 i = 0; i < count; ++i) {
        hndls[i] = pool->buffers[ind + i].hndl;
    }
    vkFreeCommandBuffers(vk->inst.device.hndl, pool->hndl, count, hndls.data);
    arr_erase(&pool->buffers, &pool->buffers[ind], &pool->buffers[ind + count]);
    arr_terminate(&hndls);
}

vkr_add_result vkr_add_descriptor_sets(vkr_descriptor_pool *pool, const vkr_context *vk, const VkDescriptorSetLayout *layouts, sizet count)
{
    ilog("Adding %lu descritor set/s", count);
    vkr_add_result result{};
    array<VkDescriptorSet> hndls;
    arr_init(&hndls, vk->cfg.arenas.command_arena);
    arr_resize(&hndls, count);

    VkDescriptorSetAllocateInfo info{};
    info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    info.descriptorPool = pool->hndl;
    info.descriptorSetCount = count;
    info.pSetLayouts = layouts;
    int err = vkAllocateDescriptorSets(vk->inst.device.hndl, &info, hndls.data);
    if (err != VK_SUCCESS) {
        elog("Failed to create descriptor set/s with code %d", err);
        arr_terminate(&hndls);
        result.err_code = err_code::VKR_CREATE_DESCRIPTOR_SETS_FAIL;
        return result;
    }

    result.begin = pool->desc_sets.size;
    result.end = result.begin + count;
    arr_resize(&pool->desc_sets, result.end);

    for (sizet i = 0; i < count; ++i) {
        pool->desc_sets[result.begin + i].hndl = hndls[i];
        ilog("Setting descriptor set at index %lu in pool %p to hndl %lu", result.begin + i, pool, hndls[i]);
    }
    ilog("Successfully added descriptor set/s");
    arr_terminate(&hndls);
    return result;
}

void vkr_remove_descriptor_sets(vkr_descriptor_pool *pool, const vkr_context *vk, u32 ind, u32 count)
{
    array<VkDescriptorSet> hndls{};
    arr_init(&hndls, vk->cfg.arenas.command_arena);
    arr_resize(&hndls, count);
    for (u32 i = 0; i < count; ++i) {
        hndls[i] = pool->desc_sets[ind + i].hndl;
    }
    vkFreeDescriptorSets(vk->inst.device.hndl, pool->hndl, count, hndls.data);
    arr_erase(&pool->desc_sets, &pool->desc_sets[ind], &pool->desc_sets[ind + count]);
    arr_terminate(&hndls);
}

int vkr_init_cmd_pool(const vkr_context *vk, u32 fam_ind, VkCommandPoolCreateFlags flags, vkr_command_pool *cpool)
{
    ilog("Initializing command pool");
    arr_init(&cpool->buffers, vk->cfg.arenas.persistent_arena);
    VkCommandPoolCreateInfo pool_info{};
    pool_info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    pool_info.flags = flags;
    pool_info.queueFamilyIndex = fam_ind;
    int err = vkCreateCommandPool(vk->inst.device.hndl, &pool_info, &vk->alloc_cbs, &cpool->hndl);
    if (err != VK_SUCCESS) {
        elog("Failed creating vulkan command pool with code %d", err);
        return err_code::VKR_CREATE_COMMAND_POOL_FAIL;
    }
    ilog("Successfully initialized command pool");
    return err_code::VKR_NO_ERROR;
}

sizet vkr_add_cmd_pool(vkr_device_queue_fam_info *qfam, const vkr_command_pool &cpool)
{
    ilog("Adding command pool to qfamily");
    sizet ind = qfam->cmd_pools.size;
    arr_push_back(&qfam->cmd_pools, cpool);
    return ind;
}

void vkr_terminate_cmd_pool(const vkr_context *vk, u32 fam_ind, vkr_command_pool *cpool)
{
    ilog("Terminating command pool");
    vkDestroyCommandPool(vk->inst.device.hndl, cpool->hndl, &vk->alloc_cbs);
    arr_terminate(&cpool->buffers);
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

int vkr_init_render_pass(const vkr_context *vk, const vkr_rpass_cfg *cfg, vkr_rpass *rpass)
{
    ilog("Initializing render pass");

    VkAttachmentReference col_att_ref{};
    col_att_ref.attachment = 0;
    col_att_ref.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    // Create just one subpass for now
    array<VkSubpassDescription> subpasses{};
    arr_init(&subpasses, vk->cfg.arenas.command_arena);
    arr_resize(&subpasses, cfg->subpasses.size);

    for (int i = 0; i < cfg->subpasses.size; ++i) {
        subpasses[i].pipelineBindPoint = cfg->subpasses[i].pipeline_bind_point;

        subpasses[i].colorAttachmentCount = cfg->subpasses[i].color_attachments.size;
        if (subpasses[i].colorAttachmentCount > 0) {
            subpasses[i].pColorAttachments = cfg->subpasses[i].color_attachments.data;
        }

        subpasses[i].inputAttachmentCount = cfg->subpasses[i].input_attachments.size;
        if (subpasses[i].inputAttachmentCount > 0) {
            subpasses[i].pInputAttachments = cfg->subpasses[i].input_attachments.data;
        }

        subpasses[i].preserveAttachmentCount = cfg->subpasses[i].preserve_attachments.size;
        if (subpasses[i].preserveAttachmentCount > 0) {
            subpasses[i].pPreserveAttachments = cfg->subpasses[i].preserve_attachments.data;
        }

        if (cfg->subpasses[i].resolve_attachments.size > 0) {
            subpasses[i].pResolveAttachments = cfg->subpasses[i].resolve_attachments.data;
        }

        subpasses[i].pDepthStencilAttachment = cfg->subpasses[i].depth_stencil_attachment;
    }

    VkRenderPassCreateInfo rpass_info{};
    rpass_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    rpass_info.attachmentCount = cfg->attachments.size;
    rpass_info.pAttachments = cfg->attachments.data;
    rpass_info.subpassCount = cfg->subpasses.size;
    rpass_info.pSubpasses = subpasses.data;
    rpass_info.dependencyCount = cfg->subpass_dependencies.size;
    rpass_info.pDependencies = cfg->subpass_dependencies.data;

    arr_terminate(&subpasses);

    if (vkCreateRenderPass(vk->inst.device.hndl, &rpass_info, &vk->alloc_cbs, &rpass->hndl) != VK_SUCCESS) {
        elog("Failed to create render pass");
        return err_code::VKR_CREATE_RENDER_PASS_FAIL;
    }
    ilog("Successfully initialized render pass");
    return err_code::VKR_NO_ERROR;
}

sizet vkr_add_render_pass(vkr_device *device, const vkr_rpass &copy)
{
    ilog("Adding render_pass to device");
    sizet ind = device->render_passes.size;
    arr_push_back(&device->render_passes, copy);
    return ind;
}

void vkr_terminate_render_pass(const vkr_context *vk, const vkr_rpass *rpass)
{
    ilog("Terminating render pass");
    vkDestroyRenderPass(vk->inst.device.hndl, rpass->hndl, &vk->alloc_cbs);
}

VkShaderStageFlagBits vkr_shader_stage_type_bits(vkr_shader_stage_type st_type)
{
    switch (st_type) {
    case (VKR_SHADER_STAGE_VERT):
        return VK_SHADER_STAGE_VERTEX_BIT;
    case (VKR_SHADER_STAGE_FRAG):
        return VK_SHADER_STAGE_FRAGMENT_BIT;
    default:
        elog("Shader type unknown");
        assert(true);
        return (VkShaderStageFlagBits)-1;
    }
}

const char *vkr_shader_stage_type_str(vkr_shader_stage_type st_type)
{
    switch (st_type) {
    case (VKR_SHADER_STAGE_VERT):
        return "vert";
    case (VKR_SHADER_STAGE_FRAG):
        return "frag";
    default:
        elog("Shader type unknown");
        assert(true);
        return "unknown";
    }
}

int vkr_init_pipeline(const vkr_context *vk, const vkr_pipeline_cfg *cfg, vkr_pipeline *pipe_info)
{
    ilog("Initializing pipeline");
    VkPipelineShaderStageCreateInfo stages[VKR_SHADER_STAGE_COUNT]{};
    u32 actual_stagei = 0;
    for (u32 stagei = 0; stagei < VKR_SHADER_STAGE_COUNT; ++stagei) {
        if (cfg->shader_stages[stagei].code.size > 0) {
            int err = vkr_init_shader_module(vk, &cfg->shader_stages[stagei].code, &stages[actual_stagei].module);
            if (err != err_code::VKR_NO_ERROR) {
                // Destroy the previously successfully initialized shader modules
                for (int previ = 0; previ < actual_stagei; ++previ) {
                    vkr_terminate_shader_module(vk, stages[previ].module);
                }
                elog("Could not initialize %s shader module", vkr_shader_stage_type_str((vkr_shader_stage_type)stagei));
                return err;
            }

            stages[actual_stagei].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
            stages[actual_stagei].stage = vkr_shader_stage_type_bits((vkr_shader_stage_type)stagei);
            stages[actual_stagei].pName = cfg->shader_stages[stagei].entry_point;
            ++actual_stagei;
        }
    }

    pipe_info->rpass = *cfg->rpass;

    // Dynamic state
    VkPipelineDynamicStateCreateInfo dyn_state{};
    dyn_state.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dyn_state.dynamicStateCount = cfg->dynamic_states.size;
    dyn_state.pDynamicStates = cfg->dynamic_states.data;

    // Vertex Input
    VkPipelineVertexInputStateCreateInfo vertex_input_info{};
    vertex_input_info.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertex_input_info.vertexBindingDescriptionCount = cfg->vert_binding_desc.size;
    vertex_input_info.pVertexBindingDescriptions = cfg->vert_binding_desc.data;
    vertex_input_info.vertexAttributeDescriptionCount = cfg->vert_attrib_desc.size;
    vertex_input_info.pVertexAttributeDescriptions = cfg->vert_attrib_desc.data;

    // Input assembly
    VkPipelineInputAssemblyStateCreateInfo input_assembly{};
    input_assembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    input_assembly.topology = cfg->input_assembly.primitive_topology;
    input_assembly.primitiveRestartEnable = cfg->input_assembly.primitive_restart_enable;

    // Viewport and scissors (default)
    VkPipelineViewportStateCreateInfo viewport_state{};
    viewport_state.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewport_state.viewportCount = cfg->viewports.size;
    viewport_state.pViewports = cfg->viewports.data;
    viewport_state.scissorCount = cfg->scissors.size;
    viewport_state.pScissors = cfg->scissors.data;

    // Rasterizer
    VkPipelineRasterizationStateCreateInfo rstr{};
    rstr.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rstr.depthClampEnable = false;
    rstr.rasterizerDiscardEnable = false;
    rstr.polygonMode = VK_POLYGON_MODE_FILL;
    rstr.lineWidth = 1.0f;
    rstr.cullMode = VK_CULL_MODE_BACK_BIT;
    rstr.frontFace = VK_FRONT_FACE_CLOCKWISE;
    rstr.depthBiasEnable = false;
    rstr.depthBiasConstantFactor = 0.0f;
    rstr.depthBiasClamp = 0.0f;
    rstr.depthBiasSlopeFactor = 0.0f;

    // Multisampling
    VkPipelineMultisampleStateCreateInfo multisampling{};
    multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisampling.sampleShadingEnable = cfg->multisampling.sample_shading_enable;
    multisampling.rasterizationSamples = cfg->multisampling.rasterization_samples;
    multisampling.minSampleShading = cfg->multisampling.min_sample_shading;
    multisampling.pSampleMask = cfg->multisampling.sample_masks;
    multisampling.alphaToCoverageEnable = cfg->multisampling.alpha_to_coverage_enable;
    multisampling.alphaToOneEnable = cfg->multisampling.alpha_to_one_enable;

    // Depth/Stencil - Skip for now

    // Color blending
    VkPipelineColorBlendStateCreateInfo col_blend_state{};
    col_blend_state.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    col_blend_state.logicOpEnable = cfg->col_blend.logic_op_enabled;
    col_blend_state.logicOp = cfg->col_blend.logic_op;
    col_blend_state.attachmentCount = cfg->col_blend.attachments.size;
    col_blend_state.pAttachments = cfg->col_blend.attachments.data;
    for (int i = 0; i < cfg->col_blend.blend_constants.size(); ++i) {
        col_blend_state.blendConstants[i] = cfg->col_blend.blend_constants[i];
    }

    // Create the descriptor set layouts
    for (int desc_i = 0; desc_i < cfg->set_layouts.size; ++desc_i) {
        VkDescriptorSetLayoutCreateInfo ci{};
        ci.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        ci.bindingCount = cfg->set_layouts[desc_i].bindings.size;
        ci.pBindings = cfg->set_layouts[desc_i].bindings.data;
        VkDescriptorSetLayout hndl{};
        int res = vkCreateDescriptorSetLayout(vk->inst.device.hndl, &ci, &vk->alloc_cbs, &hndl);
        if (res == VK_SUCCESS) {
            arr_push_back(&pipe_info->descriptor_layouts, hndl);
        }
        else {
            elog("Could not create descriptor set layout with vk err %d", res);
            for (u32 i = 0; i < pipe_info->descriptor_layouts.size; ++i) {
                vkDestroyDescriptorSetLayout(vk->inst.device.hndl, pipe_info->descriptor_layouts[i], &vk->alloc_cbs);
            }
        }
    }

    // Pipeline layout - where we would bind uniforms and such
    VkPipelineLayoutCreateInfo pipeline_layout_create_inf{};
    pipeline_layout_create_inf.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipeline_layout_create_inf.setLayoutCount = pipe_info->descriptor_layouts.size;
    pipeline_layout_create_inf.pSetLayouts = pipe_info->descriptor_layouts.data;
    pipeline_layout_create_inf.pushConstantRangeCount = cfg->push_constant_ranges.size;
    pipeline_layout_create_inf.pPushConstantRanges = cfg->push_constant_ranges.data;

    if (vkCreatePipelineLayout(vk->inst.device.hndl, &pipeline_layout_create_inf, &vk->alloc_cbs, &pipe_info->layout_hndl) != VK_SUCCESS) {
        elog("Failed to create pileline layout");
        for (u32 si = 0; si < actual_stagei; ++si) {
            vkr_terminate_shader_module(vk, stages[si].module);
        }
        for (u32 i = 0; i < pipe_info->descriptor_layouts.size; ++i) {
            vkDestroyDescriptorSetLayout(vk->inst.device.hndl, pipe_info->descriptor_layouts[i], &vk->alloc_cbs);
        }
        return err_code::VKR_CREATE_PIPELINE_LAYOUT_FAIL;
    }

    VkGraphicsPipelineCreateInfo pipeline_info{};
    pipeline_info.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipeline_info.stageCount = actual_stagei;
    pipeline_info.pStages = stages;                       // done
    pipeline_info.pVertexInputState = &vertex_input_info; // done
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

    // This could possibly be used in future but who knows
    pipeline_info.basePipelineHandle = VK_NULL_HANDLE;
    pipeline_info.basePipelineIndex = -1;

    int err_ret = vkCreateGraphicsPipelines(vk->inst.device.hndl, VK_NULL_HANDLE, 1, &pipeline_info, &vk->alloc_cbs, &pipe_info->hndl);
    if (err_ret != VK_SUCCESS) {
        for (u32 si = 0; si < actual_stagei; ++si) {
            vkr_terminate_shader_module(vk, stages[si].module);
        }
        for (u32 i = 0; i < pipe_info->descriptor_layouts.size; ++i) {
            vkDestroyDescriptorSetLayout(vk->inst.device.hndl, pipe_info->descriptor_layouts[i], &vk->alloc_cbs);
        }
        vkDestroyPipelineLayout(vk->inst.device.hndl, pipe_info->layout_hndl, &vk->alloc_cbs);
        err_ret = err_code::VKR_CREATE_PIPELINE_FAIL;
    }
    for (u32 si = 0; si < actual_stagei; ++si) {
        vkr_terminate_shader_module(vk, stages[si].module);
    }
    ilog("Successfully initialized pipeline");
    err_ret = err_code::VKR_NO_ERROR;
    return err_ret;
}

sizet vkr_add_pipeline(vkr_device *device, const vkr_pipeline &copy)
{
    ilog("Adding pipeline to device");
    sizet ind = device->pipelines.size;
    arr_push_back(&device->pipelines, copy);
    return ind;
}

void vkr_terminate_pipeline(const vkr_context *vk, const vkr_pipeline *pipe_info)
{
    ilog("Terminating pipeline");
    vkDestroyPipeline(vk->inst.device.hndl, pipe_info->hndl, &vk->alloc_cbs);
    vkDestroyPipelineLayout(vk->inst.device.hndl, pipe_info->layout_hndl, &vk->alloc_cbs);
    for (int i = 0; i < pipe_info->descriptor_layouts.size; ++i) {
        vkDestroyDescriptorSetLayout(vk->inst.device.hndl, pipe_info->descriptor_layouts[i], &vk->alloc_cbs);
    }
}

int vkr_init_framebuffer(const vkr_context *vk, const vkr_framebuffer_cfg *cfg, vkr_framebuffer *fb)
{
    ilog("Initializing framebuffer");
    arr_init(&fb->attachments, vk->cfg.arenas.persistent_arena);
    fb->size = cfg->size;
    fb->rpass = *cfg->rpass;
    fb->layers = fb->layers;
    arr_copy(&fb->attachments, cfg->attachments, cfg->attachment_count);

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

sizet vkr_add_framebuffer(vkr_device *device, const vkr_framebuffer &copy)
{
    ilog("Adding framebuffer to device");
    sizet ind = device->framebuffers.size;
    arr_push_back(&device->framebuffers, copy);
    return ind;
}

void vkr_terminate_framebuffer(const vkr_context *vk, vkr_framebuffer *fb)
{
    ilog("Terminating framebuffer");
    vkDestroyFramebuffer(vk->inst.device.hndl, fb->hndl, &vk->alloc_cbs);
    arr_terminate(&fb->attachments);
}

u32 vkr_find_mem_type(u32 type_flags, VkMemoryPropertyFlags property_flags, const vkr_phys_device *pdev)
{
    for (u32 i = 0; i < pdev->mem_properties.memoryTypeCount; ++i) {
        if ((type_flags & (1 << i)) && test_all_flags(pdev->mem_properties.memoryTypes[i].propertyFlags & property_flags, property_flags)) {
            return i;
        }
    }
    return -1;
}

int vkr_init_descriptor_pool(vkr_descriptor_pool *desc_pool, const vkr_context *vk, u32 max_sets)
{
    arr_init(&desc_pool->desc_sets, vk->cfg.arenas.persistent_arena);
    
    // Get a count of the number of descriptors we are making avaialable for each desc type
    VkDescriptorPoolSize psize[VKR_DESCRIPTOR_TYPE_COUNT] = {};
    u32 desc_size_count{};
    for (int desc_t = 0; desc_t < VKR_DESCRIPTOR_TYPE_COUNT; ++desc_t) {
        if (vk->cfg.max_desc_per_type_per_pool.count[desc_t] > 0) {
            psize[desc_size_count].descriptorCount = vk->cfg.max_desc_per_type_per_pool.count[desc_t];
            psize[desc_size_count].type = (VkDescriptorType)desc_t;
            ilog("Adding desc type %d to frame descriptor pool with %d desc available",
                 psize[desc_size_count].type,
                 psize[desc_size_count].descriptorCount);
            ++desc_size_count;
        }
    }

    // Create the frame descriptor pool
    VkDescriptorPoolCreateInfo pool_info{};
    pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    pool_info.poolSizeCount = desc_size_count;
    pool_info.pPoolSizes = psize;
    pool_info.maxSets = vk->cfg.max_desc_sets_per_pool;
    
    int err = vkCreateDescriptorPool(vk->inst.device.hndl, &pool_info, &vk->alloc_cbs, &desc_pool->hndl);
    if (err != VK_SUCCESS) {
        elog("Failed to create descriptor pool with vk err code %d", err);
        return err_code::VKR_CREATE_DESCRIPTOR_POOL_FAIL;
    }
    return err_code::VKR_NO_ERROR;
}

void vkr_terminate_descriptor_pool(vkr_descriptor_pool *desc_pool, const vkr_context *vk)
{
    vkDestroyDescriptorPool(vk->inst.device.hndl, desc_pool->hndl, &vk->alloc_cbs);
    arr_terminate(&desc_pool->desc_sets);
}

sizet vkr_add_buffer(vkr_device *device, const vkr_buffer &copy)
{
    ilog("Adding buffer to device");
    sizet ind = device->buffers.size;
    arr_push_back(&device->buffers, copy);
    return ind;
}

void *vkr_map_buffer(vkr_buffer *buf)
{
    void *ret;
    vmaMapMemory(buf->cfg.gpu_alloc, buf->mem_hndl, &ret);
    return ret;
}

void vkr_unmap_buffer(vkr_buffer *buf)
{
    vmaUnmapMemory(buf->cfg.gpu_alloc, buf->mem_hndl);
}

void vkr_stage_and_upload_buffer_data(vkr_buffer *dest_buffer,
                                      const void *src_data,
                                      sizet src_data_size,
                                      vkr_device_queue_fam_info *cmd_q,
                                      const vkr_context *vk)
{
    vkr_buffer staging_buf{};
    auto buf_cfg = dest_buffer->cfg;
    buf_cfg.alloc_flags |= VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;
    buf_cfg.usage &= ~VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    buf_cfg.usage |= VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
    buf_cfg.mem_usage = VMA_MEMORY_USAGE_AUTO_PREFER_HOST;
    vkr_init_buffer(&staging_buf, &buf_cfg);

    void *mem = vkr_map_buffer(&staging_buf);
    memcpy(mem, src_data, src_data_size);
    vkr_unmap_buffer(&staging_buf);

    vkr_copy_buffer(dest_buffer, &staging_buf, cmd_q, vk);
    vkr_terminate_buffer(&staging_buf, vk);
}

int vkr_init_buffer(vkr_buffer *buffer, const vkr_buffer_cfg *cfg)
{
    buffer->cfg = *cfg;
    VkBufferCreateInfo cinfo{};
    cinfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    cinfo.size = cfg->buffer_size;
    cinfo.usage = cfg->usage;
    cinfo.sharingMode = cfg->sharing_mode;

    VmaAllocationCreateInfo alloc_info = {};
    alloc_info.flags = cfg->alloc_flags;
    alloc_info.usage = cfg->mem_usage;
    alloc_info.requiredFlags = cfg->required_flags;
    alloc_info.preferredFlags = cfg->preferred_flags;

    int err = vmaCreateBuffer(cfg->gpu_alloc, &cinfo, &alloc_info, &buffer->hndl, &buffer->mem_hndl, &buffer->mem_info);
    if (err != VK_SUCCESS) {
        elog("Failed in creating buffer with vk err %d", err);
        return err_code::VKR_CREATE_BUFFER_FAIL;
    }
    return err_code::VKR_NO_ERROR;
}

void vkr_terminate_buffer(const vkr_buffer *buffer, const vkr_context *vk)
{
    ilog("Terminating buffer");
    vmaDestroyBuffer(vk->inst.device.vma_alloc.hndl, buffer->hndl, buffer->mem_hndl);
}

sizet vkr_add_swapchain_framebuffers(vkr_device *device)
{
    sizet cur_ind = device->framebuffers.size;
    arr_resize(&device->framebuffers, cur_ind + device->swapchain.image_views.size);
    return cur_ind;
}

void vkr_init_swapchain_framebuffers(vkr_device *device,
                                     const vkr_context *vk,
                                     const vkr_rpass *rpass,
                                     const array<array<VkImageView>> *other_attachments,
                                     sizet fb_offset)
{
    for (int i = 0; i < vk->inst.device.swapchain.image_views.size; ++i) {
        vkr_framebuffer_cfg cfg{};
        cfg.size = {vk->inst.device.swapchain.extent.width, vk->inst.device.swapchain.extent.height};
        cfg.rpass = rpass;
        array<VkImageView> iviews;
        arr_init(&iviews, vk->cfg.arenas.command_arena);
        arr_push_back(&iviews, device->swapchain.image_views[i]);
        if (other_attachments) {
            arr_append(&iviews, (*other_attachments)[i].data, other_attachments[i].size);
        }
        cfg.attachment_count = iviews.size;
        cfg.attachments = iviews.data;
        vkr_init_framebuffer(vk, &cfg, &device->framebuffers[fb_offset + i]);
        arr_terminate(&iviews);
    }
}

void vkr_terminate_swapchain_framebuffers(vkr_device *device, const vkr_context *vk, sizet fb_offset)
{
    for (int i = 0; i < vk->inst.device.swapchain.image_views.size; ++i) {
        vkr_terminate_framebuffer(vk, &device->framebuffers[i + fb_offset]);
        device->framebuffers[i + fb_offset] = {};
    }
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
    code = vkr_init_device(&vk->inst.device,
                           vk,
                           cfg->validation_layer_names,
                           cfg->validation_layer_count,
                           cfg->device_extension_names,
                           cfg->device_extension_count);
    if (code != err_code::VKR_NO_ERROR) {

        vkr_terminate(vk);
        return code;
    }

    code = vkr_init_swapchain(&vk->inst.device.swapchain, vk, cfg->window);
    if (code != err_code::VKR_NO_ERROR) {
        vkr_terminate(vk);
        return code;
    }

    vkr_add_swapchain_framebuffers(&vk->inst.device);
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

void vkr_terminate_swapchain(vkr_swapchain *sw_info, const vkr_context *vk)
{
    ilog("Terminating swapchain");
    for (int i = 0; i < sw_info->image_views.size; ++i) {
        vkDestroyImageView(vk->inst.device.hndl, sw_info->image_views[i], &vk->alloc_cbs);
    }
    vkDestroySwapchainKHR(vk->inst.device.hndl, sw_info->swapchain, &vk->alloc_cbs);
    arr_terminate(&sw_info->images);
    arr_terminate(&sw_info->image_views);
}

void vkr_terminate_device(vkr_device *dev, const vkr_context *vk)
{
    ilog("Waiting for sync objects before terminating device...");

    // TODO: Make this wait on our semaphores and fences more explicitly
    vkDeviceWaitIdle(dev->hndl);
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
    for (int i = 0; i < dev->buffers.size; ++i) {
        vkr_terminate_buffer(&dev->buffers[i], vk);
    }

    arr_terminate(&dev->render_passes);
    arr_terminate(&dev->pipelines);
    arr_terminate(&dev->framebuffers);
    arr_terminate(&dev->buffers);

    for (sizet qfam_i = 0; qfam_i < VKR_QUEUE_FAM_TYPE_COUNT; ++qfam_i) {
        auto cur_fam = &dev->qfams[qfam_i];
        arr_terminate(&cur_fam->qs);
        for (sizet pool_i = 0; pool_i < cur_fam->cmd_pools.size; ++pool_i) {
            auto cur_pool = &cur_fam->cmd_pools[pool_i];
            vkr_terminate_cmd_pool(vk, cur_fam->fam_ind, cur_pool);
        }
        arr_terminate(&cur_fam->cmd_pools);
    }

    vkr_terminate_swapchain(&dev->swapchain, vk);

    for (int framei = 0; framei < VKR_RENDER_FRAME_COUNT; ++framei) {
        vkr_terminate_descriptor_pool(&dev->rframes[framei].desc_pool, vk);
        vkDestroySemaphore(dev->hndl, dev->rframes[framei].image_avail, &vk->alloc_cbs);
        vkDestroySemaphore(dev->hndl, dev->rframes[framei].render_finished, &vk->alloc_cbs);
        vkDestroyFence(dev->hndl, dev->rframes[framei].in_flight, &vk->alloc_cbs);
    }

    vmaDestroyAllocator(dev->vma_alloc.hndl);
    vkDestroyDevice(dev->hndl, &vk->alloc_cbs);
}

intern void log_mem_stats(const char *type, const vk_mem_alloc_stats *stats)
{
    ilog("%s alloc_count:%d free_count:%d realloc_count:%d",
         type,
         stats->alloc_count,
         stats->free_count,
         stats->realloc_count);
    ilog("%s req_alloc:%lu req_free:%lu actual_alloc:%lu actual_free:%lu",
         type,
         stats->req_alloc,
         stats->req_free,
         stats->actual_alloc,
         stats->actual_free);
    
}

void vkr_terminate_instance(const vkr_context *vk, vkr_instance *inst)
{
    ilog("Terminating vkr instance");
    vkr_terminate_device(&inst->device, vk);
    vkr_terminate_pdevice_swapchain_support(&inst->pdev_info.swap_support);
    vkr_terminate_surface(vk, inst->surface);
    inst->ext_funcs.destroy_debug_utils_messenger(inst->hndl, inst->dbg_messenger, &vk->alloc_cbs);

    // Destroying the instance calls more vk alloc calls
    vkDestroyInstance(inst->hndl, &vk->alloc_cbs);
}

void vkr_terminate(vkr_context *vk)
{
    ilog("Terminating vulkan");
    vkr_terminate_instance(vk, &vk->inst);
    for (int i = 0; i < MEM_ALLOC_TYPE_COUNT; ++i) {
        log_mem_stats(alloc_scope_str(i), &vk->cfg.arenas.stats[i]);
    }
    ilog("Persistant mem size:%lu peak:%lu  Command mem size:%lu peak:%lu", vk->cfg.arenas.persistent_arena->total_size, vk->cfg.arenas.persistent_arena->peak, vk->cfg.arenas.command_arena->total_size, vk->cfg.arenas.command_arena->peak);
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

void vkr_cmd_begin_rpass(const vkr_command_buffer *cmd_buf, const vkr_framebuffer *fb)
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

int vkr_copy_buffer(vkr_buffer *dest, const vkr_buffer *src, vkr_device_queue_fam_info *cmd_q, const vkr_context *vk, VkBufferCopy region)
{
    auto pool = &cmd_q->cmd_pools[cmd_q->transient_pool];
    vkr_add_result tmp_buf = vkr_add_cmd_bufs(pool, vk);
    if (tmp_buf.err_code != err_code::VKR_NO_ERROR) {
        return tmp_buf.err_code;
    }
    auto tmp_buf_hndl = pool->buffers[tmp_buf.begin].hndl;

    if (region.size == 0) {
        region.size = src->mem_info.size;
    }

    VkCommandBufferBeginInfo beg_info{};
    beg_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beg_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    vkBeginCommandBuffer(tmp_buf_hndl, &beg_info);
    vkCmdCopyBuffer(tmp_buf_hndl, src->hndl, dest->hndl, 1, &region);
    vkEndCommandBuffer(tmp_buf_hndl);

    VkSubmitInfo submit_info{};
    submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submit_info.commandBufferCount = 1;
    submit_info.pCommandBuffers = &tmp_buf_hndl;

    vkQueueSubmit(cmd_q->qs[0].hndl, 1, &submit_info, VK_NULL_HANDLE);
    vkQueueWaitIdle(cmd_q->qs[0].hndl);

    vkr_remove_cmd_bufs(pool, vk, tmp_buf.begin);
    return err_code::VKR_NO_ERROR;
}

} // namespace nslib
