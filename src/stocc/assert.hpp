#pragma once

#include <cstdlib>
#include <print>
#include <source_location>

namespace stocc {
    template<typename... Args>
    void assert_true(std::source_location location, bool statement, std::format_string<Args...> fmt, Args&&... args) {
        if (!statement) {
            std::print(
                stderr,
                "Assertion failed (at {}:{}:{}): ",
                location.file_name(),
                location.line(),
                location.column()
            );
            std::println(
                stderr,
                fmt,
                std::forward<Args>(args)...
            );
            std::exit(EXIT_FAILURE);
        }
    }
} // namespace stocc

#define STOCC_ASSERT_TRUE(STATEMENT, ...) ::stocc::assert_true(::std::source_location::current(), STATEMENT, __VA_ARGS__)
