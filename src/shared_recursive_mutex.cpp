void recursive_shared_mutex::lock()
{
    const auto                   threadId = std::this_thread::get_id();
    std::unique_lock<std::mutex> lock(_mtx);
    // Increase level of ownership if thread has already exclusive ownership.
    if (_writerThreadId == threadId)
    {
        ++_writersOwnership;
        return;
    }
    //we have to wait until other writers have finished
    while (_writersOwnership > 0)
        _write_queue.wait(lock);

    //we indicate that we want to write to reading threads
    _writerThreadId   = threadId;
    _writersOwnership = 1;
    //we wait until all the readers are finished
    while (_readersOwnership.size() > 0)
        _read_queue.wait(lock);    // wait for writing, no readers
}

void recursive_shared_mutex::lock_shared()
{
    auto                         threadId = std::this_thread::get_id();
    std::unique_lock<std::mutex> lock(_mtx);

    // Increase level of ownership if thread has already exclusive ownership as writer
    if (_writerThreadId == threadId)
    {
        ++_writersOwnership;
        return;
    }

    // As reader we have to check if our thread already has read ownership, if yes we simply increase it
    auto readersIt = _readersOwnership.find(threadId);
    if (readersIt != end(_readersOwnership))
    {
        ++(readersIt->second);
        return;
    }
    //if not we have to check if there are any waiting writers - they have priority
    while (_writersOwnership > 0)
        _write_queue.wait(lock);

    //now we can be sure there are no writers and we are the first reader on this thread
    _readersOwnership.emplace(threadId, 1);
}

void recursive_shared_mutex::unlock()
{
    assert(std::this_thread::get_id() == _writerThreadId);
    {
        std::lock_guard<std::mutex> lock(_mtx);
        // Decrease writer threads level of ownership if not 1.
        if (_writersOwnership != 1)
        {
            --_writersOwnership;
            return;
        }
        _writersOwnership = 0;
        _writerThreadId   = std::thread::id();
    }

    // Reset threads ownership.
    _write_queue.notify_all();
}

void recursive_shared_mutex::unlock_shared()
{
    const auto                   threadId = std::this_thread::get_id();
    std::unique_lock<std::mutex> lock(_mtx);
    // decrease level of ownership if thread has already exclusive ownership as writer
    if (_writerThreadId == threadId)
    {
        --_writersOwnership;
        assert(_writersOwnership > 0);
        return;
    }
    auto readerIt = _readersOwnership.find(threadId);
    assert(readerIt != end(_readersOwnership));

    // Decrease this reader threads level of ownership by one (if we are not the last one)
    if (readerIt->second != 1)
    {
        --(readerIt->second);
        return;
    }
    //if we are the last reader of this this thread we remove the ownership and check if we 
    //have to notify any waiting writers
    _readersOwnership.erase(readerIt);
    const bool isWriterWating = _writersOwnership > 0;
    const bool isLastReader   = _readersOwnership.size() == 0;
    // unlock before notifying, for efficiency
    lock.unlock();
    // notify waiting writer
    if (isWriterWating && isLastReader)
        _read_queue.notify_one();
}

bool recursive_shared_mutex::try_lock()
{
    auto                        threadId = std::this_thread::get_id();
    std::lock_guard<std::mutex> lock(_mtx);
    // Increase level of ownership if thread has already exclusive ownership as writer
    if (_writerThreadId == threadId)
    {
        ++_writersOwnership;
        return true;
    }
    //only lock if there are no readers and writers
    if (_readersOwnership.size() == 0 && _writersOwnership == 0)
    {
        _writerThreadId   = threadId;
        _writersOwnership = 1;
        return true;
    }

    return false;
}

bool recursive_shared_mutex::try_lock_shared()
{
    const auto                  threadId = std::this_thread::get_id();
    std::lock_guard<std::mutex> lock(_mtx);
    // Increase level of ownership if thread has already exclusive ownership as writer
    if (_writerThreadId == threadId)
    {
        ++_writersOwnership;
        return true;
    }
    //if there are no writers (or writers waiting) we increase this threads read ownership
    if (_writersOwnership == 0)
    {
        ++_readersOwnership[threadId];
        return true;
    }

    return false;
}