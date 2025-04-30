#include "platform.h"
#include "logging.h"
#include "rid.h"
#include "hashfuncs.h"
#include "containers/string.h"
#include "containers/hmap.h"
#include "containers/hset.h"

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

void test_hashsets()
{
    ilog("Starting new hashset test");

    hset<char> hs1{};
    hset_init(&hs1);

    ilog("Inserting a through x");
    hset_insert(&hs1, 'a');
    hset_insert(&hs1, 'b');
    hset_insert(&hs1, 'c');
    hset_insert(&hs1, 'd');
    hset_insert(&hs1, 'e');
    hset_insert(&hs1, 'f');
    hset_insert(&hs1, 'g');
    hset_insert(&hs1, 'h');
    hset_insert(&hs1, 'i');
    hset_insert(&hs1, 'j');
    hset_insert(&hs1, 'k');
    hset_insert(&hs1, 'l');
    hset_insert(&hs1, 'm');
    hset_insert(&hs1, 'n');
    hset_insert(&hs1, 'o');
    hset_insert(&hs1, 'p');
    hset_insert(&hs1, 'q');
    hset_insert(&hs1, 'r');
    hset_insert(&hs1, 's');
    hset_insert(&hs1, 't');
    hset_insert(&hs1, 'u');
    hset_insert(&hs1, 'v');
    hset_insert(&hs1, 'w');
    hset_insert(&hs1, 'x');

    ilog("Forward...");
    auto iter = hset_begin(&hs1);
    while (iter) {
        ilog("item: %s", to_cstr(iter->val));
        iter = hset_next(&hs1, iter);
    }
    ilog("Reverse...");
    iter = hset_rbegin(&hs1);
    while (iter) {
        ilog("item: %s", to_cstr(iter->val));
        iter = hset_prev(&hs1, iter);
    }
    ilog("Buckets...");
    hset_debug_print(hs1.buckets);

    auto fnd = hset_find(&hs1, 'a');
    ilog("Found value %c", fnd->val);
    fnd = hset_find(&hs1, 'e');
    ilog("Found value %c", fnd->val);
    fnd = hset_find(&hs1, 'i');
    ilog("Found value %c", fnd->val);
    fnd = hset_find(&hs1, 'o');
    ilog("Found value %c", fnd->val);
    fnd = hset_find(&hs1, 'u');
    ilog("Found value %c", fnd->val);
    fnd = hset_find(&hs1, 'd');
    ilog("Found value %c", fnd->val);
    fnd = hset_find(&hs1, 'c');
    ilog("Found value %c", fnd->val);
    fnd = hset_find(&hs1, 'z');
    if (fnd) {
        ilog("Found value %s for key %s", fnd->val);
    }
    else {
        ilog("Could not find key %c", 'z');
    }

    ilog("Removed a: %s", (hset_remove(&hs1, 'a')) ? "true" : "false");
    ilog("Removed b: %s", (hset_remove(&hs1, 'b')) ? "true" : "false");
    ilog("Removed c: %s", (hset_remove(&hs1, 'c')) ? "true" : "false");
    ilog("Removed e: %s", (hset_remove(&hs1, 'e')) ? "true" : "false");
    ilog("Removed i: %s", (hset_remove(&hs1, 'i')) ? "true" : "false");
    ilog("Removed o: %s", (hset_remove(&hs1, 'o')) ? "true" : "false");
    ilog("Removed u: %s", (hset_remove(&hs1, 'u')) ? "true" : "false");
    ilog("Removed y: %s", (hset_remove(&hs1, 'y')) ? "true" : "false");

    ilog("Forward...");
    iter = hset_begin(&hs1);
    while (iter) {
        ilog("item: %s", to_cstr(iter->val));
        iter = hset_next(&hs1, iter);
    }
    ilog("Reverse...");
    iter = hset_rbegin(&hs1);
    while (iter) {
        ilog("item: %s", to_cstr(iter->val));
        iter = hset_prev(&hs1, iter);
    }
    ilog("Buckets...");
    hset_debug_print(hs1.buckets);

    auto ins = hset_insert(&hs1, 'a');
    ilog("Inserted a ptr: %p", ins);

    ins = hset_insert(&hs1, 'b');
    ilog("Inserted b ptr: %p", ins);

    ins = hset_insert(&hs1, 'c');
    ilog("Inserted c ptr: %p", ins);

    ins = hset_insert(&hs1, 'd');
    ilog("Inserted d ptr: %p", ins);

    ins = hset_insert(&hs1, 'e');
    ilog("Inserted e ptr: %p", ins);

    ins = hset_insert(&hs1, 'f');
    ilog("Inserted f ptr: %p", ins);

    ins = hset_insert(&hs1, 'g');
    ilog("Inserted g ptr: %p", ins);

    ins = hset_insert(&hs1, 'o');
    ilog("Inserted o ptr: %p", ins);

    ilog("Forward...");
    iter = hset_begin(&hs1);
    while (iter) {
        ilog("item: %s", to_cstr(iter->val));
        iter = hset_next(&hs1, iter);
    }

    ilog("Reverse...");
    iter = hset_rbegin(&hs1);
    while (iter) {
        ilog("item: %s", to_cstr(iter->val));
        iter = hset_prev(&hs1, iter);
    }

    ilog("Buckets...");
    hset_debug_print(hs1.buckets);

    hset_terminate(&hs1);
}

