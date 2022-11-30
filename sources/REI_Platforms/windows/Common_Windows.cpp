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

#include <stdio.h>
#include <windows.h>

#include "REI/Common.h"
#include "REI/Thread.h"

int REI_FailedAssert(const char* file, int line, const char* statement, const char* message, ...)
{
    char    str[1024];
    int     pos = sprintf_s(str, "Assertion failed: (%s)\n\nFile: %s\nLine: %d\n\n", statement, file, line);
    if (message)
    {
        va_list ap;
        va_start(ap, message);
        if (pos < 1023)
            vsprintf_s(str + pos, 1024 - pos, message, ap);
        va_end(ap);
    }

    strcat_s(str, message ? "\n\nDebug?" : "Debug?");

    return MessageBoxA(NULL, str, "Assert failed", MB_YESNO | MB_ICONERROR) == IDYES;
}

void REI_DebugOutput(const char* str)
{
    OutputDebugStringA(str);
}

void REI_Print(const char* str)
{
    printf("%s", str);
}

DWORD WINAPI ThreadFunctionStatic(void* data)
{
    ThreadDesc* pDesc = (ThreadDesc*)data;
    pDesc->pFunc(pDesc->pData);
    return 0;
}

Mutex::Mutex() { InitializeCriticalSection((CRITICAL_SECTION*)&criticalSection); }

Mutex::~Mutex() { DeleteCriticalSection(&criticalSection); }

void Mutex::Acquire() { EnterCriticalSection((CRITICAL_SECTION*)&criticalSection); }

void Mutex::Release() { LeaveCriticalSection((CRITICAL_SECTION*)&criticalSection); }

ConditionVariable::ConditionVariable() { InitializeConditionVariable((PCONDITION_VARIABLE)&conditionVariable); }

ConditionVariable::~ConditionVariable() {}

void ConditionVariable::Wait(const Mutex& mutex, uint32_t ms)
{
    SleepConditionVariableCS((PCONDITION_VARIABLE)&conditionVariable, (PCRITICAL_SECTION)&mutex.criticalSection, ms);
}

void ConditionVariable::WakeOne() { WakeConditionVariable((PCONDITION_VARIABLE)&conditionVariable); }

void ConditionVariable::WakeAll() { WakeAllConditionVariable((PCONDITION_VARIABLE)&conditionVariable); }

ThreadID Thread::mainThreadID;

void Thread::SetMainThread() { mainThreadID = GetCurrentThreadID(); }

ThreadID Thread::GetCurrentThreadID() { return GetCurrentThreadId(); }

char* thread_name()
{
    __declspec(thread) static char name[MAX_THREAD_NAME_LENGTH + 1];
    return name;
}

void Thread::GetCurrentThreadName(char* buffer, int size)
{
    if (const char* name = thread_name())
        snprintf(buffer, (size_t)size, "%s", name);
    else
        buffer[0] = 0;
}

void Thread::SetCurrentThreadName(const char* name) { strcpy_s(thread_name(), MAX_THREAD_NAME_LENGTH + 1, name); }

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
