#include <cstring>

#include "string.h"
#include "../logging.h"
#include "../hashfuncs.h"

namespace nslib
{
void str_set_capacity(string *str, sizet new_cap)
{
    sizet dyn_cap = 0;
    sizet prev_sz = str->buf.size;
    sizet prev_cap = str->buf.capacity;
    
    if (new_cap > string::SMALL_STR_SIZE) {
        dyn_cap = new_cap;
    }
    else if (str->buf.data) {
        // If the new capacity is within our small string range and the dynamic buffer is non null, copy the dynamic
        // buffer data to our small string
        memcpy(str->sos, str->buf.data, (new_cap < prev_sz)?new_cap:prev_sz);
    }

    // This will set allocate our dynamic buffer if new cap is over small string amount, otherwise it will free the
    // dynamic buffer mem
    arr_set_capacity(&str->buf, dyn_cap);
    str->buf.capacity = new_cap;
    str->buf.size = (new_cap < prev_sz)?new_cap:prev_sz;

    // If we are moving from the small string buffer to a dynamic buffer, copy the small string
    // to the dynamic string buffer
    if (prev_cap <= string::SMALL_STR_SIZE && str->buf.data) {
        memcpy(str->buf.data, str->sos, prev_sz);
    }
}

string::string()
{
    str_init(this);
}

string::string(const string &copy)
{
    str_init(this, copy.buf.arena);
    str_copy(this, copy);
}

string::string(const char *copy, mem_arena *arena)
{
    if (!arena) {
        arena = mem_global_arena();
    }
    str_init(this, arena);
    str_copy(this, copy);
}

string::~string()
{
    str_terminate(this);
}

string &string::operator=(string rhs)
{
    swap(this, &rhs);
    return *this;
}

string &string::operator+=(const string &rhs)
{
    return *str_append(this, rhs);
}

const char &string::operator[](sizet ind) const
{
    return str_cstr(this)[ind];
}

inline char &string::operator[](sizet ind)
{
    return str_data(this)[ind];
}

string operator+(const string &lhs, const string &rhs)
{
    string ret(lhs);
    ret += rhs;
    return ret;
}

bool operator==(const string &lhs, const string &rhs) {
    if (str_len(lhs) != str_len(rhs)) {
        return false;
    }
    else if (str_len(lhs) == 0) {
        return true;
    }
    sizet i{0};
    while (i < str_len(lhs) && lhs[i] == rhs[i])
        ++i;
    return i == lhs.buf.size;
}

string::iterator str_begin(string *str)
{
    return str_data(str);
}

string::const_iterator str_begin(const string &str)
{
    return str_cstr(str);
}

string::iterator str_end(string *str)
{
    return str_data(str) + str_len(*str);
}

string::const_iterator str_end(const string &str)
{
    return str_cstr(str) + str_len(str);
}

bool str_empty(const string &str)
{
    return (str_len(str) == 0);
}

void swap(string *lhs, string *rhs)
{
    for (sizet i = 0; i < string::SMALL_STR_SIZE; ++i) {
        std::swap(lhs->sos[i], rhs->sos[i]);
    }
    swap(&lhs->buf, &rhs->buf);
}

void str_init(string *str, mem_arena *arena)
{
    arr_init(&str->buf, arena);
    str->buf.capacity = string::SMALL_STR_SIZE;
}

void str_terminate(string *str)
{
    arr_terminate(&str->buf);
}

string *str_copy(string *dest, const string &src)
{
    str_resize(dest, src.buf.size);
    memcpy(str_data(dest), str_cstr(src), dest->buf.size);
    return dest;
}

string *str_copy(string *dest, const char *src)
{
    str_resize(dest, strlen(src));
    memcpy(str_data(dest), src, dest->buf.size);
    return dest;
}

string *str_resize(string *str, sizet new_size, char c)
{
    if (str_len(*str) == new_size)
        return str;

    // Make sure our current size doesn't exceed the capacity - it shouldnt that would definitely be a bug if it did.
    assert(str_len(*str) < str_capacity(*str));

    sizet cap = str_capacity(*str);
    if ((new_size + 1) > cap) {
        if (cap < 1) {
            cap = 1;
        }
        while (cap < (new_size + 1))
            cap *= 2;
        str_set_capacity(str, cap);
    }
    
    for (int i = str_len(*str); i < new_size; ++i) {
        (*str)[i] = c;
    }
    (*str)[new_size] = 0; // null terminator
    str->buf.size = new_size;
    return str;
}

string *str_clear(string *str)
{
    str_resize(str, 0);
    return str;
}

string *str_reserve(string *str, sizet new_cap)
{
    if (new_cap > str_capacity(*str)) {
        str_set_capacity(str, new_cap);
    }
    return str;
}

string *str_shrink_to_fit(string *str)
{
    assert(str_len(*str) <= str_capacity(*str));
    if (str_len(*str)+1 < str_capacity(*str)) {
        str_set_capacity(str, str_len(*str)+1);
    }
    return str;
}

string *str_push_back(string *str, char c)
{
    sizet sz = str_len(*str);
    str_resize(str, sz + 1);
    (*str)[sz] = c;
    return str;
}

string *str_append(string *str, const string &to_append)
{
    sizet sz = str_len(*str);
    sizet append_len = str_len(to_append);
    str_resize(str, sz + append_len);
    memcpy(str_data(str) + sz, str_cstr(to_append), append_len);
    return str;
}

string *str_append(string *str, const char *to_append)
{
    sizet sz = str_len(*str);
    sizet append_len = strlen(to_append);
    str_resize(str, sz + append_len);
    memcpy(str_data(str) + sz, to_append, append_len);
    return str;
}

u64 hash_type(const string &key, u64 seed0, u64 seed1)
{
    return hash_type(str_cstr(&key), seed0, seed1);
}

string makestr(char c)
{
    string ret;
    str_args(&ret, "%c", c);
    return ret;
}

string makestr(u64 i)
{
    string ret;
    str_args(&ret, "%lu", i);
    return ret;
}

string makestr(i64 i)
{
    string ret;
    str_args(&ret, "%ld", i);
    return ret;
}

string makestr(void* i)
{
    string ret;
    str_args(&ret, "%p", i);
    return ret;
}



} // namespace nslib
