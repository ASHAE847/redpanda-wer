#pragma once

#include "seastarx.h"

#include <seastar/core/sstring.hh>

#include <boost/range/algorithm/generate.hpp>
#include <bytes/bytes.h>

#include <iostream>
#include <random>

// Random generators useful for testing.
namespace random_generators {

namespace internal {

inline std::random_device::result_type get_seed() {
    std::random_device rd;
    auto seed = rd();
    return seed;
}
static thread_local std::default_random_engine gen(internal::get_seed());
} // namespace internal

template<typename T>
T get_int() {
    std::uniform_int_distribution<T> dist;
    return dist(internal::gen);
}

template<typename T>
T get_int(T min, T max) {
    std::uniform_int_distribution<T> dist(min, max);
    return dist(internal::gen);
}

template<typename T>
T get_int(T max) {
    return get_int<T>(0, max);
}

inline bytes get_bytes(size_t n) {
    bytes b(bytes::initialized_later(), n);
    boost::generate(b, [] { return get_int<bytes::value_type>(); });
    return b;
}

inline bytes get_bytes() {
    return get_bytes(get_int<unsigned>(128 * 1024));
}

inline sstring gen_alphanum_string(size_t n) {
    static constexpr char chars[]
      = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
    std::uniform_int_distribution<size_t> dist(0, sizeof(chars) - 1);
    sstring s(sstring::initialized_later(), n);
    std::generate_n(
      s.begin(), n, [&dist] { return chars[dist(internal::gen)]; });
    return s;
}

} // namespace random_generators
