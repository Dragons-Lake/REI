/*
 * This file is based on The-Forge source code
 * (see https://github.com/ConfettiFX/The-Forge).
*/

#ifdef _WIN32

#include <stdio.h>

#include "REI/Interface/Thread.h"
#include "REI/Interface/OperatingSystem.h"
#include "REI/Interface/Common.h"

DWORD WINAPI ThreadFunctionStatic(void* data)
{
	ThreadDesc* pDesc = (ThreadDesc*)data;
	pDesc->pFunc(pDesc->pData);
	return 0;
}

Mutex::Mutex()
{
	pHandle = (CRITICAL_SECTION*)REI_calloc(1, sizeof(CRITICAL_SECTION));
	InitializeCriticalSection((CRITICAL_SECTION*)pHandle);
}

Mutex::~Mutex()
{
	CRITICAL_SECTION* cs = (CRITICAL_SECTION*)pHandle;
	DeleteCriticalSection(cs);
	REI_free(cs);
	pHandle = 0;
}

void Mutex::Acquire() { EnterCriticalSection((CRITICAL_SECTION*)pHandle); }

void Mutex::Release() { LeaveCriticalSection((CRITICAL_SECTION*)pHandle); }

ConditionVariable::ConditionVariable()
{
	pHandle = (CONDITION_VARIABLE*)REI_calloc(1, sizeof(CONDITION_VARIABLE));
	InitializeConditionVariable((PCONDITION_VARIABLE)pHandle);
}

ConditionVariable::~ConditionVariable() { REI_free(pHandle); }

void ConditionVariable::Wait(const Mutex& mutex, uint32_t ms)
{
	SleepConditionVariableCS((PCONDITION_VARIABLE)pHandle, (PCRITICAL_SECTION)mutex.pHandle, ms);
}

void ConditionVariable::WakeOne() { WakeConditionVariable((PCONDITION_VARIABLE)pHandle); }

void ConditionVariable::WakeAll() { WakeAllConditionVariable((PCONDITION_VARIABLE)pHandle); }

ThreadID Thread::mainThreadID;

void Thread::SetMainThread() { mainThreadID = GetCurrentThreadID(); }

ThreadID Thread::GetCurrentThreadID() { return GetCurrentThreadId(); }

char * thread_name()
{
	__declspec(thread) static char name[MAX_THREAD_NAME_LENGTH + 1];
	return name;
}

void Thread::GetCurrentThreadName(char * buffer, int size)
{
	if (const char* name = thread_name())
		snprintf(buffer, (size_t)size, "%s", name);
	else
		buffer[0] = 0;
}

void Thread::SetCurrentThreadName(const char * name) { strcpy_s(thread_name(), MAX_THREAD_NAME_LENGTH + 1, name); }

bool Thread::IsMainThread() { return GetCurrentThreadID() == mainThreadID; }

ThreadHandle create_thread(ThreadDesc* pDesc)
{
	ThreadHandle handle = CreateThread(0, 0, ThreadFunctionStatic, pDesc, 0, 0);
	REI_ASSERT(handle != NULL);
	return handle;
}

void destroy_thread(ThreadHandle handle)
{
	REI_ASSERT(handle != NULL);
	WaitForSingleObject((HANDLE)handle, INFINITE);
	CloseHandle((HANDLE)handle);
	handle = 0;
}

void join_thread(ThreadHandle handle) { WaitForSingleObject((HANDLE)handle, INFINITE); }

void Thread::Sleep(unsigned mSec) { ::Sleep(mSec); }

// threading class (Static functions)
unsigned int Thread::GetNumCPUCores(void)
{
	_SYSTEM_INFO systemInfo;
	GetSystemInfo(&systemInfo);
	return systemInfo.dwNumberOfProcessors;
}

void sleep(uint32_t mSec) { ::Sleep((DWORD)mSec); }

#endif