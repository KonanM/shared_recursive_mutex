// Licensed under the MIT License <http://opensource.org/licenses/MIT>.
// SPDX-License-Identifier: MIT

#pragma once
#include <shared_mutex>

namespace mtx
{
    /**
    * @brief Implementation of a fast shared_recursive_mutex
    */
    //the template parameter is needed to be able to define multiple instances of the shared_recursive_mutex 
    //since the implementation relies upon thread local storage we need a unique type per lock that is needed
    //is doesn't matter what the input type is, along as it's unique (that's why it's called PhantomType)
    template<typename PhantomType>
    class shared_recursive_mutex_t {
    public:
        /**
        * @brief Copying the mutex is not allowed
        */
        shared_recursive_mutex_t(const shared_recursive_mutex_t&) = delete;
        shared_recursive_mutex_t& operator =(const shared_recursive_mutex_t&) = delete;
        /**
        * @brief The shared_recursive_mutex_t is relying on thread local storage, so there can only be 1 valid instance of it
        */
        static shared_recursive_mutex_t& instance()
        {
            static shared_recursive_mutex_t instance;
            return instance;
        }

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

        /**
         * @brief Locks the mutex for sharable read access.
         *        Blocks execution as long as read access is not available:
         *        * other thread has write access
         *        * other threads try to get write access
         *
         *        A thread may call lock repeatedly. If the thread already has write access the level of write access will be increased.
         *        Ownership will only be released after the thread makes a matching number of calls to unlock_shared.
         */
        void lock_shared();

        /**
         * @brief Unlocks the mutex for this thread if its level of write ownership is 1 and has no read ownership.
         *        If the thread has write ownership of 1 and read ownership, the mutex will change from write to read access.
         *        Otherwise reduces the level of ownership by 1.
         */
        void unlock();

        /**
         * @brief Unlocks the mutex for this thread if its level of ownership is 1. Otherwise reduces the level of ownership
         *              by 1.
         */
        void unlock_shared();
        /**
        * @brief Tries to get write ownership if possible. If the thread has read (but no write) ownership this function returns false,
        *        because to upgrade a read lock to a write lock we have to give up read ownership, so if we can't aquire write ownership
        *        we have to reaquire the read ownership again, which might be a blocking operation. Use try_lock_upgrade is this
        *        is the wanted behavior.
        */
        [[nodiscard]] bool try_lock();
        /**
        * @brief Tries to get read ownership if possible.
        */
        [[nodiscard]] bool try_lock_shared();
        /**
        * @brief Returns if this thread has write ownership.
        */
        [[nodiscard]] bool is_locked() const;
        /**
        * @brief Returns true if this thread has only read ownership.
        */
        [[nodiscard]]  bool is_locked_shared() const;

    private:
        shared_recursive_mutex_t() = default;

        std::shared_mutex m_sharedMtx;
        static inline thread_local uint32_t g_readers = 0;
        static inline thread_local uint32_t g_writers = 0;
    };

    template<typename PhantomType>
    void shared_recursive_mutex_t<PhantomType>::lock()
    {
        if (g_writers == 0 && g_readers == 0)
        {
            m_sharedMtx.lock();
        }
        else if (g_writers == 0 && g_readers > 0)
        {
            m_sharedMtx.unlock_shared();
            m_sharedMtx.lock();
        }
        ++g_writers;
    }
    template<typename PhantomType>
    void shared_recursive_mutex_t<PhantomType>::lock_shared()
    {
        //if we are locking shared
        if (g_writers > 0)
        {
            ++g_writers;
        }
        else if (g_readers > 0)
        {
            ++g_readers;
        }
        else if (g_readers == 0)
        {
            m_sharedMtx.lock_shared();
            ++g_readers;
        }
    }
    template<typename PhantomType>
    void shared_recursive_mutex_t<PhantomType>::unlock()
    {
        --g_writers;
        if (g_writers > 0)
            return;
        if (g_writers == 0)
        {
            m_sharedMtx.unlock();
            if (g_readers > 0)
                m_sharedMtx.lock_shared();
        }
    }
    template<typename PhantomType>
    void shared_recursive_mutex_t<PhantomType>::unlock_shared()
    {
        //if the g_writers are > 0 it means that when we got the read lock, this thread already has the write lock
        if (g_writers > 0)
        {
            unlock();
            return;
        }
        --g_readers;
        if (g_readers == 0)
        {
            m_sharedMtx.unlock_shared();
        }
    }
    template<typename PhantomType>
    bool shared_recursive_mutex_t<PhantomType>::try_lock()
    {
        //we already have the lock, so we can simply increase the writer count
        if (g_writers > 0)
        {
            ++g_writers;
            return true;
        }
        //we already have a read lock, but we can't aquire the write lock without giving up the read lock
        //so we have to return false here
        if (g_readers > 0)
        {
            return false;
        }
        const bool aquiredLock = m_sharedMtx.try_lock();
        if (aquiredLock)
        {
            ++g_writers;
        }

        return aquiredLock;
    }
    template<typename PhantomType>
    bool shared_recursive_mutex_t<PhantomType>::try_lock_shared()
    {
        //we already have the lock, so we can simply increase the lock count
        if (g_writers > 0 || g_readers > 0)
        {
            lock_shared();
            return true;
        }
        const bool aquiredLock = m_sharedMtx.try_lock_shared();
        if (aquiredLock)
        {
            ++g_readers;
        }
        return aquiredLock;
    }
    template<typename PhantomType>
    bool shared_recursive_mutex_t<PhantomType>::is_locked() const
    {
        return g_writers > 0;
    }
    template<typename PhantomType>
    bool shared_recursive_mutex_t<PhantomType>::is_locked_shared() const
    {
        return g_readers > 0 && g_writers == 0;
    }

    using shared_recursive_global_mutex = shared_recursive_mutex_t<struct AnonymousType>;

}
