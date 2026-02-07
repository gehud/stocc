#pragma once

#include <expected>
#include <format>
#include <stdexcept>
#include <string_view>

#include <boost/algorithm/string.hpp>
#include <spdlog/spdlog.h>

namespace algo = boost::algorithm;

namespace stocc {

class log_error : public std::runtime_error {
public:
    log_error(const std::string& what)
        : std::runtime_error(std::format("Log error: {}", what)) {}
};

auto set_log_level(const std::string_view& level) -> std::expected<void, log_error> {
    auto normalized_level = algo::trim_copy(level);

    if (normalized_level == "off") {
        spdlog::set_level(spdlog::level::off);
    } else if (normalized_level == "debug") {
        spdlog::set_level(spdlog::level::debug);
    } else if (normalized_level == "info") {
        spdlog::set_level(spdlog::level::info);
    } else if (normalized_level == "warn") {
        spdlog::set_level(spdlog::level::warn);
    } else if (normalized_level == "err") {
        spdlog::set_level(spdlog::level::err);
    } else if (normalized_level == "critical") {
        spdlog::set_level(spdlog::level::critical);
    } else if (normalized_level == "trace") {
        spdlog::set_level(spdlog::level::trace);
    } else {
        return std::unexpected(log_error(std::format(
            "Unexpected log level specified: '{}'. \
Valid level: (trace,debug,info,warn,err,critical,off)", normalized_level)));
    }

    return {};
}

} // namespace stocc

#define LOG_TRACE(...) ::spdlog::trace(__VA_ARGS__)
#define LOG_DEBUG(...) ::spdlog::debug(__VA_ARGS__)
#define LOG_INFO(...) ::spdlog::info(__VA_ARGS__)
#define LOG_WARN(...) ::spdlog::warn(__VA_ARGS__)
#define LOG_ERROR(...) ::spdlog::error(__VA_ARGS__)
#define LOG_CRITICAL(...) ::spdlog::critical(__VA_ARGS__)
