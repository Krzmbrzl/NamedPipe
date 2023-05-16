// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file at the root of the
// source tree or at
// <https://github.com/Krzmbrzl/NamedPipe/blob/main/LICENSE>.

#include <npipe/Exception.hpp>
#include <npipe/InterruptException.hpp>
#include <npipe/NamedPipe.hpp>
#include <npipe/TimeoutException.hpp>

#include <atomic>
#include <chrono>
#include <csignal>
#include <cstddef>
#include <iostream>
#include <vector>

std::atomic_bool interrupt = false;

void interruptHandler(int) {
	interrupt = true;
}

int main() {
	signal(SIGINT, interruptHandler);

	npipe::NamedPipe pipe = npipe::NamedPipe::create("testPipe");

	try {
		while (true) {
			if (interrupt.load()) {
				break;
			}

			try {
				std::vector< std::byte > message = pipe.read_blocking(std::chrono::seconds(1));

				std::cout << "Received: '" << reinterpret_cast< const char * >(message.data()) << "'" << std::endl;
			} catch (const npipe::TimeoutException &) {
				std::cout << "Didn't receive any data within the last second" << std::endl;
			}
		}
	} catch (const npipe::InterruptException &) {
		std::cout << "Read pipe got interrupted" << std::endl;
	} catch (const npipe::Exception &e) {
		std::cout << "[ERROR]: " << e.what() << std::endl;
	}
}
