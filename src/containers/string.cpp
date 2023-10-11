#include "string.h"
#include <cstring>
namespace nslib
{
intern void str_set_capacity(string *str, sizet new_cap)
{
    char *src = str->buf.data;
    if (new_cap > string::SMALL_STR_SIZE) {
        while (new_cap < sizeof(mem_node)) {
            ++new_cap;
        }
        str->buf.data = (char *)mem_alloc(new_cap, str->buf.arena);
    }
    else {
        str->buf.data = str->sos;
    }
    sizet copy_cap = new_cap;
    if (str->buf.capacity < copy_cap) {
        copy_cap = str->buf.capacity;
    }
    str->buf.capacity = new_cap;

    // Copy data from prev buffer to current buffer - using the
    if (str->buf.data != src) {
        memcpy(str->buf.data, src, copy_cap);
        if (src != str->sos) {
            mem_free(str->buf.data, str->buf.arena);
        }
    }

    // Shrink the old size if its greater than the new capacity (so we only copy those items)
    if (str->buf.size > str->buf.capacity)
        str->buf.size = str->buf.capacity;

#if !defined(NDEBUG)
    for (sizet i = str->buf.size; i < str->buf.capacity; ++i)
        str->buf.data[i] = {};
#endif
}

string::string()
{    
}

string::string(const string &copy)
{}

string::~string()
{}

string &string::operator=(string rhs)
{
    swap(this, &rhs);
    return *this;
}

void swap(string *lhs, string *rhs)
{
    for (sizet i = 0; i < string::SMALL_STR_SIZE; ++i) {
        std::swap(lhs->sos[i], rhs->sos[i]);
    }
    swap(&lhs->dyn, &rhs->dyn);
}

void str_init(string *str, mem_arena *arena)
{
    // Will initialize to global arena within arr_init if arena is nullptr
    arr_init(&str->buf, arena);
    str->buf.data = str->sos;
    str->buf.capacity = string::SMALL_STR_SIZE;
    str->buf.size = 0;
}

void str_terminate(string *str)
{
    // Only delete memory if we have used more than the small string amount
    if (str->buf.capacity > string::SMALL_STR_SIZE) {
        arr_terminate(&str->buf);
    }
}

void str_copy(string *dest, const string *src)
{
    str_resize(dest, src->buf.size);
    for (sizet i = 0; i < src->buf.size; ++i) {
        (*dest)[i] = (*src)[i];
    }
}

void str_resize(string *str, sizet new_size)
{
    if (str->buf.size == new_size)
        return;

    // Make sure our current size doesn't exceed the capacity - it shouldnt that would definitely be a bug if it did.
    assert(str->buf.size <= str->buf.capacity);

    sizet cap = str->buf.capacity;
    if (new_size > cap) {
        if (cap < 1) {
            cap = 1;
        }
        while (cap < new_size)
            cap *= 2;
        str_set_capacity(str, cap);
    }

// If in debug - clear the removed data (if any) to 0
#if !defined(NDEBUG)
    for (sizet i = new_size; i < str->buf.size; ++i)
        str[i] = {};
#endif
    str->buf.size = new_size;
    
}

void str_clear(string *str){
    arr_clear(&str->buf);
}

void str_reserve(string *str, sizet new_cap)
{
    
}

void str_shrink_to_fit(string *str);

void str_append(string *str, char c);

void str_append(string *str, string *to_append);

void str_append(string *str, const char *to_append);



} // namespace nslib
