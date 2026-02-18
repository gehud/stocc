#pragma once

#include <charconv>
#include <cstring>
#include <expected>
#include <filesystem>
#include <format>
#include <functional>
#include <iostream>
#include <limits>
#include <memory>
#include <queue>
#include <ranges>
#include <string>

#include "stocc/config.hpp"
#include "stocc/log.hpp"

namespace fs = std::filesystem;

namespace stocc {

class dataset_error : public std::runtime_error {
public:
    dataset_error(const std::string& what) :
        std::runtime_error(std::format("Dataset error: {}", what)) {}

    dataset_error(
        const fs::path& path,
        int line,
        int column,
        const std::string& what
    ) :
        std::runtime_error(std::format(
            "Dataset error (at {}:{}:{}): {}",
            path.string(),
            line,
            column,
            what
        ))
    {}
};

struct dataset_record {
    uint64_t receive_ts;
    double_t value;

    constexpr dataset_record() = default;

    constexpr dataset_record(uint64_t receive_ts, double_t value) :
        receive_ts(receive_ts),
        value(value)
    {}
};

class dataset {
public:
    static constexpr const char* file_extension = ".csv";

    dataset(dataset&& other) = default;

    dataset(const dataset& other) = delete;

    static std::expected<dataset, dataset_error> load(const fs::path& path) {
        std::ifstream file(path);

        if (!file.is_open()) {
            return std::unexpected(dataset_error(std::format(
                "failed to open file ('{}'): {}",
                path.string(),
                std::strerror(errno)
            )));
        }

        std::string line;
        std::getline(file, line);

        std::stringstream row(line);
        std::string cell;

        auto receive_ts_field = std::numeric_limits<size_t>::max();
        auto price_field = std::numeric_limits<size_t>::max();

        size_t field = 0;
        while (std::getline(row, cell, ';')) {
            if (cell == "receive_ts") {
                receive_ts_field = field;
            } else if (cell == "price") {
                price_field = field;
            }

            ++field;
        }

        if (receive_ts_field == std::numeric_limits<size_t>::max()) {
            return std::unexpected(dataset_error(
                path,
                1,
                1,
                "missing 'receive_ts' field"
            ));
        }

        if (price_field == std::numeric_limits<size_t>::max()) {
            return std::unexpected(dataset_error(
                path,
                1,
                1,
                "missing 'price' field"
            ));
        }

        dataset dataset(path, std::move(file), receive_ts_field, price_field);

        auto next_result = dataset.next();

        if (!next_result.has_value()) {
            return std::unexpected(next_result.error());
        }

        return dataset;
    }

    std::expected<void, dataset_error> next() {
        if (!_file) {
            return {};
        }

        auto read_line_result = read_line();

        if (!read_line_result.has_value()) {
            _file.reset();
            return std::unexpected(read_line_result.error());
        }

        if (!read_line_result.value()) {
            _file.reset();
        }

        return {};
    }

    std::optional<dataset_record> current() const {
        return _file ? std::optional(_record) : std::nullopt;
    }
private:
    fs::path _path;
    std::optional<std::ifstream> _file;
    size_t _receive_ts_field;
    size_t _price_field;
    dataset_record _record;
    size_t _line;

    dataset(
        const fs::path& path,
        std::ifstream&& file,
        size_t receive_ts_index,
        size_t price_index
    ) :
        _path(path),
        _receive_ts_field(receive_ts_index),
        _price_field(price_index),
        _file(std::move(file)),
        _line(1)
    {}

