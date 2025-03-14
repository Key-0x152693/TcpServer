#pragma once

#include <pthread.h>

class LockGuard
{
public:
    LockGuard(pthread_mutex_t *mutex) : _mutex(mutex)
    {
        pthread_mutex_lock(_mutex);
    }
    ~LockGuard()
    {
        pthread_mutex_unlock(_mutex);
    }

private:
    pthread_mutex_t *_mutex;
};