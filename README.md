# NamedPipe

Cross-platform implementation of named pipes. Supported platforms are all Posix flavors (including macOS and Linux) as well as Windows.

## Usage

### CMake

You can use this library from your cmake project as follows:
```cmake
include(FetchContent)

FetchContent_Declare(
    NamedPipe
    GIT_REPOSITORY https://github.com/Krzmbrzl/NamedPipe
    GIT_TAG        v1.0.0
    GIT_SHALLOW    ON
)

FetchContent_MakeAvailable(NamedPipe)

target_link_libraries(yourTarget PRIVATE NamedPipe::NamedPipe)
```

### In code

See the [examples](./examples/) for how to implement basic communication between a reading and a writing end.

