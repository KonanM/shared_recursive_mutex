
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
//
#include <shared_recursive_mutex/shared_recursive_mutex2.hpp>
#include <shared_recursive_mutex/shared_recursive_mutex.hpp>
#include <mutex>
#include <shared_mutex>
#include <thread>
#include <memory>
#include <array>
#include <future>
#include <iostream>


int main()
{
	int counter = 0;
	mtx::shared_recursive_global_mutex& mutex = mtx::shared_recursive_global_mutex::instance();
	auto increment_and_print = [&counter, &mutex]() {
		for (int i = 0; i < 10000000; i++) {
			
			if (i % 20 == 0)
			{
				std::unique_lock write_guard(mutex);
				counter++;
			}
		}
	};
	using namespace std::chrono;
	
	std::array<std::future<void>, 20> threads;
	high_resolution_clock::time_point t1 = high_resolution_clock::now();
	for (auto& future : threads)
		future = std::async(std::launch::async, increment_and_print);

	for (auto& future : threads)
		future.get();

	high_resolution_clock::time_point t2 = high_resolution_clock::now();

	duration<double> time_span = duration_cast<duration<double>>(t2 - t1);

	std::cout << "shared_recursive_global_mutex: " << time_span.count() << " seconds \n";


	mtx::shared_recursive_mutex mutex2;

	auto increment_and_print2 = [&counter, &mutex2]() {
		
		for (int i = 0; i < 10000000; i++) {
			std::shared_lock read_guard(mutex2);
			//std::cout << counter << "\n";
			if (i % 20 == 0)
			{
				std::unique_lock write_guard(mutex2);
				counter++;
			}
		}
	};
	using namespace std::chrono;

	high_resolution_clock::time_point t5 = high_resolution_clock::now();
	for (auto& future : threads)
		future = std::async(std::launch::async, increment_and_print2);

	for (auto& future : threads)
		future.get();

	high_resolution_clock::time_point t6 = high_resolution_clock::now();

	duration<double> time_span3 = duration_cast<duration<double>>(t6 - t5);

	std::cout << "shared_recursive_global_mutex: " << time_span3.count() << " seconds \n";


	std::shared_mutex sharedMutex;
	struct ReadLockFromWriteLock
	{
		ReadLockFromWriteLock(std::shared_lock<std::shared_mutex>& readGuard)
			: m_readGuard(std::addressof(readGuard))
			, m_writeGuard(*(readGuard.mutex()), std::defer_lock)
		{
			m_readGuard->unlock();
			m_writeGuard.lock();
		}
		~ReadLockFromWriteLock()
		{
			m_writeGuard.unlock();
			m_readGuard->lock();
		}
		std::shared_lock<std::shared_mutex>* m_readGuard;
		std::unique_lock<std::shared_mutex> m_writeGuard;
	};
	auto createReadLock  = [&sharedMutex]() { return std::shared_lock(sharedMutex);};
	auto createWriteLock = [&sharedMutex]() { return std::unique_lock(sharedMutex);};
	
	auto increment_and_print_shared_mutex = [&counter, &createReadLock]()
	{
		for (int i = 0; i < 10000000; i++)
		{
			auto read_guard = createReadLock();
			//std::cout << counter << "\n";
			if (i % 20 == 0)
			{
				auto write_guard = ReadLockFromWriteLock(read_guard);
				counter++;
			}
		}
	};

	high_resolution_clock::time_point t3 = high_resolution_clock::now();

	for (auto& future : threads)
		future = std::async(std::launch::async, increment_and_print_shared_mutex);

	for (auto& future : threads)
		future.get();

	high_resolution_clock::time_point t4 = high_resolution_clock::now();

	duration<double> time_span2 = duration_cast<duration<double>>(t4 - t3);

	std::cout << "It took " << time_span2.count() << " seconds.";

}