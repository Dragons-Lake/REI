/*
 * This file is based on The-Forge source code
 * (see https://github.com/ConfettiFX/The-Forge).
*/

#include "OperatingSystem.h"

#ifndef _THREAD_H_
#    define _THREAD_H_

#    ifndef _WIN32
#        include <pthread.h>
#    endif

#    ifndef _WIN32
#        define ThreadID pthread_t
#    else
typedef unsigned ThreadID;
#    endif

#    define TIMEOUT_INFINITE UINT32_MAX

/// Operating system mutual exclusion primitive.
struct Mutex
{
    Mutex();
    ~Mutex();

    void Acquire();
    void Release();

#    ifdef _WIN32
    void* pHandle;
#    else
    pthread_mutex_t pHandle;
#    endif
};

struct MutexLock
{
    MutexLock(Mutex& rhs): mMutex(rhs) { rhs.Acquire(); }
    ~MutexLock() { mMutex.Release(); }

    /// Prevent copy construction.
    MutexLock(const MutexLock& rhs) = delete;
    /// Prevent assignment.
    MutexLock& operator=(const MutexLock& rhs) = delete;

    Mutex& mMutex;
};

struct ConditionVariable
{
    ConditionVariable();
    ~ConditionVariable();

    void Wait(const Mutex& mutex, uint32_t ms = TIMEOUT_INFINITE);
    void WakeOne();
    void WakeAll();

#    ifdef _WIN32
    void* pHandle;
#    else
    pthread_cond_t  pHandle;
#    endif
};

typedef void (*ThreadFunction)(void*);

/// Work queue item.
struct ThreadDesc
{
    /// Work item description and thread index (Main thread => 0)
    ThreadFunction pFunc;
    void*          pData;
};

#    ifdef _WIN32
typedef void* ThreadHandle;
#    else
typedef pthread_t ThreadHandle;
#    endif

ThreadHandle create_thread(ThreadDesc* pItem);
void         destroy_thread(ThreadHandle handle);
void         join_thread(ThreadHandle handle);

struct Thread
{
    static ThreadID     mainThreadID;
    static void         SetMainThread();
    static ThreadID     GetCurrentThreadID();
    static void         GetCurrentThreadName(char* buffer, int buffer_size);
    static void         SetCurrentThreadName(const char* name);
    static bool         IsMainThread();
    static void         Sleep(unsigned mSec);
    static unsigned int GetNumCPUCores(void);
};

// Max thread name should be 15 + null character
#    ifndef MAX_THREAD_NAME_LENGTH
#        define MAX_THREAD_NAME_LENGTH 15
#    endif

#    ifdef _WIN32
void sleep(unsigned mSec);
#    endif

#endif
