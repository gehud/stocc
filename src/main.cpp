#include <filesystem>
#include <iostream>
#include <limits>
#include <print>
#include <string>

#include <boost/accumulators/accumulators.hpp>
#include <boost/accumulators/statistics.hpp>
#include <boost/program_options.hpp>

#include <config.hpp>
#include <csv.hpp>
#include <log.hpp>

namespace accum = boost::accumulators;
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

    auto load_config_result = stocc::load_config(config_path);
    if (!load_config_result.has_value()) {
        print_exception(load_config_result.error());
        return EXIT_FAILURE;
    }

    auto config = load_config_result.value();

    LOG_INFO("Loaded config: {}", config);

    auto collect_prices_result = stocc::collect_prices(config);
    if (!collect_prices_result.has_value()) {
        print_exception(collect_prices_result.error());
        return EXIT_FAILURE;
    }

    auto prices = collect_prices_result.value();

    accum::accumulator_set<
        double,
        accum::stats<accum::tag::median(accum::with_p_square_quantile)>
    > stats;

    auto last_median = std::numeric_limits<double_t>::infinity();
    size_t index = 0;
    for (const auto& pair : prices) {
        stats(pair.second);

        double_t median = 0;
        if (index == 0) {
            median = pair.second;
        } else if (index == 1) {
            median = (pair.second + last_median) / 2;
        } else {
            median = accum::median(stats);
        }

        if (median != last_median) {
            last_median = median;
            LOG_INFO("Median changed: {} - {}", pair.first, median);
        }

        ++index;
    }

    return EXIT_SUCCESS;
}
