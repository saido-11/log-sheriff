# Contributing

Thanks for contributing to `log-sheriff`.

## Local setup

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DLOG_SHERIFF_BUILD_TESTS=ON
cmake --build build --parallel
ctest --test-dir build --output-on-failure
```

## Guidelines

- Keep it streaming: do not introduce full-file loading behavior.
- Preserve CLI behavior unless there is a clear bug fix or a documented feature addition.
- Add or update tests for behavior changes.
- Keep dependencies minimal and avoid external runtime dependencies.
- Prefer small, focused pull requests with clear commit messages.

## Style

- Use C++20.
- Run formatting with the repository `.clang-format` profile.
- Keep comments concise and focused on non-obvious logic.
