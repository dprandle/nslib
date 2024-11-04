#include "robj_common.h"
#include "platform.h"
#include "renderer.h"
#include "input_mapping.h"
#include "sim_region.h"
#include "vk_context.h"
#include "basic_types.h"
using namespace nslib;

struct app_data
{
    renderer rndr{};
    sim_region rgn{};
    robj_cache_group cg{};

    input_keymap global_km;
    input_keymap_stack stack{};

    u32 cam_id;
    vec2 mpos;
    ivec2 movement{};

    u32 cube_1;
    u32 plane_1;
};

intern void setup_camera_controller(platform_ctxt *ctxt, app_data *app, input_keymap *kmap)
{
    // Create camera
    auto sz = get_framebuffer_size(ctxt->win_hndl);
    auto cam = add_entity("Editor_Cam", &app->rgn);
    auto cam_comp = add_comp<camera>(cam);
    auto cam_tcomp = add_comp<transform>(cam);

    cam_comp->proj = (math::perspective(60.0f, (f32)sz.w / (f32)sz.h, 0.1f, 1000.0f));
    cam_comp->view = (math::look_at(vec3{0.0f, 10.0f, -5.0f}, vec3{0.0f}, vec3{0.0f, 1.0f, 0.0f}));

    cam_tcomp->cached = math::inverse(cam_comp->view);
    cam_tcomp->orientation = math::orientation(cam_tcomp->cached);
    cam_tcomp->scale = math::scaling_vec(cam_tcomp->cached);
    cam_tcomp->world_pos = math::translation_vec(cam_tcomp->cached);
    app->cam_id = cam->id;

    cam_tcomp->cached = math::model_tform(cam_tcomp->world_pos, cam_tcomp->orientation, cam_tcomp->scale);
    cam_comp->view = math::inverse(cam_tcomp->cached);

    // Setup some keys
    input_keymap_entry cam_turn{};
    cam_turn.name = "cam_turn";
    cam_turn.cb_user_param = app;
    cam_turn.cb = [](const input_event *ev, void *data) {
        auto app = (app_data *)data;
        auto cam_ent = get_entity(app->cam_id, &app->rgn);
        auto camc = get_comp<camera>(cam_ent);
        auto camt = get_comp<transform>(cam_ent);

        auto delta = ev->norm_pos - app->mpos;
        app->mpos = ev->norm_pos;

        vec3 right = math::right_vec(camt->orientation);
        f32 factor = 2.5;
        vec4 horizontal = {right, -(f32)delta.y * factor};
        vec4 vertical = {{0, 0, 1}, (f32)delta.x * factor};
        camt->orientation = math::orientation(vertical) * math::orientation(horizontal) * camt->orientation;
        camt->cached = math::model_tform(camt->world_pos, camt->orientation, camt->scale);
        camc->view = math::inverse(camt->cached);
    };

    u32 k = keymap_cursor_key(CURSOR_SCROLL_MOD_MOUSE_RIGHT);
    set_keymap_entry(k, &cam_turn, kmap);

    input_keymap_entry cam_track_cursor{"reset_pos"};
    cam_track_cursor.cb_user_param = app;
    cam_track_cursor.cb = [](const input_event *ev, void *data) {
        auto app = (app_data *)data;
        app->mpos = ev->norm_pos;
    };
    k = keymap_button_key(MOUSE_BTN_RIGHT, MOD_ANY, INPUT_ACTION_PRESS);
    set_keymap_entry(k, &cam_track_cursor, kmap);

    input_keymap_entry cam_move{};
    cam_move.name = "cam_forward_press";
    cam_move.cb_user_param = app;
    cam_move.cb = [](const input_event *ev, void *data) {
        auto app = (app_data *)data;
        app->movement.y += 1;
    };
    k = keymap_button_key(KEY_W, MOD_NONE, INPUT_ACTION_PRESS);
    set_keymap_entry(k, &cam_move, kmap);

    cam_move.name = "cam_forward_release";
    cam_move.cb = [](const input_event *ev, void *data) {
        auto app = (app_data *)data;
        app->movement.y -= 1;
    };
    k = keymap_button_key(KEY_W, MOD_NONE, INPUT_ACTION_RELEASE);
    set_keymap_entry(k, &cam_move, kmap);

    cam_move.name = "cam_back_press";
    cam_move.cb = [](const input_event *ev, void *data) {
        auto app = (app_data *)data;
        app->movement.y -= 1;
    };
    k = keymap_button_key(KEY_S, MOD_NONE, INPUT_ACTION_PRESS);
    set_keymap_entry(k, &cam_move, kmap);

    cam_move.name = "cam_back_release";
    cam_move.cb = [](const input_event *ev, void *data) {
        auto app = (app_data *)data;
        app->movement.y += 1;
    };
    k = keymap_button_key(KEY_S, MOD_NONE, INPUT_ACTION_RELEASE);
    set_keymap_entry(k, &cam_move, kmap);

    cam_move.name = "cam_left_press";
    cam_move.cb = [](const input_event *ev, void *data) {
        auto app = (app_data *)data;
        app->movement.x -= 1;
    };
    k = keymap_button_key(KEY_A, MOD_NONE, INPUT_ACTION_PRESS);
    set_keymap_entry(k, &cam_move, kmap);

    cam_move.name = "cam_left_release";
    cam_move.cb = [](const input_event *ev, void *data) {
        auto app = (app_data *)data;
        app->movement.x += 1;
    };
    k = keymap_button_key(KEY_A, MOD_NONE, INPUT_ACTION_RELEASE);
    set_keymap_entry(k, &cam_move, kmap);

    cam_move.name = "cam_right_press";
    cam_move.cb = [](const input_event *ev, void *data) {
        auto app = (app_data *)data;
        app->movement.x += 1;
    };
    k = keymap_button_key(KEY_D, MOD_NONE, INPUT_ACTION_PRESS);
    set_keymap_entry(k, &cam_move, kmap);

    cam_move.name = "cam_right_release";
    cam_move.cb = [](const input_event *ev, void *data) {
        auto app = (app_data *)data;
        app->movement.x -= 1;
    };
    k = keymap_button_key(KEY_D, MOD_NONE, INPUT_ACTION_RELEASE);
    set_keymap_entry(k, &cam_move, kmap);
}

