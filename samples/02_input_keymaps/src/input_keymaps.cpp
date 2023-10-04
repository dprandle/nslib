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

int load_platform_settings(platform_init_info *settings, app_data *app)
{
    settings->wind.resolution = {1920, 1080};
    settings->wind.title = "02 Input Keymaps";
    return err_code::PLATFORM_NO_ERROR;
}

int app_init(platform_ctxt *ctxt, app_data *app)
{
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
             ev->cursor_data.pos.x,
             ev->cursor_data.pos.y,
             ev->cursor_data.norm_pos.x,
             ev->cursor_data.norm_pos.y);
    };

    // Create a set of entires for keymap 1 - some mouse movement on left mouse button pressed, an entry for right
    // clicking, pressing w, and scrolling
    input_keymap_entry mmove{"MMove"};
    mmove.key = input_keymap_cursor_key(CURSOR_SCROLL_MOD_MOUSE_LEFT);
    mmove.cb = mouse_func;

    input_keymap_entry mzoom{"MZoom"};
    mzoom.key = input_keymap_scroll_key(MOD_ANY);
    mzoom.cb = scroll_func;

    input_keymap_entry fwd{"Forward Start"};
    fwd.key = input_keymap_button_key(KEY_W, MOD_ANY, INPUT_ACTION_PRESS);
    fwd.cb = key_mbutton_func;

    input_keymap_entry fwdr{"Forward Stop"};
    fwdr.key = input_keymap_button_key(KEY_W, MOD_ANY, INPUT_ACTION_RELEASE);
    fwdr.cb = key_mbutton_func;

    // The don't consume flag is set so that keymaps lower in the context stack will still react to this key
    input_keymap_entry sel{"Select"};
    int sel_key = input_keymap_button_key(MOUSE_BTN_RIGHT, MOD_NONE, INPUT_ACTION_RELEASE);
    sel.cb = key_mbutton_func;
    sel.flags = IEVENT_FLAG_DONT_CONSUME;
    
    input_set_keymap_entry(&mmove, &app->km1);
    input_set_keymap_entry(&mzoom, &app->km1);
    input_set_keymap_entry(&fwd, &app->km1);
    input_set_keymap_entry(&fwdr, &app->km1);
    input_set_keymap_entry(&sel, &app->km1);

    input_keymap_entry mdrag{"MDrag"};
    dlog("CSM: %d", CURSOR_SCROLL_MOD_MOUSE_MIDDLE);
    mdrag.key = input_keymap_cursor_key(CURSOR_SCROLL_MOD_MOUSE_MIDDLE);
    mdrag.cb = mouse_func;

    // Now add some entries for keymap 2 - mouse movement on middle mouse button pressing, the same w key, and the same
    // right click. This right click should always fire as the other keymap's right click entry has dont consume set
    input_keymap_entry mscroll{"MScroll"};
    mscroll.key = input_keymap_scroll_key(MOD_ANY);
    mscroll.cb = scroll_func;

    input_keymap_entry press{"Press"};
    press.key = input_keymap_button_key(KEY_W, MOD_ANY, INPUT_ACTION_PRESS);
    press.cb = key_mbutton_func;

    input_keymap_entry release{"Release"};
    release.key = input_keymap_button_key(KEY_W, MOD_ANY, INPUT_ACTION_RELEASE);
    release.cb = key_mbutton_func;

    input_keymap_entry cmenu{"Context Menu"};
    cmenu.key = input_keymap_button_key(MOUSE_BTN_RIGHT, MOD_NONE, INPUT_ACTION_RELEASE);
    cmenu.cb = key_mbutton_func;

    input_set_keymap_entry(&mdrag, &app->km2);
    input_set_keymap_entry(&mscroll, &app->km2);
    input_set_keymap_entry(&press, &app->km2);
    input_set_keymap_entry(&release, &app->km2);
    input_set_keymap_entry(&cmenu, &app->km2);

    auto kk = input_get_keymap_entry("MDrag", &app->km2);
    if (kk && kk->key == mdrag.key) {
        dlog("Good to Go!");
    }

    // Now on keymap 3 which will sit at the bottom of the keymap stack, add some entries to push the other two keymaps
    // to the back of the stack and pop them also
    input_keymap_entry push_km{"Push KM 1"};
    push_km.key = input_keymap_button_key(KEY_N1, MOD_ANY, INPUT_ACTION_PRESS);
    push_km.cb = [](const input_event *ev, void *user) {
        ilog("%s", ev->name);
        auto app = (app_data *)user;
        if (!input_keymap_in_stack(&app->km1, &app->stack)) {
            input_push_keymap(&app->km1, &app->stack);
        }
    };
    push_km.cb_user_param = app;

    input_keymap_entry push_km2{"Push KM 2"};
    push_km2.key = input_keymap_button_key(KEY_N2, MOD_ANY, INPUT_ACTION_PRESS);
    push_km2.cb = [](const input_event *ev, void *user) {
        ilog("%s", ev->name);
        auto app = (app_data *)user;
        if (!input_keymap_in_stack(&app->km2, &app->stack)) {
            input_push_keymap(&app->km2, &app->stack);
        }
    };
    push_km2.cb_user_param = app;

    input_keymap_entry pop_km{"Pop KM"};
    pop_km.key = input_keymap_button_key(KEY_P, MOD_ANY, INPUT_ACTION_PRESS);
    pop_km.cb = [](const input_event *ev, void *user) {
        ilog("%s", ev->name);
        auto app = (app_data *)user;
        if (app->stack.count > 1) {
            input_pop_keymap(&app->stack);
        }
    };
    pop_km.cb_user_param = app;
    input_set_keymap_entry(&push_km, &app->km3);
    input_set_keymap_entry(&push_km2, &app->km3);
    input_set_keymap_entry(&pop_km, &app->km3);
    input_push_keymap(&app->km3, &app->stack);

    return err_code::PLATFORM_NO_ERROR;
}

int app_terminate(platform_ctxt *ctxt, app_data *app)
{
    ilog("App terminate");
    input_terminate_keymap(&app->km1);
    input_terminate_keymap(&app->km2);
    input_terminate_keymap(&app->km3);
    return err_code::PLATFORM_NO_ERROR;
}

int app_run_frame(platform_ctxt *ctxt, app_data *app)
{
    // Use our context stack to map the platform input to callback functions
    input_map_frame(&ctxt->finp, &app->stack);
    return err_code::PLATFORM_NO_ERROR;
}

DEFINE_APPLICATION_MAIN(app_data)
