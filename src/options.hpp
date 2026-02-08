#pragma once

#include <expected>
#include <filesystem>
#include <iostream>
#include <sstream>
#include <string>

#include <boost/program_options.hpp>

namespace fs = std::filesystem;
namespace po = boost::program_options;

namespace stocc {

class options_error : public std::runtime_error {
public:
    std::string allowed_options;

    options_error(const std::string& what, std::string&& allowed_options) :
        std::runtime_error(std::format("Options error: {}", what)),
        allowed_options(std::move(allowed_options))
    {}
};

class options {
public:
    static std::expected<options, options_error> parse(int argc, char* argv[]) {
        options options;

        options._description.add_options()
            ("help,h", "Produce help message");

        options._description.add_options()(
            "log-level,l",
            po::value(&options._log_level),
            "Specify log level (trace,debug,info,warn,err,critical,off)");

        options._description.add_options()
            ("config,c", po::value(&options._config_path), "Specify config path");

        po::variables_map vm;

        try {
            po::store(po::parse_command_line(argc, argv, options._description), vm);
            po::notify(vm);
        } catch (const po::error& error) {
            return std::unexpected(options_error(error.what(), options.description()));
        }

        options._help = vm.count("help");

        return options;
    }

    bool help() const {
        return _help;
    }

    const fs::path& config_path() const {
        return _config_path;
    }

    const std::string& log_level() const {
        return _log_level;
    }

    std::string description() const {
        std::stringstream stream;
        stream << _description;
        return stream.str();
    }
private:
    po::options_description _description;
    bool _help;
    fs::path _config_path;
    std::string _log_level;

    options() :
        _help(false),
        _config_path("./config.toml"),
        _log_level("off"),
        _description(
            "Allowed options",
            po::options_description::m_default_line_length * 1.5
        )
    {}
};

} // namespace stocc
