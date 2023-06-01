// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file at the root of the
// source tree or at
// <https://github.com/Krzmbrzl/NamedPipe/blob/main/LICENSE>.

#include "npipe/NamedPipe.hpp"
#include "npipe/InterruptException.hpp"
#include "npipe/PipeException.hpp"
#include "npipe/TimeoutException.hpp"

#ifdef PIPE_PLATFORM_UNIX
#	include <fcntl.h>
#	include <unistd.h>
#	include <poll.h>
#	include <sys/stat.h>
#endif

#ifdef PIPE_PLATFORM_WINDOWS
#	include <windows.h>
#endif

#include <array>
#include <cassert>
#include <chrono>
#include <fstream>
#include <iostream>
#include <thread>


namespace npipe {

/**
 * RAII wrapper for file handles
 */
template< typename handle_t, typename close_handle_function_t, handle_t invalid_handle, int successCode >
class FileHandleWrapper {
private:
	handle_t m_handle;
	close_handle_function_t m_closeFunc;

public:
	explicit FileHandleWrapper(handle_t handle, close_handle_function_t closeFunc)
		: m_handle(handle), m_closeFunc(closeFunc) {}

	explicit FileHandleWrapper() : m_handle(invalid_handle) {}

	~FileHandleWrapper() {
		if (m_handle != invalid_handle) {
			if (m_closeFunc(m_handle) != successCode) {
				std::cerr << "Failed at closing guarded handle" << std::endl;
			}
		}
	}

	FileHandleWrapper(const FileHandleWrapper &) = delete;
	FileHandleWrapper &operator=(const FileHandleWrapper &) = delete;

	// Be on the safe-side and delete move-constructor as it isn't explicitly implemented
	FileHandleWrapper(FileHandleWrapper &&) = delete;

	// Move-assignment operator
	FileHandleWrapper &operator=(FileHandleWrapper &&other) {
		// Move handle
		m_handle       = other.m_handle;
		other.m_handle = invalid_handle;

		// Copy over the close-func in case this instance was created using the default
		// constructor in which case the close-func is not specified.
		m_closeFunc = other.m_closeFunc;

		return *this;
	}

	handle_t &get() { return m_handle; }

	operator handle_t() { return m_handle; }

