#pragma once

#include <expected>
#include <filesystem>
#include <format>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

#include <boost/algorithm/string.hpp>
#include <toml++/toml.hpp>

#include "stocc/log.hpp"

namespace algo = boost::algorithm;
namespace fs = std::filesystem;

namespace std {

template<>
struct formatter<toml::source_path_ptr> {
    constexpr auto parse(format_parse_context& ctx) {
        return ctx.begin();
    }

    auto format(
        const toml::source_path_ptr& source_path,
        format_context& ctx
    ) const {
        auto path = source_path ? *source_path : "unknown";
        return format_to(ctx.out(), "{}", path);
    }
};

template<>
struct formatter<toml::source_position> {
    constexpr auto parse(format_parse_context& ctx) {
        return ctx.begin();
    }

    auto format(
        const toml::source_position& source_position,
        format_context& ctx
    ) const {
        return format_to(
            ctx.out(),
            "{}:{}",
            source_position.line,
            source_position.column
        );
    }
};

template<>
struct formatter<toml::source_region> {
    constexpr auto parse(format_parse_context& ctx) {
        return ctx.begin();
    }

    auto format(
        const toml::source_region& source_region,
        format_context& ctx
    ) const {
        return format_to(
            ctx.out(),
            "{}:{}",
            source_region.path,
            source_region.begin
        );
    }
};

} // namespace std

namespace stocc {

class config_error : public std::runtime_error {
public:
    config_error(const std::string& what) :
        std::runtime_error(
            std::format("Config error: {}", what)
        )
    {}

    config_error(const fs::path& path, const std::string& what) :
        std::runtime_error(
            std::format("Config error (at {}): {}", path.string(), what)
        )
    {}

    config_error(const toml::source_region& source, const std::string& what) :
        std::runtime_error(
            std::format("Config error (at {}): {}", source, what)
        )
    {}

    config_error(
        const toml::source_path_ptr& path,
        const toml::source_position& position,
        const std::string& what
    ) :
        std::runtime_error(
            std::format("Config error (at {}:{}): {}", path, position, what)
        )
    {}
};

struct config {
    static constexpr const char* default_path = "./config.toml";

    fs::path input;
    fs::path output;
    std::vector<std::string> filename_mask;

    bool is_filename_suitable(
        const std::string& filename
    ) const noexcept {
        if (filename_mask.empty()) {
            return true;
        }

        for (const auto& filter : filename_mask) {
            if (filename.contains(filter)) {
                return true;
            }
        }

        return false;
    }

