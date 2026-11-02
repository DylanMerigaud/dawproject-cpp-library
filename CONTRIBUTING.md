# Contributing

Bug reports, files that do not load and pull requests are welcome.

## Build

You need a C++17 compiler and CMake 3.16 or newer.

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j2
```

Add `-DDAWPROJECT_WARNINGS_AS_ERRORS=ON` to build the way CI does.

## Test

```sh
ctest --test-dir build --output-on-failure
```

The `xsd_validate` test checks every XML the unit tests wrote against the official schemas in
`spec/`. It needs Python 3 with lxml (`python3 -m pip install lxml`) and is skipped otherwise; CI
always runs it. A change to the model or the serializer should come with a test in `tests/` that
writes the new element, so the schema check covers it.

## Formatting

The code follows `.clang-format` (Google based, 100 columns). Before sending a change:

```sh
clang-format -i include/dawproject/*.hpp src/*.cpp src/*.hpp tests/*.cpp tests/*.hpp examples/*.cpp
```

CI runs `clang-format --dry-run --Werror` on the same files. `.clang-tidy` holds the static
analysis settings.

## A file that does not load

Open an issue with the bug report template and attach the `.dawproject` file, or only its
`project.xml` if the audio cannot be shared (a `.dawproject` is a zip file; unzip it to get
`project.xml`). Say which application wrote it, and paste the `Error` message, including whether
you parsed in strict mode. If the file cannot be public, cut it down to the smallest
`project.xml` that still fails.

## Pull requests

Keep a pull request to one change, with its tests, and add a line under `[Unreleased]` in
`CHANGELOG.md`. The XSD files under `spec/` are copies of Bitwig's and are not edited by hand.
