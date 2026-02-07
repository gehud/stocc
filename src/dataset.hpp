#pragma once

#include <expected>
#include <filesystem>
#include <functional>
#include <limits>
#include <memory>
#include <queue>
#include <ranges>
#include <string>
#include <iostream>

#include <config.hpp>
#include <log.hpp>

namespace fs = std::filesystem;

namespace stocc {

class dataset_error : public std::runtime_error {
public:
    dataset_error(const std::string& what)
        : std::runtime_error(std::format("Dataset error: {}", what)) {}

    dataset_error(const fs::path& path, const std::string& what)
        : std::runtime_error(std::format("Dataset error (at '{}'): {}", path.string(), what)) {}
};

struct dataset_record {
    uint64_t receive_ts;
    double_t price;
};

class dataset {
public:
    static constexpr const char* file_extension = ".csv";

    dataset(dataset&& other) = default;

    dataset(const dataset& other) = delete;

    dataset(const fs::path& path) {
        std::ifstream stream(path);

        std::string line;
        std::getline(stream, line);

        std::stringstream line_stream(line);
        std::string cell;

        _receive_ts_index = std::numeric_limits<size_t>::max();
        _price_index = std::numeric_limits<size_t>::max();

        size_t index = 0;
        while (std::getline(line_stream, cell, ';')) {
            if (cell == "receive_ts") {
                _receive_ts_index = index;
            } else if (cell == "price") {
                _price_index = index;
            }

            ++index;
        }

        if (_receive_ts_index == std::numeric_limits<size_t>::max()) {
            throw dataset_error(path, "missing 'receive_ts' field at line 1");
        }

        if (_price_index == std::numeric_limits<size_t>::max()) {
            throw dataset_error(path, "missing 'price' field at line 1");
        }

        _stream.emplace(std::move(stream));

        ++(*this);
    }

    void next() {
        if (_stream) {
            if (!read_line()) {
                _stream.reset();
            }
        }
    }

    dataset& operator++() {
        next();
        return *this;
    }

    std::optional<dataset_record> current() const {
        return _stream ? std::optional(_record) : std::nullopt;
    }

    std::optional<dataset_record> operator*() const {
        return current();
    }

    bool has_pair() const {
        return (bool)_stream;
    }

    explicit operator bool() const {
        return has_pair();
    }
private:
    size_t _receive_ts_index;
    size_t _price_index;
    std::optional<std::ifstream> _stream;
    dataset_record _record;

    bool read_line() {
        if (!_stream) {
            return false;
        }

        std::string line;
        std::getline(*_stream, line);

        std::stringstream line_stream(line);
        std::string cell;

        size_t index = 0;
        while (std::getline(line_stream, cell, ';')) {
            if (index == _receive_ts_index) {
                _record.receive_ts = std::stoul(cell);
            } else if (index == _price_index) {
                _record.price = std::stod(cell);
            }

            ++index;
        }

        return !_stream->eof();
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
        std::priority_queue<entry, std::vector<entry>, std::greater<entry>> queue;
    };
public:
    class iterator {
    public:
        using value_type = dataset_record;
        using difference_type = std::ptrdiff_t;
        using pointer = const value_type*;
        using reference = const value_type&;
        using iterator_category = std::input_iterator_tag;

        reference operator*() const {
            return _current.record;
        }

        pointer operator->() const {
            return &_current.record;
        }

        iterator& operator++() {
            _data->queue.pop();

            auto record = *++_data->files[_current.stream_id];
            if (record) {
                _data->queue.emplace(_current.stream_id, *record);
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
        entry _current;

        iterator() : _data(nullptr) {}

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

        for (const auto& entry : fs::directory_iterator(config.input)) {
            if (!entry.is_regular_file()) {
                continue;
            }

            auto path = entry.path();

            if (path.extension() != dataset::file_extension) {
                continue;
            }

            if (config.is_filename_suitable(path.filename())) {
                try {
                    datasets._data.files.emplace_back(path);
                } catch (const dataset_error& error) {
                    return std::unexpected(dataset_error(error.what()));
                }
            }
        }

        LOG_INFO("Files found: {}", datasets._data.files.size());

        for (size_t i = 0; i < datasets._data.files.size(); ++i) {
            auto first = *datasets._data.files[i];

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
