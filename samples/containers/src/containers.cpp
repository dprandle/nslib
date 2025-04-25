#include "platform.h"
#include "logging.h"
#include "rid.h"
#include "containers/string.h"
#include "hashfuncs.h"
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

    hmap<char, string> hm1{};
    hmap_init(&hm1, hash_type);
    int orig_mult = 24;

#define IND_EXP(bucket_ind, bucket_cnt) (u32)(bucket_ind + bucket_cnt * orig_mult)

    hmap_insert(&hm1, 'a', string("a"));
    hmap_insert(&hm1, 'b', string("b"));
    hmap_insert(&hm1, 'c', string("c"));
    hmap_insert(&hm1, 'd', string("d"));
    hmap_insert(&hm1, 'e', string("e"));
    hmap_insert(&hm1, 'f', string("f"));
    hmap_insert(&hm1, 'g', string("g"));
    hmap_insert(&hm1, 'h', string("h"));
    hmap_insert(&hm1, 'i', string("i"));
    hmap_insert(&hm1, 'j', string("j"));
    hmap_insert(&hm1, 'k', string("k"));
    hmap_insert(&hm1, 'l', string("l"));
    hmap_insert(&hm1, 'm', string("m"));
    hmap_insert(&hm1, 'n', string("n"));
    hmap_insert(&hm1, 'o', string("o"));
    hmap_insert(&hm1, 'p', string("p"));
    hmap_insert(&hm1, 'q', string("q"));
    hmap_insert(&hm1, 'r', string("r"));
    hmap_insert(&hm1, 's', string("s"));
    hmap_insert(&hm1, 't', string("t"));
    hmap_insert(&hm1, 'u', string("u"));
    hmap_insert(&hm1, 'v', string("v"));
    hmap_insert(&hm1, 'w', string("w"));
    hmap_insert(&hm1, 'x', string("x"));

    ilog("Forward...");
    auto iter = hmap_first(&hm1);
    while (iter) {
        ilog("key: %s  value:%s", to_cstr((u32)iter->key), str_cstr(iter->val));
        iter = hmap_next(&hm1, iter);
    }
    ilog("Reverse...");
    iter = hmap_last(&hm1);
    while (iter) {
        ilog("key: %s  value:%s", to_cstr(iter->key), str_cstr(iter->val));
        iter = hmap_prev(&hm1, iter);
    }
    ilog("Buckets...");
    hmap_debug_print(hm1.buckets);

    auto fnd = hmap_find(&hm1, 'a');
    ilog("Found value %s for key %s", to_cstr(fnd->val), to_cstr(fnd->key));
    fnd = hmap_find(&hm1, 'e');
    ilog("Found value %s for key %s", to_cstr(fnd->val), to_cstr(fnd->key));
    fnd = hmap_find(&hm1, 'i');
    ilog("Found value %s for key %s", to_cstr(fnd->val), to_cstr(fnd->key));
    fnd = hmap_find(&hm1, 'o');
    ilog("Found value %s for key %s", to_cstr(fnd->val), to_cstr(fnd->key));
    fnd = hmap_find(&hm1, 'u');
    ilog("Found value %s for key %s", to_cstr(fnd->val), to_cstr(fnd->key));
    fnd = hmap_find(&hm1, 'd');
    ilog("Found value %s for key %s", to_cstr(fnd->val), to_cstr(fnd->key));
    fnd = hmap_find(&hm1, 'c');
    ilog("Found value %s for key %s", to_cstr(fnd->val), to_cstr(fnd->key));
    fnd = hmap_find(&hm1, 'z');
    if (fnd) {
        ilog("Found value %s for key %s", to_cstr(fnd->val), to_cstr(fnd->key));
    }
    else {
        ilog("Could not find key %c", 'z');
    }

    ilog("Removed a: %s", (hmap_remove(&hm1, 'a')) ? "true" : "false");
    ilog("Removed b: %s", (hmap_remove(&hm1, 'b')) ? "true" : "false");
    ilog("Removed c: %s", (hmap_remove(&hm1, 'c')) ? "true" : "false");
    ilog("Removed e: %s", (hmap_remove(&hm1, 'e')) ? "true" : "false");
    ilog("Removed i: %s", (hmap_remove(&hm1, 'i')) ? "true" : "false");
    ilog("Removed o: %s", (hmap_remove(&hm1, 'o')) ? "true" : "false");
    ilog("Removed u: %s", (hmap_remove(&hm1, 'u')) ? "true" : "false");
    ilog("Removed y: %s", (hmap_remove(&hm1, 'y')) ? "true" : "false");

    ilog("Forward...");
    iter = hmap_first(&hm1);
    while (iter) {
        ilog("key: %s  value:%s", to_cstr((u32)iter->key), str_cstr(iter->val));
        iter = hmap_next(&hm1, iter);
    }
    ilog("Reverse...");
    iter = hmap_last(&hm1);
    while (iter) {
        ilog("key: %s  value:%s", to_cstr(iter->key), str_cstr(iter->val));
        iter = hmap_prev(&hm1, iter);
    }
    ilog("Buckets...");
    hmap_debug_print(hm1.buckets);

    auto ins = hmap_insert(&hm1, 'a', string("a"));
    ilog("Inserted a ptr: %p", ins);

    ins = hmap_insert(&hm1, 'b', string("b"));
    ilog("Inserted b ptr: %p", ins);

    ins = hmap_insert(&hm1, 'c', string("c"));
    ilog("Inserted c ptr: %p", ins);

    ins = hmap_insert(&hm1, 'd', string("d"));
    ilog("Inserted d ptr: %p", ins);

    ins = hmap_insert(&hm1, 'e', string("e"));
    ilog("Inserted e ptr: %p", ins);

    ins = hmap_insert(&hm1, 'f', string("f"));
    ilog("Inserted f ptr: %p", ins);

    ins = hmap_insert(&hm1, 'g', string("g"));
    ilog("Inserted g ptr: %p", ins);

    ins = hmap_insert(&hm1, 'o', string("o"));
    ilog("Inserted o ptr: %p", ins);

    ilog("Forward...");
    iter = hmap_first(&hm1);
    while (iter) {
        ilog("key: %s  value:%s", to_cstr((u32)iter->key), str_cstr(iter->val));
        iter = hmap_next(&hm1, iter);
    }

    ilog("Reverse...");
    iter = hmap_last(&hm1);
    while (iter) {
        ilog("key: %s  value:%s", to_cstr(iter->key), str_cstr(iter->val));
        iter = hmap_prev(&hm1, iter);
    }

    ilog("Buckets...");
    hmap_debug_print(hm1.buckets);

    hmap_terminate(&hm1);
}

