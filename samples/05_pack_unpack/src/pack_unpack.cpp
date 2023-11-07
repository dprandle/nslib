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
    ilog("Seeding data");
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
    ilog("Clearing data");
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
    settings->default_log_level = LOG_DEBUG;
    return err_code::PLATFORM_NO_ERROR;
}

int app_init(platform_ctxt *ctxt, app_data *app)
{
    ilog("App init");
    data_to_pup data{};

    seed_data(&data);

    static_binary_buffer_archive<10000> ba{};
    ilog("Packing to static binary buffer archive");
    pup_var(&ba, data, {"data_to_pup"});

    platform_file_err_desc err;
    ilog("Saving binary data to data.bin");
    platform_write_file("data.bin", ba.data, 1, ba.cur_offset, 0, &err);
    if (err.code != err_code::FILE_NO_ERROR) {
        wlog("File write error: %s", err.str);
    }

    ilog("Clearing static binary buffer archive and setting to unpack mode");
    err = {};
    ba = {};
    ba.opmode = archive_opmode::UNPACK;
    clear_data(&data);

    ilog("Reading in binary data to static binary buffer archive");
    sizet read_ind = platform_read_file("data.bin", ba.data, 1, ba.size, 0, &err);
    if (err.code != err_code::FILE_NO_ERROR) {
        wlog("File read error: %s", err.str);
    }

    ilog("Unpacking binary buffer archive to data_to_pup");
    pup_var(&ba, data, {"data_to_pup"});
    
    ilog("data_to_pup after unpacking: \n%s", makecstr(data));
    
    json_archive ja{};
    jsa_init(&ja);
    ilog("Packing data_to_pup to json archive");
    pup_var(&ja, data, {"data_to_pup"});
    string js_str = jsa_to_json_string(&ja, true);
    string js_compact_str = jsa_to_json_string(&ja, false);
    
    jsa_terminate(&ja);
    ilog("Resulting JSON pretty string:\n%s", str_cstr(js_str));
    ilog("Resulting JSON compact string:\n%s", str_cstr(js_compact_str));
    platform_write_file("data.json", str_cstr(js_str), 1, str_len(js_str));

    clear_data(&data);
    
    json_archive ja_in{};
    jsa_init(&ja_in, str_cstr(js_str));
    pup_var(&ja_in, data, {"data_to_pup"});
    jsa_terminate(&ja_in);

    ilog("data_to_pup json in: \n%s", makecstr(data));
    
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
