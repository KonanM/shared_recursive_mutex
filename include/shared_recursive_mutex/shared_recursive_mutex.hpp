
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
#include <shared_mutex>
#include <thread>
#include <unordered_map>
#include <cassert>
#include <iostream>


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
		std::shared_mutex m_sharedMtx;
		std::shared_mutex m_mtx;
		// level of (recursive) read accesses
		struct OwnerShipLevel
		{
			uint32_t readers = 0;
			uint32_t writers = 0;
		};
		using OwnerShipMap = std::unordered_map<std::thread::id, OwnerShipLevel>;
		OwnerShipMap m_threadOwnership;
	};
	void shared_recursive_mutex::lock()
	{
		const auto threadId = std::this_thread::get_id();
		OwnerShipMap::iterator ownerShipIt;
		{
			std::shared_lock<std::shared_mutex> lock(m_mtx);
			ownerShipIt = m_threadOwnership.find(threadId);
		}
		// Increase level of ownership if thread has already exclusive ownership.
		if (ownerShipIt == end(m_threadOwnership))
		{
			{
				std::unique_lock<std::shared_mutex> lock(m_mtx);
				m_threadOwnership.emplace(threadId, OwnerShipLevel{ 0, 1 });
			}
			m_sharedMtx.lock();
			return;
		}
		auto& ownerShipLevel = ownerShipIt->second;
		if (ownerShipLevel.readers > 0 && ownerShipLevel.writers == 0)
		{
			++ownerShipLevel.writers;
			m_sharedMtx.unlock_shared();
			m_sharedMtx.lock();
		}
		else if (ownerShipLevel.writers > 0)
		{
			++ownerShipLevel.writers;
		}
		else
		{
			assert(false);
		};
	}

	void shared_recursive_mutex::lock_shared()
	{
		const auto threadId = std::this_thread::get_id();
		OwnerShipMap::iterator ownerShipIt;
		{
			std::shared_lock<std::shared_mutex> lock(m_mtx);
			ownerShipIt = m_threadOwnership.find(threadId);
		}
		//check if this thread had no ownership before
		if (ownerShipIt == end(m_threadOwnership))
		{
			{
				std::unique_lock<std::shared_mutex> lock(m_mtx);
				m_threadOwnership.emplace(threadId, OwnerShipLevel{ 1, 0 });
			}
			m_sharedMtx.lock_shared();
			return;
		}
		auto& ownerShipLevel = ownerShipIt->second;
		if (ownerShipLevel.writers == 0 && ownerShipLevel.readers == 0)
		{
			ownerShipLevel.readers = 1;
			m_sharedMtx.lock_shared();
		}
		else
		{
			++ownerShipLevel.readers;
		}
	}

	void shared_recursive_mutex::unlock()
	{
		const auto threadId = std::this_thread::get_id();
		OwnerShipMap::iterator ownerShipIt;
		{
			std::shared_lock<std::shared_mutex> lock(m_mtx);
			ownerShipIt = m_threadOwnership.find(threadId);
		}
		//check if this thread had no ownership before
		assert(ownerShipIt != m_threadOwnership.end());

		auto& ownerShipLevel = ownerShipIt->second;
		if (ownerShipLevel.writers == 1 && ownerShipLevel.readers == 0)
		{
			ownerShipLevel.writers = 0;
			m_sharedMtx.unlock();
		}
		else if (ownerShipLevel.writers == 1 && ownerShipLevel.readers > 0)
		{
			--ownerShipLevel.writers;
			m_sharedMtx.unlock();
			m_sharedMtx.lock_shared();
		}
		else if (ownerShipLevel.writers > 0)
		{
			--ownerShipLevel.writers;
		}
		else
		{
			assert(false && "lock called without write access!");
		}
	}

	void shared_recursive_mutex::unlock_shared()
	{
		const auto threadId = std::this_thread::get_id();
		OwnerShipMap::iterator ownerShipIt;
		{
			std::shared_lock<std::shared_mutex> lock(m_mtx);
			ownerShipIt = m_threadOwnership.find(threadId);
		}
		//check if this thread had no ownership before
		if (ownerShipIt == m_threadOwnership.end())
			return;

		auto& ownerShipLevel = ownerShipIt->second;
		if (ownerShipLevel.writers == 0 && ownerShipLevel.readers == 1)
		{
			ownerShipLevel.readers = 0;
			m_sharedMtx.unlock_shared();
		}
		else if (ownerShipLevel.readers > 0)
		{
			ownerShipLevel.readers--;
		}
		else
		{
			assert(false && "unlock_shared called without read access!");
		}
	}

	bool shared_recursive_mutex::try_lock()
	{
		return false;
	}

	bool shared_recursive_mutex::try_lock_shared()
	{
		return false;
	}
}
