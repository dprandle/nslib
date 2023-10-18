#pragma once
#include <stdio.h>
#include "array.h"
#include "../basic_type_traits.h"

namespace nslib
{

#define makecstr(p) str_cstr(makestr(p))
struct string
{
    using iterator = char *;
    using const_iterator = const char *;

    static constexpr sizet SMALL_STR_SIZE = 19;
    char sos[SMALL_STR_SIZE]{};
    array<char> buf{};

    string();
    string(const string &copy);
    string(const char *copy, mem_arena *arena = nullptr);
    ~string();
    
    string &operator=(string rhs);
    string &operator+=(const string &rhs);
    const char &operator[](sizet ind) const;
    char &operator[](sizet ind);
};

string operator+(const string &lhs, const string &rhs);

bool operator==(const string &lhs, const string &rhs);

inline bool operator!=(const string &lhs, const string &rhs)
{
    return !(lhs == rhs);
}

inline const char *str_cstr(const string *str)
{
    return (str->buf.capacity > string::SMALL_STR_SIZE) ? str->buf.data : str->sos;
}

inline const char *str_cstr(const string &str)
{
    return (str.buf.capacity > string::SMALL_STR_SIZE) ? str.buf.data : str.sos;
}

inline char *str_data(string *str)
{
    return (str->buf.capacity > string::SMALL_STR_SIZE) ? str->buf.data : str->sos;
}

inline sizet str_len(const string *str)
{
    return str->buf.size;
}

inline sizet str_capacity(const string *str)
{
    return str->buf.capacity;
}

void swap(string *lhs, string *rhs);

void str_init(string *str, mem_arena *arena = nullptr);

void str_terminate(string *str);

void str_set_capacity(string *str, sizet new_cap);

string::iterator str_begin(string *str);

string::const_iterator str_begin(const string *str);

string::iterator str_end(string *str);

string::const_iterator str_end(const string *str);

bool str_empty(const string *str);

string *str_copy(string *dest, const string *src);

string *str_copy(string *dest, const char *src);

string *str_resize(string *str, sizet new_size, char c=char());

string *str_clear(string *str);

string *str_reserve(string *str, sizet new_cal);

string *str_shrink_to_fit(string *str);

string *str_push_back(string *str, char c);

string *str_append(string *str, const string *to_append);

string *str_append(string *str, const char *to_append);

template<class... Args>
string *str_args(string *dest, const char *format_txt, Args &&...args)
{
    sizet needed_space = snprintf(nullptr, 0, format_txt, std::forward<Args>(args)...);
    sizet sz = str_len(dest);
    str_resize(dest, sz + needed_space);
    snprintf(str_data(dest)+sz, needed_space+1, format_txt, args...);
    return dest;
}

u64 hash_type(const string &key, u64 seed0, u64 seed1);

template<signed_integral T>
string makestr(T n) {
    string ret;
    str_args(&ret, "%d", n);
    return ret;
}

template<unsigned_integral T>
string makestr(T n) {
    string ret;
    str_args(&ret, "%u", n);
    return ret;
}

template<floating_pt T>
string makestr(T n) {
    string ret;
    str_args(&ret, "%f", n);
    return ret;
}

inline const string& makestr(const string &str) {return str;}


string makestr(void* i);
string makestr(i64 i);
string makestr(u64 i);
string makestr(char c);



} // namespace nslib