int init(platform_ctxt *ctxt, void *user_data)
{
    auto app = (app_data *)user_data;

    init_cache_group_default_types(&app->cg, mem_global_arena());

    // Create meshes
    auto mc = get_cache<mesh>(&app->cg);
    auto cube_msh = add_robj(mc);
    auto rect_msh = add_robj(mc);
    init_mesh(cube_msh, mem_global_arena());
    init_mesh(rect_msh, mem_global_arena());
    make_rect(rect_msh);
    make_cube(cube_msh);

    ilog("Rect mesh submesh count %d and vert count %d and ind count %d",
         rect_msh->submeshes.size,
         rect_msh->submeshes[0].verts.size,
         rect_msh->submeshes[0].inds.size);
    ilog("Cube mesh submesh count %d and vert count %d and ind count %d",
         cube_msh->submeshes.size,
         cube_msh->submeshes[0].verts.size,
         cube_msh->submeshes[0].inds.size);

    // Create our sim region aka scene
    init_sim_region(&app->rgn, mem_global_arena());

    // Create a grid of entities with odd ones being cubes and even being rectangles
    int len = 32, width = 32, height = 10;
    add_entities(len * width * height, &app->rgn);

    for (int zind = 0; zind < height; ++zind) {
        for (int yind = 0; yind < len; ++yind) {
            for (int xind = 0; xind < width; ++xind) {
                int ent_ind = zind * (width * len) + yind * width + xind;
                auto ent = &app->rgn.ents[ent_ind];
                auto tfcomp = add_comp<transform>(ent);
                auto sc = add_comp<static_model>(ent);
                if (xind % 2) {
                    sc->mesh_id = cube_msh->id;
                    ent->name = to_str("cube-%d", ent_ind);
                }
                else {
                    sc->mesh_id = rect_msh->id;
                    ent->name = to_str("rect-%d", ent_ind);
                }
                tfcomp->world_pos = vec3{xind * 2.0f, yind * 2.0f, zind * 2.0f};
                tfcomp->cached = math::model_tform(tfcomp->world_pos, tfcomp->orientation, tfcomp->scale);
            }
        }
    }

    // Create input map
    init_keymap("global", &app->global_km, &ctxt->arenas.free_list);

    // Create and setup input for camera
    setup_camera_controller(ctxt, app, &app->global_km);

    push_keymap(&app->global_km, &app->stack);

    int ret = init_renderer(&app->rndr, &app->cg, ctxt->win_hndl, &ctxt->arenas.free_list);
    if (ret == err_code::RENDER_NO_ERROR) {
        upload_to_gpu(cube_msh, &app->rndr);
        upload_to_gpu(rect_msh, &app->rndr);
    }
    return ret;
}

