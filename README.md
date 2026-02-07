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

Now you can run program from the current directory with example config:

```bash
./build/bin/stocc
```

## Usage

Run the program with configuration from the working direcotry:

```bash
stocc
```

Or specify it using the `--config` or `-c` argment:

```bash
stocc --config path_to_toml_config_file
```

To produce help message use `--help` or `-h` argument:

```bash
stocc --help
```

To enable logging specify log level with `--log-level` or `-l` argument:

```bash
stocc --log-level info
```

Available logging levels (trace, debug, info, warn, err, critical, off).

## Configuration

An example config file: `config.toml`

```toml
[main]
input = "./examples/input" # 1
output = "./examples/output" # 2
filename_mask = ['level', 'trade'] # 3
```

1. Input files durectory
2. Output files directory *(optional)*. By default `./output` direcotry created.
3. File name filter *(optional)*. Only files whose names contain at least one of the specified masks are read. If the list is empty, all CSV files are read.

## License

This project is licensed under the MIT License — see `LICENSE` for details.
