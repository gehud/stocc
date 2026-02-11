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
    log_error(const std::string& what) :
        std::runtime_error(std::format("Log error: {}", what)) {}
};

static std::expected<spdlog::level::level_enum, log_error> parse_log_level(
    const std::string_view& level
) {
    if (level == "off") {
        return spdlog::level::off;
    } else if (level == "trace") {
        return spdlog::level::trace;
    } else if (level == "debug") {
        return spdlog::level::debug;
    } else if (level == "info") {
        return spdlog::level::info;
    } else if (level == "warn") {
        return spdlog::level::warn;
    } else if (level == "err") {
        return spdlog::level::err;
    } else if (level == "critical") {
        return spdlog::level::critical;
    }

    return std::unexpected(log_error(std::format(
        "unexpected log level specified: '{}'. "
        "Valid levels: (trace,debug,info,warn,err,critical,off)",
        level
    )));
}

std::expected<void, log_error> set_log_level(const std::string_view& level) {
    auto log_level = algo::trim_copy(level);

    auto parse_level_result = parse_log_level(algo::trim_copy(level));
    if (!parse_level_result.has_value()) {
        return std::unexpected(parse_level_result.error());
    }

    spdlog::set_level(parse_level_result.value());

    return {};
}

} // namespace stocc

#define STOCC_LOG_TRACE(...) ::spdlog::trace(__VA_ARGS__)
#define STOCC_LOG_DEBUG(...) ::spdlog::debug(__VA_ARGS__)
#define STOCC_LOG_INFO(...) ::spdlog::info(__VA_ARGS__)
#define STOCC_LOG_WARN(...) ::spdlog::warn(__VA_ARGS__)
#define STOCC_LOG_ERROR(...) ::spdlog::error(__VA_ARGS__)
#define STOCC_LOG_CRITICAL(...) ::spdlog::critical(__VA_ARGS__)
