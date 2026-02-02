#pragma once

#include <expected>
#include <stdexcept>
#include <string_view>

#include <boost/algorithm/string.hpp>
#include <spdlog/spdlog.h>

namespace algo = boost::algorithm;

namespace logger {
    class set_level_error : public std::logic_error {
    public:
        set_level_error(const std::string& what) : std::logic_error(what) {}
    };

    auto set_level(const std::string_view& level) -> std::expected<void, set_level_error> {
        auto normalized_level = algo::trim_copy(level);

        if (normalized_level == "trace") {
            spdlog::set_level(spdlog::level::trace);
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
        } else if (normalized_level == "off") {
            spdlog::set_level(spdlog::level::off);
        } else {
            return std::unexpected(set_level_error(std::format(
                "Unexpected log level specified: '{}'. \
Valid level: (trace,debug,info,warn,err,critical,off)", normalized_level)));
        }

        return {};
    }
}

#define LOG_TRACE(...) ::spdlog::trace(__VA_ARGS__)
#define LOG_DEBUG(...) ::spdlog::debug(__VA_ARGS__)
#define LOG_INFO(...) ::spdlog::info(__VA_ARGS__)
#define LOG_WARN(...) ::spdlog::warn(__VA_ARGS__)
#define LOG_ERROR(...) ::spdlog::error(__VA_ARGS__)
#define LOG_CRITICAL(...) ::spdlog::critical(__VA_ARGS__)
