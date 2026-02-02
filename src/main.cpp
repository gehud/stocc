#include <iostream>
#include <filesystem>
#include <string>
#include <map>

#include <boost/program_options.hpp>

#include "logger.hpp"

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
    } catch (const po::error& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        std::cerr << options << std::endl;
        return EXIT_FAILURE;
    }

    if (vm.count("help")) {
        std::cout << options << "\n";
        return EXIT_SUCCESS;
    }

    auto set_level_result = logger::set_level(log_level);
    if (!set_level_result.has_value()) {
        std::cerr << "Error: " << set_level_result.error().what() << std::endl;
        return EXIT_FAILURE;
    }

    LOG_INFO("Config path: {}", config_path.string());

    return EXIT_SUCCESS;
}
