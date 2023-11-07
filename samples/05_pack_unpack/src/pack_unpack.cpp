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
    string str1{"ohhbabay"};
    string str2{"yeah"};
    string strarr[5]{"yes", "no", "maybe", "so", "okay"};
};

pup_func(fancy_struct)
{
    pup_member(str1);
    pup_member(str2);
    pup_member(strarr);
}    

int load_platform_settings(platform_init_info *settings, app_data *app)
{
    settings->wind.resolution = {1920,1080};
    settings->wind.title = "05 Pack Unpack";
    return err_code::PLATFORM_NO_ERROR;
}

int app_init(platform_ctxt *ctxt, app_data *app)
{
    ilog("App init");

    robj_common test;
    test.id = rid("dangman");
    fancy_struct fs;
    vec4 bla{4,3,2,1};
    vec4 bla_arr[5];
    vec4 bla_arr_2[5][5];
    for (int i = 0; i < 5; ++i) {
        bla_arr[i] = {i*1.0f, i*2.0f, i*3.0f, i*4.0f};
        for (int j = 0; j < 5; ++j) {
            bla_arr_2[i][j] = {i + j*1.0f, i + 2.0f*j, i + 3.0f*j, i + 4.0f*j };
        }
    }
    
    binary_fixed_buffer_archive<1000> ba{};
    pup_var(&ba, test, {"test"});
    pup_var(&ba, bla, {"bla"});
    pup_var(&ba, bla_arr, {"bla_arr"});
    pup_var(&ba, bla_arr_2, {"bla_arr_2"});
    pup_var(&ba, fs, {"fancy_struc"});
    
    
    platform_file_err_desc err;
    platform_write_file("bin_out.bin", ba.data, 1, ba.cur_offset, 0, &err);
    if (err.code != err_code::FILE_NO_ERROR) {
        wlog("File write error: %s", err.str);
    }
    

    err = {};
    ba = {};
    ba.opmode = archive_opmode::UNPACK;
    
    sizet read_ind = platform_read_file("bin_out.bin", ba.data, 1, ba.size, 0, &err);
    if (err.code != err_code::FILE_NO_ERROR) {
        wlog("File read error: %s", err.str);
    }

    test = {};
    bla = {};
    str_clear(&fs.str1);
    str_clear(&fs.str2);
    for (int i = 0; i < 5; ++i) {
        str_clear(&fs.strarr[i]);
    }
    for (int i = 0; i < 5; ++i) {
        bla_arr[i] = {};
        for (int j = 0; j < 5; ++j) {
            bla_arr_2[i][j] = {};
        }
    }
    
    
    
    pup_var(&ba, test, {"test"});
    pup_var(&ba, bla, {"bla"});
    pup_var(&ba, bla_arr, {"bla_arr"});
    pup_var(&ba, bla_arr_2, {"bla_arr_2"});
    pup_var(&ba, fs, {"bla_arr"});

    ilog("test: \n%s", makecstr(test));
    ilog("bla: \n%s", makecstr(bla));
    ilog("bla_arr: \n%s", makecstr(bla_arr));
    ilog("bla_arr_2: \n%s", makecstr(bla_arr_2));
    ilog("fancy_strc: \n%s", makecstr(fs));

    string simpl_str_test = "simple_test";
    json_archive ja{};
    jsa_init(&ja, nullptr);

    pup_var(&ja, simpl_str_test, {"str_test"});
    pup_var(&ja, bla, {"bla"});
    pup_var(&ja, bla_arr, {"bla_arr"});
    pup_var(&ja, bla_arr_2, {"bla_arr_2"});
    pup_var(&ja, fs, {"fancy_struc"});
    string js_str = jsa_to_json_string(&ja);
    platform_write_file("check_me_out.json", str_cstr(js_str), 1, str_len(js_str));
    ilog("JSON:\n%s", str_cstr(js_str));
    jsa_terminate(&ja);

    str_clear(&simpl_str_test);
    test = {};
    bla = {};
    str_clear(&fs.str1);
    str_clear(&fs.str2);
    for (int i = 0; i < 5; ++i) {
        str_clear(&fs.strarr[i]);
    }
    for (int i = 0; i < 5; ++i) {
        bla_arr[i] = {};
        for (int j = 0; j < 5; ++j) {
            bla_arr_2[i][j] = {};
        }
    }

    json_archive ja_in{};
    jsa_init(&ja_in, str_cstr(js_str));
    pup_var(&ja_in, simpl_str_test, {"str_test"});
    pup_var(&ja_in, bla, {"bla"});
    pup_var(&ja_in, bla_arr, {"bla_arr"});
    pup_var(&ja_in, bla_arr_2, {"bla_arr_2"});
    pup_var(&ja_in, fs, {"fancy_struc"});

    ilog("test2: \n%s", makecstr(test));
    ilog("bla2: \n%s", makecstr(bla));
    ilog("bla_arr2: \n%s", makecstr(bla_arr));
    ilog("bla_arr_22: \n%s", makecstr(bla_arr_2));
    ilog("fancy_strc2: \n%s", makecstr(fs));
    
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

