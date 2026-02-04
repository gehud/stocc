#include <filesystem>
#include <iostream>
#include <string>

#include <boost/program_options.hpp>

#include <config.hpp>
#include <log.hpp>

namespace fs = std::filesystem;
namespace po = boost::program_options;

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
        std::cerr << "Error: " << error.what() << std::endl;
        std::cout << options << std::endl;
        return EXIT_FAILURE;
    }

    if (vm.count("help")) {
        std::cout << options << std::endl;
        return EXIT_SUCCESS;
    }

    auto set_log_level_result = stocc::set_log_level(log_level);
    if (!set_log_level_result.has_value()) {
        std::cerr << "Error: " << set_log_level_result.error().what() << std::endl;
        return EXIT_FAILURE;
    }

    LOG_INFO("Config path: {}", config_path.string());

    auto parse_config_result = stocc::parse_config(config_path);
    if (!parse_config_result.has_value()) {
        std::cerr << "Error: " << parse_config_result.error().what() << std::endl;
        return EXIT_FAILURE;
    }

    auto config = parse_config_result.value();

    LOG_INFO("Config: {}", config);

    return EXIT_SUCCESS;
}
