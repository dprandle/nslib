/*
  Copyright (c) 2009-2017 Dave Gamble and json contributors

  Permission is hereby granted, free of charge, to any person obtaining a copy
  of this software and associated documentation files (the "Software"), to deal
  in the Software without restriction, including without limitation the rights
  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
  copies of the Software, and to permit persons to whom the Software is
  furnished to do so, subject to the following conditions:

  The above copyright notice and this permission notice shall be included in
  all copies or substantial portions of the Software.

  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
  THE SOFTWARE.
*/

#ifndef json__h
#define json__h

#ifdef __cplusplus
extern "C"
{
#endif

#if !defined(__WINDOWS__) && (defined(WIN32) || defined(WIN64) || defined(_MSC_VER) || defined(_WIN32))
#define __WINDOWS__
#endif

#ifdef __WINDOWS__

/* When compiling for windows, we specify a specific calling convention to avoid issues where we are being called from a project with a different default calling convention.  For windows you have 3 define options:

CJSON_HIDE_SYMBOLS - Define this in the case where you don't want to ever dllexport symbols
CJSON_EXPORT_SYMBOLS - Define this on library build when you want to dllexport symbols (default)
CJSON_IMPORT_SYMBOLS - Define this if you want to dllimport symbol

For *nix builds that support visibility attribute, you can define similar behavior by

setting default visibility to hidden by adding
-fvisibility=hidden (for gcc)
or
-xldscope=hidden (for sun cc)
to CFLAGS

then using the CJSON_API_VISIBILITY flag to "export" the same symbols the way CJSON_EXPORT_SYMBOLS does

*/

#define CJSON_CDECL __cdecl
#define CJSON_STDCALL __stdcall

/* export symbols by default, this is necessary for copy pasting the C and header file */
#if !defined(CJSON_HIDE_SYMBOLS) && !defined(CJSON_IMPORT_SYMBOLS) && !defined(CJSON_EXPORT_SYMBOLS)
#define CJSON_EXPORT_SYMBOLS
#endif

#if defined(CJSON_HIDE_SYMBOLS)
#define CJSON_PUBLIC(type)   type CJSON_STDCALL
#elif defined(CJSON_EXPORT_SYMBOLS)
#define CJSON_PUBLIC(type)   __declspec(dllexport) type CJSON_STDCALL
#elif defined(CJSON_IMPORT_SYMBOLS)
#define CJSON_PUBLIC(type)   __declspec(dllimport) type CJSON_STDCALL
#endif
#else /* !__WINDOWS__ */
#define CJSON_CDECL
#define CJSON_STDCALL

#if (defined(__GNUC__) || defined(__SUNPRO_CC) || defined (__SUNPRO_C)) && defined(CJSON_API_VISIBILITY)
#define CJSON_PUBLIC(type)   __attribute__((visibility("default"))) type
#else
#define CJSON_PUBLIC(type) type
#endif
#endif

/* project version */
#define CJSON_VERSION_MAJOR 1
#define CJSON_VERSION_MINOR 7
#define CJSON_VERSION_PATCH 16

#include <stddef.h>

/* json Types: */
#define JSON_INVALID (0)
#define JSON_FALSE  (1 << 0)
#define JSON_TRUE   (1 << 1)
#define JSON_NULL   (1 << 2)
#define JSON_NUMBER (1 << 3)
#define JSON_STRING (1 << 4)
#define JSON_ARRAY  (1 << 5)
#define JSON_OBJECT (1 << 6)
#define JSON_RAW    (1 << 7) /* raw json */

#define JSON_IS_REFERENCE 256
#define JSON_STRING_IS_CONST 512

/* The json structure: */
typedef struct json
{
    /* next/prev allow you to walk array/object chains. Alternatively, use GetArraySize/GetArrayItem/GetObjectItem */
    struct json *next;
    struct json *prev;
    /* An array or object item will have a child pointer pointing to a chain of the items in the array/object. */
    struct json *child;

    /* The type of the item, as above. */
    int type;

    /* The item's string, if type==json_String  and type == json_Raw */
    char *valuestring;
    /* writing to valueint is DEPRECATED, use json_set_number_value instead */
    int valueint;
    /* The item's number, if type==json_Number */
    double valuedouble;

    /* The item's name string, if this item is the child of, or is in the list of subitems of an object. */
    char *string;
} json_obj;

typedef struct json_Hooks
{
      /* malloc/free are CDECL on Windows regardless of the default calling convention of the compiler, so ensure the hooks allow passing those functions directly. */
      void *(CJSON_CDECL *malloc_fn)(size_t sz);
      void (CJSON_CDECL *free_fn)(void *ptr);
} json_hooks;

typedef int json_bool;

/* Limits how deeply nested arrays/objects can be before json rejects to parse them.
 * This is to prevent stack overflows. */
#ifndef CJSON_NESTING_LIMIT
#define CJSON_NESTING_LIMIT 1000
#endif

/* returns the version of json as a string */
CJSON_PUBLIC(const char*) json_version(void);

/* Supply malloc, realloc and free functions to json */
CJSON_PUBLIC(void) json_init_hooks(json_hooks* hooks);

/* Memory Management: the caller is always responsible to free the results from all variants of json_Parse (with json_Delete) and json_Print (with stdlib free, json_Hooks.free_fn, or json_free as appropriate). The exception is json_PrintPreallocated, where the caller has full responsibility of the buffer. */
/* Supply a block of JSON, and this returns a json object you can interrogate. */
CJSON_PUBLIC(json_obj *) json_parse(const char *value);
CJSON_PUBLIC(json_obj *) json_parse_with_length(const char *value, size_t buffer_length);
/* ParseWithOpts allows you to require (and check) that the JSON is null terminated, and to retrieve the pointer to the final byte parsed. */
/* If you supply a ptr in return_parse_end and parsing fails, then return_parse_end will contain a pointer to the error so will match json_GetErrorPtr(). */
CJSON_PUBLIC(json_obj *) json_parse_with_opts(const char *value, const char **return_parse_end, json_bool require_null_terminated);
CJSON_PUBLIC(json_obj *) json_parse_with_length_opts(const char *value, size_t buffer_length, const char **return_parse_end, json_bool require_null_terminated);

/* Render a json entity to text for transfer/storage. */
CJSON_PUBLIC(char *) json_print(const json_obj *item);
/* Render a json entity to text for transfer/storage without any formatting. */
CJSON_PUBLIC(char *) json_print_unformatted(const json_obj *item);
/* Render a json entity to text using a buffered strategy. prebuffer is a guess at the final size. guessing well reduces reallocation. fmt=0 gives unformatted, =1 gives formatted */
CJSON_PUBLIC(char *) json_print_buffered(const json_obj *item, int prebuffer, json_bool fmt);
/* Render a json entity to text using a buffer already allocated in memory with given length. Returns 1 on success and 0 on failure. */
/* NOTE: json is not always 100% accurate in estimating how much memory it will use, so to be safe allocate 5 bytes more than you actually need */
CJSON_PUBLIC(json_bool) json_print_preallocated(json_obj *item, char *buffer, const int length, const json_bool format);
/* Delete a json entity and all subentities. */
CJSON_PUBLIC(void) json_delete(json_obj *item);

/* Returns the number of items in an array (or object). */
CJSON_PUBLIC(int) json_get_array_size(const json_obj *array);
/* Retrieve item number "index" from array "array". Returns NULL if unsuccessful. */
CJSON_PUBLIC(json_obj *) json_get_array_item(const json_obj *array, int index);
/* Get item "string" from object. Case insensitive. */
CJSON_PUBLIC(json_obj *) json_get_object_item(const json_obj * const object, const char * const string);
CJSON_PUBLIC(json_obj *) json_get_object_item_case_sensitive(const json_obj * const object, const char * const string);
CJSON_PUBLIC(json_bool) json_has_object_item(const json_obj *object, const char *string);
/* For analysing failed parses. This returns a pointer to the parse error. You'll probably need to look a few chars back to make sense of it. Defined when json_Parse() returns 0. 0 when json_Parse() succeeds. */
CJSON_PUBLIC(const char *) json_get_error_ptr(void);

/* Check item type and return its value */
CJSON_PUBLIC(char *) json_get_string_value(const json_obj * const item);
CJSON_PUBLIC(double) json_get_number_value(const json_obj * const item);

/* These functions check the type of an item */
CJSON_PUBLIC(json_bool) json_is_invalid(const json_obj * const item);
CJSON_PUBLIC(json_bool) json_is_false(const json_obj * const item);
CJSON_PUBLIC(json_bool) json_is_true(const json_obj * const item);
CJSON_PUBLIC(json_bool) json_is_bool(const json_obj * const item);
CJSON_PUBLIC(json_bool) json_is_null(const json_obj * const item);
CJSON_PUBLIC(json_bool) json_is_number(const json_obj * const item);
CJSON_PUBLIC(json_bool) json_is_string(const json_obj * const item);
CJSON_PUBLIC(json_bool) json_is_array(const json_obj * const item);
CJSON_PUBLIC(json_bool) json_is_object(const json_obj * const item);
CJSON_PUBLIC(json_bool) json_is_raw(const json_obj * const item);

/* These calls create a json item of the appropriate type. */
CJSON_PUBLIC(json_obj *) json_create_null(void);
CJSON_PUBLIC(json_obj *) json_create_true(void);
CJSON_PUBLIC(json_obj *) json_create_false(void);
CJSON_PUBLIC(json_obj *) json_create_bool(json_bool boolean);
CJSON_PUBLIC(json_obj *) json_create_number(double num);
CJSON_PUBLIC(json_obj *) json_create_string(const char *string);
/* raw json */
CJSON_PUBLIC(json_obj *) json_create_raw(const char *raw);
CJSON_PUBLIC(json_obj *) json_create_array(void);
CJSON_PUBLIC(json_obj *) json_create_object(void);

/* Create a string where valuestring references a string so
 * it will not be freed by json_Delete */
CJSON_PUBLIC(json_obj *) json_create_string_reference(const char *string);
/* Create an object/array that only references it's elements so
 * they will not be freed by json_Delete */
CJSON_PUBLIC(json_obj *) json_create_object_reference(const json_obj *child);
CJSON_PUBLIC(json_obj *) json_create_array_reference(const json_obj *child);

/* These utilities create an Array of count items.
 * The parameter count cannot be greater than the number of elements in the number array, otherwise array access will be out of bounds.*/
CJSON_PUBLIC(json_obj *) json_create_int_array(const int *numbers, int count);
CJSON_PUBLIC(json_obj *) json_create_float_array(const float *numbers, int count);
CJSON_PUBLIC(json_obj *) json_create_double_array(const double *numbers, int count);
CJSON_PUBLIC(json_obj *) json_create_string_array(const char *const *strings, int count);

/* Append item to the specified array/object. */
CJSON_PUBLIC(json_bool) json_add_item_to_array(json_obj *array, json_obj *item);
CJSON_PUBLIC(json_bool) json_add_item_to_object(json_obj *object, const char *string, json_obj *item);
/* Use this when string is definitely const (i.e. a literal, or as good as), and will definitely survive the json object.
 * WARNING: When this function was used, make sure to always check that (item->type & json_StringIsConst) is zero before
 * writing to `item->string` */
CJSON_PUBLIC(json_bool) json_add_item_to_object_cs(json_obj *object, const char *string, json_obj *item);
/* Append reference to item to the specified array/object. Use this when you want to add an existing json to a new json, but don't want to corrupt your existing json. */
CJSON_PUBLIC(json_bool) json_add_item_reference_to_array(json_obj *array, json_obj *item);
CJSON_PUBLIC(json_bool) json_add_item_reference_to_object(json_obj *object, const char *string, json_obj *item);

/* Remove/Detach items from Arrays/Objects. */
CJSON_PUBLIC(json_obj *) json_detach_item_via_pointer(json_obj *parent, json_obj * const item);
CJSON_PUBLIC(json_obj *) json_detach_item_from_array(json_obj *array, int which);
CJSON_PUBLIC(void) json_delete_item_from_array(json_obj *array, int which);
CJSON_PUBLIC(json_obj *) json_detach_item_from_object(json_obj *object, const char *string);
CJSON_PUBLIC(json_obj *) json_detach_item_from_object_case_sensitive(json_obj *object, const char *string);
CJSON_PUBLIC(void) json_delete_item_from_object(json_obj *object, const char *string);
CJSON_PUBLIC(void) json_delete_item_from_object_case_sensitive(json_obj *object, const char *string);

/* Update array items. */
CJSON_PUBLIC(json_bool) json_insert_item_in_array(json_obj *array, int which, json_obj *newitem); /* Shifts pre-existing items to the right. */
CJSON_PUBLIC(json_bool) json_replace_item_via_pointer(json_obj * const parent, json_obj * const item, json_obj * replacement);
CJSON_PUBLIC(json_bool) json_replace_item_in_array(json_obj *array, int which, json_obj *newitem);
CJSON_PUBLIC(json_bool) json_replace_item_in_object(json_obj *object,const char *string,json_obj *newitem);
CJSON_PUBLIC(json_bool) json_replace_item_in_object_case_sensitive(json_obj *object,const char *string,json_obj *newitem);

/* Duplicate a json item */
CJSON_PUBLIC(json_obj *) json_duplicate(const json_obj *item, json_bool recurse);
/* Duplicate will create a new, identical json item to the one you pass, in new memory that will
 * need to be released. With recurse!=0, it will duplicate any children connected to the item.
 * The item->next and ->prev pointers are always zero on return from Duplicate. */
/* Recursively compare two json items for equality. If either a or b is NULL or invalid, they will be considered unequal.
 * case_sensitive determines if object keys are treated case sensitive (1) or case insensitive (0) */
CJSON_PUBLIC(json_bool) json_compare(const json_obj * const a, const json_obj * const b, const json_bool case_sensitive);

/* Minify a strings, remove blank characters(such as ' ', '\t', '\r', '\n') from strings.
 * The input pointer json cannot point to a read-only address area, such as a string constant, 
 * but should point to a readable and writable address area. */
CJSON_PUBLIC(void) json_minify(char *json);

/* Helper functions for creating and adding items to an object at the same time.
 * They return the added item or NULL on failure. */
CJSON_PUBLIC(json_obj*) json_add_null_to_object(json_obj * const object, const char * const name);
CJSON_PUBLIC(json_obj*) json_add_true_to_object(json_obj * const object, const char * const name);
CJSON_PUBLIC(json_obj*) json_add_false_to_object(json_obj * const object, const char * const name);
CJSON_PUBLIC(json_obj*) json_add_bool_to_object(json_obj * const object, const char * const name, const json_bool boolean);
CJSON_PUBLIC(json_obj*) json_add_number_to_object(json_obj * const object, const char * const name, const double number);
CJSON_PUBLIC(json_obj*) json_add_string_to_object(json_obj * const object, const char * const name, const char * const string);
CJSON_PUBLIC(json_obj*) json_add_raw_to_object(json_obj * const object, const char * const name, const char * const raw);
CJSON_PUBLIC(json_obj*) json_add_object_to_object(json_obj * const object, const char * const name);
CJSON_PUBLIC(json_obj*) json_add_array_to_object(json_obj * const object, const char * const name);

/* When assigning an integer value, it needs to be propagated to valuedouble too. */
#define json_set_int_value(object, number) ((object) ? (object)->valueint = (object)->valuedouble = (number) : (number))
/* helper for the json_set_number_value macro */
CJSON_PUBLIC(double) json_set_number_helper(json_obj *object, double number);
#define json_set_number_value(object, number) ((object != NULL) ? json_SetNumberHelper(object, (double)number) : (number))
/* Change the valuestring of a json_string object, only takes effect when type of object is json_String */
CJSON_PUBLIC(char*) json_set_valuestring(json_obj *object, const char *valuestring);

/* If the object is not a boolean type this does nothing and returns JSON_INVALID else it returns the new type*/
#define json_set_bool_value(object, bool_value) ( \
    (object != NULL && ((object)->type & (json_false|json_true))) ? \
    (object)->type=((object)->type &(~(json_false|json_true)))|((bool_value)?json_true:json_false) : \
    JSON_INVALID\
)

/* Macro for iterating over an array or object */
#define json_array_for_each(element, array) for(element = (array != NULL) ? (array)->child : NULL; element != NULL; element = element->next)

/* malloc/free objects using the malloc/free functions that have been set with json_InitHooks */
CJSON_PUBLIC(void *) json_malloc(size_t size);
CJSON_PUBLIC(void) json_free(void *object);

#ifdef __cplusplus
}
#endif

#endif
