#pragma once

#include <concepts>
#include <expected>
#include <filesystem>
#include <fstream>
#include <print>
#include <ranges>
#include <string_view>

#include <boost/accumulators/accumulators.hpp>
#include <boost/accumulators/statistics.hpp>

#include "stocc/config.hpp"
#include "stocc/dataset.hpp"

namespace accum = boost::accumulators;
namespace fs = std::filesystem;

namespace stocc {

namespace metrics {

template<typename T>
concept tag = requires (T & tag) {
    [] <typename... Types>(accum::depends_on<Types...>&) {}(tag);
};

template<typename M>
concept metric = std::default_initializable<M>
&& tag<typename M::tag>
&& requires(M & metric, double_t value) {
    { M::name } -> std::convertible_to<std::string_view>;
    { metric.collect(value) } -> std::convertible_to<double_t>;
};

template<tag T>
class metric_base {
public:
    using tag = T;

    virtual double_t collect(double_t value) {
        accumulator()(value);
        return extractor()(accumulator());
    }
protected:
    using accumulator_set = accum::accumulator_set<double_t, accum::stats<tag>>;
    using accumulator_extractor = accum::extractor<T>;

    accumulator_set& accumulator() {
        return _accumulator;
    }

    accumulator_extractor& extractor() {
        return _extractor;
    }
private:
    accumulator_set _accumulator;
    accumulator_extractor _extractor;
};

class median : public metric_base<accum::tag::median> {
public:
    static constexpr std::string_view name = "median";

    median() : _index(0) {}

    double_t collect(double_t value) override {
        accumulator()(value);

        if (_index == 0) {
            ++_index;
            _first = value;
            return value;
        } else if (_index == 1) {
            ++_index;
            return (_first + value) / 2.0;
        } else {
            return accum::median(accumulator());
        }
    }
private:
    size_t _index;
    double_t _first;
};

class mean : public metric_base<accum::tag::mean> {
public:
    static constexpr std::string_view name = "mean";
};

class variance : public metric_base<accum::tag::variance> {
public:
    static constexpr std::string_view name = "variance";
};

} // namespace metrics

template<metrics::metric M>
class stats {
public:
    class iterator {
    public:
        using value_type = std::expected<dataset_record, dataset_error>;
        using difference_type = std::ptrdiff_t;
        using iterator_category = std::input_iterator_tag;

        iterator() : _metric(nullptr) {}

        value_type operator*() const {
            return _current;
        }

        iterator& operator++() {
            if (!_current.has_value()) {
                _metric = nullptr;
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

                auto value = _metric->collect(record.value);
                if (_last_change != value) {
                    _last_change = value;
                    _current = dataset_record(record.receive_ts, _last_change);
                    return *this;
                }
            }

            _metric = nullptr;

            return *this;
        }

        iterator operator++(int) {
            auto tmp = *this;
            ++*this;
            return tmp;
        }

        bool operator==(const iterator& other) const {
            return _metric == other._metric;
        }

        bool operator!=(const iterator& other) const {
            return !(*this == other);
        }
    private:
        M* _metric;
        datasets::iterator _cursor;
        datasets::iterator _end;
        value_type _current;
        size_t* _index;
        double_t _last_change;

        iterator(datasets& datasets, M& metric, size_t& index) :
            _metric(&metric),
            _cursor(datasets.begin()),
            _end(datasets.end()),
            _index(&index)
        {
            if (_cursor == _end) {
                _metric = nullptr;
                return;
            }

            auto record_fetch_result = *(_cursor++);
            if (!record_fetch_result.has_value()) {
                _current = std::unexpected(record_fetch_result.error());
                return;
            }

            const auto& record = record_fetch_result.value();
            ++(*_index);

            _last_change = _metric->collect(record.value);
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
        return iterator(_datasets, _metric, _index);
    }

    iterator end() {
        return iterator();
    }
private:
    M _metric;
    datasets _datasets;
    size_t _index;
};

template<metrics::metric M>
std::expected<void, dataset_error> print_metric(const config& config, datasets&& datasets) {
    auto output_file_path = config.output / std::format("{}_result.csv", M::name);
    std::ofstream output_file(output_file_path);
    std::println(output_file, "receive_ts;price_{}", M::name);

    stats<M> stats(std::move(datasets));

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
    STOCC_LOG_INFO("Recorded changes in {}: {}", M::name, mutations);
    STOCC_LOG_INFO("Result saved: '{}'", output_file_path.string());

    return {};
}

} // namespace stocc
