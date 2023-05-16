# Use of this source code is governed by a BSD-style license
# that can be found in the LICENSE file at the root of the
# source tree or at
# <https://github.com/Krzmbrzl/NamedPipe/blob/main/LICENSE>.

add_library(named_pipe
	STATIC
		NamedPipe.cpp
)

if (CMAKE_SOURCE_DIR STREQUAL PROJECT_SOURCE_DIR)
	set(SYSTEM_PROP "")
else()
	# Used as sub-project
	set(SYSTEM_PROP "SYSTEM")
endif()

target_include_directories(named_pipe ${SYSTEM_PROP} PUBLIC "${PROJECT_SOURCE_DIR}/include")

if (UNIX)
	target_compile_definitions(named_pipe PUBLIC PIPE_PLATFORM_UNIX)
elseif (WIN32)
	target_compile_definitions(named_pipe PUBLIC PIPE_PLATFORM_WINDOWS)
else()
	message(FATAL_ERROR "Unsupported platform")
endif()

add_library(NamedPipe::NamedPipe ALIAS named_pipe)
