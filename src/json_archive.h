#pragma once

#include "archive_common.h"
#include <type_traits>
#include "cJSON.h"
#include "containers/string.h"

namespace nslib
{

struct jsa_stack_frame
{
    json_obj *current{nullptr};
    sizet cur_arr_ind{0};
};

struct json_archive
{
    archive_opmode opmode{};
    array<jsa_stack_frame> stack;
};

// If json_str is null then we will be set to output mode, otherwise input mode
inline void jsa_init(json_archive *jsa, const char *json_str)
{
    if (json_str) {
        jsa->opmode = archive_opmode::UNPACK;
        json_obj *parsed = json_parse(json_str);
        if (!parsed) {
            wlog("Could not parse json_str!");
        }
        else {
            arr_emplace_back(&jsa->stack, parsed);
        }
    }
    else {
        jsa->opmode = archive_opmode::PACK;
        arr_emplace_back(&jsa->stack, json_create_object());
    }
}

inline string jsa_to_json_string(json_archive *jsa)
{
    string ret{};
    if (jsa->stack.size > 0) {
        // Get the json string - this allocates
        char *src = json_print(arr_front(&jsa->stack)->current);

        // Copy the string in to the ret string
        ret = src;

        // Free the allocation
        json_free(src);
    }
    return ret;
}

inline void jsa_terminate(json_archive *jsa)
{
    while (arr_begin(&jsa->stack) != arr_end(&jsa->stack)) {
        json_delete(arr_back(&jsa->stack)->current);
        arr_pop_back(&jsa->stack);
    }
}

template<class T>
void pack_unpack_begin(json_archive *ar, T &, const pack_var_info &vinfo)
{
    jsa_stack_frame *cur_frame = arr_back(&ar->stack);
    assert(cur_frame);
    bool is_array = json_is_array(cur_frame->current);
    bool is_obj = json_is_object(cur_frame->current);
    assert(is_array || is_obj);

    if (ar->opmode == archive_opmode::UNPACK) {
        json_obj *item{};
        if (is_array) {
            item = json_get_object_item(cur_frame->current, vinfo.name);
        }
        else if (is_obj) {
            item = json_get_array_item(cur_frame->current, cur_frame->cur_arr_ind);
        }

        if (item && json_is_object(item)) {
            arr_emplace_back(&ar->stack, item);
        }
        else if (item) {
            wlog("Found %s in %s but it wasn't correct type (was %d)", vinfo.name, cur_frame->current->string, item->type);
        }
        else {
            wlog("Unable to find %s in %s", vinfo.name, cur_frame->current->string);
        }
    }
    else {
        auto new_item = json_create_object();
        if (is_array) {
            assert(json_add_item_to_array(cur_frame->current, new_item));
        }
        else if (is_obj) {
            assert(json_add_item_to_object(cur_frame->current, vinfo.name, new_item));
        }
        arr_emplace_back(&ar->stack, new_item);
    }
}

template<class T>
void pack_unpack_end(json_archive *ar, T &, const pack_var_info &vinfo)
{
    arr_pop_back(&ar->stack);
}

template<arithmetic_type T>
void pack_unpack_begin(json_archive *ar, T &, const pack_var_info &vinfo)
{
    // Do nothing
}

template<arithmetic_type T>
void pack_unpack_end(json_archive *ar, T &, const pack_var_info &vinfo)
{
    // Do nothing
}

template<class T, class CheckTAssignFunc, class CreateItemFunc>
void pack_unpack_helper(json_archive *ar, T &val, const pack_var_info &vinfo, CheckTAssignFunc check_func, CreateItemFunc create_func)
{
    jsa_stack_frame *cur_frame = arr_back(&ar->stack);
    assert(cur_frame);
    bool is_array = json_is_array(cur_frame->current);
    bool is_obj = json_is_object(cur_frame->current);
    assert(is_array || is_obj);

    if (ar->opmode == archive_opmode::UNPACK) {
        if (is_array) {
            json_obj *item = json_get_array_item(cur_frame->current, cur_frame->cur_arr_ind);
            if (!check_func(&val, item) && item) {
                wlog("Expected item at ind %d to be a string but it is %d instead", cur_frame->cur_arr_ind, item->type);
            }
            else {
                wlog("Array ind %d null in parent json item %s", cur_frame->cur_arr_ind, cur_frame->current->string);
            }
        }
        else if (is_obj) {
            json_obj *item = json_get_object_item(cur_frame->current, vinfo.name);
            if (!check_func(&val, item) && item) {
                wlog("Expected item %s to be a string but it is %d instead", item->string, item->type);
            }
            else {
                wlog("Could not find %s in parent json item %s", vinfo.name, cur_frame->current->string);
            }
        }
    }
    else {
        json_obj *item = create_func(&val);
        if (is_array) {
            assert(json_add_item_to_array(cur_frame->current, item));
        }
        else if (is_obj) {
            assert(json_add_item_to_object(cur_frame->current, vinfo.name, item));
        }
    }
}

template<arithmetic_type T>
void pack_unpack(json_archive *ar, T &val, const pack_var_info &vinfo)
{
    auto create_func = [](const T *val) -> json_obj * { return json_create_number((double)*val); };

    auto check_func = [](T *val, json_obj *item) -> bool {
        if (item && json_is_number(item)) {
            *val = (T)item->valuedouble;
            return true;
        }
        return false;
    };
    pack_unpack_helper(ar, val, vinfo, check_func, create_func);
}

inline void pack_unpack(json_archive *ar, bool &val, const pack_var_info &vinfo)
{
    auto create_func = [](const bool *val) -> json_obj * { return json_create_bool(*val); };

    auto check_func = [](bool *val, json_obj *item) -> bool {
        if (item && json_is_bool(item)) {
            *val = json_is_true(item);
            return true;
        }
        return false;
    };
    pack_unpack_helper(ar, val, vinfo, check_func, create_func);
}

inline void pack_unpack_begin(json_archive *ar, string &, const pack_var_info &vinfo)
{
    // Do nothing
}

inline void pack_unpack_end(json_archive *ar, string &, const pack_var_info &vinfo)
{
    // Do nothing
}

inline void pack_unpack(json_archive *ar, string &val, const pack_var_info &vinfo)
{
    auto create_func = [](const string *val) -> json_obj * { return json_create_string(str_cstr(val)); };

    auto check_func = [](string *val, json_obj *item) -> bool {
        if (item && json_is_string(item)) {
            *val = item->valuestring;
            return true;
        }
        return false;
    };
    pack_unpack_helper(ar, val, vinfo, check_func, create_func);
}

template<class T, sizet N>
void pack_unpack_begin(json_archive *ar, T (&val)[N], const pack_var_info &vinfo)
{
    jsa_stack_frame *cur_frame = arr_back(&ar->stack);
    assert(cur_frame);
    bool is_array = json_is_array(cur_frame->current);
    bool is_obj = json_is_object(cur_frame->current);
    assert(is_array || is_obj);

    if (ar->opmode == archive_opmode::UNPACK) {
        json_obj *item{};
        if (is_obj) {
            item = json_get_object_item(cur_frame->current, vinfo.name);
        }
        else if (is_array) {
            item = json_get_array_item(cur_frame->current, cur_frame->cur_arr_ind);
        }

        if (item && json_is_array(item)) {
            arr_emplace_back(&ar->stack, item);
        }
        else if (item) {
            wlog("Found %s in object %s but it is not an array (it is %d)", vinfo.name, cur_frame->current->string, item->type);
        }
        else {
            wlog("Unable to find %s in object %s", vinfo.name, cur_frame->current->string);
        }
    }
    else {
        auto new_item = json_create_array();
        if (is_array) {
            assert(json_add_item_to_array(cur_frame->current, new_item));
        }
        else if (is_obj) {
            assert(json_add_item_to_object(cur_frame->current, vinfo.name, new_item));
        }
        arr_emplace_back(&ar->stack, new_item);
    }
}

template<class T, sizet N>
void pack_unpack_end(json_archive *ar, T (&val)[N], const pack_var_info &vinfo)
{
    arr_pop_back(&ar->stack);
}

template<class T, sizet N>
void pack_unpack(json_archive *ar, T (&val)[N], const pack_var_info &vinfo)
{
    jsa_stack_frame *cur_frame = arr_back(&ar->stack);
    assert(cur_frame);
    for (sizet i = 0; i < N; ++i) {
        pup_var(ar, val[i], {});
        ++cur_frame->cur_arr_ind;
    }
}

} // namespace nslib
