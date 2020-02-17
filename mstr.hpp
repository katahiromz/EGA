// mstr.hpp --- string manipulation library
// Copyright (C) 2020 Katayama Hirofumi MZ <katayama.hirofumi.mz@gmail.com>
// This file is public domain software.

#pragma once

#include <vector>
#include <string>
#include <cstring>
#include <cassert>

inline void
mstr_trim(std::string& str, const char *spaces)
{
    size_t i = str.find_first_not_of(spaces);
    size_t j = str.find_last_not_of(spaces);
    if ((i == std::string::npos) || (j == std::string::npos))
    {
        str.clear();
    }
    else
    {
        str = str.substr(i, j - i + 1);
    }
}

template <size_t t_siz>
inline void
mstr_trim(char (&str)[t_siz], const char *spaces)
{
    std::string s = str;
    mstr_trim(s, spaces);
    std::strcpy(str, s.c_str());
}

template <typename T_STR_CONTAINER>
inline void
mstr_split(T_STR_CONTAINER& container,
           const typename T_STR_CONTAINER::value_type& str,
           const typename T_STR_CONTAINER::value_type& chars)
{
    typedef typename T_STR_CONTAINER::value_type string_type;

    container.clear();

    if (chars.empty())
    {
        for (size_t i = 0; i < str.size(); ++i)
        {
            string_type s;
            s += str[i];
            container.push_back(s);
        }
        return;
    }

    size_t i = 0, k = str.find_first_of(chars);
    while (k != T_STR_CONTAINER::value_type::npos)
    {
        container.push_back(str.substr(i, k - i));
        i = k + 1;
        k = str.find_first_of(chars, i);
    }
    container.push_back(str.substr(i));
}

template <typename T_STR_CONTAINER>
inline typename T_STR_CONTAINER::value_type
mstr_join(const T_STR_CONTAINER& container,
          const typename T_STR_CONTAINER::value_type& sep)
{
    typename T_STR_CONTAINER::value_type result;
    typename T_STR_CONTAINER::const_iterator it, end;
    it = container.begin();
    end = container.end();
    if (it != end)
    {
        result = *it;
        for (++it; it != end; ++it)
        {
            result += sep;
            result += *it;
        }
    }
    return result;
}

inline void
mstr_reverse(std::string& ret)
{
    if (ret.size() <= 1)
        return;

    for (size_t i = 0, k = ret.size() - 1; i < k; ++i, --k)
    {
        std::swap(ret[i], ret[k]);
    }
}

inline std::string
mstr_to_string(long value)
{
    if (value == 0)
        return "0";

    if (value < 0)
    {
        std::string ret = "-";
        ret += mstr_to_string(-value);
        return ret;
    }

    unsigned long uvalue = value;

    std::string ret;
    while (uvalue != 0)
    {
        ret += (char)('0' + (uvalue % 10));
        uvalue /= 10;
    }

    mstr_reverse(ret);
    return ret;
}

inline void
mstr_unittest(void)
{
    std::string str = " \tABC \t ";
    mstr_trim(str, " \t");
    assert(str == "ABC");

    char buf[] = " \tABC \t ";
    mstr_trim(buf, " \t");
    str = buf;
    assert(str == "ABC");

    std::vector<std::string> list;
    mstr_split(list, "TEST1|test2|TEST3|", "|");
    assert(list[0] == "TEST1");
    assert(list[1] == "test2");
    assert(list[2] == "TEST3");
    assert(list[3] == "");

    mstr_split(list, "ABC", "");
    assert(list.size() == 3);
    assert(list[0] == "A");
    assert(list[1] == "B");
    assert(list[2] == "C");

    str = mstr_join(list, "|");
    assert(str == "A|B|C");

    mstr_reverse(str);
    assert(str == "C|B|A");

    str = mstr_to_string(0);
    assert(str == "0");

    str = mstr_to_string(-12);
    assert(str == "-12");

    str = mstr_to_string(999);
    assert(str == "999");
}
