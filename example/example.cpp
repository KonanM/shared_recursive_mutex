
// Licensed under the MIT License <http://opensource.org/licenses/MIT>.
// SPDX-License-Identifier: MIT

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
		for (int i = 0; i < 1000000; i++) {
			std::shared_lock read_guard(mutex);
			if (i % 20 == 0)
			{
				//read lock will automatically upgrade to a write lock
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

	std::cout << "mtx::shared_recursive_global_mutex: " << time_span.count() << " seconds \n";

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
		for (int i = 0; i < 1000000; i++)
		{
			//doing the same is much more complicated for a shared mutex
			auto read_guard = createReadLock();
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

	std::cout << "std::shared_mutex" << time_span2.count() << " seconds.";

}