# Shared recursive mutex

## What is a shared recursive mutex?

The C++ 17 standard defines a [shared_mutex](https://en.cppreference.com/w/cpp/thread/shared_mutex), which is another term for reader/writer lock and a [recursive mutex](https://en.cppreference.com/w/cpp/thread/recursive_mutex) which is a mutex which you can lock multiple times from the same thread. 
There is no shared_recursive_mutex though. My buest guess is because it's semantics are quite complex and it's very hard to implement it efficiently (without drawbacks).

The shared_recursive_mutex class is a synchronization primitive that can be used to protect shared data from being simultaneously accessed by multiple threads. It has two level of access:

* shared - several threads can share ownership of the same mutex.
* exclusive - only one thread can own the mutex.

* If one thread has acquired the exclusive lock (through `lock`, `try_lock`), no other threads can acquire the lock. While the thread has exclusive ownership, the thread may make additional calls to `lock`/`try_lock` or `lock_shared`/`try_lock_shared`. The period of exclusive ownership ends when the thread makes a matching number of calls to `unlock` / `unlock_shared`. 
* If one thread has acquired the shared lock (through `lock_shared`, `try_lock_shared`), no other thread can acquire the exclusive lock, but can acquire the shared lock. 
During this period, the thread may make additional calls to `lock_shared` or `try_lock_shared`. The period of shared ownership ends when the thread makes a matching number of calls to `unlock_shared`.
* When a thread has shared ownership, calling `try_lock` will return false. 
* When a thread has shared ownership and tries to get exclusive ownership (through `lock`), it will release the shared ownership and tries to get exclusive ownership. When the thread releases the exclusive ownership (by calling a matching number of `unlock`) during this period, it will try to aquire shared ownership again.


tldr:
* calling `lock_shared` when the thread already has shared ownership the thread will give up shared ownership and tries to aquire exclusive ownership (to avoid deadlocks). When it releases the exclusive ownership it will try to get the shared ownership again.
* calling `lock_shared` when the thread already has exclusive ownership behaves like calling `lock` and will increase the level of exclusive ownership.

## How does the implementation work?
I tried several implementations of a shared_recursive_mutex. As soon as you need an `std::mutex` plus a map for the bookkeeping of the shared/exclusive ownership count per thread the performance of a shared_recursive_mutex is not good enough to be worth the hassle.
I came up with the idea to use thread_local storage for the bookkeeping and not only the implementation became really simple and easy to understand, but also the overhead of the bookkeeping vanished.
The drawback (besides the thread_local memory overhead) is that thread_local storage can only be defined as a static class member. This means there can only be ever one instance of the shared_recursive_mutex. To work around this I introduced a so called phatom type, so we can define different instances of the shared_global_mutex. 
So the class looks like this:
```cpp
template<typename PhantomType>
class shared_recursive_mutex_t {
public:
shared_recursive_mutex_t& instance(){ static shared_recursive_mutex_t instance; return instance; }
//public interface functions lock()/unlock(), lock_shared()/unlock_shared(), try_lock()/try_lock_shared()
private:
	shared_recursive_mutex_t() = default;

	std::shared_mutex m_sharedMtx;
	static inline thread_local uint32_t g_readers = 0;
	static inline thread_local uint32_t g_writers = 0;
};
using shared_recursive_global_mutex = shared_recursive_mutex_t<struct AnonymousType>;
```

## Features

* C++17
* Single Header
* Dependency-free


## Licensed under the [MIT License](LICENSE)
