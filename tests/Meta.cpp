// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file at the root of the
// source tree or at
// <https://github.com/Krzmbrzl/NamedPipe/blob/main/LICENSE>.

#include "npipe/Exception.hpp"
#include "npipe/InterruptException.hpp"
#include "npipe/NamedPipe.hpp"
#include "npipe/TimeoutException.hpp"

#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstddef>
#include <mutex>
#include <thread>
#include <vector>

constexpr const char *metaPipeName = "metaTestPipe";

static const std::vector< std::byte > sampleMessage = { std::byte(0), std::byte(1), std::byte(1), std::byte(2),
														std::byte(3), std::byte(5), std::byte(8) };

// Note: Mutexes are used to ensure the desired order of operations

TEST(NamedPipe, first_read_then_write) {
	std::mutex mutex;
	std::condition_variable waiter;
	std::unique_lock< std::mutex > writeLocker(mutex);
	std::atomic_bool threadStarted = false;

	std::thread readThread([&]() {
		std::unique_lock< std::mutex > readLocker(mutex);
		npipe::NamedPipe pipe = npipe::NamedPipe::create(metaPipeName);

		threadStarted = true;
		readLocker.unlock();
		waiter.notify_all();
		std::vector< std::byte > message = pipe.read_blocking(std::chrono::seconds(6));

		ASSERT_EQ(message, sampleMessage);
	});

	ASSERT_TRUE(waiter.wait_for(writeLocker, std::chrono::seconds(5), [&]() { return threadStarted.load(); }));

	// Sleep an additional second to ensure that the reader is fully active
	std::this_thread::sleep_for(std::chrono::seconds(1));

	npipe::NamedPipe::write(metaPipeName, sampleMessage.data(), sampleMessage.size(), std::chrono::seconds(5));

	readThread.join();
}

TEST(NamedPipe, first_write_then_read) {
	std::mutex mutex;
	std::condition_variable waiter;
	std::unique_lock< std::mutex > readLocker(mutex);
	npipe::NamedPipe pipe          = npipe::NamedPipe::create(metaPipeName);
	std::atomic_bool threadStarted = false;

	std::thread writeThread([&]() {
		threadStarted = true;
		waiter.notify_all();
		npipe::NamedPipe::write(metaPipeName, sampleMessage.data(), sampleMessage.size(), std::chrono::seconds(5));
	});

	ASSERT_TRUE(waiter.wait_for(readLocker, std::chrono::seconds(5), [&]() { return threadStarted.load(); }));

	// Sleep an additional second to ensure that the writer is fully active
	std::this_thread::sleep_for(std::chrono::seconds(1));

	std::vector< std::byte > message = pipe.read_blocking(std::chrono::seconds(6));

	ASSERT_EQ(message, sampleMessage);

	writeThread.join();
}

TEST(NamedPipe, read_timeout) {
	npipe::NamedPipe pipe = npipe::NamedPipe::create(metaPipeName);

	std::vector< std::byte > dummy;

	ASSERT_THROW(dummy = pipe.read_blocking(std::chrono::milliseconds(500)), npipe::TimeoutException);
}

TEST(NamedPipe, write_timeout) {
	ASSERT_THROW(npipe::NamedPipe::write(metaPipeName, sampleMessage.data(), sampleMessage.size(),
										 std::chrono::milliseconds(500)),
				 npipe::TimeoutException);
}

TEST(NamedPipe, interrupt) {
	std::mutex mutex;
	std::condition_variable waiter;
	std::unique_lock< std::mutex > locker(mutex);
	std::atomic_bool threadStarted = false;

	npipe::NamedPipe pipe = npipe::NamedPipe::create(metaPipeName);
	std::thread thread([&]() {
		std::vector< std::byte > dummy;
		threadStarted = true;
		waiter.notify_all();
		ASSERT_THROW(dummy = pipe.read_blocking(std::chrono::seconds(5)), npipe::InterruptException);
	});

	ASSERT_TRUE(waiter.wait_for(locker, std::chrono::seconds(5), [&]() { return threadStarted.load(); }));

	// Wait an extra second to ensure the read operation has started
	std::this_thread::sleep_for(std::chrono::seconds(1));

	pipe.interrupt();

	thread.join();
}
