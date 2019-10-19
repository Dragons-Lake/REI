/*
 * This file is based on The-Forge source code
 * (see https://github.com/ConfettiFX/The-Forge).
*/

#include <deque>

#include "ThreadSystem.h"

#include "REI/Interface/Thread.h"
#include "REI/Interface/Common.h"

struct ThreadedTask
{
    TaskFunc  mTask;
    void*     mUser;
    uintptr_t mStart;
    uintptr_t mEnd;
};

enum
{
    MAX_LOAD_THREADS = 64
};

struct ThreadSystem
{
    ThreadDesc               mThreadDescs[MAX_LOAD_THREADS];
    ThreadHandle             mThread[MAX_LOAD_THREADS];
    std::deque<ThreadedTask> mLoadQueue;
    ConditionVariable        mQueueCond;
    Mutex                    mQueueMutex;
    ConditionVariable        mIdleCond;
    uint32_t                 mNumThreads;
    uint32_t                 mNumIdleThreads;
    volatile bool            mRun;
};

bool assistThreadSystem(ThreadSystem* pThreadSystem)
{
    pThreadSystem->mQueueMutex.Acquire();
    if (!pThreadSystem->mLoadQueue.empty())
    {
        ThreadedTask resourceTask = pThreadSystem->mLoadQueue.front();
        if (resourceTask.mStart + 1 == resourceTask.mEnd)
            pThreadSystem->mLoadQueue.pop_front();
        else
            ++pThreadSystem->mLoadQueue.front().mStart;
        pThreadSystem->mQueueMutex.Release();
        resourceTask.mTask(resourceTask.mUser, resourceTask.mStart);

        return true;
    }
    else
    {
        pThreadSystem->mQueueMutex.Release();
        return false;
    }
}

static void taskThreadFunc(void* pThreadData)
{
    ThreadSystem* pThreadSystem = (ThreadSystem*)pThreadData;
    while (pThreadSystem->mRun)
    {
        pThreadSystem->mQueueMutex.Acquire();
        ++pThreadSystem->mNumIdleThreads;
        while (pThreadSystem->mRun && pThreadSystem->mLoadQueue.empty())
        {
            pThreadSystem->mIdleCond.WakeAll();
            pThreadSystem->mQueueCond.Wait(pThreadSystem->mQueueMutex);
        }
        --pThreadSystem->mNumIdleThreads;
        if (!pThreadSystem->mLoadQueue.empty())
        {
            ThreadedTask resourceTask = pThreadSystem->mLoadQueue.front();
            if (resourceTask.mStart + 1 == resourceTask.mEnd)
                pThreadSystem->mLoadQueue.pop_front();
            else
                ++pThreadSystem->mLoadQueue.front().mStart;
            pThreadSystem->mQueueMutex.Release();
            resourceTask.mTask(resourceTask.mUser, resourceTask.mStart);
        }
        else
        {
            pThreadSystem->mQueueMutex.Release();
        }
    }
    pThreadSystem->mQueueMutex.Acquire();
    ++pThreadSystem->mNumIdleThreads;
    pThreadSystem->mIdleCond.WakeAll();
    pThreadSystem->mQueueMutex.Release();
}

void initThreadSystem(uint32_t numThreads, ThreadSystem** ppThreadSystem)
{
    ThreadSystem* pThreadSystem = REI_new(ThreadSystem);

    if (!numThreads)
    {
        numThreads = REI_max<uint32_t>(Thread::GetNumCPUCores(), 1);
    }

    numThreads = REI_min<uint32_t>(numThreads, MAX_LOAD_THREADS);

    pThreadSystem->mRun = true;
    pThreadSystem->mNumIdleThreads = 0;

    for (unsigned i = 0; i < numThreads; ++i)
    {
        pThreadSystem->mThreadDescs[i].pFunc = taskThreadFunc;
        pThreadSystem->mThreadDescs[i].pData = pThreadSystem;

        pThreadSystem->mThread[i] = create_thread(&pThreadSystem->mThreadDescs[i]);
    }
    pThreadSystem->mNumThreads = numThreads;

    *ppThreadSystem = pThreadSystem;
}

void addThreadSystemTask(ThreadSystem* pThreadSystem, TaskFunc task, void* user, uintptr_t index)
{
    pThreadSystem->mQueueMutex.Acquire();
    pThreadSystem->mLoadQueue.emplace_back(ThreadedTask{ task, user, index, index + 1 });
    pThreadSystem->mQueueMutex.Release();
    pThreadSystem->mQueueCond.WakeOne();
}

void addThreadSystemRangeTask(ThreadSystem* pThreadSystem, TaskFunc task, void* user, uintptr_t count)
{
    if (!count)
        return;

    pThreadSystem->mQueueMutex.Acquire();
    pThreadSystem->mLoadQueue.emplace_back(ThreadedTask{ task, user, 0, count });
    pThreadSystem->mQueueMutex.Release();
    if (count > 1)
        pThreadSystem->mQueueCond.WakeAll();
    else
        pThreadSystem->mQueueCond.WakeOne();
}

void addThreadSystemRangeTask(ThreadSystem* pThreadSystem, TaskFunc task, void* user, uintptr_t start, uintptr_t end)
{
    if (start >= end)
        return;

    pThreadSystem->mQueueMutex.Acquire();
    pThreadSystem->mLoadQueue.emplace_back(ThreadedTask{ task, user, start, end });
    pThreadSystem->mQueueMutex.Release();
    if (end - start > 1)
        pThreadSystem->mQueueCond.WakeAll();
    else
        pThreadSystem->mQueueCond.WakeOne();
}

void shutdownThreadSystem(ThreadSystem* pThreadSystem)
{
    pThreadSystem->mQueueMutex.Acquire();
    pThreadSystem->mRun = false;
    pThreadSystem->mQueueMutex.Release();
    pThreadSystem->mQueueCond.WakeAll();

    uint32_t numThreads = pThreadSystem->mNumThreads;
    for (uint32_t i = 0; i < numThreads; ++i)
    {
        destroy_thread(pThreadSystem->mThread[i]);
    }

    REI_delete(pThreadSystem);
}

bool isThreadSystemIdle(ThreadSystem* pThreadSystem)
{
    pThreadSystem->mQueueMutex.Acquire();
    bool idle = (pThreadSystem->mLoadQueue.empty() && pThreadSystem->mNumIdleThreads == pThreadSystem->mNumThreads) ||
                !pThreadSystem->mRun;
    pThreadSystem->mQueueMutex.Release();
    return idle;
}

void waitThreadSystemIdle(ThreadSystem* pThreadSystem)
{
    pThreadSystem->mQueueMutex.Acquire();
    while ((!pThreadSystem->mLoadQueue.empty() || pThreadSystem->mNumIdleThreads < pThreadSystem->mNumThreads) &&
           pThreadSystem->mRun)
        pThreadSystem->mIdleCond.Wait(pThreadSystem->mQueueMutex);
    pThreadSystem->mQueueMutex.Release();
}
