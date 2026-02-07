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

class csv_error : public std::runtime_error {
public:
    csv_error(const std::string& what)
        : std::runtime_error(std::format("Csv error: {}", what)) {}

    csv_error(const fs::path& path, const std::string& what)
        : std::runtime_error(std::format("Csv error (at '{}'): {}", path.string(), what)) {}
};

struct csv_row {
    uint64_t receive_ts;
    double_t price;
};

class csv {
public:
    csv(const fs::path& path) {
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
            throw csv_error("Missing 'receive_ts' column");
        }

        if (_price_index == std::numeric_limits<size_t>::max()) {
            throw csv_error("Missing 'price' column");
        }

        _stream.emplace(std::move(stream));

        next();
    }

    void next() {
        if (_stream) {
            if (!read_line()) {
                _stream.reset();
            }
        }
    }

    std::optional<csv_row> operator*() const {
        return _stream ? std::optional(_row) : std::nullopt;
    }

    explicit operator bool() const {
        return (bool)_stream;
    }
private:
    size_t _receive_ts_index;
    size_t _price_index;
    std::optional<std::ifstream> _stream;
    csv_row _row;

    auto read_line() -> bool {
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
                _row.receive_ts = std::stoul(cell);
            } else if (index == _price_index) {
                _row.price = std::stod(cell);
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
        csv_row value;

        bool operator>(const entry& other) const {
            return value.receive_ts > other.value.receive_ts;
        }
    };

    struct data {
        std::vector<csv> files;
        std::priority_queue<entry, std::vector<entry>, std::greater<entry>> queue;
    };
public:
    class iterator {
    public:
        using value_type = csv_row;
        using difference_type = std::ptrdiff_t;
        using pointer = const value_type*;
        using reference = const value_type&;
        using iterator_category = std::input_iterator_tag;

        reference operator*() const {
            return _current.value;
        }

        pointer operator->() const {
            return &_current.value;
        }

        iterator& operator++() {
            _data->queue.pop();

            auto& file = _data->files[_current.stream_id];
            file.next();
            auto value = *file;
            if (value) {
                _data->queue.emplace(_current.stream_id, *value);
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

    static std::expected<datasets, csv_error> collect(const config& config) {
        datasets datasets;

        for (const auto& entry : fs::directory_iterator(config.input)) {
            auto path = entry.path();
            if (config.is_file_matches_mask(path)) {
                try {
                    datasets._data.files.emplace_back(path);
                } catch (const csv_error& error) {
                    return std::unexpected(csv_error(error.what()));
                }
            }
        }

        for (size_t i = 0; i < datasets._data.files.size(); ++i) {
            auto& file = datasets._data.files[i];
            auto first = *file;

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
