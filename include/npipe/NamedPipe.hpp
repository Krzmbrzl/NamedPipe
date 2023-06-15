// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file at the root of the
// source tree or at
// <https://github.com/Krzmbrzl/NamedPipe/blob/main/LICENSE>.

#pragma once

#include <atomic>
#include <chrono>
#include <cstddef>
#include <filesystem>
#include <limits>
#include <vector>

#ifdef PIPE_PLATFORM_WINDOWS
#	include <windows.h>
#endif

namespace npipe {


/**
 * Wrapper class around working with NamedPipes. Its main purpose is to abstract away the implementation differences
 * between different platforms (e.g. Windows vs Posix-compliant systems).
 * At the same time it serves as a RAII wrapper.
 */
class NamedPipe {
public:
	/**
	 * Creates a new named pipe at the specified location. If such a pipe (or other file) already exists at the
	 * given location, this function will fail.
	 *
	 * @param pipePath The path at which the pipe shall be created
	 * @returns A NamedPipe object wrapping the newly created pipe
	 */
	[[nodiscard]] static NamedPipe create(std::filesystem::path pipePath);

	/**
	 * Writes a message to the named pipe at the given location
	 *
	 * @param pipePath The path at which the pipe is expected to exist. If the pipe does not exist, the function
	 * will poll for its existence until it times out.
	 * @param message A pointer to the beginning of the message that shall be sent
	 * @param messageSize The size of the message to write
	 * @param timeout How long this function is allowed to take. Note that the timeout is only
	 * respected very roughly (especially on Windows) and should therefore rather be used to specify the general
	 * order of magnitude of the timeout instead of the exact timeout-interval.
	 */
	static void write(std::filesystem::path pipePath, const std::byte *message, std::size_t messageSize,
					  std::chrono::milliseconds timeout = std::chrono::milliseconds(10));

	/**
	 * @returns Whether a named pipe at the given path currently exists
	 */
	[[nodiscard]] static bool exists(const std::filesystem::path &pipePath);



	/**
	 * Creates an empty (invalid) instance
	 */
	NamedPipe() = default;
	~NamedPipe();

	NamedPipe(const NamedPipe &) = delete;
	NamedPipe &operator=(const NamedPipe &) = delete;

	NamedPipe(NamedPipe &&other);
	NamedPipe &operator=(NamedPipe &&other);

	/**
	 * Writes to the named pipe wrapped by this object
	 * @param message A pointer to the beginning of the message to send
	 * @param messageSize The size of the message that shall be sent
	 * @param timeout How long this function is allowed to take. The remarks from NamedPipe::write apply.
	 *
	 * @see Mumble::JsonBridge::NamedPipe::write()
	 */
	void write(const std::byte *message, std::size_t messageSize,
			   std::chrono::milliseconds timeout = std::chrono::milliseconds(10)) const;

	/**
	 * Reads content from the wrapped named pipe. This function will block until there is content available or the
	 * timeout is over. Once started this function will read all available content until EOF in a single block.
	 *
	 * @param timeout How long this function may wait for content. Note that this will not be respected precisely.
	 * Rather this specifies the general order of magnitude of the timeout.
	 * @returns The read content
	 */
	[[nodiscard]] std::vector< std::byte > read_blocking(std::chrono::milliseconds timeout = std::chrono::milliseconds{
															 (std::numeric_limits< unsigned int >::max)() }) const;

	/**
	 * @returns The path of the wrapped named pipe
	 */
	[[nodiscard]] std::filesystem::path getPath() const noexcept;

	/**
	 * Destroys the wrapped named pipe. After having called this function the pipe does no
	 * longer exist in the OS's filesystem and this wrapper will become unusable.
	 *
	 * @note This function is called automatically by the object's destructor
	 * @note Calling this function multiple times is allowed. All but the first invocation are turned into no-opts.
	 */
	void destroy();

	/**
	 * Interrupt any ongoing read or write process.
	 * Note: Once interrupted, the pipe has to be reconstructed before using it again
	 */
	void interrupt();

	/**
	 * @returns Whether this wrapper is currently in a valid state
	 */
	operator bool() const noexcept;

private:
	/**
	 * The path to the wrapped pipe
	 */
	std::filesystem::path m_pipePath;
	mutable std::atomic_bool m_break = false;

#ifdef PIPE_PLATFORM_WINDOWS
	/**
	 * On Windows this holds the handle to the pipe. On other platforms this variable doesn't exist.
	 */
	HANDLE m_handle = INVALID_HANDLE_VALUE;
#endif

	/**
	 * Instantiates this wrapper. On Windows the m_handle member variable has to be set
	 * explicitly after having constructed this object.
	 *
	 * @param The path to the pipe that should be wrapped by this object
	 */
	explicit NamedPipe(const std::filesystem::path &path);
};


} // namespace npipe
