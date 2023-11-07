#include "platform.h"
#include "string_archive.h"
#include "json_archive.h"
#include "binary_archive.h"
#include "robj_common.h"
#include "math/vector4.h"

using namespace nslib;

struct app_data
{};

struct fancy_struct
{
    string str1;
    string str2;
    string strarr[5];
};

pup_func(fancy_struct)
{
    pup_member(str1);
    pup_member(str2);
    pup_member(strarr);
}

struct data_to_pup
{
    robj_common robj;
    fancy_struct fs;
    vec4 v4;
    vec4 v4_arr[5];
    vec4 v4_arr_of_arr[5][5];
    static_array<vec2, 5> v2_sa;
};

pup_func(data_to_pup)
{
    pup_member(robj);
    pup_member(fs);
    pup_member(v4);
    pup_member(v4_arr);
    pup_member(v4_arr_of_arr);
    pup_member(v2_sa);
}

void seed_data(data_to_pup *data)
{
    data->robj.id = rid("sample_id");
    data->fs = {"str1_text", "str2_text", {"choice1", "choice2", "choice3", "choice4", "choice5"}};
    data->v2_sa = {{2, 3, 4.4, 9.1, 2.3}, 2};
    data->v4 = {4, 3, 2, 1};
    for (int i = 0; i < 5; ++i) {
        data->v4_arr[i] = {i * 1.5f, i * 2.2f, i * 3.5f, i * 4.2f};
        for (int j = 0; j < 5; ++j) {
            data->v4_arr_of_arr[i][j] = {i + j * 1.4f, i + 2.8f * j, i + 3.3f * j, i + 4.2f * j};
        }
    }
}

void clear_data(data_to_pup *data)
{
    data->robj.id = {};
    data->fs = {{}, {}, {}};
    data->v2_sa = {{}, 0};
    data->v4 = {};
    for (int i = 0; i < 5; ++i) {
        data->v4_arr[i] = {};
        for (int j = 0; j < 5; ++j) {
            data->v4_arr_of_arr[i][j] = {};
        }
    }
}

int load_platform_settings(platform_init_info *settings, app_data *app)
{
    settings->wind.resolution = {1920, 1080};
    settings->wind.title = "05 Pack Unpack";
    return err_code::PLATFORM_NO_ERROR;
}

int app_init(platform_ctxt *ctxt, app_data *app)
{
    ilog("App init");
    data_to_pup data{};
    seed_data(&data);

    json_archive ja{};
    jsa_init(&ja);
    pup_var(&ja, data, {"data_to_pup"});
    string js_str = jsa_to_json_string(&ja);
    jsa_terminate(&ja);
    ilog("JSON:\n%s", str_cstr(js_str));
    str_terminate(&js_str);
    

    // platform_write_file("data.json", str_cstr(js_str), 1, str_len(js_str));
    // jsa_terminate(&ja);
    

    // binary_fixed_buffer_archive<1000> ba{};
    // pup_var(&ba, data, {"data_to_pup"});

    // platform_file_err_desc err;
    // platform_write_file("data.bin", ba.data, 1, ba.cur_offset, 0, &err);
    // if (err.code != err_code::FILE_NO_ERROR) {
    //     wlog("File write error: %s", err.str);
    // }

    // err = {};
    // ba = {};
    // ba.opmode = archive_opmode::UNPACK;

    // sizet read_ind = platform_read_file("data.bin", ba.data, 1, ba.size, 0, &err);
    // if (err.code != err_code::FILE_NO_ERROR) {
    //     wlog("File read error: %s", err.str);
    // }

    // clear_data(&data);
    // pup_var(&ba, data, {"data_to_pup"});




    // json_archive ja_in{};
    // jsa_init(&ja_in, str_cstr(js_str));
    // clear_data(&data);
    // pup_var(&ja_in, data, {"data_to_pup"});
    // jsa_terminate(&ja_in);

//    ilog("data_to_pup: \n%s", makecstr(data));
    return err_code::PLATFORM_NO_ERROR;
}

int app_terminate(platform_ctxt *ctxt, app_data *app)
{
    ilog("App terminate");
    return err_code::PLATFORM_NO_ERROR;
}

int app_run_frame(platform_ctxt *ctxt, app_data *app)
{
    return err_code::PLATFORM_NO_ERROR;
}

DEFINE_APPLICATION_MAIN(app_data)
