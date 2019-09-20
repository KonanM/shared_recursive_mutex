// Licensed under the MIT License <http://opensource.org/licenses/MIT>.
// SPDX-License-Identifier: MIT
// Copyright (c) 2019 Kinan Mahdi
//
// Permission is hereby  granted, free of charge, to any  person obtaining a copy
// of this software and associated  documentation files (the "Software"), to deal
// in the Software  without restriction, including without  limitation the rights
// to  use, copy,  modify, merge,  publish, distribute,  sublicense, and/or  sell
// copies  of  the Software,  and  to  permit persons  to  whom  the Software  is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE  IS PROVIDED "AS  IS", WITHOUT WARRANTY  OF ANY KIND,  EXPRESS OR
// IMPLIED,  INCLUDING BUT  NOT  LIMITED TO  THE  WARRANTIES OF  MERCHANTABILITY,
// FITNESS FOR  A PARTICULAR PURPOSE AND  NONINFRINGEMENT. IN NO EVENT  SHALL THE
// AUTHORS  OR COPYRIGHT  HOLDERS  BE  LIABLE FOR  ANY  CLAIM,  DAMAGES OR  OTHER
// LIABILITY, WHETHER IN AN ACTION OF  CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE  OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

#include <gtest/gtest.h>
#include <shared_recursive_mutex/shared_recursive_mutex2.hpp>
#include <shared_recursive_mutex/shared_recursive_mutex.hpp>
#include <mutex>
#include <shared_mutex>
#include <thread>
#include <memory>
#include <array>
#include <future>

class ThreadSafeCounter {
public:
	ThreadSafeCounter() = default;

	// Multiple threads/readers can read the counter's value at the same time.
	unsigned int get()  {
		std::shared_lock lock(mutex_);
		return value_;
	}

	// Only one thread/writer can increment/write the counter's value.
	void increment() {
		std::unique_lock lock(mutex_);
		value_++;
	}

	// Only one thread/writer can reset/write the counter's value.
	void reset() {
		std::unique_lock lock(mutex_);
		value_ = 0;
	}

private:
	mtx::shared_recursive_global_mutex& mutex_ = mtx::shared_recursive_global_mutex::instance();
	unsigned int value_ = 0;
};

TEST(shared_recursive_mutex, test_concurrent_access)
{
	ThreadSafeCounter counter;

	auto increment_and_print = [&counter]() {
		for (int i = 0; i < 10; i++) {
			counter.increment();
			std::cout << counter.get() << std::endl;
		}
	};

	std::thread t1(increment_and_print);
	std::thread t2(increment_and_print);
	std::thread t3(increment_and_print);
	
	t1.join();
	t2.join();
	t3.join();

	ASSERT_TRUE(counter.get() == 30);
}

TEST(shared_recursive_mutex, write_guard_before_read)
{
	int counter = 0;
	auto& mutex = mtx::shared_recursive_global_mutex::instance();
	auto increment_and_print = [&]() {
		for (int i = 0; i < 10; i++) {
			std::unique_lock write_guard(mutex);
			counter++;
			std::shared_lock read_guard(mutex);
			std::cout << counter << std::endl;
		}
	};

	std::thread t1(increment_and_print);
	std::thread t2(increment_and_print);
	std::thread t3(increment_and_print);

	t1.join();
	t2.join();
	t3.join();

	ASSERT_TRUE(counter == 30);
}


TEST(shared_recursive_mutex, read_guard_before_write)
{
	int counter = 0;
	auto& mutex = mtx::shared_recursive_global_mutex::instance();
	auto increment_and_print = [&]() {
		for (int i = 0; i < 10; i++) {
			std::shared_lock read_guard(mutex);
			std::cout << counter << std::endl;
			std::unique_lock write_guard(mutex);
			counter++;
			// Note: Writing to std::cout actually needs to be synchronized as well
			// by another std::mutex. This has been omitted to keep the example small.
		}
	};

	std::thread t1(increment_and_print);
	std::thread t2(increment_and_print);
	std::thread t3(increment_and_print);

	t1.join();
	t2.join();
	t3.join();

	ASSERT_TRUE(counter == 30);
}
constexpr int numThreads = 20;
constexpr int numIterations = 1000;
TEST(shared_recursive_mutex, poor_mans_fuzzing)
{
	int counter = 0;
	auto& mutex = mtx::shared_recursive_global_mutex::instance();
	
	auto increment_and_print = [&counter, &mutex]() {
		for (int i = 0; i < numIterations; i++) {
			if (i % 4 == 0)
			{
				std::unique_lock write_guard(mutex);
				counter++;
				std::shared_lock read_guard(mutex);
				std::cout << counter << std::endl;
			}
			else if (i % 3 == 0)
			{
				std::shared_lock read_guard(mutex);
				std::cout << counter << std::endl;
				std::unique_lock write_guard(mutex);
				counter++;
			}
			else if (i % 2 == 0)
			{
				std::unique_lock write_guard(mutex);
				counter--;
				std::unique_lock write_guard2(mutex);
				counter+=2;
				std::shared_lock read_guard(mutex);
				std::cout << counter << std::endl;
			}
			else
			{
				std::shared_lock read_guard(mutex);
				std::shared_lock read_guard2(mutex);
				std::cout << counter << std::endl;
				std::unique_lock write_guard(mutex);
				counter++;
			}
		}
	};
	std::array<std::future<void>, numThreads> threads;
	for (auto& future : threads)
		future = std::async(std::launch::async, increment_and_print);
	
	for (auto& future : threads)
		future.get();

	ASSERT_TRUE(numThreads * numIterations == counter);
}