    std::expected<bool, dataset_error> read_line() {
        std::string line;
        if (!std::getline(*_file, line)) {
            return false;
        }

        ++_line;

        std::stringstream row(line);
        std::string cell;

        auto receive_ts_read = false;
        auto price_read = false;
        int column = 1;
        size_t field = 0;
        while (std::getline(row, cell, ';')) {
            if (field == _receive_ts_field) {
                auto result = std::from_chars(
                    cell.data(),
                    cell.data() + cell.size(),
                    _record.receive_ts
                );

                if (result.ec != std::errc()) {
                    return std::unexpected(dataset_error(
                        _path,
                        _line,
                        column,
                        "could not convert 'receive_ts' cell"
                    ));
                }

                receive_ts_read = true;
            } else if (field == _price_field) {
                auto result = std::from_chars(
                    cell.data(),
                    cell.data() + cell.size(),
                    _record.value
                );

                if (result.ec != std::errc()) {
                    return std::unexpected(dataset_error(
                        _path,
                        _line,
                        column,
                        "could not convert 'price' cell"
                    ));
                }

                price_read = true;
            }

            column += cell.size() + 1;
            ++field;
        }

        if (!receive_ts_read) {
            return std::unexpected(dataset_error(
                _path,
                _line,
                column,
                "missing 'receive_ts' cell"
            ));
        }

        if (!price_read) {
            return std::unexpected(dataset_error(
                _path,
                _line,
                column,
                "missing 'price' cell"
            ));
        }

        return true;
    }
};

struct datasets {
private:
    struct entry {
        size_t stream_id;
        dataset_record record;

        bool operator>(const entry& other) const {
            return record.receive_ts > other.record.receive_ts;
        }
    };

    struct data {
        std::vector<dataset> files;
        std::priority_queue<
            entry,
            std::vector<entry>,
            std::greater<entry>
        > queue;
    };
public:
    class iterator {
    public:
        using value_type = std::expected<dataset_record, dataset_error>;
        using difference_type = std::ptrdiff_t;
        using iterator_category = std::input_iterator_tag;

        iterator() : _data(nullptr) {}

        value_type operator*() const {
            if (_current.has_value()) {
                return _current.value().record;
            } else {
                return std::unexpected(_current.error());
            }
        }

        iterator& operator++() {
            if (!_current.has_value()) {
                _data = nullptr;
                return *this;
            }

            _data->queue.pop();

            auto& file = _data->files[_current.value().stream_id];
            auto next_result = file.next();

            if (!next_result.has_value()) {
                _current = std::unexpected(next_result.error());
                return *this;
            }

            auto record = file.current();
            if (record) {
                _data->queue.emplace(_current.value().stream_id, *record);
            }

            if (_data->queue.empty()) {
                _data = nullptr;
            } else {
                _current = _data->queue.top();
            }

            return *this;
        }

        iterator operator++(int) {
            auto tmp = *this;
            ++*this;
            return tmp;
        }

        bool operator==(const iterator& other) const {
            return _data == other._data;
        }

        bool operator!=(const iterator& other) const {
            return !(*this == other);
        }
    private:
        data* _data;
        std::expected<entry, dataset_error> _current;

        iterator(data& data) : _data(&data) {
            if (_data->queue.empty()) {
                _data = nullptr;
            } else {
                _current = _data->queue.top();
            }
        }

        friend class datasets;
    };

    datasets(datasets&& other) = default;

    datasets(const datasets& other) = delete;

    static std::expected<datasets, dataset_error> collect(const config& config) {
        datasets datasets;

        std::error_code error_code;

        fs::directory_iterator directory_iterator(config.input, error_code);

        if (error_code) {
            return std::unexpected(dataset_error(std::format(
                "failed to read directory ('{}'): {}",
                config.input.string(),
                error_code.message()
            )));
        }

        for (const auto& entry : directory_iterator) {
            if (!entry.is_regular_file()) {
                continue;
            }

            auto path = entry.path();

            if (path.extension() != dataset::file_extension) {
                continue;
            }

            if (config.is_filename_suitable(path.filename())) {
                auto dataset_load_result = dataset::load(path);

                if (!dataset_load_result.has_value()) {
                    return std::unexpected(dataset_load_result.error());
                }

                datasets._data.files.emplace_back(
                    std::move(dataset_load_result.value())
                );
            }
        }

        STOCC_LOG_INFO("Files found: {}", datasets._data.files.size());

        for (size_t i = 0; i < datasets._data.files.size(); ++i) {
            auto first = datasets._data.files[i].current();

            if (first) {
                datasets._data.queue.emplace(i, *first);
            }
        }

        return datasets;
    }

    iterator begin() {
        return iterator(_data);
    }

    iterator end() {
        return iterator();
    }
private:
    datasets() = default;

    data _data;
};

} // namespace sotcc
