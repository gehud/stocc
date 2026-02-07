#pragma once

#include <filesystem>
#include <fstream>
#include <limits>
#include <print>

#include <boost/accumulators/accumulators.hpp>
#include <boost/accumulators/statistics.hpp>

#include <dataset.hpp>
#include <config.hpp>

namespace accum = boost::accumulators;
namespace fs = std::filesystem;

namespace stocc {

void print_stats(const config& config, datasets&& datasets) {
    auto output_file_path = config.output;
    output_file_path.append("median_result.csv");
    std::ofstream output_file(output_file_path);
    std::println(output_file, "receive_ts;price_median");

    accum::accumulator_set<
        double,
        accum::stats<accum::tag::median(accum::with_p_square_quantile)>
    > stats;

    auto last_median = std::numeric_limits<double_t>::infinity();
    size_t index = 0;
    for (const auto& entry : datasets) {
        stats(entry.price);

        double_t median = 0;
        if (index == 0) {
            median = entry.price;
        } else if (index == 1) {
            median = (entry.price + last_median) / 2;
        } else {
            median = accum::median(stats);
        }

        if (median != last_median) {
            last_median = median;
            std::println(output_file, "{};{}", entry.receive_ts, last_median);
        }

        ++index;
    }
}

} // namespace stocc
