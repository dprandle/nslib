#include "input_mapping.h"
#include "platform.h"
#include "logging.h"

using namespace nslib;

struct app_data
{
    input_keymap km1{};
    input_keymap km2{};
    input_keymap km3{};
    input_keymap_stack stack{};
};

int app_init(platform_ctxt *ctxt, void *user_data)
{
    auto app = (app_data*)user_data;
    ilog("App init");

    // Create three different keymaps which well add some entries in
    input_init_keymap("KM1", &app->km1);
    input_init_keymap("KM2", &app->km2);
    input_init_keymap("KM3", &app->km3);

    // Just print the entry name button and action for key/mouse button entries
    auto key_mbutton_func = [](const input_event *ev, void *) {
        ilog("%s : key code:%d action:%d", ev->name, ev->btn_data.key_or_button, ev->btn_data.action);
    };

    // Name and scroll offset
    auto scroll_func = [](const input_event *ev, void *) {
        ilog("%s : offset: {%f %f}", ev->name, ev->scroll_data.offset.x, ev->scroll_data.offset.y);
    };

    // Name and mouse screen coord position and normalized position
    auto mouse_func = [](const input_event *ev, void *) {
        ilog("%s : abs pos: {%f %f}  norm pos: {%f %f}",
             ev->name,
             ev->pos.x,
             ev->pos.y,
             ev->norm_pos.x,
             ev->norm_pos.y);
    };

    // Create a set of entires for keymap 1 - some mouse movement on left mouse button pressed, an entry for right
    // clicking, pressing w, and scrolling
    input_keymap_entry mmove{"MMove"};
    u32 mmove_key = input_keymap_cursor_key(CURSOR_SCROLL_MOD_MOUSE_LEFT);
    mmove.cb = mouse_func;

    input_keymap_entry mzoom{"MZoom"};
    u32 mzoom_key = input_keymap_scroll_key(MOD_ANY);
    mzoom.cb = scroll_func;

    input_keymap_entry fwd{"Forward Start"};
    u32 fwd_key = input_keymap_button_key(KEY_W, MOD_ANY, INPUT_ACTION_PRESS);
    fwd.cb = key_mbutton_func;

    input_keymap_entry fwdr{"Forward Stop"};
    u32 fwdr_key = input_keymap_button_key(KEY_W, MOD_ANY, INPUT_ACTION_RELEASE);
    fwdr.cb = key_mbutton_func;

    // The don't consume flag is set so that keymaps lower in the context stack will still react to this key
    input_keymap_entry sel{"Select"};
    int sel_key = input_keymap_button_key(MOUSE_BTN_RIGHT, MOD_NONE, INPUT_ACTION_RELEASE);
    sel.cb = key_mbutton_func;
    sel.flags = IEVENT_FLAG_DONT_CONSUME;
    
    input_set_keymap_entry(mmove_key, &mmove, &app->km1);
    input_set_keymap_entry(mzoom_key, &mzoom, &app->km1);
    input_set_keymap_entry(fwd_key, &fwd, &app->km1);
    input_set_keymap_entry(fwdr_key, &fwdr, &app->km1);
    input_set_keymap_entry(sel_key, &sel, &app->km1);

    input_keymap_entry mdrag{"MDrag"};
    dlog("CSM: %d", CURSOR_SCROLL_MOD_MOUSE_MIDDLE);
    mdrag.cb = mouse_func;

    // Now add some entries for keymap 2 - mouse movement on middle mouse button pressing, the same w key, and the same
    // right click. This right click should always fire as the other keymap's right click entry has dont consume set
    input_keymap_entry mscroll{"MScroll"};
    mscroll.cb = scroll_func;

    input_keymap_entry press{"Press"};
    press.cb = key_mbutton_func;

    input_keymap_entry release{"Release"};
    release.cb = key_mbutton_func;

    input_keymap_entry cmenu{"Context Menu"};
    cmenu.cb = key_mbutton_func;

    input_set_keymap_entry(input_keymap_cursor_key(CURSOR_SCROLL_MOD_MOUSE_MIDDLE), &mdrag, &app->km2);
    input_set_keymap_entry(input_keymap_cursor_key(CURSOR_SCROLL_MOD_MOUSE_MIDDLE), &mscroll, &app->km2);
    input_set_keymap_entry(input_keymap_button_key(KEY_W, MOD_ANY, INPUT_ACTION_PRESS), &press, &app->km2);
    input_set_keymap_entry(input_keymap_button_key(KEY_W, MOD_ANY, INPUT_ACTION_RELEASE), &release, &app->km2);
    input_set_keymap_entry(input_keymap_button_key(MOUSE_BTN_RIGHT, MOD_NONE, INPUT_ACTION_RELEASE), &cmenu, &app->km2);

    // Now on keymap 3 which will sit at the bottom of the keymap stack, add some entries to push the other two keymaps
    // to the back of the stack and pop them also
    input_keymap_entry push_km{"Push KM 1"};
    push_km.cb = [](const input_event *ev, void *user) {
        ilog("%s", ev->name);
        auto app = (app_data *)user;
        if (!input_keymap_in_stack(&app->km1, &app->stack)) {
            input_push_keymap(&app->km1, &app->stack);
        }
    };   
    push_km.cb_user_param = app;

    input_keymap_entry push_km2{"Push KM 2"};
    push_km2.flags = 27;
    push_km2.cb_user_param = app;
    push_km2.cb = [](const input_event *ev, void *user) {
        ilog("%s", ev->name);
        auto app = (app_data *)user;
        if (!input_keymap_in_stack(&app->km2, &app->stack)) {
            input_push_keymap(&app->km2, &app->stack);
        }
    };

    input_keymap_entry pop_km{"Pop KM"};
    pop_km.cb = [](const input_event *ev, void *user) {
        ilog("%s", ev->name);
        auto app = (app_data *)user;
        if (app->stack.count > 1) {
            input_pop_keymap(&app->stack);
        }
    };

    pop_km.cb_user_param = app;
    input_set_keymap_entry(input_keymap_button_key(KEY_N1, MOD_ANY, INPUT_ACTION_PRESS), &push_km, &app->km3);
    input_set_keymap_entry(input_keymap_button_key(KEY_N2, MOD_ANY, INPUT_ACTION_PRESS), &push_km2, &app->km3);
    input_set_keymap_entry(input_keymap_button_key(KEY_P, MOD_ANY, INPUT_ACTION_PRESS), &pop_km, &app->km3);
    input_push_keymap(&app->km3, &app->stack);

    return err_code::PLATFORM_NO_ERROR;
}

int app_terminate(platform_ctxt *ctxt, void *user_data)
{
    auto app = (app_data*)user_data;
    ilog("App terminate");
    input_terminate_keymap(&app->km1);
    input_terminate_keymap(&app->km2);
    input_terminate_keymap(&app->km3);
    return err_code::PLATFORM_NO_ERROR;
}

int app_run_frame(platform_ctxt *ctxt, void *user_data)
{
    auto app = (app_data*)user_data;
    // Use our context stack to map the platform input to callback functions
    input_map_frame(&ctxt->finp, &app->stack);
    return err_code::PLATFORM_NO_ERROR;
}

int configure_platform(platform_init_info *settings, app_data *app)
{
    settings->wind.resolution = {1920, 1080};
    settings->wind.title = "Input Keymaps";
    settings->user_cb.init = app_init;
    settings->user_cb.terminate = app_terminate;
    settings->user_cb.run_frame = app_run_frame;
    return err_code::PLATFORM_NO_ERROR;
}

DEFINE_APPLICATION_MAIN(app_data, configure_platform)
