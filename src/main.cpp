#include <print>

#include <config.hpp>
#include <dataset.hpp>
#include <log.hpp>
#include <options.hpp>
#include <stats.hpp>

int main(int argc, char* argv[]) {
    auto options_parse_result = stocc::options::parse(argc, argv);
    if (!options_parse_result.has_value()) {
        std::println(stderr, "{}", options_parse_result.error().what());
        std::println(stdout, "{}", options_parse_result.error().allowed_options);
        return EXIT_FAILURE;
    }

    const auto& options = options_parse_result.value();

    if (options.help()) {
        std::println(stdout, "{}", options.description());
        return EXIT_SUCCESS;
    }

    auto set_log_level_result = stocc::set_log_level(options.log_level());
    if (!set_log_level_result.has_value()) {
        std::println(stderr, "{}", set_log_level_result.error().what());
        return EXIT_FAILURE;
    }

    LOG_INFO("Loading config at path: {}", options.config_path().string());

    auto config_load_result = stocc::config::load(options.config_path());
    if (!config_load_result.has_value()) {
        std::println(stderr, "{}", config_load_result.error().what());
        return EXIT_FAILURE;
    }

    const auto& config = config_load_result.value();

    LOG_INFO("Loaded config: {}", config);

    auto datasets_collect_result = stocc::datasets::collect(config);
    if (!datasets_collect_result.has_value()) {
        std::println(stderr, "{}", datasets_collect_result.error().what());
        return EXIT_FAILURE;
    }

    stocc::print_stats(config, std::move(datasets_collect_result.value()));

    return EXIT_SUCCESS;
}