    static std::expected<config, config_error> load(const fs::path& path) {
        std::error_code error_code;

        auto is_path_exists = fs::exists(path, error_code);

        if (error_code) {
            return std::unexpected(config_error(error_code.message()));
        }

        if (!is_path_exists) {
            auto is_default_path = path.generic_string() == config::default_path;

            std::string error_message;
            if (is_default_path) {
                error_message = std::format(
                    "file does not exists in the working directory: '{}'. "
                    "You can specify it with '--config' argument",
                    path.string()
                );
            } else {
                error_message = std::format(
                    "file does not exists: '{}'",
                    path.string()
                );
            }

            return std::unexpected(config_error(error_message));
        }

        if (!fs::is_regular_file(path)) {
            return std::unexpected(config_error(std::format(
                "path does not lead to a file: '{}'",
                path.string()
            )));
        }

        STOCC_LOG_INFO("Loading config: '{}'", path.string());

        toml::table table;

        try {
            table = toml::parse_file(path.string());
        } catch (const toml::parse_error& error) {
            return std::unexpected(config_error(
                error.source(),
                error.what()
            ));
        }

        auto main = table["main"];

        if (!main) {
            return std::unexpected(config_error(
                path,
                "missing '[main]' table"
            ));
        }

        if (!main.is_table()) {
            return std::unexpected(config_error(
                main.node()->source(),
                "'[main]' must be a table")
            );
        }

        fs::path input_value;
        auto input = main["input"];

        if (!input) {
            return std::unexpected(config_error(
                main.node()->source(),
                "missing 'input' value"
            ));
        }

        if (!input.is_value()) {
            return std::unexpected(config_error(
                input.node()->source(),
                "'input' must be a value"
            ));
        }

        auto new_input_value = input.value<std::string>();

        if (!new_input_value) {
            return std::unexpected(config_error(
                input.node()->source(),
                "'input' must be a string"
            ));
        }

        input_value = *new_input_value;

        auto input_exists = fs::exists(input_value, error_code);

        if (error_code) {
            return std::unexpected(config_error(error_code.message()));
        }

        if (!input_exists) {
            return std::unexpected(config_error(
                input.node()->source(),
                std::format(
                    "specified 'input' path ('{}') does not exist",
                    input_value.string()
                )
            ));
        }

        auto input_is_directory = fs::is_directory(input_value, error_code);

        if (error_code) {
            return std::unexpected(config_error(error_code.message()));
        }

        if (!input_is_directory) {
            return std::unexpected(config_error(
                input.node()->source(),
                std::format(
                    "specified 'input' path ('{}') "
                    "does not point to a valid directory",
                    input_value.string()
                )
            ));
        }

        fs::path output_value("./output");
        auto output = main["output"];

        if (output) {
            auto new_output_value = output.value<std::string>();

            if (!new_output_value) {
                return std::unexpected(config_error(
                    output.node()->source(),
                    "'output' must be a string"
                ));
            }

            output_value = *new_output_value;
        }

        auto output_exists = fs::exists(output_value, error_code);

        if (error_code) {
            return std::unexpected(config_error(error_code.message()));
        }

        auto output_is_directory = fs::is_directory(output_value, error_code);

        if (error_code) {
            return std::unexpected(config_error(error_code.message()));
        }

        if (output_exists) {
            if (!output_is_directory) {
                return std::unexpected(config_error(
                    output.node()->source(),
                    std::format(
                        "specified 'output' path ('{}') exist "
                        "and does not point to a valid directory",
                        output_value.string()
                    )
                ));
            }

            STOCC_LOG_TRACE(
                "Output directory found: '{}'. Skipping creation",
                output_value.string()
            );
        } else {
            fs::create_directories(output_value, error_code);

            if (error_code) {
                return std::unexpected(config_error(
                    output.node()->source(),
                    std::format(
                        "could not create 'output' directory ('{}'): {}",
                        output_value.string(),
                        error_code.message()
                    )
                ));
            }

            STOCC_LOG_TRACE("Output directory created: '{}'", output_value.string());
        }

        std::vector<std::string> filename_mask_value;
        auto filename_mask = main["filename_mask"];

        if (filename_mask) {
            auto new_filename_mask = filename_mask.as_array();

            if (!new_filename_mask) {
                return std::unexpected(config_error(
                    filename_mask.node()->source(),
                    "'filename_mask' field must be an array"
                ));
            }

            for (const auto& node : *new_filename_mask) {
                auto filter_value = node.as_string();

                if (!filter_value) {
                    return std::unexpected(config_error(
                        node.source(),
                        "'filename_mask' item must be a string"
                    ));
                }

                filename_mask_value.emplace_back(*filter_value);
            }
        }

        config config = {
            .input = input_value,
            .output = output_value,
            .filename_mask = filename_mask_value
        };

        return config;
    }
};

} // namespace stocc

namespace std {

template<>
struct formatter<stocc::config> {
    static constexpr const char* config_fmt = R"(
{{
    input = {},
    output = {},
    filename_mask = [{}]
}})";

    constexpr auto parse(format_parse_context& ctx) {
        return ctx.begin();
    }

    auto format(const stocc::config& config, format_context& ctx) const {
        auto filename_mask = algo::join(config.filename_mask, ", ");
        return format_to(
            ctx.out(),
            config_fmt,
            config.input.string(),
            config.output.string(),
            filename_mask
        );
    }
};

} // namespace std
