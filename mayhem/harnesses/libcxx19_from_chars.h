// libc++ 19 (the base image's) does not implement floating-point std::from_chars; LeviLamina's
// StringUtils.h instantiates it for float/double/long double. Force-included by build.sh;
// a strtod-family stand-in with the same result/error contract.
#pragma once
#include <charconv>
#include <cstdlib>
#include <string>
#include <cerrno>
#include <cmath>
namespace std {
template <class T, class F>
inline from_chars_result __ll_fc(const char* f, const char* l, T& v, F fn) {
    std::string s(f, l); char* e = nullptr; errno = 0; T r = fn(s.c_str(), &e);
    if (e == s.c_str()) return {f, errc::invalid_argument};
    if (errno == ERANGE) return {f + (e - s.c_str()), errc::result_out_of_range};
    v = r; return {f + (e - s.c_str()), errc{}};
}
inline from_chars_result from_chars(const char* f, const char* l, long double& v, chars_format = chars_format::general) { return __ll_fc(f, l, v, [](const char* p, char** e){ return strtold(p, e);}); }
inline from_chars_result from_chars(const char* f, const char* l, double& v, chars_format = chars_format::general) { return __ll_fc(f, l, v, [](const char* p, char** e){ return strtod(p, e);}); }
inline from_chars_result from_chars(const char* f, const char* l, float& v, chars_format = chars_format::general) { return __ll_fc(f, l, v, [](const char* p, char** e){ return strtof(p, e);}); }
}
