#include "input_mapping.h"
#include "platform.h"
#include "logging.h"
#include "robj_common.h"
#include "containers/string.h"
#include "containers/hashmap.h"
#include "containers/hashset.h"

using namespace nslib;

struct app_data
{};

struct custom_type_0 {
    int val1;
    rid id;
};

u64 hash_type(const custom_type_0 &item, u64 s0, u64 s1) {
    return hash_type(item.id, s0, s1);
}

bool operator==(const custom_type_0 &lhs, const custom_type_0 &rhs) {
    return (lhs.id == rhs.id && lhs.val1 == rhs.val1);
}

string makestr(const custom_type_0 &item) {
    string ret;
    str_args(&ret, "val1:%d str:%s", item.val1, makecstr(item.id));
    return ret;
}

struct custom_type_1 {
    int val1;
    string str;
};

u64 hash_type(const custom_type_1 &item, u64 s0, u64 s1) {
    return hash_type(item.str, s0, s1);
}

bool operator==(const custom_type_1 &lhs, const custom_type_1 &rhs) {
    return (lhs.str == rhs.str && lhs.val1 == rhs.val1);
}

string makestr(const custom_type_1 &item) {
    string ret;
    str_args(&ret, "val1:%d str:%s", item.val1, str_cstr(&item.str));
    return ret;
}

struct custom_type_2 {
    int val1;
    int val2;
};

string makestr(const custom_type_2 &item) {
    string ret;
    str_args(&ret, "val1:%d val2:%d", item.val1, item.val2);
    return ret;
}

int load_platform_settings(platform_init_info *settings, app_data *app)
{
    settings->wind.resolution = {1920, 1080};
    settings->wind.title = "04 Containers";
    return err_code::PLATFORM_NO_ERROR;
}

void test_strings()
{
    
}

void test_arrays()
{
    ilog("Starting array test");
    array<int> arr1;
    array<rid> rids;
    string output;
    
    arr_emplace_back(&arr1, 35);
    arr_emplace_back(&arr1, 22);
    arr_emplace_back(&arr1, 12);
    arr_emplace_back(&arr1, 9);
    arr_emplace_back(&arr1, -122);

    for (int i = 0; i < arr1.size; ++i) {
        ilog("Arr1[%d]: %d", i, arr1[i]);
        auto arr2(arr1);
        for (int i = 0; i < arr2.size; ++i) {
            ilog("Arr2[%d]: %d", i, arr2[i]);
        }
    }

    arr_push_back(&rids, rid("key1"));
    arr_push_back(&rids, rid("key2"));
    arr_push_back(&rids, rid("key3"));
    arr_push_back(&rids, rid("key4"));
    
    auto iter = arr_begin(&rids);
    while (iter != arr_end(&rids)) {
        output += makestr(*iter);
        ++iter;
    }
    ilog("Output: %s", str_cstr(&output));
    ilog("Finished array test");
}

void test_hashmaps()
{
    hashmap<rid, custom_type_2> hm1;
    hashmap<string, custom_type_2> hm2;
    hashmap_init(&hm1);
    hashmap_init(&hm2);
    
    hashset<rid> hs1;
    hashset<string> hs2;
    hashset_init(&hs1);
    hashset_init(&hs2);

    hashset<custom_type_0> hs3;
    hashset<custom_type_1> hs4;
    hashset_init(&hs3);
    hashset_init(&hs4);

    hashmap_set(&hm1, rid("key1"), {1, 2});
    hashmap_set(&hm1, rid("key2"), {3, 4});
    hashmap_set(&hm1, rid("key3"), {5, 6});
    hashmap_set(&hm1, rid{"key4"}, {7, 8});

    hashmap_set(&hm2, string("key1"), {1, 2});
    hashmap_set(&hm2, string("key2"), {3, 4});
    hashmap_set(&hm2, string("key3"), {5, 6});
    hashmap_set(&hm2, string{"key4"}, {7, 8});

    hashset_set(&hs1, rid{"key1"});
    hashset_set(&hs1, rid{"key2"});
    hashset_set(&hs1, rid{"key3"});
    hashset_set(&hs1, rid{"key4"});

    hashset_set(&hs2, string{"key1"});
    hashset_set(&hs2, string{"key2"});
    hashset_set(&hs2, string{"key3"});
    hashset_set(&hs2, string{"key4"});
    
    hashset_set(&hs3, custom_type_0{1, rid{"key1"}});
    hashset_set(&hs3, custom_type_0{2, rid{"key2"}});
    hashset_set(&hs3, custom_type_0{3, rid{"key3"}});
    hashset_set(&hs3, custom_type_0{4, rid{"key4"}});

    hashset_set(&hs4, custom_type_1{1, "key1"});
    hashset_set(&hs4, custom_type_1{2, "key2"});
    hashset_set(&hs4, custom_type_1{3, "key3"});
    hashset_set(&hs4, custom_type_1{4, "key4"});

    ilog("HM1 %s", makecstr(hm1));
    ilog("HM2 %s", makecstr(hm2));
    ilog("HS1 %s", makecstr(hs1));
    ilog("HS2 %s", makecstr(hs2));
    ilog("HS3 %s", makecstr(hs3));
    ilog("HS4 %s", makecstr(hs4));
}


int app_init(platform_ctxt *ctxt, app_data *app)
{
//    test_strings();
    test_arrays();
    test_hashmaps();
    return err_code::PLATFORM_NO_ERROR;
}

int app_terminate(platform_ctxt *ctxt, app_data *app)
{

    return err_code::PLATFORM_NO_ERROR;
}

int app_run_frame(platform_ctxt *ctxt, app_data *app)
{
    return err_code::PLATFORM_TERMINATE;
}

DEFINE_APPLICATION_MAIN(app_data)