	bool operator==(handle_t other) { return m_handle == other; }
	bool operator!=(handle_t other) { return m_handle != other; }
	operator bool() { return m_handle != invalid_handle; }
};


constexpr std::chrono::milliseconds PIPE_WAIT_INTERVAL(1);
constexpr std::chrono::milliseconds PIPE_WRITE_WAIT_INTERVAL(1);
constexpr std::size_t PIPE_BUFFER_SIZE = 256;


NamedPipe::NamedPipe(const std::filesystem::path &path) : m_pipePath(path) {
}

NamedPipe::~NamedPipe() {
	destroy();
}

std::filesystem::path NamedPipe::getPath() const noexcept {
	return m_pipePath;
}

void NamedPipe::write(const std::byte *message, std::size_t messageSize, std::chrono::milliseconds timeout) const {
	assert(message);
	write(m_pipePath, message, messageSize, timeout);
}

NamedPipe::operator bool() const noexcept {
	return !m_pipePath.empty();
}

void NamedPipe::interrupt() {
	m_break.store(true);
}


#ifdef PIPE_PLATFORM_UNIX
using handle_t = FileHandleWrapper< int, int (*)(int), -1, 0 >;

NamedPipe NamedPipe::create(std::filesystem::path pipePath) {
	// Create fifo that only the same user can read & write
	if (mkfifo(pipePath.c_str(), S_IRUSR | S_IWUSR) != 0) {
		throw PipeException< int >(errno, "Create");
	}

	return NamedPipe(pipePath);
}

void NamedPipe::write(std::filesystem::path pipePath, const std::byte *message, std::size_t messageSize,
					  std::chrono::milliseconds timeout, bool waitUntilWritten) {
	assert(message);

	// Wait until the target pipe is found or until the provided timeout has elapsed
	handle_t handle;
	do {
		handle = handle_t(::open(pipePath.c_str(), O_WRONLY | O_NONBLOCK), &::close);

		if (!handle) {
			if (timeout > PIPE_WRITE_WAIT_INTERVAL) {
				timeout -= PIPE_WRITE_WAIT_INTERVAL;
				std::this_thread::sleep_for(PIPE_WRITE_WAIT_INTERVAL);
			} else {
				throw TimeoutException();
			}
		}
	} while (!handle);

	// Once the pipe exists, write the desired content to it
	if (::write(handle, message, messageSize) < 0) {
		throw PipeException< int >(errno, "Write");
	}

	(void) waitUntilWritten;
}

bool NamedPipe::exists(const std::filesystem::path &pipePath) {
	// We don't explicitly check whether the given path is a pipe or a regular file
	return std::filesystem::exists(pipePath);
}

std::vector< std::byte > NamedPipe::read_blocking(std::chrono::milliseconds timeout) const {
	std::vector< std::byte > message;

	// At this point, we are assuming that the pipe already exists
	handle_t handle(::open(m_pipePath.c_str(), O_RDONLY | O_NONBLOCK), &::close);

	if (handle == -1) {
		throw PipeException< int >(errno, "Open");
	}

	pollfd pollData = { handle, POLLIN, -1 };
	while (::poll(&pollData, 1, std::chrono::duration_cast< std::chrono::milliseconds >(PIPE_WAIT_INTERVAL).count())
			   != -1
		   && !(pollData.revents & POLLIN)) {
		// Check if the thread has been interrupted
		if (m_break) {
			throw InterruptException();
		}

		if (timeout > PIPE_WAIT_INTERVAL) {
			timeout -= PIPE_WAIT_INTERVAL;
		} else {
			throw TimeoutException();
		}
	}

	std::array< std::byte, PIPE_BUFFER_SIZE > buffer;

	ssize_t readBytes;
	while ((readBytes = ::read(handle, buffer.data(), PIPE_BUFFER_SIZE)) > 0) {
		message.insert(message.end(), buffer.begin(), buffer.begin() + readBytes);
	}

	// 0 Means there is no more input, negative numbers indicate errors
	// If the error simply is EAGAIN this means that the message has been read completely
	// and a request for further data would block (since atm there is no more data available).
	if (readBytes == -1 && errno != EAGAIN) {
		throw PipeException< int >(errno, "Read");
	}

	return message;
}

NamedPipe::NamedPipe(NamedPipe &&other) : m_pipePath(std::move(other.m_pipePath)) {
	other.m_pipePath.clear();
}

NamedPipe &NamedPipe::operator=(NamedPipe &&other) {
	m_pipePath = std::move(other.m_pipePath);

	other.m_pipePath.clear();

	return *this;
}

void NamedPipe::destroy() {
	m_break.store(true);

	if (!m_pipePath.empty()) {
		std::error_code errorCode;
		std::filesystem::remove(m_pipePath, errorCode);

		if (errorCode) {
			std::cerr << "Failed at deleting pipe-object: " << errorCode << std::endl;
		}

		m_pipePath.clear();
	}
}
#endif // PIPE_PLATFORM_UNIX

#ifdef PIPE_PLATFORM_WINDOWS
using handle_t = FileHandleWrapper< HANDLE, decltype(&CloseHandle), INVALID_HANDLE_VALUE, true >;

void waitOnAsyncIO(HANDLE handle, LPOVERLAPPED overlappedPtr, std::chrono::milliseconds &timeout) {
	constexpr std::chrono::milliseconds pendingWaitInterval(10);

	DWORD transferedBytes;
	BOOL result;
	// Explicit comparison to FALSE in order to avoid warning C4706
	while ((result = GetOverlappedResult(handle, overlappedPtr, &transferedBytes, FALSE)) == FALSE
		   && GetLastError() == ERROR_IO_INCOMPLETE) {
		if (timeout > pendingWaitInterval) {
			timeout -= pendingWaitInterval;
		} else {
			throw TimeoutException();
		}

		std::this_thread::sleep_for(pendingWaitInterval);
	}

	if (!result) {
		throw PipeException< DWORD >(GetLastError(), "Waiting for pending IO");
	}
}

NamedPipe NamedPipe::create(std::filesystem::path pipePath) {
	if (pipePath.parent_path().empty()) {
		pipePath = std::filesystem::path("\\\\.\\pipe") / pipePath;
	}

	assert(pipePath.parent_path() == "\\\\.\\pipe");

	HANDLE pipeHandle = CreateNamedPipe(pipePath.string().c_str(),
										PIPE_ACCESS_INBOUND | FILE_FLAG_OVERLAPPED | FILE_FLAG_FIRST_PIPE_INSTANCE,
										PIPE_TYPE_BYTE | PIPE_WAIT,
										1,                // # of allowed pipe instances
										PIPE_BUFFER_SIZE, // Initial size of outbound buffer
										PIPE_BUFFER_SIZE, // Initial size of inbound buffer
										0,                // Use default wait time
										NULL              // Use default security attributes
	);

	if (pipeHandle == INVALID_HANDLE_VALUE) {
		throw PipeException< DWORD >(GetLastError(), "Create");
	}

	NamedPipe pipe(pipePath);
	pipe.m_handle = pipeHandle;

	return pipe;
}

void NamedPipe::write(std::filesystem::path pipePath, const std::byte *message, std::size_t messageSize,
					  std::chrono::milliseconds timeout, bool waitUntilWritten) {
	assert(message);

	if (pipePath.parent_path().empty()) {
		pipePath = std::filesystem::path("\\\\.\\pipe") / pipePath;
	}

	assert(pipePath.parent_path() == "\\\\.\\pipe");

	// Wait until the target named pipe is available
	while (true) {
		// We can't use a timeout of 0 as this would be the special value NMPWAIT_USE_DEFAULT_WAIT causing
		// the function to use a default wait-time
		if (!WaitNamedPipe(pipePath.string().c_str(), 1)) {
			if (GetLastError() == ERROR_FILE_NOT_FOUND || GetLastError() == ERROR_SEM_TIMEOUT) {
				if (timeout > PIPE_WRITE_WAIT_INTERVAL) {
					timeout -= PIPE_WRITE_WAIT_INTERVAL;
				} else {
					throw TimeoutException();
				}

				// Decrease wait interval by 1ms as this is the timeout we have waited on the pipe above already
				static_assert(PIPE_WRITE_WAIT_INTERVAL.count() >= 1);
				std::this_thread::sleep_for(PIPE_WRITE_WAIT_INTERVAL - std::chrono::milliseconds(1));
			} else {
				throw PipeException< DWORD >(GetLastError(), "WaitNamedPipe");
			}
		} else {
			break;
		}
	}

	handle_t handle(CreateFile(pipePath.string().c_str(),
							   GENERIC_WRITE,        // Access mode
							   0,                    // Share mode
							   NULL,                 // Security attributes
							   OPEN_EXISTING,        // Expect to open an existing "file"
							   FILE_FLAG_OVERLAPPED, // Use overlapped mode
							   NULL                  // Template
							   ),
					&CloseHandle);

	if (!handle) {
		throw PipeException< DWORD >(GetLastError(), "Open for write");
	}

	// Actually write to the pipe
	OVERLAPPED overlapped;
	memset(&overlapped, 0, sizeof(OVERLAPPED));
	if (!WriteFile(handle, message, static_cast< DWORD >(messageSize), NULL, &overlapped)) {
		if (GetLastError() == ERROR_IO_PENDING) {
			if (waitUntilWritten) {
				waitOnAsyncIO(handle, &overlapped, timeout);
			}
		} else {
			throw PipeException< DWORD >(GetLastError(), "Write");
		}
	}
}

// Implementation from https://stackoverflow.com/a/66588424/3907364
bool NamedPipe::exists(const std::filesystem::path &pipePath) {
	std::string pipeName = pipePath.string();
	if ((pipeName.size() < 10) || (pipeName.compare(0, 9, "\\\\.\\pipe\\") != 0)
		|| (pipeName.find('\\', 9) != std::string::npos)) {
		// This can't be a pipe, so it also can't exist
		return false;
	}
	pipeName.erase(0, 9);

	WIN32_FIND_DATA fd;
	DWORD dwErrCode;

	HANDLE hFind = FindFirstFileA("\\\\.\\pipe\\*", &fd);
	if (hFind == INVALID_HANDLE_VALUE) {
		dwErrCode = GetLastError();
	} else {
		do {
			if (pipeName == fd.cFileName) {
				FindClose(hFind);
				return true;
			}
		} while (FindNextFileA(hFind, &fd));

		dwErrCode = GetLastError();
		FindClose(hFind);
	}

	if ((dwErrCode != ERROR_FILE_NOT_FOUND) && (dwErrCode != ERROR_NO_MORE_FILES)) {
		throw PipeException< DWORD >(dwErrCode, "CheckExistance");
	}

	return false;
}

void disconnectAndReconnect(HANDLE pipeHandle, LPOVERLAPPED overlappedPtr, bool disconnectFirst,
							std::chrono::milliseconds &timeout) {
	if (disconnectFirst) {
		if (!DisconnectNamedPipe(pipeHandle)) {
			throw PipeException< DWORD >(GetLastError(), "Disconnect");
		}
	}

	if (!ConnectNamedPipe(pipeHandle, overlappedPtr)) {
		switch (GetLastError()) {
			case ERROR_IO_PENDING:
				// Wait for async IO operation to complete
				waitOnAsyncIO(pipeHandle, overlappedPtr, timeout);

				return;

			case ERROR_NO_DATA:
			case ERROR_PIPE_CONNECTED:
				// These error codes mean that there is a client connected already.
				// In theory ERROR_NO_DATA means that the client has closed its handle
				// to the pipe already but it seems that we can read from the pipe just fine
				// so we count this as a success.
				return;
			default:
				throw PipeException< DWORD >(GetLastError(), "Connect");
		}
	}
}

std::vector< std::byte > NamedPipe::read_blocking(std::chrono::milliseconds timeout) const {
	std::vector< std::byte > message;

	OVERLAPPED overlapped;
	memset(&overlapped, 0, sizeof(OVERLAPPED));

	handle_t eventHandle(CreateEvent(NULL, TRUE, TRUE, NULL), &CloseHandle);
	overlapped.hEvent = eventHandle;

	// Connect to pipe
	disconnectAndReconnect(m_handle, &overlapped, false, timeout);

	// Reset overlapped structure
	memset(&overlapped, 0, sizeof(OVERLAPPED));
	overlapped.hEvent = eventHandle;

	std::array< std::byte, PIPE_BUFFER_SIZE > buffer;

	// Loop until we explicitly break from it (because we're done reading)
	while (true) {
		DWORD readBytes = 0;
		BOOL success    = ReadFile(m_handle, buffer.data(), PIPE_BUFFER_SIZE, &readBytes, &overlapped);
		if (!success && GetLastError() == ERROR_IO_PENDING) {
			// Wait for the async IO to complete (note that the thread can't be
			// interrupted while waiting this way)
			success = GetOverlappedResult(m_handle, &overlapped, &readBytes, TRUE);

			if (!success && GetLastError() != ERROR_BROKEN_PIPE) {
				throw PipeException< DWORD >(GetLastError(), "Overlapped waiting");
			}
		}

		if (!success && message.size() > 0) {
			// We have already read some data -> assume that we reached the end of it
			break;
		}

		if (success) {
			message.insert(message.end(), buffer.begin(), buffer.begin() + readBytes);

			if (readBytes < PIPE_BUFFER_SIZE) {
				// It seems like we read the complete message
				break;
			}
		} else {
			switch (GetLastError()) {
				case ERROR_BROKEN_PIPE:
					// "Un-break" pipe

					// Reset overlapped structure
					memset(&overlapped, 0, sizeof(OVERLAPPED));
					overlapped.hEvent = eventHandle;

					disconnectAndReconnect(m_handle, &overlapped, true, timeout);

					// Reset overlapped structure
					memset(&overlapped, 0, sizeof(OVERLAPPED));
					overlapped.hEvent = eventHandle;
					break;
				case ERROR_PIPE_LISTENING:
					break;
				default:
					throw PipeException< DWORD >(GetLastError(), "Read");
			}

			if (timeout > PIPE_WAIT_INTERVAL) {
				timeout -= PIPE_WAIT_INTERVAL;
			} else {
				throw TimeoutException();
			}

			std::this_thread::sleep_for(PIPE_WAIT_INTERVAL);
		}
	}

	DisconnectNamedPipe(m_handle);

	return message;
}

NamedPipe::NamedPipe(NamedPipe &&other) : m_pipePath(std::move(other.m_pipePath)), m_handle(other.m_handle) {
	other.m_pipePath.clear();
	other.m_handle = INVALID_HANDLE_VALUE;
}

NamedPipe &NamedPipe::operator=(NamedPipe &&other) {
	m_pipePath = std::move(other.m_pipePath);
	m_handle   = other.m_handle;
	m_break.store(other.m_break.load());

	other.m_break.store(true);
	other.m_pipePath.clear();
	other.m_handle = INVALID_HANDLE_VALUE;

	return *this;
}

void NamedPipe::destroy() {
	m_break.store(true);

	if (!m_pipePath.empty()) {
		if (!CloseHandle(m_handle)) {
			std::cerr << "Failed at closing pipe handle: " << GetLastError() << std::endl;
		}

		m_pipePath.clear();
		m_handle = INVALID_HANDLE_VALUE;
	}
}
#endif

} // namespace npipe
