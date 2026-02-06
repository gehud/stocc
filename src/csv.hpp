#pragma once

#include <expected>
#include <filesystem>
#include <limits>
#include <map>
#include <memory>
#include <string>
#include <iostream>

#include <config.hpp>

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

class csv_iterator {
public:
    using value_type = csv_row;
    using difference_type = std::ptrdiff_t;
    using pointer = value_type*;
    using reference = value_type&;
    using iterator_category = std::forward_iterator_tag;

    csv_iterator(std::istream& stream) {
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

        _stream = &stream;
        ++(*this);
    }

    csv_iterator() : _stream(nullptr) {}

    csv_iterator& operator++() {
        if (_stream) {
            if (!read_line()) {
                _stream = nullptr;
            }
        }

        return *this;
    }

    csv_iterator operator++(int) {
        csv_iterator tmp(*this);
        ++(*this);
        return tmp;
    }

    csv_row const& operator*() const {
        return _row;
    }

    csv_row const* operator->() const {
        return &_row;
    }

    bool operator==(csv_iterator const& rhs) const {
        return (this == &rhs) || (_stream == nullptr && rhs._stream == nullptr);
    }

    bool operator!=(csv_iterator const& rhs) const {
        return !(*this == rhs);
    }
private:
    size_t _receive_ts_index;
    size_t _price_index;
    std::istream* _stream;
    csv_row _row;

    auto read_line() -> bool {
        if (_stream == nullptr) {
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

class csv {
public:
    using iterator = csv_iterator;

    csv(std::istream& stream) : _stream(stream) {}

    iterator begin() const {
        return iterator(_stream);
    }

    iterator end() const {
        return iterator();
    }
private:
    std::istream& _stream;
};

using prices = std::multimap<uint64_t, double_t>;

auto collect_prices(const config& config) -> std::expected<prices, csv_error> {
    prices prices;

    for (const auto& entry : fs::directory_iterator(config.input)) {
        auto path = entry.path();
        if (config.is_file_matches_mask(path)) {
            auto file = std::ifstream(path);
            stocc::csv csv(file);
            try {
                for (const auto& row : csv) {
                    prices.insert({ row.receive_ts, row.price });
                }
            } catch (const stocc::csv_error& error) {
                return std::unexpected(csv_error(path, error.what()));
            }
        }
    }

    return prices;
}

} // namespace sotcc