void test_hashmaps()
{
    ilog("Starting new hashmap test");

    hmap<char, string> hm1{};
    hmap_init(&hm1, hash_type);

    ilog("Inserting a through x");
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
    auto iter = hmap_begin(&hm1);
    while (iter) {
        ilog("key: %s  value:%s", to_cstr((u32)iter->key), str_cstr(iter->val));
        iter = hmap_next(&hm1, iter);
    }
    ilog("Reverse...");
    iter = hmap_rbegin(&hm1);
    while (iter) {
        ilog("key: %s  value:%s", to_cstr(iter->key), str_cstr(iter->val));
        iter = hmap_prev(&hm1, iter);
    }
    ilog("Buckets...");
    hmap_debug_print(hm1.buckets);

    auto fnd = hmap_find(&hm1, 'a');
    ilog("Found value %s for key %s", to_cstr(fnd->val));
    fnd = hmap_find(&hm1, 'e');
    ilog("Found value %s for key %s", to_cstr(fnd->val));
    fnd = hmap_find(&hm1, 'i');
    ilog("Found value %s for key %s", to_cstr(fnd->val));
    fnd = hmap_find(&hm1, 'o');
    ilog("Found value %s for key %s", to_cstr(fnd->val));
    fnd = hmap_find(&hm1, 'u');
    ilog("Found value %s for key %s", to_cstr(fnd->val));
    fnd = hmap_find(&hm1, 'd');
    ilog("Found value %s for key %s", to_cstr(fnd->val));
    fnd = hmap_find(&hm1, 'c');
    ilog("Found value %s for key %s", to_cstr(fnd->val));
    fnd = hmap_find(&hm1, 'z');
    if (fnd) {
        ilog("Found value %s for key %s", to_cstr(fnd->val));
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
    iter = hmap_begin(&hm1);
    while (iter) {
        ilog("key: %s  value:%s", to_cstr((u32)iter->key), str_cstr(iter->val));
        iter = hmap_next(&hm1, iter);
    }
    ilog("Reverse...");
    iter = hmap_rbegin(&hm1);
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
    iter = hmap_begin(&hm1);
    while (iter) {
        ilog("key: %s  value:%s", to_cstr((u32)iter->key), str_cstr(iter->val));
        iter = hmap_next(&hm1, iter);
    }

    ilog("Reverse...");
    iter = hmap_rbegin(&hm1);
    while (iter) {
        ilog("key: %s  value:%s", to_cstr(iter->key), str_cstr(iter->val));
        iter = hmap_prev(&hm1, iter);
    }

    ilog("Buckets...");
    hmap_debug_print(hm1.buckets);

    hmap_terminate(&hm1);
}

void test_hashmaps_string_keys()
{
    ilog("Starting new hashmap string key test");

    hmap<rid, string> hm1{};

    hmap_init(&hm1, hash_type);
    ilog("Inserting 9 strange strings");
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
    auto iter = hmap_begin(&hm1);
    while (iter) {
        ilog("key: %s  value:%s", to_cstr(iter->key), str_cstr(iter->val));
        iter = hmap_next(&hm1, iter);
    }

    ilog("Reverse...");
    iter = hmap_rbegin(&hm1);
    while (iter) {
        ilog("key: %s  value:%s", to_cstr(iter->key), str_cstr(iter->val));
        iter = hmap_prev(&hm1, iter);
    }

    ilog("Buckets...");
    hmap_debug_print(hm1.buckets);

    ilog("Removing 4 entries");
    hmap_remove(&hm1, rid("do-the-dance"));
    hmap_remove(&hm1, rid("booty_cake"));
    hmap_remove(&hm1, rid("gogogo300"));
    hmap_remove(&hm1, rid("67-under"));

    ilog("Forward...");
    iter = hmap_begin(&hm1);
    while (iter) {
        ilog("key: %s  value:%s", to_cstr(iter->key), str_cstr(iter->val));
        iter = hmap_next(&hm1, iter);
    }

    ilog("Reverse...");
    iter = hmap_rbegin(&hm1);
    while (iter) {
        ilog("key: %s  value:%s", to_cstr(iter->key), str_cstr(iter->val));
        iter = hmap_prev(&hm1, iter);
    }

    ilog("Buckets...");
    hmap_debug_print(hm1.buckets);

    ilog("Inserting 5 more strange strings");
    hmap_insert(&hm1, rid("another"), string("another-data"));
    hmap_insert(&hm1, rid("type-of"), string("type-of-data"));
    hmap_insert(&hm1, rid("thing-that"), string("thing-that-data"));
    hmap_insert(&hm1, rid("wereallyshould"), string("wereallyshould-data"));
    hmap_insert(&hm1, rid("beadding"), string("beadding-data"));

    ilog("Forward...");
    iter = hmap_begin(&hm1);
    while (iter) {
        ilog("key: %s  value:%s", to_cstr(iter->key), str_cstr(iter->val));
        iter = hmap_next(&hm1, iter);
    }

    ilog("Reverse...");
    iter = hmap_rbegin(&hm1);
    while (iter) {
        ilog("key: %s  value:%s", to_cstr(iter->key), str_cstr(iter->val));
        iter = hmap_prev(&hm1, iter);
    }

    ilog("Buckets...");
    hmap_debug_print(hm1.buckets);
    hmap_terminate(&hm1);
}

void test_hashset_string_keys()
{
    ilog("Starting new hashset string test");

    hset<rid> hs1{};

    hset_init(&hs1);
    ilog("Inserting 9 strange strings");
    hset_insert(&hs1, rid("scooby"));
    hset_insert(&hs1, rid("sandwiches"));
    hset_insert(&hs1, rid("alowishish"));
    hset_insert(&hs1, rid("do-the-dance"));
    hset_insert(&hs1, rid("booty_cake"));
    hset_insert(&hs1, rid("gogogo300"));
    hset_insert(&hs1, rid("67-under"));
    hset_insert(&hs1, rid("kjhj"));
    hset_insert(&hs1, rid("lemar"));

    ilog("Forward...");
    auto iter = hset_begin(&hs1);
    while (iter) {
        ilog("key: %s  value:%s", to_cstr(iter->val));
        iter = hset_next(&hs1, iter);
    }

    ilog("Reverse...");
    iter = hset_rbegin(&hs1);
    while (iter) {
        ilog("key: %s  value:%s", to_cstr(iter->val));
        iter = hset_prev(&hs1, iter);
    }

    ilog("Buckets...");
    hset_debug_print(hs1.buckets);

    ilog("Removing 4 strings");
    hset_remove(&hs1, rid("do-the-dance"));
    hset_remove(&hs1, rid("booty_cake"));
    hset_remove(&hs1, rid("gogogo300"));
    hset_remove(&hs1, rid("67-under"));

    ilog("Forward...");
    iter = hset_begin(&hs1);
    while (iter) {
        ilog("key: %s  value:%s", to_cstr(iter->val));
        iter = hset_next(&hs1, iter);
    }

    ilog("Reverse...");
    iter = hset_rbegin(&hs1);
    while (iter) {
        ilog("key: %s  value:%s", to_cstr(iter->val));
        iter = hset_prev(&hs1, iter);
    }

    ilog("Buckets...");
    hset_debug_print(hs1.buckets);

    ilog("Inserting 5 more strange strings");
    hset_insert(&hs1, rid("another"));
    hset_insert(&hs1, rid("type-of"));
    hset_insert(&hs1, rid("thing-that"));
    hset_insert(&hs1, rid("wereallyshould"));
    hset_insert(&hs1, rid("beadding"));

    ilog("Forward...");
    iter = hset_begin(&hs1);
    while (iter) {
        ilog("key: %s  value:%s", to_cstr(iter->val));
        iter = hset_next(&hs1, iter);
    }

    ilog("Reverse...");
    iter = hset_rbegin(&hs1);
    while (iter) {
        ilog("key: %s  value:%s", to_cstr(iter->val));
        iter = hset_prev(&hs1, iter);
    }

    ilog("Buckets...");
    hset_debug_print(hs1.buckets);
    hset_terminate(&hs1);
}


int app_init(platform_ctxt *ctxt, void *)
{
    test_strings();
    test_arrays();
    test_hashmaps();
    test_hashmaps_string_keys();
    test_hashsets();
    test_hashset_string_keys();
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
