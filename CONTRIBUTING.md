# Contributing

## Code Style

C++ is formatted with clang-format 19 (the `.clang-format` at the repo root
is OBS Studio's) and CMake with gersemi. Both run in CI on every pull
request.

```bash
./build-aux/run-clang-format --check
./build-aux/run-gersemi --check
```

Drop `--check` to rewrite files in place. The runner insists on clang-format
19.1.x exactly; newer or older versions are rejected so local and CI output
match.

## OBS Versions

OBS 30 and newer are supported; the floor is the `obs_frontend_add_dock_by_id`
API. The prebuilt dependencies the Windows and macOS presets download are
pinned in `buildspec.json`. CMake 3.28+ is required on every platform.

## Developer Setup

```bash
git clone https://github.com/nathan-v/vertical-stats-for-obs.git
cd vertical-stats-for-obs
cmake --preset macos            # or windows-x64, ubuntu-x86_64
cmake --build --preset macos --config RelWithDebInfo
cmake --install build_macos --config RelWithDebInfo --prefix release
```

The first configure on Windows and macOS downloads the matching OBS and Qt
dependencies and builds `obs-frontend-api` from source; expect it to take a
while. The README lists the Ubuntu apt packages.

## Testing

The plugin is split in two. `src/stats-logic.cpp` holds every calculation
(colour thresholds, unit scaling, bitrate and dropped-frame baselines, the
disk-full estimate, Aitum output-name parsing, recording-path selection) and
depends on nothing but the standard library. `src/vertical-stats.cpp` reads
libobs and paints Qt labels. Put new arithmetic in the logic file and test it.

```bash
cmake -S tests -B build_tests
cmake --build build_tests
ctest --test-dir build_tests --output-on-failure
```

The tests build with any C++17 compiler; no OBS or Qt needed. Pass a name
substring to run a subset; `./build_tests/stats-logic-tests bitrate`. You can
also build them inside the plugin project with `-DENABLE_TESTS=ON`.

Unit tests should be included with every PR that touches the logic. If the PR
is a bug fix, include a regression test as well.

The widget has no automated tests. Verify it by installing the built plugin
into OBS and opening **Docks → Stats (Vertical)**. For anything that touches
the output blocks, check all four states: idle, streaming, recording, and
streaming with Aitum Vertical loaded. Describe what you exercised in the pull
request.

CI (`.github/workflows/ci.yml`) runs the unit tests on Ubuntu, macOS and Windows, and builds the plugin on
all three, on every push and pull request; a change must build clean
(warnings are errors) everywhere.

## Pull Request Process

1. Make sure the clang-format and gersemi checks pass
2. Make sure the unit tests and the CI builds pass on all three platforms
3. Update the README if your change affects usage, install, or build steps
4. Reference any related issues in your PR description
