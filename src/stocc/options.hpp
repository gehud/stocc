#pragma once

#include <expected>
#include <filesystem>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>

#include <boost/program_options.hpp>

#include "stocc/config.hpp"
#include "stocc/stats.hpp"

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
    static std::expected<options, options_error> parse(
        int argc,
        char* argv[]
    ) {
        po::options_description description(
            po::options_description::m_default_line_length * 2.0
        );

        description.add_options()
            ("help,h", "Produce help message");

        std::string log_level("off");

        description.add_options()
            ("log-level,l", po::value(&log_level),
                "Specify log level (trace,debug,info,warn,err,critical,off)");

        fs::path config_path(config::default_path);

        description.add_options()
            ("config,c", po::value(&config_path), "Specify config path");

        std::string metric("median");

        auto metrics_description = std::format("Specify metric type ({})", registry.names());

        description.add_options()
            ("metric,m", po::value(&metric), metrics_description.c_str());

        std::stringstream description_stream;
        description_stream << description;

        po::variables_map map;

        try {
            po::store(po::parse_command_line(argc, argv, description), map);
            po::notify(map);
        } catch (const po::error& error) {
            return std::unexpected(options_error(
                error.what(),
                description_stream.str()
            ));
        }

        bool help = map.count("help");

        return options(
            description_stream.str(),
            help,
            std::move(config_path),
            std::move(metric),
            std::move(log_level)
        );
    }

    const std::string& description() const noexcept {
        return _description;
    }

    bool help() const noexcept {
        return _help;
    }

    const fs::path& config_path() const noexcept {
        return _config_path;
    }

    const std::string& metric() const noexcept {
        return _metric;
    }

    const std::string& log_level() const noexcept {
        return _log_level;
    }
private:
    std::string _description;
    bool _help;
    fs::path _config_path;
    std::string _metric;
    std::string _log_level;

    options(
        std::string&& description,
        bool help,
        fs::path&& config_path,
        std::string&& metric,
        std::string&& log_level
    ) :
        _description(std::move(description)),
        _help(help),
        _config_path(std::move(config_path)),
        _metric(std::move(metric)),
        _log_level(std::move(log_level))
    {}
};

} // namespace stocc
