
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

#pragma once
#include <atomic>
#include <mutex>
#include <thread>
#include <unordered_map>
#include <cassert>

namespace mtx
{
	class shared_recursive_mutex {
	public:
		/**
		 * @brief Constructs the mutex.
		 */
		shared_recursive_mutex() = default;

		/**
		 * @brief Locks the mutex for exclusive write access for this thread.
		 *              Blocks execution as long as write access is not available:
		 *              * other thread has write access
		 *              * other threads try to get write access
		 *              * other threads have read access
		 *
		 *              A thread may call lock repeatedly.
		 *              Ownership will only be released after the thread makes a matching number of calls to unlock.
		 */
		void lock();

		bool try_lock();

		bool try_lock_shared();

		/**
		 * @brief Locks the mutex for sharable read access.
		 *              Blocks execution as long as read access is not available:
		 *              * other thread has write access
		 *              * other threads try to get write access
		 *
		 *              A thread may call lock repeatedly.
		 *              Ownership will only be released after the thread makes a matching number of calls to unlock_shared.
		 */
		void lock_shared();

		/**
		 * @brief Unlocks the mutex for this thread if its level of ownership is 1. Otherwise reduces the level of ownership
		 *              by 1.
		 */
		void unlock();

		/**
		 * @brief Unlocks the mutex for this thread if its level of ownership is 1. Otherwise reduces the level of ownership
		 *              by 1.
		 */
		void unlock_shared();

	private:
		// protects for race conditions of member accesses
		std::mutex m_mtx;
		// reader and writer queues for fast notification of
		std::condition_variable m_read_queue, m_write_queue;
		// the thread id of the current writer thread
		std::thread::id m_writerThreadId;
		// level of recursive write accesses
		uint32_t m_writersOwnership = 0, m_readerOwnershipBeforeUpgrade = 0;
		// level of (recursive) read accesses
		std::unordered_map<std::thread::id, uint32_t> m_readersOwnership;
	};
	void shared_recursive_mutex::lock()
	{
		const auto                   threadId = std::this_thread::get_id();
		std::unique_lock<std::mutex> lock(m_mtx);
		// Increase level of ownership if thread has already exclusive ownership.
		if (m_writerThreadId == threadId)
		{
			++m_writersOwnership;
			return;
		}
		auto readerIt = m_readersOwnership.find(threadId);
		unsigned readersOnThisThread = 0;
		//we have to check if we have to upgrade the lock from reader to writer
		//this is the case when some reader locks are already on this thread
		if (readerIt != m_readersOwnership.end())
		{
			readersOnThisThread = readerIt->second;
			m_readersOwnership.erase(readerIt);
			const bool isWriterWating = m_writersOwnership > 0;
			const bool isLastReader = m_readersOwnership.size() == 0;
			if(isWriterWating && isLastReader)
				m_read_queue.notify_one();
		}
		
		//we have to wait until other writers have finished
		while (m_writersOwnership > 0)
			m_write_queue.wait(lock);

		//we are the first writer thread to 
		if(readersOnThisThread != 0)
			m_readerOwnershipBeforeUpgrade = readersOnThisThread;

		//we indicate that we want to write to reading threads
		m_writerThreadId = threadId;
		m_writersOwnership = 1;

		//we wait until all the readers are finished
		while (m_readersOwnership.size() > 0)
			m_read_queue.wait(lock);    // wait for writing, no readers
	}

	void shared_recursive_mutex::lock_shared()
	{
		auto                         threadId = std::this_thread::get_id();
		std::unique_lock<std::mutex> lock(m_mtx);

		// Increase level of ownership if thread has already exclusive ownership as writer
		if (m_writerThreadId == threadId)
		{
			++m_writersOwnership;
			return;
		}

		// As reader we have to check if our thread already has read ownership, if yes we simply increase it
		auto readersIt = m_readersOwnership.find(threadId);
		if (readersIt != m_readersOwnership.end())
		{
			++(readersIt->second);
			return;
		}
		//if not we have to check if there are any waiting writers - they have priority
		while (m_writersOwnership > 0)
			m_write_queue.wait(lock);

		//now we can be sure there are no writers and we are the first reader on this thread
		m_readersOwnership.emplace(threadId, 1);
	}

	void shared_recursive_mutex::unlock()
	{
		assert(std::this_thread::get_id() == m_writerThreadId);
		{
			std::lock_guard<std::mutex> lock(m_mtx);
			// Decrease writer threads level of ownership if not 1.
			if (m_writersOwnership != 1)
			{
				--m_writersOwnership;
				return;
			}
			
			if (m_readerOwnershipBeforeUpgrade != 0)
			{
				m_readersOwnership[m_writerThreadId] = m_readerOwnershipBeforeUpgrade;
				m_readerOwnershipBeforeUpgrade = 0;
			}

			m_writersOwnership = 0;
			m_writerThreadId = std::thread::id();
		}

		// Reset threads ownership.
		m_write_queue.notify_all();
	}

	void shared_recursive_mutex::unlock_shared()
	{
		const auto                   threadId = std::this_thread::get_id();
		std::unique_lock<std::mutex> lock(m_mtx);
		// decrease level of ownership if thread has already exclusive ownership as writer
		if (m_writerThreadId == threadId)
		{
			--m_writersOwnership;
			assert(m_writersOwnership > 0);
			return;
		}
		auto readerIt = m_readersOwnership.find(threadId);
		assert(readerIt != m_readersOwnership.end());

		// Decrease this reader threads level of ownership by one (if we are not the last one)
		if (readerIt->second != 1)
		{
			--(readerIt->second);
			return;
		}
		//if we are the last reader of this this thread we remove the ownership and check if we 
		//have to notify any waiting writers
		m_readersOwnership.erase(readerIt);
		const bool isWriterWating = m_writersOwnership > 0;
		const bool isLastReader = m_readersOwnership.size() == 0;
		// unlock before notifying, for efficiency
		lock.unlock();
		// notify waiting writer
		if (isWriterWating && isLastReader)
			m_read_queue.notify_one();
	}

	bool shared_recursive_mutex::try_lock()
	{
		auto                        threadId = std::this_thread::get_id();
		std::lock_guard<std::mutex> lock(m_mtx);
		// Increase level of ownership if thread has already exclusive ownership as writer
		if (m_writerThreadId == threadId)
		{
			++m_writersOwnership;
			return true;
		}
		//only lock if there are no readers and writers
		if (m_readersOwnership.size() == 0 && m_writersOwnership == 0)
		{
			m_writerThreadId = threadId;
			m_writersOwnership = 1;
			return true;
		}

		return false;
	}

	bool shared_recursive_mutex::try_lock_shared()
	{
		const auto                  threadId = std::this_thread::get_id();
		std::lock_guard<std::mutex> lock(m_mtx);
		// Increase level of ownership if thread has already exclusive ownership as writer
		if (m_writerThreadId == threadId)
		{
			++m_writersOwnership;
			return true;
		}
		//if there are no writers (or writers waiting) we increase this threads read ownership
		if (m_writersOwnership == 0)
		{
			++m_readersOwnership[threadId];
			return true;
		}

		return false;
	}
}