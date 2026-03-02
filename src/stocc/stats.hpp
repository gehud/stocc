#pragma once

#include <algorithm>
#include <cmath>
#include <concepts>
#include <expected>
#include <filesystem>
#include <fstream>
#include <print>
#include <queue>
#include <ranges>
#include <string_view>
#include <vector>

#include <boost/accumulators/accumulators.hpp>
#include <boost/accumulators/statistics.hpp>

#include "stocc/config.hpp"
#include "stocc/dataset.hpp"

namespace accum = boost::accumulators;
namespace fs = std::filesystem;

namespace boost {
namespace accumulators {

namespace impl {

template<typename Sample>
struct with_queue_median_accumulator : accumulator_base
{
    using result_type = Sample;

    template<typename Args>
    with_queue_median_accumulator(Args const& args) {}

    template<typename Args>
    void operator()(Args const& args) {
        if (_max_heap.empty() || args[sample] <= _max_heap.top()) {
            _max_heap.push(args[sample]);
        } else {
            _min_heap.push(args[sample]);
        }

        // Balance the heaps
        if (_max_heap.size() > _min_heap.size() + 1) {
            _min_heap.push(_max_heap.top());
            _max_heap.pop();
        } else if (_min_heap.size() > _max_heap.size()) {
            _max_heap.push(_min_heap.top());
            _min_heap.pop();
        }
    }

    result_type result(dont_care) const {
        if (_max_heap.size() == 0) {
            return 0.0f; // Nothing happened. Probably all zeros.
        } else if (_max_heap.size() == _min_heap.size()) {
            return (_max_heap.top() + _min_heap.top()) / 2.0;
        } else {
            return _max_heap.top();
        }
    }
private:
    std::priority_queue<Sample> _max_heap; // Heap for the smaller half of the data
    std::priority_queue<
        Sample,
        std::vector<Sample>,
        std::greater<Sample>
    > _min_heap; // Heap for the larger half of the data
};

} // namespace impl

namespace tag {

struct with_queue_median : depends_on<count> {
    using impl = accumulators::impl::with_queue_median_accumulator<mpl::_1>;
};

} // namespace tag

namespace extract {

extractor<tag::with_queue_median> const with_queue_median = {};

} // namespace extract

using extract::with_queue_median;

} // namespace accumulators
} // namespace boost

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
        set()(value);
        return extractor()(set());
    }
protected:
    using accumulator_set = accum::accumulator_set<double_t, accum::stats<tag>>;
    using accumulator_extractor = accum::extractor<T>;

    accumulator_set& set() {
        return _set;
    }

    accumulator_extractor& extractor() {
        return _extractor;
    }
private:
    accumulator_set _set;
    accumulator_extractor _extractor;
};

class median : public metric_base<accum::tag::with_queue_median> {
public:
    static constexpr std::string_view name = "median";
};

class mean : public metric_base<accum::tag::mean> {
public:
    static constexpr std::string_view name = "mean";
};

class variance : public metric_base<accum::tag::variance> {
public:
    static constexpr std::string_view name = "variance";
};

class deviation : public variance {
public:
    static constexpr std::string_view name = "deviation";

    double_t collect(double_t value) override {
        return std::sqrt(variance::collect(value));
    }
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
