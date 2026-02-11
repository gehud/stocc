#pragma once

#include <expected>
#include <filesystem>
#include <fstream>
#include <limits>
#include <print>

#include <boost/accumulators/accumulators.hpp>
#include <boost/accumulators/statistics.hpp>

#include "stocc/config.hpp"
#include "stocc/dataset.hpp"

namespace accum = boost::accumulators;
namespace fs = std::filesystem;

namespace stocc {

std::expected<void, dataset_error> print_stats(const config& config, datasets&& datasets) {
    auto output_file_path = config.output / "median_result.csv";
    std::ofstream output_file(output_file_path);
    std::println(output_file, "receive_ts;price_median");

    accum::accumulator_set<
        double,
        accum::stats<accum::tag::median(accum::with_p_square_quantile)>
    > stats;

    auto last_median = std::numeric_limits<double_t>::infinity();
    size_t index = 0;
    size_t median_mutations = 0;
    for (const auto& record_fetch_result : datasets) {
        if (!record_fetch_result.has_value()) {
            return std::unexpected(record_fetch_result.error());
        }

        const auto& record = record_fetch_result.value();

        stats(record.price);

        double_t median = 0;
        if (index == 0) {
            median = record.price;
        } else if (index == 1) {
            median = (record.price + last_median) / 2;
        } else {
            median = accum::median(stats);
        }

        if (median != last_median) {
            last_median = median;
            std::println(output_file, "{};{}", record.receive_ts, last_median);
            ++median_mutations;
        }

        ++index;
    }

    STOCC_LOG_INFO("Records read: {}", index);
    STOCC_LOG_INFO("Recorded changes in median: {}", median_mutations);
    STOCC_LOG_INFO("Result saved: '{}'", output_file_path.string());

    return {};
}

} // namespace stocc
