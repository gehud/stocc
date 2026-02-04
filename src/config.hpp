#include <expected>
#include <filesystem>
#include <format>
#include <stdexcept>
#include <string>
#include <vector>

#include <boost/algorithm/string.hpp>
#include <toml++/toml.hpp>

namespace fs = std::filesystem;
namespace algo = boost::algorithm;

namespace stocc {

struct config {
    fs::path input;
    fs::path output;
    std::vector<std::string> filename_mask;
};

class config_error : public std::runtime_error {
public:
    config_error(const std::string& what)
        : std::runtime_error(std::format("Config error: {}", what)) {}
};

auto parse_config(
    const fs::path& path
) -> std::expected<config, config_error>
{
    toml::table table;

    try {
        table = toml::parse_file(path.string());
    } catch (const toml::parse_error& error) {
        return std::unexpected(config_error(error.what()));
    }

    auto main = table["main"];

    if (!main) {
        return std::unexpected(config_error("Missing '[main]' table"));
    }

    if (!main.is_table()) {
        return std::unexpected(config_error("'[main]' must be a table"));
    }

    std::string input_value;
    auto input = main["input"];

    if (!input) {
        return std::unexpected(config_error("Missing 'input' value"));
    }

    if (!input.is_value()) {
        return std::unexpected(config_error("'input' must be a value"));
    }

    auto new_input_value = input.value<std::string>();

    if (!new_input_value) {
        return std::unexpected(config_error("'input' must be a string"));
    }

    input_value = *new_input_value;

    std::string output_value;
    auto output = main["output"];

    if (output) {
        auto new_output_value = output.value<std::string>();

        if (!new_output_value) {
            return std::unexpected(config_error("'output' must be a string"));
        }

        output_value = *new_output_value;
    }

    std::vector<std::string> filename_mask_value;
    auto filename_mask = main["filename_mask"];

    if (filename_mask) {
        auto new_filename_mask = filename_mask.as_array();

        if (!new_filename_mask) {
            return std::unexpected(
                config_error("'filename_mask' field must be an array")
            );
        }

        for (const auto& item : *new_filename_mask) {
            auto filter = item.as_string();

            if (!filter) {
                return std::unexpected(
                    config_error("'filename_mask' item must be a string")
                );
            }

            filename_mask_value.emplace_back(*filter);
        }
    }

    config config = {
        .input = input_value,
        .output = output_value,
        .filename_mask = filename_mask_value
    };

    return config;
}

} // namespace stocc

template<>
struct std::formatter<stocc::config> {
    static constexpr const char* config_fmt = R"(
{{
    input = {},
    output = {},
    filename_mask = [{}]
}})";

    constexpr auto parse(std::format_parse_context& ctx) {
        return ctx.begin();
    }

    auto format(stocc::config config, format_context& ctx) const {
        auto filename_mask = algo::join(config.filename_mask, ", ");
        return std::format_to(
            ctx.out(),
            config_fmt,
            config.input.string(),
            config.output.string(),
            filename_mask
        );
    }
};