int run_frame(platform_ctxt *ctxt, void *user_data)
{
    auto app = (app_data *)user_data;
    profile_timepoints pt;
    map_input_frame(&ctxt->finp, &app->stack);

    int res = render_frame_begin(&app->rndr, ctxt->finished_frames);
    if (res != err_code::VKR_NO_ERROR) {
        return res;
    }

    // Move the cam if needed
    auto cam = get_comp<camera>(app->cam_id, &app->rgn.cdb);
    if (app->movement != ivec2{}) {
        auto cam_tform = get_comp<transform>(app->cam_id, &app->rgn.cdb);
        auto right = math::right_vec(cam_tform->orientation);
        auto target = math::target_vec(cam_tform->orientation);
        cam_tform->world_pos += (right * app->movement.x + target * app->movement.y) * ctxt->time_pts.dt * 10;
        cam_tform->cached = math::model_tform(cam_tform->world_pos, cam_tform->orientation, cam_tform->scale);
        cam->view = math::inverse(cam_tform->cached);
    }
    static double update_tm = 0.0;
    static double render_tm = 0.0;

    // Spin some entities
    ptimer_restart(&pt);
    auto tform_tbl = get_comp_tbl<transform>(&app->rgn.cdb);
    auto mat_cache = get_cache<material>(&app->cg);
    auto msh_cache = get_cache<mesh>(&app->cg);
    for (sizet i = 0; i < tform_tbl->entries.size; ++i) {
        auto curtf = &tform_tbl->entries[i];
        if (curtf->ent_id != app->cam_id) {
            if (i % 3 == 0) {
                curtf->orientation *= math::orientation(vec4{1.0, 0.0, 0.0, (f32)ctxt->time_pts.dt});
            }
            else if (i % 3 == 2) {
                curtf->orientation *= math::orientation(vec4{0.0, 1.0, 0.0, (f32)ctxt->time_pts.dt});
            }
            else {
                curtf->orientation *= math::orientation(vec4{0.0, 0.0, 1.0, (f32)ctxt->time_pts.dt});
            }
            curtf->cached = math::model_tform(curtf->world_pos, curtf->orientation, curtf->scale);
        }
        auto sm = get_comp<static_model>(tform_tbl->entries[i].ent_id, &app->rgn.cdb);
        if (sm) {
            rpush_sm(&app->rndr, sm, curtf, msh_cache, mat_cache);
        }
    }

    ptimer_split(&pt);
    update_tm += pt.dt;

    res = render_frame_end(&app->rndr, cam);
    ptimer_split(&pt);
    render_tm += pt.dt;

    static double counter = 2.0;
    double elapsed = nanos_to_sec(ptimer_elapsed_dt(&ctxt->time_pts));
    if (elapsed > counter) {
        double tot_factor = 100 / (update_tm + render_tm);
        double update_factor = 100 / (update_tm);

        ilog("Average FPS: %.02f  Update:%.02f%%  Render:%.02f%%",
             ctxt->finished_frames / elapsed,
             update_tm * tot_factor,
             render_tm * tot_factor);
        counter += 2.0;
        update_tm = 0.0;
        render_tm = 0.0;
    }

    return res;
}

int terminate(platform_ctxt *ctxt, void *user_data)
{
    auto app = (app_data *)user_data;
    terminate_renderer(&app->rndr);
    terminate_keymap(&app->global_km);
    terminate_sim_region(&app->rgn);
    terminate_cache_group_default_types(&app->cg);
    return err_code::PLATFORM_NO_ERROR;
}

int configure_platform(platform_init_info *settings, app_data *app)
{
    settings->wind.resolution = {800, 600};
    settings->wind.title = "RDev";
    settings->default_log_level = LOG_DEBUG;
    settings->user_cb.init = init;
    settings->user_cb.run_frame = run_frame;
    settings->user_cb.terminate = terminate;
    settings->mem.free_list_size = 4 * 1024 * MB_SIZE;

    return err_code::PLATFORM_NO_ERROR;
}

DEFINE_APPLICATION_MAIN(app_data, configure_platform);