void test_new_hashmaps_string_keys()
{
    ilog("Starting new hashmap test");

    hmap<rid, string> hm1{};

    hmap_init(&hm1, hash_type);
    hmap_insert(&hm1, rid("scooby"), string("scooby-data"));
    hmap_insert(&hm1, rid("sandwiches"), string("sandwiches-data"));
    hmap_insert(&hm1, rid("alowishish"), string("alowishish-data"));
    hmap_insert(&hm1, rid("do-the-dance"), string("do-the-dance-data"));
    hmap_insert(&hm1, rid("booty_cake"), string("booty_cake-data"));
    hmap_insert(&hm1, rid("gogogo300"), string("gogogo300-data"));
    hmap_insert(&hm1, rid("67-under"), string("67-under-data"));
    hmap_insert(&hm1, rid("kjhj"), string("kjhj-data"));
    hmap_insert(&hm1, rid("lemar"), string("lemar-data"));

    ilog("Forward...");
    auto iter = hmap_first(&hm1);
    while (iter) {
        ilog("key: %s  value:%s", to_cstr(iter->key), str_cstr(iter->val));
        iter = hmap_next(&hm1, iter);
    }

    ilog("Reverse...");
    iter = hmap_last(&hm1);
    while (iter) {
        ilog("key: %s  value:%s", to_cstr(iter->key), str_cstr(iter->val));
        iter = hmap_prev(&hm1, iter);
    }

    ilog("Buckets...");
    hmap_debug_print(hm1.buckets);

    hmap_remove(&hm1, rid("do-the-dance"));
    hmap_remove(&hm1, rid("booty_cake"));
    hmap_remove(&hm1, rid("gogogo300"));
    hmap_remove(&hm1, rid("67-under"));

    ilog("Forward...");
    iter = hmap_first(&hm1);
    while (iter) {
        ilog("key: %s  value:%s", to_cstr(iter->key), str_cstr(iter->val));
        iter = hmap_next(&hm1, iter);
    }

    ilog("Reverse...");
    iter = hmap_last(&hm1);
    while (iter) {
        ilog("key: %s  value:%s", to_cstr(iter->key), str_cstr(iter->val));
        iter = hmap_prev(&hm1, iter);
    }

    ilog("Buckets...");
    hmap_debug_print(hm1.buckets);

    hmap_insert(&hm1, rid("another"), string("another-data"));
    hmap_insert(&hm1, rid("type-of"), string("type-of-data"));
    hmap_insert(&hm1, rid("thing-that"), string("thing-that-data"));
    hmap_insert(&hm1, rid("wereallyshould"), string("wereallyshould-data"));
    hmap_insert(&hm1, rid("beadding"), string("beadding-data"));

    ilog("Forward...");
    iter = hmap_first(&hm1);
    while (iter) {
        ilog("key: %s  value:%s", to_cstr(iter->key), str_cstr(iter->val));
        iter = hmap_next(&hm1, iter);
    }

    ilog("Reverse...");
    iter = hmap_last(&hm1);
    while (iter) {
        ilog("key: %s  value:%s", to_cstr(iter->key), str_cstr(iter->val));
        iter = hmap_prev(&hm1, iter);
    }

    ilog("Buckets...");
    hmap_debug_print(hm1.buckets);
    hmap_terminate(&hm1);
}

int app_init(platform_ctxt *ctxt, void *)
{
    test_strings();
    test_arrays();
    test_hashmaps();
    test_new_hashmaps();
    test_new_hashmaps_string_keys();
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
