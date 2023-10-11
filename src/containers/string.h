#pragma once

#include "array.h"
namespace nslib
{


struct string
{
    static constexpr sizet SMALL_STR_SIZE = 19;
    
    string();
    string(const string &copy);
    ~string();
    string &operator=(string rhs);
    
    char sos[SMALL_STR_SIZE]{};
    array<char> buf{};

    inline const char &operator[](sizet ind) const
    {
        return buf[ind];
    }
    inline char &operator[](sizet ind)
    {
        return buf[ind];
    }
    
};

void swap(string *lhs, string *rhs);

void str_init(string *str, mem_arena *arena=nullptr);

void str_terminate(string *str);

void str_copy(string *dest, const string *src);

void str_resize(string *str, sizet new_size);

void str_clear(string *str);

void str_reserve(string *str, sizet new_cal);

void str_shrink_to_fit(string *str);

void str_append(string *str, char c);

void str_append(string *str, string *to_append);

void str_append(string *str, const char *to_append);


} // namespace nslib
