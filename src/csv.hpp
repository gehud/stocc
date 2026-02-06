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
        ++(*this);
    }

    csv& operator++() {
        if (_stream) {
            if (!read_line()) {
                _stream.reset();
            }
        }

        return *this;
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

auto collect_prices(const config& config) -> std::expected<void, csv_error> {
    std::vector<csv> files;

    for (const auto& entry : fs::directory_iterator(config.input)) {
        auto path = entry.path();
        if (config.is_file_matches_mask(path)) {
            try {
                files.emplace_back(path);
            } catch (const csv_error& error) {
                return std::unexpected(csv_error(error.what()));
            }
        }
    }

    struct entry {
        size_t stream_id;
        csv_row value;

        bool operator>(const entry& other) const {
            return value.receive_ts > other.value.receive_ts;
        }
    };

    std::priority_queue<entry, std::vector<entry>, std::greater<entry>> queue;

    for (size_t i = 0; i < files.size(); ++i) {
        auto first = *files[i];

        if (first) {
            queue.emplace(i, *first);
        }
    }

    while (!queue.empty()) {
        auto current = queue.top();
        queue.pop();

        LOG_INFO("{} - {}", current.value.receive_ts, current.value.price);

        auto value = *++files[current.stream_id];
        if (value) {
            queue.emplace(current.stream_id, *value);
        }
    }

    return {};
}

} // namespace sotcc
