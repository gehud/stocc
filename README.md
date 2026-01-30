# Stocс — Stock exchange calculator

A small command-line utility to analyze stock exchange median price.

## Features
- Compute and print the median value
- Minimal, fast C++ implementation (C++23)

## Requirements
- C++23-compatible compiler
- CMake 3.23 or newer

## Build
From the project root directory:

```bash
cmake -S . -B build
cmake --build build --config Release
```

The resulting executable will be in `build/bin/stocc`.

## Usage
Run the program with the default configuration:

```bash
stocc
```

Or specify it using the ```-config``` or ```-cfg``` argment:

```bash
stocc -cfg path_to_toml_config_file
```

## License
This project is licensed under the MIT License — see `LICENSE` for details.
