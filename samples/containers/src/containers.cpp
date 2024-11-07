#include "platform.h"
#include "logging.h"
#include "rid.h"
#include "string_archive.h"
#include "containers/string.h"
#include "containers/hashmap.h"
#include "containers/hashset.h"
#include "containers/hmap.h"

using namespace nslib;

struct app_data
{};

struct custom_type_0
{
    int val1;
    rid id;
};

u64 hash_type(const custom_type_0 &item, u64 s0, u64 s1)
{
    return hash_type(item.id, s0, s1);
}

bool operator==(const custom_type_0 &lhs, const custom_type_0 &rhs)
{
    return (lhs.id == rhs.id && lhs.val1 == rhs.val1);
}

string to_str(const custom_type_0 &item)
{
    string ret;
    str_printf(&ret, "val1:%d str:%s", item.val1, to_cstr(item.id));
    return ret;
}

struct custom_type_1
{
    int val1;
    string str;
};

u64 hash_type(const custom_type_1 &item, u64 s0, u64 s1)
{
    return hash_type(item.str, s0, s1);
}

bool operator==(const custom_type_1 &lhs, const custom_type_1 &rhs)
{
    return (lhs.str == rhs.str && lhs.val1 == rhs.val1);
}

string to_str(const custom_type_1 &item)
{
    string ret;
    str_printf(&ret, "val1:%d str:%s", item.val1, str_cstr(&item.str));
    return ret;
}

struct custom_type_2
{
    int val1;
    int val2;
};

string to_str(const custom_type_2 &item)
{
    string ret;
    str_printf(&ret, "val1:%d val2:%d", item.val1, item.val2);
    return ret;
}

void test_strings()
{
    dlog("Starting string test");
    string s = "test this range we are going to make a big fatty string";
    auto first = &s[4];
    auto last = &s[9];
    ilog("String before erase: %s  size:%lu  cap:%lu", str_cstr(s), str_len(s), str_capacity(s));
    str_erase(&s, first, last);
    ilog("String after erase: %s  size:%lu  cap:%lu", str_cstr(s), str_len(s), str_capacity(s));
    str_shrink_to_fit(&s);
    ilog("String cap after shrink to fit:%lu", str_capacity(s));
    str_erase(&s, &s[2], &s[10]);
    str_erase(&s, &s[2], &s[10]);
    ilog("String after more erasing: %s  size:%lu  cap:%lu", str_cstr(s), str_len(s), str_capacity(s));
    str_shrink_to_fit(&s);
    ilog("String cap after shrink to fit:%lu", str_capacity(s));
    str_erase(&s, &s[2], &s[10]);
    str_erase(&s, &s[2], &s[10]);
    ilog("String after even more erasing: %s  size:%lu  cap:%lu", str_cstr(s), str_len(s), str_capacity(s));
    str_shrink_to_fit(&s);
    ilog("String cap after shrink to fit:%lu", str_capacity(s));
}

void test_arrays()
{
    dlog("Starting array test");
    array<int> arr1;
    array<rid> rids;
    array<array<int>> arr_arr;
    string output;

    arr_emplace_back(&arr1, 35);
    arr_emplace_back(&arr1, 22);
    arr_emplace_back(&arr1, 12);
    arr_emplace_back(&arr1, 9);
    arr_emplace_back(&arr1, -122);

    arr_push_back(&arr_arr, arr1);

    for (int i = 0; i < arr1.size; ++i) {
        ilog("Arr1[%d]: %d", i, arr1[i]);
        auto arr2(arr1);
        for (int i = 0; i < arr2.size; ++i) {
            ilog("Arr2[%d]: %d", i, arr2[i]);
        }
        // arr_push_back(&arr_arr, arr2);
    }

    arr_push_back(&rids, rid("key1"));
    arr_push_back(&rids, rid("key2"));
    arr_push_back(&rids, rid("key3"));
    arr_push_back(&rids, rid("key4"));

    auto iter = arr_begin(&rids);
    while (iter != arr_end(&rids)) {
        output += to_str(*iter);
        ++iter;
    }

    // auto iter2 = arr_begin(&arr_arr);
    // while (iter2 != arr_end(&arr_arr)) {
    //     output += to_str(*iter2);
    //     ++iter2;
    // }
    ilog("Output: %s", str_cstr(&output));
}

void test_hashmaps()
{
    dlog("Starting hashmap test");
    hashmap<rid, custom_type_2> hm1;
    hashmap<string, custom_type_2> hm2;
    hashmap_init(&hm1, mem_global_arena());
    hashmap_init(&hm2, mem_global_arena());

    hashset<rid> hs1;
    hashset<string> hs2;
    hashset_init(&hs1, mem_global_arena());
    hashset_init(&hs2, mem_global_arena());

    hashset<custom_type_0> hs3;
    hashset<custom_type_1> hs4;
    hashset_init(&hs3, mem_global_arena());
    hashset_init(&hs4, mem_global_arena());

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

    ilog("HM1 %s", to_cstr(hm1));
    ilog("HM2 %s", to_cstr(hm2));
    ilog("HS1 %s", to_cstr(hs1));
    ilog("HS2 %s", to_cstr(hs2));
    ilog("HS3 %s", to_cstr(hs3));
    ilog("HS4 %s", to_cstr(hs4));
}

void test_new_hashmaps()
{
    ilog("Starting new hashmap test");

    hmap<rid, custom_type_0> hm1{};
    hmap_init(&hm1, hash_type, generate_rand_seed(), generate_rand_seed(), mem_global_arena(), 4);

    hmap_insert(&hm1, rid("blabla"), {5, rid("blabla")});
    hmap_insert(&hm1, rid("bleeblee"), {8, rid("bleeblee")});
    hmap_insert(&hm1, rid("kowabungu"), {405, rid("kowabungu")});

    ilog("Forward...");
    auto iter = hmap_first(&hm1);
    while (iter) {
        ilog("key: %s  value:%s", to_cstr(iter->key), to_cstr(iter->val));
        iter = hmap_next(&hm1, iter);
    }

    ilog("Reverse...");
    iter = hmap_last(&hm1);
    while (iter) {
        ilog("key: %s  value:%s", to_cstr(iter->key), to_cstr(iter->val));
        iter = hmap_prev(&hm1, iter);
    }

    hmap_terminate(&hm1);
}

int app_init(platform_ctxt *ctxt, void *)
{
    test_strings();
    test_arrays();
    test_hashmaps();
    test_new_hashmaps();
    return err_code::PLATFORM_NO_ERROR;
}

int configure_platform(platform_init_info *config, app_data *app)
{
    config->wind.resolution = {1920, 1080};
    config->wind.title = "Containers";
    config->user_cb.init = app_init;
    return err_code::PLATFORM_NO_ERROR;
}

DEFINE_APPLICATION_MAIN(app_data, configure_platform)
