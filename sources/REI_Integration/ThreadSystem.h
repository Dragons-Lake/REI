/*
 * This file is based on The-Forge source code
 * (see https://github.com/ConfettiFX/The-Forge).
*/

typedef void (*TaskFunc)(void* user, uintptr_t arg);

template<class T, void (T::*callback)(size_t)>
static void memberTaskFunc(void* userData, size_t arg)
{
    T* pThis = static_cast<T*>(userData);
    (pThis->*callback)(arg);
}

template<class T, void (T::*callback)()>
static void memberTaskFunc0(void* userData, size_t)
{
    T* pThis = static_cast<T*>(userData);
    (pThis->*callback)();
}

struct ThreadSystem;

void initThreadSystem(uint32_t numThreads, ThreadSystem** ppThreadSystem);

void shutdownThreadSystem(ThreadSystem* pThreadSystem);

void addThreadSystemRangeTask(ThreadSystem* pThreadSystem, TaskFunc task, void* user, uintptr_t count);
void addThreadSystemRangeTask(ThreadSystem* pThreadSystem, TaskFunc task, void* user, uintptr_t start, uintptr_t end);
void addThreadSystemTask(ThreadSystem* pThreadSystem, TaskFunc task, void* user, uintptr_t index = 0);

bool assistThreadSystem(ThreadSystem* pThreadSystem);

bool isThreadSystemIdle(ThreadSystem* pThreadSystem);
void waitThreadSystemIdle(ThreadSystem* pThreadSystem);
