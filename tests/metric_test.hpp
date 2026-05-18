#pragma once

#include <cmath>
#include <format>

#include "stocc/assert.hpp"
#include "stocc/stats.hpp"

namespace stocc {
namespace test {

static constexpr size_t dataset_size = 100;

static const double_t dataset[dataset_size] = {
    41.99383529,67.19714737,48.23770647,46.28224103,86.84558096,12.05884434,88.86901770,70.27479245,22.71788769,9.37358172,50.07414728,20.30140134,74.78314943,56.90474104,58.33009032,49.18352601,53.87387962,91.27924325,11.10909357,35.28999965,56.00706334,46.57308102,90.22020991,35.74099028,20.34019261,42.08862309,30.91564937,78.39906175,33.54069790,51.77720277,47.39487903,34.78322438,49.96576554,79.66359254,49.06374958,20.29286749,83.76701436,99.82241224,68.88876843,33.80521317,86.02555614,69.65063534,14.41745969,18.09734518,35.20372719,31.97729446,51.54679810,55.87956577,34.12488984,48.59719773,89.00847218,83.35038496,87.67507360,73.14925328,10.40178916,47.44322901,67.42702726,69.90537712,1.33867619,36.26721360,5.24961210,31.41859283,37.66579584,70.80502688,43.37063506,81.35993365,93.26981193,59.58286634,82.50096336,12.51513661,88.49269928,1.07630272,46.39854836,55.77690741,50.09386535,82.96183576,31.22933342,92.86842867,97.51817901,60.77082517,40.01493002,43.58519336,40.92828903,2.42643173,89.60986654,53.02226566,57.31527926,5.47806984,26.66348981,38.38733112,72.96480440,13.33882144,7.93304172,39.74590809,9.95286028,26.39663802,72.55644357,94.77809689,34.70750030,43.98015693
};

static const unsigned int precision = 8;

double_t round_with_precision(double_t value) {
    static const auto pow = std::pow(10.0, precision);
    return std::round(value * pow) / pow;
}

template<stocc::metrics::metric M>
void test_metric(const double_t collection[]) {
    M metric;
    for (size_t i = 0; i < dataset_size; i++) {
        const auto unrounded = metric.collect(dataset[i]);
        const auto actual = std::format("{:.8f}", round_with_precision(unrounded));
        const auto expected = std::format("{:.8f}", collection[i]);
        STOCC_ASSERT_TRUE(
            actual == expected,
            "wrong {} calculation "
            "at iteration {}. Got {} (unrounded: {}), expected {}.",
            M::name,
            i,
            actual,
            unrounded,
            expected
        );
    }
}

} // namespace test
} // namespace stocc
