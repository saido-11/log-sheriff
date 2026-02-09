# log-sheriff

`log-sheriff` is a fast C++20 CLI for streaming large log files and producing compact summaries.

## Motivation

Production logs can be multi-GB and noisy. `grep` is useful for spot checks, but it does not give a compact
overview of what is happening across large files.

`log-sheriff` is built for first-pass triage:
- Streams line-by-line, so memory stays effectively constant with file size
- Filters matches by substring and/or log level
- Surfaces top recurring patterns by normalizing lines

Normalization currently:
- Trims leading/trailing whitespace
- Collapses repeated whitespace to a single space
- Replaces digit runs with `<num>` to group similar lines

It runs as a single compiled binary with no external runtime dependencies.

## Project layout

- `include/log_sheriff/`: public headers
- `src/`: CLI + implementation
- `tests/`: unit tests (Catch2)
- `samples/`: sample logs
- `.github/workflows/`: CI pipeline

## Build

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DLOG_SHERIFF_BUILD_TESTS=ON
cmake --build build --parallel
```

## Test

```bash
ctest --test-dir build --output-on-failure
```

## Usage

### Basic summary

```bash
./build/log-sheriff summarize samples/sample.log
```

### Filter by substring

```bash
./build/log-sheriff summarize samples/sample.log --contains "database timeout"
```

### Filter by log level (case-insensitive)

```bash
./build/log-sheriff summarize samples/sample.log --level ERROR
```

### Show top 3 lines in JSON

```bash
./build/log-sheriff summarize samples/sample.log --top 3 --json
```

### Multiple files

```bash
./build/log-sheriff summarize samples/sample.log samples/sample.log --level warn
```

## Example output

Command:

```bash
./build/log-sheriff summarize samples/sample.log --top 3
```

Output:

```text
Files processed: 1
Total lines:    8
Matched lines:  8
Matched by level: error=2 warn=2 info=3 debug=1

Top lines:
Rank  Count  Normalized line
1     2      <num>-<num>-<num>T<num>:<num>:<num>Z ERROR database timeout shard=<num> retries=<num>
2     2      <num>-<num>-<num>T<num>:<num>:<num>Z INFO request completed method=GET path=/health status=<num> latency_ms=<num>
3     2      <num>-<num>-<num>T<num>:<num>:<num>Z WARN request completed method=POST path=/api/v<num>/orders status=<num> latency_ms=<num>
```

## Command reference

`log-sheriff summarize <files...> [options]`

Options:
- `--contains <substring>`: optional substring filter
- `--level <error|warn|info|debug>`: optional case-insensitive level filter
- `--top <N>`: number of top normalized lines to show (default: `10`)
- `--json`: print JSON output instead of table output

## Notes on large files

`log-sheriff` processes each input file as a stream using `std::getline` over `std::ifstream`, so memory usage does not scale with total file size. This makes it suitable for multi-GB logs.

## Roadmap

- [ ] `--since` / `--until` time filtering for ISO timestamps
- [ ] Better normalization: UUIDs/hex/request IDs to `<id>`
- [ ] `--csv` output
- [ ] Optional `index` command for repeated queries
