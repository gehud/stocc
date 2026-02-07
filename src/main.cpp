#include <filesystem>
#include <iostream>
#include <limits>
#include <print>
#include <string>

#include <boost/program_options.hpp>

#include <config.hpp>
#include <dataset.hpp>
#include <log.hpp>
#include <stats.hpp>

namespace fs = std::filesystem;
namespace po = boost::program_options;

auto print_exception(const std::exception& exception) {
    std::println(stderr, "Error: {}", exception.what());
}

auto main(int argc, char* argv[]) -> int {
    fs::path config_path("./config.toml");
    std::string log_level("off");

    po::options_description options(
        "Allowed options",
        po::options_description::m_default_line_length * 1.5
    );

    options.add_options()
        ("help,h", "Produce help message")
        ("log-level,l", po::value(&log_level), "Specify log level (trace,debug,info,warn,err,critical,off)")
        ("config,c", po::value(&config_path), "Specify config path");

    po::variables_map vm;

    try {
        po::store(po::parse_command_line(argc, argv, options), vm);
        po::notify(vm);
    } catch (const po::error& error) {
        print_exception(error);
        std::cout << options << std::endl;
        return EXIT_FAILURE;
    }

    if (vm.count("help")) {
        std::cout << options << std::endl;
        return EXIT_SUCCESS;
    }

    auto set_log_level_result = stocc::set_log_level(log_level);
    if (!set_log_level_result.has_value()) {
        print_exception(set_log_level_result.error());
        return EXIT_FAILURE;
    }

    LOG_INFO("Loading config at path: {}", config_path.string());

    auto config_load_result = stocc::config::load(config_path);
    if (!config_load_result.has_value()) {
        print_exception(config_load_result.error());
        return EXIT_FAILURE;
    }

    const auto& config = config_load_result.value();

    LOG_INFO("Loaded config: {}", config);

    auto datasets_collect_result = stocc::datasets::collect(config);
    if (!datasets_collect_result.has_value()) {
        print_exception(datasets_collect_result.error());
        return EXIT_FAILURE;
    }

    stocc::print_stats(config, std::move(datasets_collect_result.value()));

    return EXIT_SUCCESS;
}
