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
    settings->wind.title = "03 Triangle";
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
    for (int i = 0; i < 5; ++i) {
        bla_arr[i] = {i*1.0f, i*2.0f, i*3.0f, i*4.0f};
    }
    
    
    binary_fixed_buffer_archive<1000> ba{};
    pup_var(&ba, test, {"test"});
    pup_var(&ba, bla, {"bla"});
    pup_var(&ba, bla_arr, {"bla_arr"});
    pup_var(&ba, fs, {"bla_arr"});
    
    
    platform_file_err_desc err;
    platform_write_file("bin_out.bin", ba.data, 1, ba.cur_offset, 0, &err);
    if (err.code != err_code::FILE_NO_ERROR) {
        wlog("File write error: %s", err.str);
    }
    

    err = {};
    ba = {};
    ba.dir = pack_dir::IN;
    
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
    memset(bla_arr, 0, sizeof(vec4)*5);
    
    
    pup_var(&ba, test, {"test"});
    pup_var(&ba, bla, {"bla"});
    pup_var(&ba, bla_arr, {"bla_arr"});
    pup_var(&ba, fs, {"bla_arr"});

    string_archive sa{};
    pup_var(&sa, test, {"test"});
    pup_var(&sa, bla, {"bla"});
    pup_var(&sa, bla_arr, {"bla_arr"});
    pup_var(&sa, fs, {"fancy_struc"});
    ilog("Scooby:\n%s", str_cstr(sa.txt));

    json_archive ja{};
    pup_var(&ja, test, {"test"});
    pup_var(&ja, bla, {"bla"});
    pup_var(&ja, bla_arr, {"bla_arr"});
    pup_var(&ja, fs, {"fancy_struc"});
    ilog("Scooby:\n%s", str_cstr(sa.txt));
    
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

