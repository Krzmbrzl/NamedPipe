# Use of this source code is governed by a BSD-style license
# that can be found in the LICENSE file at the root of the
# source tree or at
# <https://github.com/Krzmbrzl/NamedPipe/blob/main/LICENSE>.

include(FetchContent)
include(GoogleTest)

get_compiler_flags(
	DISABLE_DEFAULT_FLAGS
	DISABLE_ALL_WARNINGS
	OUTPUT_VARIABLE DISABLE_WARNINGS_FLAG
)

FetchContent_MakeAvailable(googletest)

target_compile_options(gtest PRIVATE ${DISABLE_WARNINGS_FLAG})
target_compile_options(gmock PRIVATE ${DISABLE_WARNINGS_FLAG})

add_executable(npipe_tests
	IO.cpp
	Meta.cpp
)

target_link_libraries(npipe_tests PRIVATE gtest_main gmock NamedPipe::NamedPipe)
gtest_discover_tests(npipe_tests)
