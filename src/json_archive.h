#pragma once

#include "archive_common.h"
#include <type_traits>
#include "cJSON.h"
#include "containers/string.h"

namespace nslib
{

struct json_archive
{
    archive_opmode opmode{};
    array<cJSON *> parent_stack{};
    cJSON *current{};
};

// If json_str is null then we will be set to output mode, otherwise input mode
inline void jsa_init(json_archive *jsa, const char *json_str)
{
    if (json_str) {
        jsa->opmode = archive_opmode::UNPACK;
        jsa->current = cJSON_Parse(json_str);
    }
    else {
        jsa->opmode = archive_opmode::PACK;
        jsa->current = cJSON_CreateObject();
    }
}

inline void jsa_terminate(json_archive *jsa)
{
    cJSON_Delete(jsa->current);
    while (arr_begin(&jsa->parent_stack) != arr_end(&jsa->parent_stack)) {
        cJSON_Delete(*arr_back(&jsa->parent_stack));
        arr_pop_back(&jsa->parent_stack);
    }
    jsa->current = nullptr;
}

template<class T>
void pack_unpack_begin(json_archive *ar, T &, const pack_var_info &vinfo)
{
    if (ar->opmode == archive_opmode::UNPACK) {
        auto item = cJSON_GetObjectItem(ar->current, vinfo.name);
        if (item) {
            arr_push_back(&ar->parent_stack, ar->current);
            ar->current = item;
        }
        else {
            wlog("Unable to find %s in object %s", vinfo.name, ar->current->string);
        }
    }
    else {
        auto new_item = cJSON_CreateObject();
        if (cJSON_AddItemToObject(ar->current, vinfo.name, new_item)) {
            arr_push_back(&ar->parent_stack, ar->current);
            ar->current = new_item;
        }
        else {
            wlog("Unable to add object with name %s to %s - deleting", vinfo.name, ar->current->string);
            cJSON_Delete(new_item);
        }
    }
}

template<class T>
void pack_unpack_end(json_archive *ar, T &, const pack_var_info &vinfo)
{
    ar->current = *arr_back(&ar->parent_stack);
    arr_pop_back(&ar->parent_stack);
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

template<arithmetic_type T>
void pack_unpack(json_archive *ar, T &val, const pack_var_info &vinfo)
{
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
    if (ar->opmode == archive_opmode::PACK) {
        
    }
}

template<class T, sizet N>
void pack_unpack_begin(json_archive *ar, T (&val)[N], const pack_var_info &vinfo)
{
    if (ar->opmode == archive_opmode::UNPACK) {
        auto item = cJSON_GetObjectItem(ar->current, vinfo.name);
        if (item) {
            if (cJSON_IsArray(item)) {
                arr_push_back(&ar->parent_stack, ar->current);
                ar->current = item;
            }
            else {
                wlog("Found %s in object %s but it is not an array as it should be", vinfo.name, ar->current->string);
            }
        }
        else {
            wlog("Unable to find %s in object %s", vinfo.name, ar->current->string);
        }
    }
    else {
        auto new_item = cJSON_CreateArray();
        if (cJSON_AddItemToObject(ar->current, vinfo.name, new_item)) {
            arr_push_back(&ar->parent_stack, ar->current);
            ar->current = new_item;
        }
        else {
            wlog("Unable to add array with name %s to %s - deleting", vinfo.name, ar->current->string);
            cJSON_Delete(new_item);
        }
    }
    
}

template<class T, sizet N>
void pack_unpack_end(json_archive *ar, T (&val)[N], const pack_var_info &vinfo)
{}

template<class T, sizet N>
void pack_unpack(json_archive *ar, T (&val)[N], const pack_var_info &vinfo)
{}

} // namespace nslib
