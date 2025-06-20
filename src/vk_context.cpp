#include <cstring>

#include "vk_context.h"
#include "platform.h"
#include "SDL3/SDL_vulkan.h"
#include "logging.h"

#define PRINT_MEM_DEBUG false
#define PRINT_MEM_INSTANCE_ONLY false
#define PRINT_MEM_OBJECT_ONLY true

#define PRINT_MEM_GPU_ALLOC false

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

intern bool log_any_sdl_error(const char *prefix = "SDL err")
{
    auto err = SDL_GetError();
    elog("%s: %s", prefix, (err) ? err : "none");
    bool ret = (err);
    SDL_ClearError();
    return ret;
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
    asrt(user);
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
    memset(header, 0, size + header_size);
    header->scope = scope;
    header->req_size = size;

    void *ret = (void *)((sizet)header + header_size);
    sizet used_actual = arena->used - used_before;

    arenas->stats[scope].actual_alloc += used_actual;
#if PRINT_MEM_DEBUG
    #if PRINT_MEM_INSTANCE_ONLY
    if (scope == VK_SYSTEM_ALLOCATION_SCOPE_INSTANCE) {
    #elif PRINT_MEM_OBJECT_ONLY
    if (scope == VK_SYSTEM_ALLOCATION_SCOPE_OBJECT) {
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
    #if PRINT_MEM_INSTANCE_ONLY || PRINT_MEM_OBJECT_ONLY
    }
    #endif
#endif
    return ret;
}

intern void vk_free(void *user, void *ptr)
{
    asrt(user);
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
    #elif PRINT_MEM_OBJECT_ONLY
        if (scope == VK_SYSTEM_ALLOCATION_SCOPE_OBJECT) {
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
    #if PRINT_MEM_INSTANCE_ONLY || PRINT_MEM_OBJECT_ONLY
    }
    #endif
#endif
}

intern void *vk_realloc(void *user, void *ptr, sizet size, sizet alignment, VkSystemAllocationScope scope)
{
    asrt(user);
    if (!ptr) {
        return nullptr;
    }
    auto arenas = (vk_arenas *)user;
    ++arenas->stats[scope].realloc_count;
    arenas->stats[scope].req_alloc += size;

    auto arena = arenas->persistent_arena;

    sizet header_size = sizeof(internal_alloc_header);
    auto old_header = (internal_alloc_header *)((sizet)ptr - header_size);
    asrt(old_header->scope == scope);
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
#if PRINT_MEM_DEBUG
    #if PRINT_MEM_INSTANCE_ONLY
    if (scope == VK_SYSTEM_ALLOCATION_SCOPE_INSTANCE) {
    #elif PRINT_MEM_OBJECT_ONLY
            if (scope == VK_SYSTEM_ALLOCATION_SCOPE_OBJECT) {
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
    #if PRINT_MEM_INSTANCE_ONLY || PRINT_MEM_OBJECT_ONLY
    }
    #endif
#endif
    if (diff != (new_block_size - old_block_size)) {
        wlog("Diff problems!");
    }
    //    asrt(diff == (new_block_size - old_block_size));
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
    asrt(res == VK_SUCCESS);
    VkExtensionProperties *ext_array =
        (VkExtensionProperties *)mem_alloc(extension_count * sizeof(VkExtensionProperties), arenas->command_arena);
    memset(ext_array, 0, extension_count * sizeof(VkExtensionProperties));
    res = vkEnumerateDeviceExtensionProperties(pdevice->hndl, nullptr, &extension_count, ext_array);
    asrt(res == VK_SUCCESS);
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
    asrt(res == VK_SUCCESS);
    VkExtensionProperties *ext_array =
        (VkExtensionProperties *)mem_alloc(extension_count * sizeof(VkExtensionProperties), arenas->command_arena);
    memset(ext_array, 0, extension_count * sizeof(VkExtensionProperties));
    res = vkEnumerateInstanceExtensionProperties(nullptr, &extension_count, ext_array);
    asrt(res == VK_SUCCESS);
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
    asrt(res == VK_SUCCESS);
    VkLayerProperties *layer_array = (VkLayerProperties *)mem_alloc(layer_count * sizeof(VkLayerProperties), arenas->command_arena);
    memset(layer_array, 0, layer_count * sizeof(VkLayerProperties));

    res = vkEnumerateInstanceLayerProperties(&layer_count, layer_array);
    asrt(res == VK_SUCCESS);

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
    int cur = logging_level(GLOBAL_LOGGER);
    set_logging_level(GLOBAL_LOGGER, *((int *)user));
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
    set_logging_level(GLOBAL_LOGGER, cur);
    return VK_FALSE;
}

intern void fill_extension_funcs(vkr_debug_extension_funcs *funcs, VkInstance hndl)
{
    funcs->create_debug_utils_messenger = (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(hndl, "vkCreateDebugUtilsMessengerEXT");
    funcs->destroy_debug_utils_messenger =
        (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(hndl, "vkDestroyDebugUtilsMessengerEXT");
    asrt(funcs->create_debug_utils_messenger);
    asrt(funcs->destroy_debug_utils_messenger);
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
    app_info.apiVersion = VKR_API_VERSION;

    VkInstanceCreateInfo create_inf{};
    create_inf.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    create_inf.pApplicationInfo = &app_info;

    // This is for clarity.. we could just directly pass the enabled extension count
    u32 ext_count{0};
    const char *const *glfw_ext = SDL_Vulkan_GetInstanceExtensions(&ext_count);
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
    VkValidationFeatureEnableEXT enabled[] = {VK_VALIDATION_FEATURE_ENABLE_DEBUG_PRINTF_EXT};
    VkValidationFeaturesEXT features{VK_STRUCTURE_TYPE_VALIDATION_FEATURES_EXT};
    features.disabledValidationFeatureCount = 0;
    features.enabledValidationFeatureCount = 1;
    features.pDisabledValidationFeatures = nullptr;
    features.pEnabledValidationFeatures = enabled;
    features.pNext = create_inf.pNext;
    create_inf.pNext = &features;
    create_inf.ppEnabledExtensionNames = ext;
    create_inf.enabledExtensionCount = total_exts;
    create_inf.ppEnabledLayerNames = vk->cfg.validation_layer_names;
    create_inf.enabledLayerCount = vk->cfg.validation_layer_count;
    create_inf.flags = vk->cfg.inst_create_flags;

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
    asrt(count <= MAX_QUEUE_REQUEST_COUNT);
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
    arr_init(&dev->images, vk->cfg.arenas.persistent_arena);
    arr_init(&dev->image_views, vk->cfg.arenas.persistent_arena);
    arr_init(&dev->samplers, vk->cfg.arenas.persistent_arena);

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
    features.samplerAnisotropy = vk->inst.pdev_info.features.samplerAnisotropy;
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
    cr_info.vulkanApiVersion = VKR_API_VERSION;
    cr_info.preferredLargeHeapBlockSize = 0; // This defaults to 256 MB
    int err = vmaCreateAllocator(&cr_info, &dev->vma_alloc.hndl);
    if (err != VK_SUCCESS) {
        elog("Failed to create vma allocator with code %d", err);
        return err_code::VKR_CREATE_VMA_ALLOCATOR_FAIL;
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
            sizet dpool = vkr_add_cmd_pool(&dev->qfams[i], qpool);
            dev->qfams[i].default_pool = dpool;
        }

        vkr_command_pool qpool_transient{};
        if (vkr_init_cmd_pool(vk, dev->qfams[i].fam_ind, VK_COMMAND_POOL_CREATE_TRANSIENT_BIT, &qpool_transient) == err_code::VKR_NO_ERROR) {
            sizet pool_transient_ind = vkr_add_cmd_pool(&dev->qfams[i], qpool_transient);
            dev->qfams[i].transient_pool = pool_transient_ind;
        }
    }

    err = vkr_init_swapchain(&dev->swapchain, vk);
    if (err != err_code::VKR_NO_ERROR) {
        return err;
    }

    vkr_add_swapchain_framebuffers(dev);

    err = vkr_init_render_frames(dev, vk);
    if (err != err_code::VKR_NO_ERROR) {
        return err;
    }

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
    VkPhysicalDeviceProperties sel_dev_props{};
    VkPhysicalDeviceFeatures sel_dev_features{};

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
        if (features.samplerAnisotropy) {
            cur_score += 3;
        }
        else {
            cur_score -= 3;
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
            high_score = cur_score;
            sel_dev_props = props;
            sel_dev_features = features;
        }
    }
    ilog("Selected device id:%d  name:%s  type:%s",
         sel_dev_props.deviceID,
         sel_dev_props.deviceName,
         vkr_physical_device_type_str(sel_dev_props.deviceType));
    if (high_score == -1) {
        return err_code::VKR_NO_SUITABLE_PHYSICAL_DEVICE;
    }
    dev_info->props = sel_dev_props;
    dev_info->features = sel_dev_features;

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

int vkr_init_swapchain(vkr_swapchain *sw_info, const vkr_context *vk)
{
    ilog("Setting up swapchain");
    arr_init(&sw_info->image_views, vk->cfg.arenas.persistent_arena);
    arr_init(&sw_info->images, vk->cfg.arenas.persistent_arena);

    // I no like typing too much
    vkr_pdevice_swapchain_support swap_support{};
    vkr_init_pdevice_swapchain_support(&swap_support, vk->cfg.arenas.command_arena);    
    vkr_fill_pdevice_swapchain_support(vk->inst.pdev_info.hndl, vk->inst.surface, &swap_support);

    // auto caps = &vk->inst.pdev_info.swap_support.capabilities;
    // auto formats = &vk->inst.pdev_info.swap_support.formats;
    // auto pmodes = &vk->inst.pdev_info.swap_support.present_modes;
    auto qfams = &vk->inst.pdev_info.qfams;

    VkSwapchainCreateInfoKHR swap_create{};
    swap_create.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    swap_create.surface = vk->inst.surface;
    swap_create.imageArrayLayers = 1;
    swap_create.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    swap_create.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    swap_create.preTransform = swap_support.capabilities.currentTransform;
    swap_create.clipped = VK_TRUE;
    swap_create.oldSwapchain = VK_NULL_HANDLE;
    swap_create.minImageCount = swap_support.capabilities.minImageCount + 1;
    if (swap_support.capabilities.maxImageCount != 0 && swap_support.capabilities.maxImageCount < swap_create.minImageCount) {
        swap_create.minImageCount = swap_support.capabilities.maxImageCount;
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
    for (int i = 0; i < swap_support.formats.size; ++i) {
        if (swap_support.formats[i].format == VK_FORMAT_B8G8R8A8_SRGB &&
            swap_support.formats[i].colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
            desired_format_ind = i;
            break;
        }
    }
    swap_create.imageFormat = swap_support.formats[desired_format_ind].format;
    swap_create.imageColorSpace = swap_support.formats[desired_format_ind].colorSpace;

    // If mailbox is available then use it, otherwise use fifo
    swap_create.presentMode = VK_PRESENT_MODE_FIFO_KHR;
    for (int i = 0; i < swap_support.present_modes.size; ++i) {
        if (swap_support.present_modes[i] == VK_PRESENT_MODE_MAILBOX_KHR) {
            swap_create.presentMode = swap_support.present_modes[i];
            break;
        }
    }

    // Handle the extents - if the screen coords don't match the pixel coords then we gotta set this from the frame
    // buffer as by default the extents are screen coords - this is really only for retina displays
    swap_create.imageExtent = swap_support.capabilities.currentExtent;
    // if (swap_support.capabilities.currentExtent.width == VKR_INVALID) {
    auto cur_win_sz = get_window_pixel_size(vk->cfg.window);
    swap_create.imageExtent = {(u32)cur_win_sz.w, (u32)cur_win_sz.h};
    swap_create.imageExtent.width = std::clamp(
        swap_create.imageExtent.width, swap_support.capabilities.minImageExtent.width, swap_support.capabilities.maxImageExtent.width);
    swap_create.imageExtent.height = std::clamp(
        swap_create.imageExtent.height, swap_support.capabilities.minImageExtent.height, swap_support.capabilities.maxImageExtent.height);
    ilog("Should be setting extent to {%d %d} (min {%d %d} max {%d %d})",
         swap_create.imageExtent.width,
         swap_create.imageExtent.height,
         swap_support.capabilities.minImageExtent.width,
         swap_support.capabilities.minImageExtent.height,
         swap_support.capabilities.maxImageExtent.width,
         swap_support.capabilities.maxImageExtent.height);
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

    array<VkImage> simages;
    arr_init(&simages, vk->cfg.arenas.command_arena);
    arr_resize(&simages, image_count);
    arr_resize(&sw_info->images, image_count, vkr_image{});

    res = vkGetSwapchainImagesKHR(vk->inst.device.hndl, sw_info->swapchain, &image_count, simages.data);
    if (res != VK_SUCCESS) {
        elog("Failed to get swapchain images with code %d", res);
        return err_code::VKR_GET_SWAPCHAIN_IMAGES_FAIL;
    }
    for (int i = 0; i < sw_info->images.size; ++i) {
        sw_info->images[i].dims = {sw_info->extent.width, sw_info->extent.height, 1};
        sw_info->images[i].format = sw_info->format;
        sw_info->images[i].hndl = simages[i];
    }
    arr_terminate(&simages);

    arr_resize(&sw_info->image_views, image_count);
    for (int i = 0; i < sw_info->image_views.size; ++i) {
        vkr_image_view_cfg iview_create{};
        iview_create.image = &sw_info->images[i];
        int err = vkr_init_image_view(&sw_info->image_views[i], &iview_create, vk);
        if (err != err_code::VKR_NO_ERROR) {
            elog("Failed to create image view at index %d", i);
            arr_terminate(&sw_info->images);
            arr_terminate(&sw_info->image_views);
            return err_code::VKR_CREATE_IMAGE_VIEW_FAIL;
        }
    }
    ilog("Successfully set up swapchain with %d image views", sw_info->image_views.size);
    vkr_terminate_pdevice_swapchain_support(&swap_support);
    
    return err_code::VKR_NO_ERROR;
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
    info.commandBufferCount = (u32)count;
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
    for (sizet i = 0; i < count; ++i) {
        hndls[i] = pool->buffers[ind + i].hndl;
    }
    vkFreeCommandBuffers(vk->inst.device.hndl, pool->hndl, (u32)count, hndls.data);
    arr_erase(&pool->buffers, &pool->buffers[ind], &pool->buffers[ind + count]);
    arr_terminate(&hndls);
}

vkr_add_result vkr_add_descriptor_sets(vkr_descriptor_pool *pool, const vkr_context *vk, const VkDescriptorSetLayout *layouts, sizet count)
{
    vkr_add_result result{};
    array<VkDescriptorSet> hndls{};
    arr_init(&hndls, vk->cfg.arenas.command_arena);
    arr_resize(&hndls, count);

    VkDescriptorSetAllocateInfo info{};
    info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    info.descriptorPool = pool->hndl;
    info.descriptorSetCount = (u32)count;
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
        pool->desc_sets[result.begin + i].layout = layouts[i];
    }
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

int vkr_init_shader_module(VkShaderModule *module, const byte_array *code, const vkr_context *vk)
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
void vkr_terminate_shader_module(VkShaderModule module, const vkr_context *vk)
{
    ilog("Terminating shader module");
    vkDestroyShaderModule(vk->inst.device.hndl, module, &vk->alloc_cbs);
}

int vkr_init_render_pass(vkr_rpass *rpass, const vkr_rpass_cfg *cfg, const vkr_context *vk)
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

        subpasses[i].colorAttachmentCount = (u32)cfg->subpasses[i].color_attachments.size;
        if (subpasses[i].colorAttachmentCount > 0) {
            subpasses[i].pColorAttachments = cfg->subpasses[i].color_attachments.data;
        }

        subpasses[i].inputAttachmentCount = (u32)cfg->subpasses[i].input_attachments.size;
        if (subpasses[i].inputAttachmentCount > 0) {
            subpasses[i].pInputAttachments = cfg->subpasses[i].input_attachments.data;
        }

        subpasses[i].preserveAttachmentCount = (u32)cfg->subpasses[i].preserve_attachments.size;
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
    rpass_info.attachmentCount = (u32)cfg->attachments.size;
    rpass_info.pAttachments = cfg->attachments.data;
    rpass_info.subpassCount = (u32)cfg->subpasses.size;
    rpass_info.pSubpasses = subpasses.data;
    rpass_info.dependencyCount = (u32)cfg->subpass_dependencies.size;
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

void vkr_terminate_render_pass(const vkr_rpass *rpass, const vkr_context *vk)
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
        asrt_break("Shader type unknown");
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
        asrt_break("Shader type unknown");
        return "unknown";
    }
}

int vkr_init_pipeline(vkr_pipeline *pipe_info, const vkr_pipeline_cfg *cfg, const vkr_context *vk)
{
    ilog("Initializing pipeline");
    VkPipelineShaderStageCreateInfo stages[VKR_SHADER_STAGE_COUNT]{};
    sizet actual_stagei = 0;
    for (u32 stagei = 0; stagei < VKR_SHADER_STAGE_COUNT; ++stagei) {
        if (cfg->shader_stages[stagei].code.size > 0) {
            int err = vkr_init_shader_module(&stages[actual_stagei].module, &cfg->shader_stages[stagei].code, vk);
            if (err != err_code::VKR_NO_ERROR) {
                // Destroy the previously successfully initialized shader modules
                for (sizet previ = 0; previ < actual_stagei; ++previ) {
                    vkr_terminate_shader_module(stages[previ].module, vk);
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
    dyn_state.dynamicStateCount = (u32)cfg->dynamic_states.size;
    dyn_state.pDynamicStates = cfg->dynamic_states.data;

    // Vertex Input
    VkPipelineVertexInputStateCreateInfo vertex_input_info{};
    vertex_input_info.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertex_input_info.vertexBindingDescriptionCount = (u32)cfg->vert_binding_desc.size;
    vertex_input_info.pVertexBindingDescriptions = cfg->vert_binding_desc.data;
    vertex_input_info.vertexAttributeDescriptionCount = (u32)cfg->vert_attrib_desc.size;
    vertex_input_info.pVertexAttributeDescriptions = cfg->vert_attrib_desc.data;

    // Input assembly
    VkPipelineInputAssemblyStateCreateInfo input_assembly{};
    input_assembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    input_assembly.topology = cfg->input_assembly.primitive_topology;
    input_assembly.primitiveRestartEnable = cfg->input_assembly.primitive_restart_enable;

    // Viewport and scissors (default)
    VkPipelineViewportStateCreateInfo viewport_state{};
    viewport_state.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewport_state.viewportCount = (u32)cfg->viewports.size;
    viewport_state.pViewports = cfg->viewports.data;
    viewport_state.scissorCount = (u32)cfg->scissors.size;
    viewport_state.pScissors = cfg->scissors.data;

    // Rasterizer
    VkPipelineRasterizationStateCreateInfo rstr{};
    rstr.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rstr.depthClampEnable = cfg->raster.depth_clamp_enable;
    rstr.rasterizerDiscardEnable = cfg->raster.rasterizer_discard_enable;
    rstr.polygonMode = cfg->raster.polygon_mode;
    rstr.lineWidth = cfg->raster.line_width;
    rstr.cullMode = cfg->raster.cull_mode;
    rstr.frontFace = cfg->raster.front_face;
    rstr.depthBiasEnable = cfg->raster.depth_bias_enable;
    rstr.depthBiasConstantFactor = cfg->raster.depth_bias_constant_factor;
    rstr.depthBiasClamp = cfg->raster.depth_bias_clamp;
    rstr.depthBiasSlopeFactor = cfg->raster.depth_bias_slope_factor;

    // Multisampling
    VkPipelineMultisampleStateCreateInfo multisampling{};
    multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisampling.sampleShadingEnable = cfg->multisampling.sample_shading_enable;
    multisampling.rasterizationSamples = cfg->multisampling.rasterization_samples;
    multisampling.minSampleShading = cfg->multisampling.min_sample_shading;
    multisampling.pSampleMask = cfg->multisampling.sample_masks;
    multisampling.alphaToCoverageEnable = cfg->multisampling.alpha_to_coverage_enable;
    multisampling.alphaToOneEnable = cfg->multisampling.alpha_to_one_enable;

    // Color blending
    VkPipelineColorBlendStateCreateInfo col_blend_state{};
    col_blend_state.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    col_blend_state.logicOpEnable = cfg->col_blend.logic_op_enabled;
    col_blend_state.logicOp = cfg->col_blend.logic_op;
    col_blend_state.attachmentCount = (u32)cfg->col_blend.attachments.size;
    col_blend_state.pAttachments = cfg->col_blend.attachments.data;
    for (int i = 0; i < cfg->col_blend.blend_constants.size(); ++i) {
        col_blend_state.blendConstants[i] = cfg->col_blend.blend_constants[i];
    }

    // Depth Stencil
    VkPipelineDepthStencilStateCreateInfo depth_stencil{};
    depth_stencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    depth_stencil.flags = cfg->depth_stencil.flags;
    depth_stencil.depthTestEnable = cfg->depth_stencil.depth_test_enable;
    depth_stencil.depthWriteEnable = cfg->depth_stencil.depth_write_enable;
    depth_stencil.depthCompareOp = cfg->depth_stencil.depth_compare_op;
    depth_stencil.depthBoundsTestEnable = cfg->depth_stencil.depth_bounds_test_enable;
    depth_stencil.stencilTestEnable = cfg->depth_stencil.stencil_test_enable;
    depth_stencil.front = cfg->depth_stencil.front;
    depth_stencil.back = cfg->depth_stencil.back;
    depth_stencil.minDepthBounds = cfg->depth_stencil.min_depth_bounds;
    depth_stencil.maxDepthBounds = cfg->depth_stencil.max_depth_bounds;

    // Create the descriptor set layouts
    for (int desc_i = 0; desc_i < cfg->set_layouts.size; ++desc_i) {
        VkDescriptorSetLayoutCreateInfo ci{};
        ci.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        ci.bindingCount = (u32)cfg->set_layouts[desc_i].bindings.size;
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
    pipeline_layout_create_inf.setLayoutCount = (u32)pipe_info->descriptor_layouts.size;
    pipeline_layout_create_inf.pSetLayouts = pipe_info->descriptor_layouts.data;
    pipeline_layout_create_inf.pushConstantRangeCount = (u32)cfg->push_constant_ranges.size;
    pipeline_layout_create_inf.pPushConstantRanges = cfg->push_constant_ranges.data;

    if (vkCreatePipelineLayout(vk->inst.device.hndl, &pipeline_layout_create_inf, &vk->alloc_cbs, &pipe_info->layout_hndl) != VK_SUCCESS) {
        elog("Failed to create pileline layout");
        for (u32 si = 0; si < actual_stagei; ++si) {
            vkr_terminate_shader_module(stages[si].module, vk);
        }
        for (u32 i = 0; i < pipe_info->descriptor_layouts.size; ++i) {
            vkDestroyDescriptorSetLayout(vk->inst.device.hndl, pipe_info->descriptor_layouts[i], &vk->alloc_cbs);
        }
        return err_code::VKR_CREATE_PIPELINE_LAYOUT_FAIL;
    }

    VkGraphicsPipelineCreateInfo pipeline_info{};
    pipeline_info.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipeline_info.stageCount = (u32)actual_stagei;
    pipeline_info.pStages = stages;                       // done
    pipeline_info.pVertexInputState = &vertex_input_info; // done
    pipeline_info.pInputAssemblyState = &input_assembly;
    pipeline_info.pViewportState = &viewport_state;
    pipeline_info.pRasterizationState = &rstr;
    pipeline_info.pMultisampleState = &multisampling;
    pipeline_info.pDepthStencilState = &depth_stencil;
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
            vkr_terminate_shader_module(stages[si].module, vk);
        }
        for (u32 i = 0; i < pipe_info->descriptor_layouts.size; ++i) {
            vkDestroyDescriptorSetLayout(vk->inst.device.hndl, pipe_info->descriptor_layouts[i], &vk->alloc_cbs);
        }
        vkDestroyPipelineLayout(vk->inst.device.hndl, pipe_info->layout_hndl, &vk->alloc_cbs);
        err_ret = err_code::VKR_CREATE_PIPELINE_FAIL;
    }
    for (u32 si = 0; si < actual_stagei; ++si) {
        vkr_terminate_shader_module(stages[si].module, vk);
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

void vkr_terminate_pipeline(const vkr_pipeline *pipe_info, const vkr_context *vk)
{
    ilog("Terminating pipeline");
    vkDestroyPipeline(vk->inst.device.hndl, pipe_info->hndl, &vk->alloc_cbs);
    vkDestroyPipelineLayout(vk->inst.device.hndl, pipe_info->layout_hndl, &vk->alloc_cbs);
    for (int i = 0; i < pipe_info->descriptor_layouts.size; ++i) {
        vkDestroyDescriptorSetLayout(vk->inst.device.hndl, pipe_info->descriptor_layouts[i], &vk->alloc_cbs);
    }
}

int vkr_init_framebuffer(vkr_framebuffer *fb, const vkr_framebuffer_cfg *cfg, const vkr_context *vk)
{
    ilog("Initializing framebuffer");
    asrt(cfg->rpass);
    asrt(cfg->attachments);

    arr_init(&fb->attachments, vk->cfg.arenas.persistent_arena);
    fb->size = cfg->size;
    fb->rpass = *cfg->rpass;
    fb->layers = fb->layers;

    arr_copy(&fb->attachments, cfg->attachments, cfg->attachment_count);

    array<VkImageView> att;
    arr_init(&att, vk->cfg.arenas.command_arena);
    arr_resize(&att, cfg->attachment_count);
    for (int i = 0; i < att.size; ++i) {
        att[i] = cfg->attachments[i].iview.hndl;
    }

    VkFramebufferCreateInfo create_info{};
    create_info.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
    create_info.renderPass = cfg->rpass->hndl;
    create_info.pAttachments = att.data;
    create_info.attachmentCount = (u32)att.size;
    create_info.width = cfg->size.x;
    create_info.height = cfg->size.y;
    create_info.layers = cfg->layers;
    int res = vkCreateFramebuffer(vk->inst.device.hndl, &create_info, &vk->alloc_cbs, &fb->hndl);
    if (res != VK_SUCCESS) {
        elog("Failed to create framebuffer with vk err %d", res);
        return err_code::VKR_CREATE_FRAMEBUFFER_FAIL;
    }
    ilog("Successfully initialized framebuffer");
    arr_terminate(&att);
    return err_code::VKR_NO_ERROR;
}

sizet vkr_add_framebuffer(vkr_device *device, const vkr_framebuffer &copy)
{
    ilog("Adding framebuffer to device");
    sizet ind = device->framebuffers.size;
    arr_push_back(&device->framebuffers, copy);
    return ind;
}

void vkr_terminate_framebuffer(vkr_framebuffer *fb, const vkr_context *vk)
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

int vkr_init_descriptor_pool(vkr_descriptor_pool *desc_pool, const vkr_context *vk, const vkr_descriptor_cfg *cfg)
{
    arr_init(&desc_pool->desc_sets, vk->cfg.arenas.persistent_arena);

    // Get a count of the number of descriptors we are making avaialable for each desc type
    u32 total_desc_cnt{};
    VkDescriptorPoolSize psize[VKR_DESCRIPTOR_TYPE_COUNT] = {};
    u32 desc_size_count{};
    for (int desc_t = 0; desc_t < VKR_DESCRIPTOR_TYPE_COUNT; ++desc_t) {
        if (cfg->max_desc_per_type[desc_t] > 0) {
            psize[desc_size_count].descriptorCount = cfg->max_desc_per_type[desc_t];
            psize[desc_size_count].type = (VkDescriptorType)desc_t;
            total_desc_cnt += cfg->max_desc_per_type[desc_t];
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
    pool_info.flags = cfg->flags;
    pool_info.pPoolSizes = psize;
    pool_info.maxSets = (cfg->max_sets != VKR_INVALID) ? cfg->max_sets : total_desc_cnt;
    ilog("Setting max sets to %u (for %u total descriptors)", pool_info.maxSets, total_desc_cnt);

    int err = vkCreateDescriptorPool(vk->inst.device.hndl, &pool_info, &vk->alloc_cbs, &desc_pool->hndl);
    if (err != VK_SUCCESS) {
        elog("Failed to create descriptor pool with vk err code %d", err);
        return err_code::VKR_CREATE_DESCRIPTOR_POOL_FAIL;
    }
    return err_code::VKR_NO_ERROR;
}

void vkr_reset_descriptor_pool(vkr_descriptor_pool *desc_pool, const vkr_context *vk)
{
    vkResetDescriptorPool(vk->inst.device.hndl, desc_pool->hndl, {});
    arr_clear(&desc_pool->desc_sets);
}

void vkr_terminate_descriptor_pool(vkr_descriptor_pool *desc_pool, const vkr_context *vk)
{
    vkDestroyDescriptorPool(vk->inst.device.hndl, desc_pool->hndl, &vk->alloc_cbs);
    arr_terminate(&desc_pool->desc_sets);
}

void *vkr_map_buffer(vkr_buffer *buf, const vkr_gpu_allocator *vma)
{
    void *ret;
    vmaMapMemory(vma->hndl, buf->mem_hndl, &ret);
    return ret;
}

void vkr_unmap_buffer(vkr_buffer *buf, const vkr_gpu_allocator *vma)
{
    vmaUnmapMemory(vma->hndl, buf->mem_hndl);
}

int vkr_stage_and_upload_buffer_data(vkr_buffer *dest_buffer,
                                     const void *src_data,
                                     sizet src_data_size,
                                     const VkBufferCopy *region,
                                     vkr_device_queue_fam_info *cmd_q,
                                     sizet qind,
                                     const vkr_context *vk)
{
    vkr_buffer staging_buf{};
    vkr_buffer_cfg buf_cfg{};
    buf_cfg.buffer_size = src_data_size;
    buf_cfg.alloc_flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;
    buf_cfg.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
    buf_cfg.mem_usage = VMA_MEMORY_USAGE_AUTO_PREFER_HOST;
    buf_cfg.sharing_mode = VK_SHARING_MODE_EXCLUSIVE;
    buf_cfg.vma_alloc = &vk->inst.device.vma_alloc;
    int err = vkr_init_buffer(&staging_buf, &buf_cfg);
    if (err != err_code::VKR_NO_ERROR) {
        return err;
    }

    void *mem = vkr_map_buffer(&staging_buf, &vk->inst.device.vma_alloc);
    memcpy(mem, src_data, src_data_size);
    vkr_unmap_buffer(&staging_buf, &vk->inst.device.vma_alloc);

    err = vkr_copy_buffer(dest_buffer, &staging_buf, region, cmd_q, qind, vk);
    vkr_terminate_buffer(&staging_buf, vk);
    return err;
}

int vkr_stage_and_upload_buffer_data(vkr_buffer *dest_buffer,
                                     const void *src_data,
                                     sizet src_data_size,
                                     vkr_device_queue_fam_info *cmd_q,
                                     sizet qind,
                                     const vkr_context *vk)
{
    VkBufferCopy region{};
    region.size = src_data_size;
    return vkr_stage_and_upload_buffer_data(dest_buffer, src_data, src_data_size, &region, cmd_q, qind, vk);
}

sizet vkr_add_buffer(vkr_device *device, const vkr_buffer &copy)
{
    ilog("Adding buffer to device");
    sizet ind = device->buffers.size;
    arr_push_back(&device->buffers, copy);
    return ind;
}

int vkr_init_buffer(vkr_buffer *buffer, const vkr_buffer_cfg *cfg)
{
    asrt(cfg->vma_alloc);

    VkBufferCreateInfo cinfo{};
    cinfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    cinfo.size = cfg->buffer_size;
    cinfo.usage = cfg->usage;
    cinfo.sharingMode = cfg->sharing_mode;
    cinfo.flags = cfg->buf_create_flags;

    VmaAllocationCreateInfo alloc_info = {};
    alloc_info.flags = cfg->alloc_flags;
    alloc_info.usage = cfg->mem_usage;
    alloc_info.requiredFlags = cfg->required_flags;
    alloc_info.preferredFlags = cfg->preferred_flags;

    int err = vmaCreateBuffer(cfg->vma_alloc->hndl, &cinfo, &alloc_info, &buffer->hndl, &buffer->mem_hndl, &buffer->mem_info);
    if (err != VK_SUCCESS) {
        elog("Failed in creating buffer with vk err %d", err);
        return err_code::VKR_CREATE_BUFFER_FAIL;
    }
    return err_code::VKR_NO_ERROR;
}

void vkr_terminate_buffer(vkr_buffer *buffer, const vkr_context *vk)
{
    ilog("Terminating buffer");
    vmaDestroyBuffer(vk->inst.device.vma_alloc.hndl, buffer->hndl, buffer->mem_hndl);
}

sizet vkr_add_image(vkr_device *device, const vkr_image &copy)
{
    ilog("Adding image to device");
    sizet ind = device->images.size;
    arr_push_back(&device->images, copy);
    return ind;
}

int vkr_init_image(vkr_image *image, const vkr_image_cfg *cfg)
{
    asrt(cfg->vma_alloc);
    VkImageCreateInfo cinfo{};
    cinfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    cinfo.imageType = cfg->type;
    cinfo.extent.width = cfg->dims.x;
    cinfo.extent.height = cfg->dims.y;
    cinfo.extent.depth = cfg->dims.z;
    cinfo.mipLevels = cfg->mip_levels;
    cinfo.arrayLayers = cfg->array_layers;
    cinfo.flags = cfg->im_create_flags;
    cinfo.initialLayout = cfg->initial_layout;
    cinfo.format = cfg->format;
    cinfo.tiling = cfg->tiling;
    cinfo.usage = cfg->usage;
    cinfo.sharingMode = cfg->sharing_mode;
    cinfo.samples = cfg->samples;

    VmaAllocationCreateInfo alloc_info = {};
    alloc_info.flags = cfg->alloc_flags;
    alloc_info.usage = cfg->mem_usage;
    alloc_info.requiredFlags = cfg->required_flags;
    alloc_info.preferredFlags = cfg->preferred_flags;

    image->format = cinfo.format;
    image->dims = cfg->dims;
    image->vma_alloc = cfg->vma_alloc;

    int err = vmaCreateImage(cfg->vma_alloc->hndl, &cinfo, &alloc_info, &image->hndl, &image->mem_hndl, &image->mem_info);
    if (err != VK_SUCCESS) {
        elog("Failed in creating image with vk err %d", err);
        return err_code::VKR_CREATE_IMAGE_FAIL;
    }
    return err_code::VKR_NO_ERROR;
}

void vkr_terminate_image(vkr_image *image)
{
    ilog("Terminating image");
    vmaDestroyImage(image->vma_alloc->hndl, image->hndl, image->mem_hndl);
}

sizet vkr_add_image_view(vkr_device *device, const vkr_image_view &copy)
{
    ilog("Adding image view to device");
    sizet ind = device->image_views.size;
    arr_push_back(&device->image_views, copy);
    return ind;
}

int vkr_init_image_view(vkr_image_view *iview, const vkr_image_view_cfg *cfg, const vkr_context *vk)
{
    asrt(cfg->image);
    iview->dev = &vk->inst.device;
    iview->alloc_cbs = &vk->alloc_cbs;    
    VkImageViewCreateInfo create_info{};
    create_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    create_info.image = cfg->image->hndl;
    create_info.viewType = cfg->view_type;
    create_info.components = cfg->components;
    create_info.flags = cfg->create_flags;
    create_info.format = cfg->image->format;
    create_info.subresourceRange = cfg->srange;
    int err = vkCreateImageView(vk->inst.device.hndl, &create_info, iview->alloc_cbs, &iview->hndl);
    if (err != VK_SUCCESS) {
        wlog("Failed creating image view with vk error code %d", err);
        return err_code::VKR_CREATE_IMAGE_VIEW_FAIL;
    }
    return err_code::VKR_NO_ERROR;
}

void vkr_terminate_image_view(vkr_image_view *iview)
{
    ilog("Terminating image view");
    vkDestroyImageView(iview->dev->hndl, iview->hndl, iview->alloc_cbs);
}

sizet vkr_add_sampler(vkr_device *device, const vkr_sampler &copy)
{
    ilog("Adding image sampler to device");
    sizet ind = device->samplers.size;
    arr_push_back(&device->samplers, copy);
    return ind;
}

int vkr_init_sampler(vkr_sampler *sampler, const vkr_sampler_cfg *cfg, const vkr_context *vk)
{
    sampler->dev = &vk->inst.device;
    sampler->alloc_cbs = &vk->alloc_cbs;
    VkSamplerCreateInfo create_info{};
    create_info.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    create_info.addressModeU = cfg->address_mode_uvw[0];
    create_info.addressModeV = cfg->address_mode_uvw[1];
    create_info.addressModeW = cfg->address_mode_uvw[2];
    create_info.magFilter = cfg->mag_filter;
    create_info.minFilter = cfg->min_filter;
    create_info.mipmapMode = cfg->mipmap_mode;
    create_info.flags = cfg->flags;
    create_info.mipLodBias = cfg->mip_lod_bias;
    create_info.anisotropyEnable = cfg->anisotropy_enable;
    create_info.maxAnisotropy = cfg->max_anisotropy;
    create_info.compareEnable = cfg->compare_enable;
    create_info.compareOp = cfg->compare_op;
    create_info.minLod = cfg->min_lod;
    create_info.maxLod = cfg->max_lod;
    create_info.borderColor = cfg->border_color;
    create_info.unnormalizedCoordinates = cfg->unnormalized_coords;
    if (!vk->inst.pdev_info.features.samplerAnisotropy) {
        create_info.anisotropyEnable = false;
        create_info.maxAnisotropy = 1.0f;
    }

    int err = vkCreateSampler(vk->inst.device.hndl, &create_info, &vk->alloc_cbs, &sampler->hndl);
    if (err != VK_SUCCESS) {
        wlog("Failed creating sampler with vk error code %d", err);
        return err_code::VKR_CREATE_SAMPLER_FAIL;
    }
    return err_code::VKR_NO_ERROR;
}

void vkr_terminate_sampler(vkr_sampler *sampler)
{
    ilog("Terminating image view");
    vkDestroySampler(sampler->dev->hndl, sampler->hndl, sampler->alloc_cbs);
}

int vkr_stage_and_upload_image_data(vkr_image *dest_buffer,
                                    const void *src_data,
                                    sizet src_data_size,
                                    vkr_device_queue_fam_info *cmd_q,
                                    sizet qind,
                                    const vkr_context *vk)
{
    VkBufferImageCopy region{};
    region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    region.imageSubresource.layerCount = 1;
    region.imageExtent = {dest_buffer->dims.x, dest_buffer->dims.y, dest_buffer->dims.z};
    return vkr_stage_and_upload_image_data(dest_buffer, src_data, src_data_size, &region, cmd_q, qind, vk);
}

int vkr_stage_and_upload_image_data(vkr_image *dest_buffer,
                                    const void *src_data,
                                    sizet src_data_size,
                                    const VkBufferImageCopy *region,
                                    vkr_device_queue_fam_info *cmd_q,
                                    sizet qind,
                                    const vkr_context *vk)
{
    vkr_buffer staging_buf{};
    vkr_buffer_cfg buf_cfg{};
    buf_cfg.buffer_size = src_data_size;
    buf_cfg.alloc_flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;
    buf_cfg.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
    buf_cfg.mem_usage = VMA_MEMORY_USAGE_AUTO_PREFER_HOST;
    buf_cfg.sharing_mode = VK_SHARING_MODE_EXCLUSIVE;
    buf_cfg.vma_alloc = &vk->inst.device.vma_alloc;
    int err = vkr_init_buffer(&staging_buf, &buf_cfg);
    if (err != err_code::VKR_NO_ERROR) {
        return err;
    }

    void *mem = vkr_map_buffer(&staging_buf, &vk->inst.device.vma_alloc);
    memcpy(mem, src_data, src_data_size);
    vkr_unmap_buffer(&staging_buf, &vk->inst.device.vma_alloc);

    // Tranition image to correct layout
    vkr_image_transition_cfg trans_cfg{};
    trans_cfg.old_layout = VK_IMAGE_LAYOUT_UNDEFINED;
    trans_cfg.new_layout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    trans_cfg.srange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    trans_cfg.srange.layerCount = 1;
    trans_cfg.srange.levelCount = 1;

    err = vkr_transition_image_layout(dest_buffer, &trans_cfg, cmd_q, qind, vk);
    if (err != err_code::VKR_NO_ERROR) {
        vkr_terminate_buffer(&staging_buf, vk);
        return err;
    }

    err = vkr_copy_buffer_to_image(dest_buffer, &staging_buf, region, cmd_q, qind, vk);
    if (err != err_code::VKR_NO_ERROR) {
        vkr_terminate_buffer(&staging_buf, vk);
        return err;
    }

    trans_cfg.old_layout = trans_cfg.new_layout;
    trans_cfg.new_layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    err = vkr_transition_image_layout(dest_buffer, &trans_cfg, cmd_q, qind, vk);
    vkr_terminate_buffer(&staging_buf, vk);
    return err;
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
                                     const array<array<vkr_framebuffer_attachment>> *other_attachments,
                                     sizet fb_offset)
{
    for (int i = 0; i < vk->inst.device.swapchain.image_views.size; ++i) {
        vkr_framebuffer_cfg cfg{};
        cfg.size = {vk->inst.device.swapchain.extent.width, vk->inst.device.swapchain.extent.height};
        cfg.rpass = rpass;
        array<vkr_framebuffer_attachment> atts;
        arr_init(&atts, vk->cfg.arenas.command_arena);

        vkr_framebuffer_attachment col_att{};
        col_att.iview = device->swapchain.image_views[i];
        arr_push_back(&atts, col_att);
        if (other_attachments) {
            arr_append(&atts, (*other_attachments)[i].data, (*other_attachments)[i].size);
        }
        cfg.attachment_count = (u32)atts.size;
        cfg.attachments = atts.data;
        vkr_init_framebuffer(&device->framebuffers[fb_offset + i], &cfg, vk);
        arr_terminate(&atts);
    }
}

void vkr_init_swapchain_framebuffers(vkr_device *device,
                                     const vkr_context *vk,
                                     const vkr_rpass *rpass,
                                     const vkr_framebuffer_attachment &other_attachment,
                                     sizet fb_offset)
{
    array<array<vkr_framebuffer_attachment>> other_atts;
    arr_init(&other_atts, vk->cfg.arenas.command_arena);
    arr_resize(&other_atts, device->swapchain.image_views.size);
    for (int i = 0; i < other_atts.size; ++i) {
        arr_init(&other_atts[i], vk->cfg.arenas.command_arena);
        arr_emplace_back(&other_atts[i], other_attachment);
    }
    vkr_init_swapchain_framebuffers(device, vk, rpass, &other_atts, fb_offset);
    for (int i = 0; i < other_atts.size; ++i) {
        arr_terminate(&other_atts[i]);
    }
    arr_terminate(&other_atts);
}

void vkr_init_swapchain_framebuffers(vkr_device *device,
                                     const vkr_context *vk,
                                     const vkr_rpass *rpass,
                                     const array<vkr_framebuffer_attachment> &other_attachments,
                                     sizet fb_offset)
{
    array<array<vkr_framebuffer_attachment>> other_atts;
    arr_init(&other_atts, vk->cfg.arenas.command_arena);
    arr_resize(&other_atts, device->swapchain.image_views.size);
    for (int i = 0; i < other_atts.size; ++i) {
        arr_init(&other_atts[i], vk->cfg.arenas.command_arena);
        arr_append(&other_atts[i], other_attachments.data, other_attachments.size);
    }
    vkr_init_swapchain_framebuffers(device, vk, rpass, &other_atts, fb_offset);
    for (int i = 0; i < other_atts.size; ++i) {
        arr_terminate(&other_atts[i]);
    }
    arr_terminate(&other_atts);
}

void vkr_terminate_swapchain_framebuffers(vkr_device *device, const vkr_context *vk, sizet fb_offset)
{
    for (int i = 0; i < vk->inst.device.swapchain.image_views.size; ++i) {
        vkr_terminate_framebuffer(&device->framebuffers[i + fb_offset], vk);
        device->framebuffers[i + fb_offset] = {};
    }
}

// Initialize surface in the vk_context from the window - the instance must have been created already
int vkr_init_surface(const vkr_context *vk, VkSurfaceKHR *surface)
{
    ilog("Initializing window surface");
    asrt(vk->cfg.window);
    // Create surface
    bool ret = SDL_Vulkan_CreateSurface((SDL_Window *)vk->cfg.window, vk->inst.hndl, &vk->alloc_cbs, surface);
    if (!ret) {
        log_any_sdl_error("Failed to create surface");
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

void vkr_terminate_render_frames(vkr_device *dev, const vkr_context *vk)
{
    for (int framei = 0; framei < dev->rframes.size; ++framei) {
        vkr_terminate_descriptor_pool(&dev->rframes[framei].desc_pool, vk);
        vkDestroySemaphore(dev->hndl, dev->rframes[framei].image_avail, &vk->alloc_cbs);
        vkDestroySemaphore(dev->hndl, dev->rframes[framei].render_finished, &vk->alloc_cbs);
        vkDestroyFence(dev->hndl, dev->rframes[framei].in_flight, &vk->alloc_cbs);
    }
}

int vkr_init_render_frames(vkr_device *dev, const vkr_context *vk)
{
    dev->rframes.size = dev->rframes.capacity;

    // Create our rframes command buffers
    auto gfx_fam = &dev->qfams[VKR_QUEUE_FAM_TYPE_GFX];

    // Add a command buffer for each frame - assign to the actual frames in the loop below
    auto buf_res = vkr_add_cmd_bufs(&gfx_fam->cmd_pools[gfx_fam->default_pool], vk, dev->rframes.size);
    asrt(buf_res.err_code == err_code::VKR_NO_ERROR);
    asrt((buf_res.end - buf_res.begin) == dev->rframes.size);

    // Create frame synchronization objects - start all fences as signalled already
    for (u32 framei = 0; framei < dev->rframes.size; ++framei) {
        VkSemaphoreCreateInfo sem_info{};
        sem_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

        int err = vkCreateSemaphore(dev->hndl, &sem_info, &vk->alloc_cbs, &dev->rframes[framei].image_avail);
        if (err != VK_SUCCESS) {
            elog("Failed to create image semaphore for frame %d with vk err code %d", framei, err);
            vkr_terminate_device(dev, vk);
            return err_code::VKR_CREATE_SEMAPHORE_FAIL;
        }

        err = vkCreateSemaphore(dev->hndl, &sem_info, &vk->alloc_cbs, &dev->rframes[framei].render_finished);
        if (err != VK_SUCCESS) {
            elog("Failed to create render semaphore for frame %d with vk err code %d", framei, err);
            vkr_terminate_device(dev, vk);
            return err_code::VKR_CREATE_SEMAPHORE_FAIL;
        }

        VkFenceCreateInfo fence_info{};
        fence_info.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
        fence_info.flags = VK_FENCE_CREATE_SIGNALED_BIT;

        err = vkCreateFence(dev->hndl, &fence_info, &vk->alloc_cbs, &dev->rframes[framei].in_flight);
        if (err != VK_SUCCESS) {
            elog("Failed to create flight fence for frame %d with vk err code %d", framei, err);
            vkr_terminate_device(dev, vk);
            return err_code::VKR_CREATE_FENCE_FAIL;
        }

        // Assign the command buffer created earlier to this frame
        dev->rframes[framei].cmd_buf_ind = {VKR_QUEUE_FAM_TYPE_GFX, gfx_fam->default_pool, buf_res.begin + framei};

        // Get a count of the number of descriptors we are making avaialable for each desc type
        err = vkr_init_descriptor_pool(&dev->rframes[framei].desc_pool, vk, &vk->cfg.desc_cfg);
        if (err != VK_SUCCESS) {
            elog("Failed to create descriptor pool for frame %d - aborting init", framei);
            vkr_terminate_device(dev, vk);
            return err;
        }
    }
    ilog("Successfully initialized %lu render frames in flight", dev->rframes.size);
    return err_code::VKR_NO_ERROR;
}

int vkr_init(const vkr_cfg *cfg, vkr_context *vk)
{
    ilog("Initializing vulkan");
    asrt(cfg);
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
        code = vkr_init_surface(vk, &vk->inst.surface);
        if (code != err_code::VKR_NO_ERROR) {
            vkr_terminate(vk);
            return code;
        }
    }

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
        vkr_terminate_image_view(&sw_info->image_views[i]);
    }
    vkDestroySwapchainKHR(vk->inst.device.hndl, sw_info->swapchain, &vk->alloc_cbs);
    arr_terminate(&sw_info->images);
    arr_terminate(&sw_info->image_views);
}

void vkr_device_wait_idle(vkr_device *dev)
{
    vkDeviceWaitIdle(dev->hndl);
}

void vkr_terminate_device(vkr_device *dev, const vkr_context *vk)
{
    ilog("Terminating vkr device");
    vkr_terminate_render_frames(dev, vk);
    vkr_terminate_swapchain(&dev->swapchain, vk);

    for (int i = 0; i < dev->render_passes.size; ++i) {
        vkr_terminate_render_pass(&dev->render_passes[i], vk);
    }
    for (int i = 0; i < dev->framebuffers.size; ++i) {
        vkr_terminate_framebuffer(&dev->framebuffers[i], vk);
    }
    for (int i = 0; i < dev->pipelines.size; ++i) {
        vkr_terminate_pipeline(&dev->pipelines[i], vk);
    }
    for (int i = 0; i < dev->buffers.size; ++i) {
        vkr_terminate_buffer(&dev->buffers[i], vk);
    }
    for (int i = 0; i < dev->images.size; ++i) {
        vkr_terminate_image(&dev->images[i]);
    }
    for (int i = 0; i < dev->image_views.size; ++i) {
        vkr_terminate_image_view(&dev->image_views[i]);
    }
    for (int i = 0; i < dev->samplers.size; ++i) {
        vkr_terminate_sampler(&dev->samplers[i]);
    }

    arr_terminate(&dev->render_passes);
    arr_terminate(&dev->framebuffers);
    arr_terminate(&dev->pipelines);
    arr_terminate(&dev->buffers);
    arr_terminate(&dev->images);
    arr_terminate(&dev->image_views);
    arr_terminate(&dev->samplers);

    for (sizet qfam_i = 0; qfam_i < VKR_QUEUE_FAM_TYPE_COUNT; ++qfam_i) {
        auto cur_fam = &dev->qfams[qfam_i];
        arr_terminate(&cur_fam->qs);
        for (sizet pool_i = 0; pool_i < cur_fam->cmd_pools.size; ++pool_i) {
            auto cur_pool = &cur_fam->cmd_pools[pool_i];
            vkr_terminate_cmd_pool(vk, cur_fam->fam_ind, cur_pool);
        }
        arr_terminate(&cur_fam->cmd_pools);
    }

    vmaDestroyAllocator(dev->vma_alloc.hndl);
    vkDestroyDevice(dev->hndl, &vk->alloc_cbs);
}

intern void log_mem_stats(const char *type, const vk_mem_alloc_stats *stats)
{
    ilog("%s alloc_count:%d free_count:%d realloc_count:%d", type, stats->alloc_count, stats->free_count, stats->realloc_count);
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
    if (inst->device.hndl != VK_NULL_HANDLE) {
        vkr_terminate_device(&inst->device, vk);
    }
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
    ilog("Persistant mem size:%lu peak:%lu  Command mem size:%lu peak:%lu",
         vk->cfg.arenas.persistent_arena->total_size,
         vk->cfg.arenas.persistent_arena->peak,
         vk->cfg.arenas.command_arena->total_size,
         vk->cfg.arenas.command_arena->peak);
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

sizet vkr_min_uniform_buffer_offset_alignment(vkr_context *vk)
{
    return vk->inst.pdev_info.props.limits.minUniformBufferOffsetAlignment;
}

sizet vkr_uniform_buffer_offset_alignment(vkr_context *vk, sizet uniform_block_size)
{
    auto min_alignment = vkr_min_uniform_buffer_offset_alignment(vk);
    if (uniform_block_size % min_alignment == 0) {
        return uniform_block_size;
    }
    else {
        return (uniform_block_size / min_alignment + 1) * min_alignment;
    }
}

void vkr_cmd_begin_rpass(const vkr_command_buffer *cmd_buf,
                         const vkr_framebuffer *fb,
                         const vkr_rpass *rpass,
                         const VkClearValue *att_clear_vals,
                         sizet clear_val_size)
{
    VkRenderPassBeginInfo info{};
    info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    info.renderPass = rpass->hndl;
    info.framebuffer = fb->hndl;
    info.renderArea.extent = {fb->size.w, fb->size.h};

    // TODO: Figure out what all these different things are with regards to screen size...
    // Viewport.. ImageView extent.. framebuffer size.. render area..
    info.renderArea.offset = {0, 0};

    // Set the clear values
    info.clearValueCount = (u32)clear_val_size;
    info.pClearValues = att_clear_vals;

    vkCmdBeginRenderPass(cmd_buf->hndl, &info, VK_SUBPASS_CONTENTS_INLINE);
}

void vkr_cmd_end_rpass(const vkr_command_buffer *cmd_buf)
{
    vkCmdEndRenderPass(cmd_buf->hndl);
}

intern vkr_add_result cmd_buf_begin(vkr_command_pool *pool, const vkr_context *vk)
{
    vkr_add_result tmp_buf = vkr_add_cmd_bufs(pool, vk);
    if (tmp_buf.err_code != err_code::VKR_NO_ERROR) {
        return tmp_buf;
    }
    VkCommandBufferBeginInfo beg_info{};
    beg_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beg_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    tmp_buf.err_code = vkBeginCommandBuffer(pool->buffers[tmp_buf.begin].hndl, &beg_info);
    if (tmp_buf.err_code != VK_SUCCESS) {
        tmp_buf.err_code = err_code::VKR_COPY_BUFFER_BEGIN_FAIL;
    }
    return tmp_buf;
}

intern int cmd_buf_end(vkr_add_result tmp_buf, vkr_command_pool *pool, vkr_device_queue_fam_info *cmd_q, sizet qind, const vkr_context *vk)
{
    vkEndCommandBuffer(pool->buffers[tmp_buf.begin].hndl);

    VkSubmitInfo submit_info{};
    submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submit_info.commandBufferCount = 1;
    submit_info.pCommandBuffers = &pool->buffers[tmp_buf.begin].hndl;

    int err = vkQueueSubmit(cmd_q->qs[qind].hndl, 1, &submit_info, VK_NULL_HANDLE);
    if (err != VK_SUCCESS) {
        wlog("Failed to submit queue with vulkan error %d", err);
        vkr_remove_cmd_bufs(pool, vk, tmp_buf.begin);
        return err_code::VKR_COPY_BUFFER_SUBMIT_FAIL;
    }

    err = vkQueueWaitIdle(cmd_q->qs[qind].hndl);
    if (err != VK_SUCCESS) {
        wlog("Failed to wait idle with vulkan error %d", err);
        vkr_remove_cmd_bufs(pool, vk, tmp_buf.begin);
        return err_code::VKR_COPY_BUFFER_WAIT_IDLE_FAIL;
    }
    vkr_remove_cmd_bufs(pool, vk, tmp_buf.begin);
    return err_code::VKR_NO_ERROR;
}

int vkr_copy_buffer(vkr_buffer *dest,
                    const vkr_buffer *src,
                    const VkBufferCopy *region,
                    vkr_device_queue_fam_info *cmd_q,
                    sizet qind,
                    const vkr_context *vk)
{
    ilog("Starting buffer copy");
    auto pool = &cmd_q->cmd_pools[cmd_q->transient_pool];
    auto tmp_buf = cmd_buf_begin(pool, vk);
    if (tmp_buf.err_code == err_code::VKR_NO_ERROR) {
        vkCmdCopyBuffer(pool->buffers[tmp_buf.begin].hndl, src->hndl, dest->hndl, 1, region);
        ilog("Finished copying buffer");
        return cmd_buf_end(tmp_buf, pool, cmd_q, qind, vk);
    }
    return tmp_buf.err_code;
}

int vkr_copy_buffer_to_image(vkr_image *dest,
                             const vkr_buffer *src,
                             const VkBufferImageCopy *region,
                             vkr_device_queue_fam_info *cmd_q,
                             sizet qind,
                             const vkr_context *vk)
{
    ilog("Copying buffer to image");
    auto pool = &cmd_q->cmd_pools[cmd_q->transient_pool];
    auto tmp_buf = cmd_buf_begin(pool, vk);
    if (tmp_buf.err_code == err_code::VKR_NO_ERROR) {
        vkCmdCopyBufferToImage(pool->buffers[tmp_buf.begin].hndl, src->hndl, dest->hndl, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, region);
        ilog("Finished copying buffer to image");
        return cmd_buf_end(tmp_buf, pool, cmd_q, qind, vk);
    }
    return tmp_buf.err_code;
}

int vkr_transition_image_layout(const vkr_image *image,
                                const vkr_image_transition_cfg *cfg,
                                vkr_device_queue_fam_info *cmd_q,
                                sizet qind,
                                const vkr_context *vk)
{
    ilog("Transitioning image layout");
    auto pool = &cmd_q->cmd_pools[cmd_q->transient_pool];
    auto tmp_buf = cmd_buf_begin(pool, vk);
    if (tmp_buf.err_code == err_code::VKR_NO_ERROR) {
        VkImageMemoryBarrier barrier{};
        barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        barrier.oldLayout = cfg->old_layout;
        barrier.newLayout = cfg->new_layout;
        barrier.srcQueueFamilyIndex = cfg->src_fam_index;
        barrier.dstQueueFamilyIndex = cfg->dest_fam_index;
        barrier.image = image->hndl;
        barrier.subresourceRange = cfg->srange;

        VkPipelineStageFlags source_stage;
        VkPipelineStageFlags dest_stage;
        if (cfg->old_layout == VK_IMAGE_LAYOUT_UNDEFINED && cfg->new_layout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL) {
            barrier.srcAccessMask = 0;
            barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
            source_stage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
            dest_stage = VK_PIPELINE_STAGE_TRANSFER_BIT;
        }
        else if (cfg->old_layout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL && cfg->new_layout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) {
            barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
            barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
            source_stage = VK_PIPELINE_STAGE_TRANSFER_BIT;
            dest_stage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
        }
        else if (cfg->old_layout == VK_IMAGE_LAYOUT_UNDEFINED &&
                 cfg->new_layout == VK_IMAGE_LAYOUT_DEPTH_READ_ONLY_STENCIL_ATTACHMENT_OPTIMAL) {
            barrier.srcAccessMask = 0;
            barrier.dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
        }
        else {
            return err_code::VKR_TRANSITION_IMAGE_UNSUPPORTED_LAYOUT;
        }
        vkCmdPipelineBarrier(pool->buffers[tmp_buf.begin].hndl, source_stage, dest_stage, 0, 0, nullptr, 0, nullptr, 1, &barrier);
        ilog("Finished transitioning image layout");
        return cmd_buf_end(tmp_buf, pool, cmd_q, qind, vk);
    }
    return tmp_buf.err_code;
}

} // namespace nslib
