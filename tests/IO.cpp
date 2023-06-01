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
#include <queue>
#include <string>
#include <thread>
#include <tuple>

constexpr const char *pipeName = "ioTestPipe";

class IOTest : public ::testing::TestWithParam< std::vector< std::byte > > {
public:
	using ParamPack = std::vector< std::byte >;

	IOTest() : m_pipe(npipe::NamedPipe::create(pipeName)), m_readThread(std::thread(&IOTest::readLoop, this)) {
		EXPECT_TRUE(npipe::NamedPipe::exists(pipeName));
	}

	~IOTest() {
		m_interrupt = true;
		m_pipe.interrupt();
		m_readThread.join();

		EXPECT_EQ(m_receivedMessages.size(), 0) << "There are unexpected left-over messages";
	}

	void readLoop() {
		try {
			while (true) {
				if (m_interrupt) {
					break;
				}

				try {
					std::vector< std::byte > message = m_pipe.read_blocking(std::chrono::seconds(5));

					std::unique_lock< std::mutex > locker(m_queueMutex);
					m_receivedMessages.push(std::move(message));

					m_waiter.notify_all();
				} catch (const npipe::TimeoutException &) {
					FAIL() << "Read-loop timed out";
				}
			}
		} catch (const npipe::InterruptException &) {
			ASSERT_TRUE(m_interrupt) << "Interrupted without having been requested to do so";
		} catch (const npipe::Exception &e) {
			FAIL() << "Read-loop encountered unexpected exception: " << e.what();
		}
	}

	std::vector< std::byte > nextMessage() {
		std::unique_lock< std::mutex > locker(m_queueMutex);

		if (!m_receivedMessages.empty()) {
			std::vector< std::byte > message = std::move(m_receivedMessages.front());
			m_receivedMessages.pop();

			return message;
		}

		// Wait until the message is read
		m_waiter.wait_for(locker, std::chrono::seconds(1), [&]() { return !m_receivedMessages.empty(); });

		if (m_receivedMessages.empty()) {
			throw std::runtime_error("Unable to read another message, but at least one more was expected");
		}

		std::vector< std::byte > message = std::move(m_receivedMessages.front());
		m_receivedMessages.pop();

		return message;
	}

private:
	npipe::NamedPipe m_pipe;
	std::thread m_readThread;
	std::atomic_bool m_interrupt = false;
	std::queue< std::vector< std::byte > > m_receivedMessages;
	std::mutex m_queueMutex;
	std::condition_variable m_waiter;
};


TEST_P(IOTest, io) {
	const std::vector< std::byte > message = GetParam();

	npipe::NamedPipe::write(pipeName, message.data(), message.size(), std::chrono::seconds(1));

	ASSERT_EQ(message, nextMessage());
}


INSTANTIATE_TEST_SUITE_P(
	IO, IOTest,
	::testing::Values(IOTest::ParamPack{ std::byte(42) }, IOTest::ParamPack{ std::byte(42), std::byte(0) },
					  IOTest::ParamPack{ std::byte(0), std::byte(1), std::byte(2), std::byte(255), std::byte(254) }));
