#include "basic_types.h"
#include "containers/string.h"
#include "osdef.h"
#ifdef PLATFORM_UNIX
    #include <unistd.h>
    #include <pthread.h>
    #define PATH_SEP '/'
#elif defined(PLATFORM_WIN32)
    #include <windows.h>
    #define PATH_SEP '\\'
#endif
#include <ctime>
#include <cerrno>

#include "input_kmcodes.h"
#include "platform.h"
#include "logging.h"
#include "containers/cjson.h"

#include "SDL3/SDL.h"

namespace nslib
{

intern platform_ctxt *platform_window_ptr(void *win)
{
    auto props = SDL_GetWindowProperties((SDL_Window *)win);
    return (platform_ctxt *)SDL_GetPointerProperty(props, "platform", nullptr);
}

intern void glfw_error_callback(s32 error, const char *description)
{
    elog("Error %d: %s", error, description);
}

intern const char *sdl_cat_str(int cat)
{
    switch (cat) {
    case SDL_LOG_CATEGORY_APPLICATION:
        return "app";
    case SDL_LOG_CATEGORY_ERROR:
        return "error";
    case SDL_LOG_CATEGORY_ASSERT:
        return "assert";
    case SDL_LOG_CATEGORY_SYSTEM:
        return "system";
    case SDL_LOG_CATEGORY_AUDIO:
        return "audio";
    case SDL_LOG_CATEGORY_VIDEO:
        return "video";
    case SDL_LOG_CATEGORY_RENDER:
        return "render";
    case SDL_LOG_CATEGORY_INPUT:
        return "input";
    case SDL_LOG_CATEGORY_TEST:
        return "test";
    case SDL_LOG_CATEGORY_GPU:
        return "gpu";
    default:
        return "unknown";
    }
}

intern void sdl_log_callback(void *userdata, int category, SDL_LogPriority priority, const char *message)
{
    switch (priority) {
    case SDL_LOG_PRIORITY_TRACE:
        tlog("SDL %s: %s", sdl_cat_str(category), message);
        break;
    case SDL_LOG_PRIORITY_VERBOSE:
        tlog("SDL %s: %s", sdl_cat_str(category), message);
        break;
    case SDL_LOG_PRIORITY_DEBUG:
        dlog("SDL %s: %s", sdl_cat_str(category), message);
        break;
    case SDL_LOG_PRIORITY_INFO:
        ilog("SDL %s: %s", sdl_cat_str(category), message);
        break;
    case SDL_LOG_PRIORITY_WARN:
        wlog("SDL %s: %s", sdl_cat_str(category), message);
        break;
    case SDL_LOG_PRIORITY_ERROR:
        elog("SDL %s: %s", sdl_cat_str(category), message);
        break;
    case SDL_LOG_PRIORITY_CRITICAL:
        clog("SDL %s: %s", sdl_cat_str(category), message);
        break;
    default:
        elog("SDL? %s: %s", sdl_cat_str(category), message);
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

platform_window_event *get_latest_window_event(platform_window_event_type type, platform_frame_window_events *fwind)
{
    for (sizet i = fwind->events.size; i > 0; --i) {
        if (fwind->events[i - 1].type == type) {
            return &fwind->events[i - 1];
        }
    }
    return nullptr;
}

const char *get_path_basename(const char *path)
{
    const char *ret = strrchr(path, PATH_SEP);
    if (!ret) {
        ret = path;
    }
    return ret;
}

bool frame_has_event_type(platform_window_event_type type, const platform_frame_window_events *fwind)
{
    for (sizet i = 0; i < fwind->events.size; ++i) {
        if (fwind->events[i].type == type) {
            return true;
        }
    }
    return false;
}

intern void init_mem_arenas(const platform_memory_init_info *info, platform_memory *mem)
{
    // Null to indicate these get platform_alloc'd
    mem_init_fl_arena(&mem->free_list, info->free_list_size, nullptr, "global");
    mem_init_stack_arena(&mem->stack, info->stack_size, nullptr, "global");
    mem_init_lin_arena(&mem->frame_linear, info->frame_linear_size, nullptr, "global");

    // Then these become our global mem arenas
    mem_set_global_arena(&mem->free_list);
    mem_set_global_stack_arena(&mem->stack);
    mem_set_global_frame_lin_arena(&mem->frame_linear);

    // Set up our json alloc and free funcs
    json_hooks hooks;
    auto mem_glob_alloc = [](sizet sz) -> void * { return mem_alloc(sz, mem_global_arena()); };
    auto mem_glob_free = [](void *ptr) -> void { mem_free(ptr, mem_global_arena()); };

    hooks.malloc_fn = mem_glob_alloc;
    hooks.free_fn = mem_glob_free;
    json_init_hooks(&hooks);
}

intern void terminate_mem_arenas(platform_memory *mem)
{
    mem_terminate_arena(&mem->stack);
    mem_terminate_arena(&mem->frame_linear);
    mem_terminate_arena(&mem->free_list);
    mem_set_global_arena(nullptr);
    mem_set_global_stack_arena(nullptr);
    mem_set_global_frame_lin_arena(nullptr);
}

void *platform_alloc(sizet byte_size)
{
    return malloc(byte_size);
}

void platform_free(void *block)
{
    free(block);
}

void *platform_realloc(void *ptr, sizet byte_size)
{
    return realloc(ptr, byte_size);
}

int init_platform(const platform_init_info *settings, platform_ctxt *ctxt)
{
    ilog("Platform init version %d.%d.%d", NSLIB_VERSION_MAJOR, NSLIB_VERSION_MINOR, NSLIB_VERSION_PATCH);
    SDL_SetLogOutputFunction(sdl_log_callback, nullptr);
    if (!SDL_Init(SDL_INIT_AUDIO | SDL_INIT_VIDEO)) {
        elog("Could not initialize SDL");
        return err_code::PLATFORM_INIT_FAIL;
    }
    ilog("Initialized SDL");

    ctxt->win_hndl = create_platform_window(&settings->wind);
    if (!ctxt->win_hndl) {
        log_any_sdl_error("Failed to create window");
        return err_code::PLATFORM_INIT_FAIL;
    }
    auto props = SDL_GetWindowProperties((SDL_Window *)ctxt->win_hndl);
    if (props == 0) {
        log_any_sdl_error("Failed to get window props");
    }
    else {
        if (!SDL_SetPointerProperty(props, "platform", ctxt->win_hndl)) {
            log_any_sdl_error("Failed to set platform ptr in window props");
        }
    }
    SDL_SetWindowPosition((SDL_Window *)ctxt->win_hndl, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED);

    // Seed random number generator
    srand((u32)time(NULL));

    set_logging_level(GLOBAL_LOGGER, settings->default_log_level);
    init_mem_arenas(&settings->mem, &ctxt->arenas);
    return err_code::PLATFORM_NO_ERROR;
}

int terminate_platform(platform_ctxt *ctxt)
{
    ilog("Platform terminate");
    
    SDL_Quit();
    ilog("Terminated SDL");
    terminate_mem_arenas(&ctxt->arenas);
    return err_code::PLATFORM_NO_ERROR;
}

intern void log_display_info()
{
    int count;
    auto ids = SDL_GetDisplays(&count);
    ilog("Got %d displays", count);
    for (int i = 0; i < count; ++i) {
        SDL_Rect r;
        auto props = SDL_GetDisplayBounds(ids[i], &r);
        ilog("Display %s - rect x:%d y:%d w:%d h:%d", SDL_GetDisplayName(ids[i]), r.x, r.y, r.w, r.h);
    }
}

intern u32 get_sdl_window_flags(u32 platform_win_flags)
{
    return platform_win_flags;
}

void *create_platform_window(const platform_window_init_info *settings)
{
    assert(settings);

    log_display_info();

    ivec2 sz = settings->resolution;
    auto sdl_flags = get_sdl_window_flags(settings->win_flags);

    auto primary = SDL_GetPrimaryDisplay();
    float scale = SDL_GetDisplayContentScale(primary);
    sz = sz * scale;
    ilog("Display scaling set to %.2f - adjusted resolution {%d, %d}", scale, sz.x, sz.y);
    return SDL_CreateWindow(settings->title, sz.w, sz.h, sdl_flags);
}

intern input_kmcode map_sdl_key(SDL_Keycode sdl_key)
{
    u16 ret{0};
    if (sdl_key <= SDLK_PLUSMINUS) {
        ret = sdl_key;
    }
    else if (sdl_key <= SDLK_RHYPER) {
        int offset = sdl_key - SDLK_LEFT_TAB;
        ret = KMCODE_KEY_LEFT_TAB + offset;
    }
    else if (sdl_key <= SDLK_PAGEUP) {
        int offset = sdl_key - SDLK_CAPSLOCK;
        ret = KMCODE_KEY_CAPSLOCK + offset;
    }
    else if (sdl_key <= SDLK_VOLUMEDOWN) {
        int offset = sdl_key - SDLK_END;
        ret = KMCODE_KEY_END + offset;
    }
    else if (sdl_key <= SDLK_KP_EQUALSAS400) {
        int offset = sdl_key - SDLK_KP_COMMA;
        ret = KMCODE_KEY_KP_COMMA + offset;
    }
    else if (sdl_key <= SDLK_EXSEL) {
        int offset = sdl_key - SDLK_ALTERASE;
        ret = KMCODE_KEY_ALTERASE + offset;
    }
    else if (sdl_key <= SDLK_KP_HEXADECIMAL) {
        int offset = sdl_key - SDLK_KP_00;
        ret = KMCODE_KEY_KP_00 + offset;
    }
    else if (sdl_key <= SDLK_RGUI) {
        int offset = sdl_key - SDLK_LCTRL;
        ret = KMCODE_KEY_LCTRL + offset;
    }
    else if (sdl_key <= SDLK_ENDCALL) {
        int offset = sdl_key - SDLK_MODE;
        ret = KMCODE_KEY_MODE + offset;
    }
    else {
        wlog("Unhandled SDL key code %u", sdl_key);
    }
    return (input_kmcode)ret;
}

intern input_kmcode map_sdl_mbutton(u8 sdl_mbutton)
{
    u16 ret{sdl_mbutton};
    return (input_kmcode)ret;
}

intern u16 map_sdl_mods(SDL_Keymod mods)
{
    // Nothing needs to be done to map these
    return mods;
}

intern u16 map_sdl_mouse_state(SDL_MouseButtonFlags mods)
{
    // Nothing needs to be done to map these
    return (u16)mods;
}

intern void handle_sdl_key_event(platform_ctxt *ctxt, const SDL_KeyboardEvent &ev)
{
    platform_input_event ievent{};
    ievent.type = INPUT_EVENT_TYPE_KEY;
    ievent.timestamp = ev.timestamp;
    ievent.kmcode = map_sdl_key(ev.key);
    ievent.keymods = map_sdl_mods(ev.mod);
    ievent.mbutton_mask = map_sdl_mouse_state(SDL_GetMouseState(nullptr, nullptr));
    ievent.win_id = ev.windowID;

    ievent.key.action = (ev.repeat) ? INPUT_ACTION_REPEAT : ((ev.down) ? INPUT_ACTION_PRESS : INPUT_ACTION_RELEASE);
    ievent.key.scancode = ev.scancode;
    ievent.key.raw_scancode = ev.raw;
    ievent.key.keyboard_id = ev.which;
    arr_push_back(&ctxt->finp.events, ievent);
}

intern void handle_sdl_mbutton_event(platform_ctxt *ctxt, const SDL_MouseButtonEvent &ev)
{
    platform_input_event ievent{};
    ievent.type = INPUT_EVENT_TYPE_MBUTTON;
    ievent.timestamp = ev.timestamp;
    ievent.kmcode = map_sdl_mbutton(ev.button);
    ievent.keymods = map_sdl_mods(SDL_GetModState());
    ievent.mbutton_mask = map_sdl_mouse_state(SDL_GetMouseState(nullptr, nullptr));
    ievent.win_id = ev.windowID;

    vec2 win_sz = get_platform_window_pixel_size(get_platform_window(ev.windowID));
    ievent.mbutton.action = (ev.down) ? INPUT_ACTION_PRESS : INPUT_ACTION_RELEASE;
    ievent.mbutton.mpos = {ev.x, ev.y};
    ievent.mbutton.norm_mpos = ievent.mbutton.mpos / win_sz;
    ievent.mbutton.mouse_id = ev.which;
    arr_push_back(&ctxt->finp.events, ievent);
}

intern void handle_sdl_mmotion_event(platform_ctxt *ctxt, const SDL_MouseMotionEvent &ev)
{
    platform_input_event ievent{};
    ievent.type = INPUT_EVENT_TYPE_MBUTTON;
    ievent.timestamp = ev.timestamp;
    ievent.kmcode = KMCODE_MMOTION;
    ievent.keymods = map_sdl_mods(SDL_GetModState());
    ievent.mbutton_mask = map_sdl_mouse_state(SDL_GetMouseState(nullptr, nullptr));
    ievent.win_id = ev.windowID;

    vec2 win_sz = get_platform_window_pixel_size(get_platform_window(ev.windowID));
    ievent.mmotion.mpos = {ev.x, ev.y};
    ievent.mmotion.norm_mpos = ievent.mbutton.mpos / win_sz;
    ievent.mmotion.delta = {ev.xrel, ev.yrel};
    ievent.mmotion.norm_delta = ievent.mmotion.delta / win_sz;
    ievent.mmotion.mouse_id = ev.which;
    arr_push_back(&ctxt->finp.events, ievent);
}

intern void handle_sdl_mwheel_event(platform_ctxt *ctxt, const SDL_MouseWheelEvent &ev)
{
    platform_input_event ievent{};
    ievent.type = INPUT_EVENT_TYPE_MBUTTON;
    ievent.timestamp = ev.timestamp;
    ievent.kmcode = KMCODE_MWHEEL;
    ievent.keymods = map_sdl_mods(SDL_GetModState());
    ievent.mbutton_mask = map_sdl_mouse_state(SDL_GetMouseState(nullptr, nullptr));
    ievent.win_id = ev.windowID;

    vec2 win_sz = get_platform_window_pixel_size(get_platform_window(ev.windowID));
    ievent.mwheel.mpos = {ev.x, ev.y};
    ievent.mwheel.norm_mpos = ievent.mbutton.mpos / win_sz;
    ievent.mwheel.delta = {ev.x, ev.y};
    ievent.mwheel.idelta = {ev.integer_x, ev.integer_y};
    ievent.mwheel.mouse_id = ev.which;
    arr_push_back(&ctxt->finp.events, ievent);
}

void process_platform_events(platform_ctxt *pf)
{
    arr_clear(&pf->finp.events);
    arr_clear(&pf->fwind.events);
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        switch (event.type) {
        case SDL_EVENT_QUIT:
            SDL_DestroyWindow((SDL_Window *)pf->win_hndl);
            pf->win_hndl = nullptr;
            break;
        case SDL_EVENT_KEY_DOWN:
            handle_sdl_key_event(pf, event.key);
            break;
        case SDL_EVENT_KEY_UP:
            handle_sdl_key_event(pf, event.key);
            break;
        case SDL_EVENT_MOUSE_BUTTON_UP:
            handle_sdl_mbutton_event(pf, event.button);
            break;
        case SDL_EVENT_MOUSE_BUTTON_DOWN:
            handle_sdl_mbutton_event(pf, event.button);
            break;
        case SDL_EVENT_MOUSE_MOTION:
            handle_sdl_mmotion_event(pf, event.motion);
            break;
        case SDL_EVENT_MOUSE_WHEEL:
            handle_sdl_mwheel_event(pf, event.wheel);
            break;
        default:
            // do nothing yet
            break;
        }

        if (pf->finp.events.size == pf->finp.events.capacity) {
            pf->finp.events.size = 0;
        }
    }
}

void *get_platform_window(u32 id)
{
    return SDL_GetWindowFromID(id);
}

ivec2 get_platform_window_size(void *win)
{
    ivec2 ret;
    if (!SDL_GetWindowSize((SDL_Window *)win, &ret.w, &ret.h)) {
        log_any_sdl_error();
    }
    return ret;
}

ivec2 get_platform_window_pixel_size(void *win)
{
    ivec2 ret;
    if (!SDL_GetWindowSizeInPixels((SDL_Window *)win, &ret.w, &ret.h)) {
        log_any_sdl_error();
    }
    return ret;
}

u64 get_thread_id()
{
#if defined(PLATFORM_UNIX)
    return (u64)pthread_self();
#elif defined(PLATFORM_WIN32)
    return (u64)GetCurrentThreadId();
#endif
}

bool platform_framebuffer_resized(void *win_hndl)
{
    platform_ctxt *pf = platform_window_ptr(win_hndl);
    return frame_has_event_type(platform_window_event_type::FB_RESIZE, &pf->fwind);
}

bool platform_window_resized(void *win_hndl)
{
    platform_ctxt *pf = platform_window_ptr(win_hndl);
    return frame_has_event_type(platform_window_event_type::WIN_RESIZE, &pf->fwind);
}

void start_platform_frame(platform_ctxt *ctxt)
{
    ptimer_split(&ctxt->time_pts);
    process_platform_events(ctxt);
    mem_reset_arena(&ctxt->arenas.frame_linear);
}

void end_platform_frame(platform_ctxt *ctxt)
{
    ++ctxt->finished_frames;
}

intern sizet platform_file_size(FILE *f, platform_file_err_desc *err)
{
    sizet ret{0};

    sizet cur_p = ftell(f);
    if (cur_p == -1L)
        goto ftell_fail;

    if (fseek(f, 0, SEEK_END) != 0)
        goto fseek_fail;

    ret = ftell(f);
    if (ret == -1L)
        goto ftell_fail;

    if (fseek(f, (long)cur_p, SEEK_SET) != 0)
        goto fseek_fail;

    return ret;

fseek_fail:
    if (err) {
        err->code = err_code::FILE_SEEK_FAIL;
        err->str = strerror(errno);
    }
    return ret;

ftell_fail:
    if (err) {
        err->code = err_code::FILE_TELL_FAIL;
        err->str = strerror(errno);
    }
    return ret;
}

sizet get_file_size(const char *fname, platform_file_err_desc *err)
{
    sizet ret{0};
    FILE *f = fopen(fname, "r");
    if (f) {
        ret = platform_file_size(f, err);
        fclose(f);
    }
    else if (err) {
        err->code = err_code::FILE_OPEN_FAIL;
        err->str = strerror(errno);
    }
    return ret;
}

intern sizet platform_read_file(FILE *f, void *data, sizet element_size, sizet nelements, sizet byte_offset, platform_file_err_desc *err)
{
    sizet nelems{0};
    if (byte_offset != 0 && fseek(f, (long)byte_offset, SEEK_SET) != 0) {
        if (err) {
            err->code = err_code::FILE_SEEK_FAIL;
            err->str = strerror(errno);
        }
    }
    else {
        nelems = fread(data, element_size, nelements, f);
    }
    return nelems;
}

sizet read_file(const char *fname, const char *mode, void *data, sizet element_size, sizet nelements, sizet byte_offset, platform_file_err_desc *err)
{
    sizet elems{0};
    FILE *f = fopen(fname, mode);
    if (f) {
        elems = platform_read_file(f, data, element_size, nelements, byte_offset, err);
        fclose(f);
    }
    else if (err) {
        err->code = err_code::FILE_OPEN_FAIL;
        err->str = strerror(errno);
    }
    return elems;
}

sizet read_file(const char *fname, void *data, sizet element_size, sizet nelements, sizet byte_offset, platform_file_err_desc *err)
{
    return read_file(fname, "rb", data, element_size, nelements, byte_offset, err);
}

sizet read_file(const char *fname, const char *mode, byte_array *buffer, sizet byte_offset, platform_file_err_desc *err)
{
    sizet elems{0};
    FILE *f = fopen(fname, mode);
    if (f) {
        sizet fsize = platform_file_size(f, err);
        if (fsize > 0) {
            arr_resize(buffer, fsize);
            elems = platform_read_file(f, buffer->data, 1, buffer->size, byte_offset, err);
        }
        fclose(f);
    }
    else if (err) {
        err->code = err_code::FILE_OPEN_FAIL;
        err->str = strerror(errno);
    }
    return elems;
}

sizet read_file(const char *fname, byte_array *buffer, sizet byte_offset, platform_file_err_desc *err)
{
    return read_file(fname, "rb", buffer, byte_offset, err);
}

intern sizet platform_write_file(FILE *f, const void *data, sizet element_size, sizet nelements, sizet byte_offset, platform_file_err_desc *err)
{
    sizet ret{0};
    int seek = SEEK_SET;
    if (byte_offset == -1) {
        byte_offset = 0;
        seek = SEEK_END;
    }

    if ((byte_offset != 0 || seek != SEEK_SET) && fseek(f, (long)byte_offset, seek) != 0) {
        if (err) {
            err->code = err_code::FILE_SEEK_FAIL;
            err->str = strerror(errno);
        }
        return ret;
    }

    ret = fwrite(data, element_size, nelements, f);
    if (ret != nelements && err) {
        err->code = err_code::FILE_WRITE_DIFF_SIZE;
        err->str = strerror(errno);
    }
    return ret;
}

sizet write_file(const char *fname,
                 const char *mode,
                 const void *data,
                 sizet element_size,
                 sizet nelements,
                 sizet byte_offset,
                 platform_file_err_desc *err)
{
    sizet ret{0};
    FILE *f = fopen(fname, mode);
    if (f) {
        ret = platform_write_file(f, data, element_size, nelements, byte_offset, err);
        fclose(f);
    }
    else if (err) {
        err->code = err_code::FILE_OPEN_FAIL;
        err->str = strerror(errno);
    }
    return ret;
}

sizet write_file(const char *fname, const void *data, sizet element_size, sizet nelements, sizet byte_offset, platform_file_err_desc *err)
{
    return write_file(fname, "wb", data, element_size, nelements, byte_offset, err);
}

sizet write_file(const char *fname, const char *mode, const byte_array *data, sizet byte_offset, platform_file_err_desc *err)
{
    sizet ret{0};
    FILE *f = fopen(fname, mode);
    if (f) {
        ret = platform_write_file(f, data, 1, data->size, byte_offset, err);
        fclose(f);
    }
    else if (err) {
        err->code = err_code::FILE_OPEN_FAIL;
        err->str = strerror(errno);
    }
    return ret;
}

sizet write_file(const char *fname, const byte_array *data, sizet byte_offset, platform_file_err_desc *err)
{
    return write_file(fname, "wb", data, byte_offset, err);
}

} // namespace nslib
