#pragma once

#include <concepts>
#include <expected>
#include <filesystem>
#include <fstream>
#include <print>

#include <boost/accumulators/accumulators.hpp>
#include <boost/accumulators/statistics.hpp>

#include "stocc/config.hpp"
#include "stocc/dataset.hpp"

namespace accum = boost::accumulators;
namespace fs = std::filesystem;

namespace stocc {

namespace metrics {

template<typename T>
using accumulator = accum::accumulator_set<double_t, accum::stats<T>>;

template<typename M>
concept metric = std::default_initializable<M>
&& std::derived_from<typename M::tag, accum::depends_on<>>
&& requires(M& metric, accumulator<typename M::tag>& accumulator, double_t value) {
    { metric.collect(accumulator, value) } -> std::convertible_to<double_t>;
};

class median {
public:
    using tag = accum::tag::median;

    constexpr median() : _index(0) {}

    double_t collect(accumulator<tag>& accumulator, double_t value) {
        accumulator(value);

        if (_index == 0) {
            ++_index;
            _first = value;
            return value;
        } else if (_index == 1) {
            ++_index;
            return (_first + value) / 2.0;
        } else {
            return accum::median(accumulator);
        }
    }
private:
    size_t _index;
    double_t _first;
};

} // namespace metrics

template<metrics::metric M>
class stats {
private:
    using accumulator = metrics::accumulator<typename M::tag>;
public:
    class iterator {
    public:
        using value_type = std::expected<dataset_record, dataset_error>;
        using difference_type = std::ptrdiff_t;
        using iterator_category = std::input_iterator_tag;

        iterator() : _accumulator(nullptr) {}

        value_type operator*() const {
            return _current;
        }

        iterator& operator++() {
            if (!_current.has_value()) {
                _accumulator = nullptr;
                return *this;
            }

            while (_cursor != _end) {
                auto record_fetch_result = *(_cursor++);
                if (!record_fetch_result.has_value()) {
                    _current = std::unexpected(record_fetch_result.error());
                    return *this;
                }

                const auto& record = record_fetch_result.value();
                ++(*_index);

                auto value = _metric->collect(*_accumulator, record.value);
                if (_last_change != value) {
                    _last_change = value;
                    _current = dataset_record(record.receive_ts, _last_change);
                    return *this;
                }
            }

            _accumulator = nullptr;

            return *this;
        }

        iterator operator++(int) {
            auto tmp = *this;
            ++*this;
            return tmp;
        }

        bool operator==(const iterator& other) const {
            return _accumulator == other._accumulator;
        }

        bool operator!=(const iterator& other) const {
            return !(*this == other);
        }
    private:
        M* _metric;
        accumulator* _accumulator;
        datasets::iterator _cursor;
        datasets::iterator _end;
        value_type _current;
        size_t* _index;
        double_t _last_change;

        iterator(datasets& datasets, M& metric, accumulator& accumulator, size_t& index) :
            _metric(&metric),
            _accumulator(&accumulator),
            _cursor(datasets.begin()),
            _end(datasets.end()),
            _index(&index)
        {
            if (_cursor == _end) {
                _accumulator = nullptr;
                return;
            }

            auto record_fetch_result = *(_cursor++);
            if (!record_fetch_result.has_value()) {
                _current = std::unexpected(record_fetch_result.error());
                return;
            }

            const auto& record = record_fetch_result.value();
            ++(*_index);

            _last_change = _metric->collect(*_accumulator, record.value);
            _current = dataset_record(record.receive_ts, _last_change);
        }

        friend class stats<M>;
    };

    stats(datasets&& datasets) :
        _datasets(std::move(datasets)),
        _index(0)
    {}

    size_t index() const {
        return _index;
    }

    iterator begin() {
        return iterator(_datasets, _metric, _accumulator, _index);
    }

    iterator end() {
        return iterator();
    }
private:
    M _metric;
    datasets _datasets;
    accumulator _accumulator;
    size_t _index;
};

std::expected<void, dataset_error> print_stats(const config& config, datasets&& datasets) {
    auto output_file_path = config.output / "median_result.csv";
    std::ofstream output_file(output_file_path);
    std::println(output_file, "receive_ts;price_median");

    stats<metrics::median> stats(std::move(datasets));

    size_t mutations = 0;
    for (const auto& metric_fetch_result : stats) {
        if (!metric_fetch_result.has_value()) {
            return std::unexpected(metric_fetch_result.error());
        }

        const auto& metric = metric_fetch_result.value();
        std::println(output_file, "{};{}", metric.receive_ts, metric.value);
        ++mutations;
    }

    STOCC_LOG_INFO("Records read: {}", stats.index());
    STOCC_LOG_INFO("Recorded changes in median: {}", mutations);
    STOCC_LOG_INFO("Result saved: '{}'", output_file_path.string());

    return {};
}

} // namespace stocc
