/*
 * Copyright (c) 2023-2024 Dragons Lake, part of Room 8 Group.
 * Copyright (c) 2019-2022 Mykhailo Parfeniuk, Vladyslav Serhiienko.
 * Copyright (c) 2017-2022 The Forge Interactive Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License"); you may not use this file except in compliance with the License. You may obtain a copy of the License at 
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software distributed under the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. See the License for the specific language governing permissions and limitations under the License.
 *
 * This file contains modified code from the REI project source code
 * (see https://github.com/Vi3LM/REI).
 */

#pragma once

#include <stdint.h>

#if defined(_WIN32)
#    include <sys/stat.h>
#    include <stdlib.h>
#    ifndef NOMINMAX
#        define NOMINMAX 1
#    endif
#    ifndef WIN32_LEAN_AND_MEAN
#        define WIN32_LEAN_AND_MEAN 1
#    endif
#    include <windows.h>
#endif

#ifndef _WIN32
#    include <pthread.h>
#endif

#ifdef _WIN32
typedef unsigned ThreadID;
#elif REI_PLATFORM_SWITCH
typedef void* ThreadID;
#elif REI_PLATFORM_PS4
typedef void* ThreadID;
#else
#    define ThreadID pthread_t
#endif

#define TIMEOUT_INFINITE UINT32_MAX

/// Operating system mutual exclusion primitive.
struct Mutex
{
    Mutex();
    ~Mutex();

    void Acquire();
    void Release();

#ifdef _WIN32
    CRITICAL_SECTION criticalSection;
#elif REI_PLATFORM_SWITCH
    void* pHandle;
#elif REI_PLATFORM_PS4
    void* pHandle;
#else
    pthread_mutex_t pHandle;
#endif
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

#ifdef _WIN32
    CONDITION_VARIABLE conditionVariable;
#elif REI_PLATFORM_SWITCH
    void* pHandle;
#elif REI_PLATFORM_PS4
    void* pHandle;
#else
    pthread_cond_t  pHandle;
#endif
};

typedef void (*ThreadFunction)(void*);

/// Work queue item.
struct ThreadDesc
{
    /// Work item description and thread index (Main thread => 0)
    ThreadFunction pFunc;
    void*          pData;
};

#ifdef _WIN32
typedef void* ThreadHandle;
#elif REI_PLATFORM_SWITCH
typedef struct
{
    void* threadHandle;
    void* stackHandle;
} ThreadHandle;
#elif REI_PLATFORM_PS4
typedef void* ThreadHandle;
#else
typedef pthread_t ThreadHandle;
#endif

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
#ifndef MAX_THREAD_NAME_LENGTH
#    define MAX_THREAD_NAME_LENGTH 15
#endif

#ifdef _WIN32
void sleep(unsigned mSec);
#endif
