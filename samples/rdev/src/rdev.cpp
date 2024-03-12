#include "platform.h"
#include "renderer.h"
#include "sim_region.h"
#include "vk_context.h"

using namespace nslib;

struct app_data
{
    renderer rndr{};
    sim_region rgn{};
    u32 cam_id;
};

int init(platform_ctxt *ctxt, void *user_data)
{
    auto app = (app_data *)user_data;    
    sim_region_init(&app->rgn, &ctxt->arenas.free_list);

    // Create a bunch of entities
    int count = 5000;
    add_entities(count, &app->rgn);

    for (int i = 0; i < app->rgn.ents.size; ++i) {
        auto tfcomp = add_comp<transform>(&app->rgn.ents[i]);
        add_comp<static_model>(&app->rgn.ents[i]);
        int mod = i - count / 2;
        tfcomp->world_pos = vec3{0, 0, mod * 0.05f};
        tfcomp->cached = math::model_tform(tfcomp->world_pos, tfcomp->orientation, tfcomp->scale);
    }

    // Create camera
    auto sz = platform_framebuffer_size(ctxt->win_hndl);
    auto cam = add_entity("Editor_Cam", &app->rgn);
    auto cam_comp = add_comp<camera>(cam);
    cam_comp->proj = (math::perspective(45.0f, (f32)sz.w / (f32)sz.h, 0.1f, 1000.0f));
    cam_comp->view = (math::look_at(vec3{0.0f, 10.0f, -5.0f}, vec3{0.0f}, vec3{0.0f, 1.0f, 0.0f}));
    app->cam_id = cam->id;
    
    int ret = renderer_init(&app->rndr, ctxt->win_hndl, &ctxt->arenas.free_list);    
    return ret;
}

int run_frame(platform_ctxt *ctxt, void *user_data)
{
    auto app = (app_data *)user_data;
    auto cam = get_comp<camera>(app->cam_id, &app->rgn.cdb);

    f64 elapsed_s = nanos_to_sec(ptimer_elapsed_dt(&ctxt->time_pts));
    auto tform_tbl = get_comp_tbl<transform>(&app->rgn.cdb);
    for (sizet i = 0; i < tform_tbl->entries.size / 100; ++i) {
        sizet ind = i * 100;
        auto curtf = &tform_tbl->entries[ind];
        curtf->orientation *= math::orientation(vec4{0.0, 0.0, 1.0, (f32)ctxt->time_pts.dt});
        curtf->world_pos.x = math::sin((f32)elapsed_s);
        curtf->world_pos.y = math::cos((f32)elapsed_s);
        curtf->cached = math::model_tform(curtf->world_pos, curtf->orientation, curtf->scale);
    }

    return render_frame(&app->rndr, &app->rgn, cam, ctxt->finished_frames);
}

int terminate(platform_ctxt *ctxt, void *user_data)
{
    auto app = (app_data *)user_data;
    sim_region_terminate(&app->rgn);
    renderer_terminate(&app->rndr);
    return err_code::PLATFORM_NO_ERROR;
}

int configure_platform(platform_init_info *settings, app_data *app)
{
    settings->wind.resolution = {800, 600};
    settings->wind.title = "RDEV";
    settings->user_cb.init = init;
    settings->user_cb.run_frame = run_frame;
    settings->user_cb.terminate = terminate;
    return err_code::PLATFORM_NO_ERROR;
}

DEFINE_APPLICATION_MAIN(app_data, configure_platform);
