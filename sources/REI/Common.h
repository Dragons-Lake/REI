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

//This file contains abstractions for compiler specific things
#pragma once

#include <stdint.h>
#include <stdlib.h>
#include <memory.h>
#include <cstdarg>

#include <string>
#include <vector>
#include <unordered_map>
#include <deque>
#include <memory>

#if _MSC_VER >= 1400
#    define REI_ASSUME(x) __assume(x)
#else
#    define REI_ASSUME(x) \
        do                \
            (void)(x);    \
        while (0)
#endif

#define REI_UNUSED(x) (void)(x)

#define REI_CONCAT_CONCAT(a, b) a##b
#define REI_CONCAT(a, b) REI_CONCAT_CONCAT(a, b)

#ifndef REI_ANON_VAR
#    define REI_ANON_VAR(str) REI_CONCAT(str, __LINE__)
#endif

#ifdef _MSC_VER
#    define REI_ALIGNAS(x) __declspec(align(x))
#    define REI_DEBUGBREAK() __debugbreak()
#elif __ANDROID__
#    define REI_ALIGNAS(x) __attribute__((aligned(x)))
#    define REI_DEBUGBREAK() __builtin_trap()
#else
#    define REI_ALIGNAS(x) __attribute__((aligned(x)))
#    define REI_DEBUGBREAK() __builtin_debugtrap()
#endif

#define REI_OFFSETOF(type, mem) ((size_t)(&(((type*)0)->mem)))

typedef enum REI_LogType
{
    REI_LOG_TYPE_INFO = 0,
    REI_LOG_TYPE_WARNING,
    REI_LOG_TYPE_DEBUG,
    REI_LOG_TYPE_ERROR
} REI_LogType;

#ifdef _DEBUG
#    define REI_ASSERT(b, ...)                                                   \
        do                                                                       \
            if (!(b) && REI_FailedAssert(__FILE__, __LINE__, #b, ##__VA_ARGS__)) \
                REI_DEBUGBREAK();                                                \
        while (0)
#else
#    define REI_ASSERT(b, ...) REI_ASSUME(b)
#endif

using REI_LogPtr = void (*)(REI_LogType, const char* msg, ...);

#if INTPTR_MAX == 0x7FFFFFFFFFFFFFFFLL
#    define REI_PTRSIZE 8
#elif INTPTR_MAX == 0x7FFFFFFF
#    define REI_PTRSIZE 4
#else
#    error unsupported platform
#endif

typedef volatile REI_ALIGNAS(4) uint32_t REI_atomic32_t;
typedef volatile REI_ALIGNAS(8) uint64_t REI_atomic64_t;
typedef volatile REI_ALIGNAS(REI_PTRSIZE) uintptr_t REI_atomicptr_t;

#ifdef _MSC_VER
#    include <intrin.h>

#    define REI_memorybarrier_acquire() _ReadWriteBarrier()
#    define REI_memorybarrier_release() _ReadWriteBarrier()

#    define REI_atomic32_load_relaxed(pVar) (*(pVar))
#    define REI_atomic32_store_relaxed(dst, val) _InterlockedExchange((volatile long*)(dst), val)
#    define REI_atomic32_add_relaxed(dst, val) _InterlockedExchangeAdd((volatile long*)(dst), (val))
#    define REI_atomic32_cas_relaxed(dst, cmp_val, new_val) \
        _InterlockedCompareExchange((volatile long*)(dst), (new_val), (cmp_val))

#    if REI_PTRSIZE == 8
#        define REI_atomic64_load_relaxed(pVar) (*(pVar))
#        define REI_atomic64_store_relaxed(dst, val) _InterlockedExchange64((volatile long long*)(dst), val)
#        define REI_atomic64_add_relaxed(dst, val) _InterlockedExchangeAdd64((volatile long long*)(dst), (val))
#        define REI_atomic64_cas_relaxed(dst, cmp_val, new_val) \
            _InterlockedCompareExchange64((volatile long long*)(dst), (new_val), (cmp_val))
#    endif

#    define REI_popcnt(x) __popcnt(x)

static inline void REI_ctz32(uint32_t* pOut, uint32_t mask32) 
{ 
    unsigned long out;
    mask32 ? _BitScanForward(&out, mask32) : out = 32; 
    *pOut = (uint32_t)out;
}
static inline void REI_ctz64(uint32_t* pOut, uint64_t mask64) 
{
    unsigned long out;
    mask64 ? _BitScanForward(&out, (unsigned long)mask64) : out = 32;
    *pOut = (uint32_t)out;
}
#else
#    define REI_memorybarrier_acquire() __asm__ __volatile__("" : : : "memory")
#    define REI_memorybarrier_release() __asm__ __volatile__("" : : : "memory")

#    define REI_atomic32_load_relaxed(pVar) (*(pVar))
#    define REI_atomic32_store_relaxed(dst, val) __sync_lock_test_and_set((dst), val)
#    define REI_atomic32_add_relaxed(dst, val) __sync_fetch_and_add((dst), (val))
#    define REI_atomic32_cas_relaxed(dst, cmp_val, new_val) __sync_val_compare_and_swap((dst), (cmp_val), (new_val))

#    if REI_PTRSIZE == 8
#        define REI_atomic64_load_relaxed(pVar) (*(pVar))
#        define REI_atomic64_store_relaxed(dst, val) __sync_lock_test_and_set((dst), val)
#        define REI_atomic64_add_relaxed(dst, val) __sync_fetch_and_add((dst), (val))
#        define REI_atomic64_cas_relaxed(dst, cmp_val, new_val) __sync_val_compare_and_swap((dst), (cmp_val), (new_val))
#    endif

#    define REI_popcnt(x) __builtin_popcount(x)

static inline void REI_ctz32(uint32_t* pOut, uint32_t mask32)
{
    *pOut = mask32 ? __builtin_ctz(mask32) : *pOut = 32;
}
static inline void REI_ctz64(uint32_t* pOut, uint64_t mask64)
{
    *pOut = mask64 ? __builtin_ctzll(mask64) : *pOut = 64;
}
#endif

static inline uint32_t REI_atomic32_load_acquire(REI_atomic32_t* pVar)
{
    uint32_t value = REI_atomic32_load_relaxed(pVar);
    REI_memorybarrier_acquire();
    return value;
}

static inline uint32_t REI_atomic32_store_release(REI_atomic32_t* pVar, uint32_t val)
{
    REI_memorybarrier_release();
    return REI_atomic32_store_relaxed(pVar, val);
}

static inline uint32_t REI_atomic32_max_relaxed(REI_atomic32_t* dst, uint32_t val)
{
    uint32_t prev_val = val;
    do
    {
        prev_val = REI_atomic32_cas_relaxed(dst, prev_val, val);
    } while (prev_val < val);
    return prev_val;
}

#if REI_PTRSIZE == 8
static inline uint64_t REI_atomic64_load_acquire(REI_atomic64_t* pVar)
{
    uint64_t value = REI_atomic64_load_relaxed(pVar);
    REI_memorybarrier_acquire();
    return value;
}

static inline uint64_t REI_atomic64_store_release(REI_atomic64_t* pVar, uint64_t val)
{
    REI_memorybarrier_release();
    return REI_atomic64_store_relaxed(pVar, val);
}

static inline uint64_t REI_atomic64_max_relaxed(REI_atomic64_t* dst, uint64_t val)
{
    uint64_t prev_val = val;
    do
    {
        prev_val = REI_atomic64_cas_relaxed(dst, prev_val, val);
    } while (prev_val < val);
    return prev_val;
}
#endif

#if REI_PTRSIZE == 4
#    define REI_atomicptr_load_relaxed REI_atomic32_load_relaxed
#    define REI_atomicptr_load_acquire REI_atomic32_load_acquire
#    define REI_atomicptr_store_relaxed REI_atomic32_store_relaxed
#    define REI_atomicptr_store_release REI_atomic32_store_release
#    define REI_atomicptr_add_relaxed REI_atomic32_add_relaxed
#    define REI_atomicptr_cas_relaxed REI_atomic32_cas_relaxed
#    define REI_atomicptr_max_relaxed REI_atomic32_max_relaxed
#elif REI_PTRSIZE == 8
#    define REI_atomicptr_load_relaxed REI_atomic64_load_relaxed
#    define REI_atomicptr_load_acquire REI_atomic64_load_acquire
#    define REI_atomicptr_store_relaxed REI_atomic64_store_relaxed
#    define REI_atomicptr_store_release REI_atomic64_store_release
#    define REI_atomicptr_add_relaxed REI_atomic64_add_relaxed
#    define REI_atomicptr_cas_relaxed REI_atomic64_cas_relaxed
#    define REI_atomicptr_max_relaxed REI_atomic64_max_relaxed
#endif

template<typename T>
static inline bool REI_isPowerOf2(T x)
{
    return (x & (x - 1)) == 0;
}    // Note: returns true for 0

template<typename T>
static inline T REI_align_up(T value, T align)
{
    return ((value + align - 1) / align) * align;
}

template<typename T>
static inline T REI_align_down(T value, T align)
{
    return value - value % align;
}

template<typename T>
static inline T REI_alignPowerOf2_up(T value, T align)
{
    return (value + align - 1) & ~align;
}

template<typename T>
static inline T REI_alignPowerOf2_down(T value, T align)
{
    return value & ~align;
}

template<typename T>
static inline T REI_min(T a, T b)
{
    return a < b ? a : b;
}

template<typename T>
static inline T REI_max(T a, T b)
{
    return a > b ? a : b;
}

template<typename T>
static inline T REI_clamp(T x, T a, T b)
{
    return REI_max(a, REI_min(x, b));
}

static inline uint32_t REI_fmix32(uint32_t h)
{
    h ^= h >> 16;
    h *= 0x85ebca6b;
    h ^= h >> 13;
    h *= 0xc2b2ae35;
    h ^= h >> 16;

    return h;
}

static inline uint64_t REI_fmix64(uint64_t k)
{
    k ^= k >> 33;
    k *= 0xff51afd7ed558ccd;
    k ^= k >> 33;
    k *= 0xc4ceb9fe1a85ec53;
    k ^= k >> 33;

    return k;
}

#if defined(_MSC_VER)
#    define REI_rotl32(value, shift) _rotl(value, shift)
#    define REI_rotl64(value, shift) _rotl64(value, shift)
#else
#    define REI_rotl32(value, shift) __builtin_rotateleft32(value, shift)
#    define REI_rotl64(value, shift) __builtin_rotateleft64(value, shift)
#endif

static inline uint32_t REI_murmurHash3_x86_32(const void* key, int len, uint32_t seed = 2166136261U)
{
    const uint8_t* data = (const uint8_t*)key;
    const int      nblocks = len / 4;

    uint32_t h1 = seed;

    const uint32_t c1 = 0xcc9e2d51;
    const uint32_t c2 = 0x1b873593;

    // body

    const uint32_t* blocks = (const uint32_t*)(data + nblocks * 4);

    for (int i = -nblocks; i; i++)
    {
        uint32_t k1 = blocks[i];

        k1 *= c1;
        k1 = REI_rotl32(k1, 15);
        k1 *= c2;

        h1 ^= k1;
        h1 = REI_rotl32(h1, 13);
        h1 = h1 * 5 + 0xe6546b64;
    }

    // tail

    const uint8_t* tail = (const uint8_t*)(data + nblocks * 4);

    uint32_t k1 = 0;

    switch (len & 3)
    {
        case 3: k1 ^= tail[2] << 16; [[fallthrough]];
        case 2: k1 ^= tail[1] << 8; [[fallthrough]];
        case 1:
            k1 ^= tail[0];
            k1 *= c1;
            k1 = REI_rotl32(k1, 15);
            k1 *= c2;
            h1 ^= k1;
    };

    // finalization

    h1 ^= len;

    h1 = REI_fmix32(h1);

    return h1;
}

static inline void REI_murmurHash3_x86_128(const void* key, const int len, const uint32_t seed, void* out)
{
    const uint8_t* data = (const uint8_t*)key;
    const int      nblocks = len / 16;
    int            i;

    uint32_t h1 = seed;
    uint32_t h2 = seed;
    uint32_t h3 = seed;
    uint32_t h4 = seed;

    uint32_t c1 = 0x239b961b;
    uint32_t c2 = 0xab0e9789;
    uint32_t c3 = 0x38b34ae5;
    uint32_t c4 = 0xa1e38b93;

    //----------
    // body

    const uint32_t* blocks = (const uint32_t*)(data + nblocks * 16);

    for (i = -nblocks; i; i++)
    {
        uint32_t k1 = blocks[i * 4 + 0];
        uint32_t k2 = blocks[i * 4 + 1];
        uint32_t k3 = blocks[i * 4 + 2];
        uint32_t k4 = blocks[i * 4 + 3];

        k1 *= c1;
        k1 = REI_rotl32(k1, 15);
        k1 *= c2;
        h1 ^= k1;

        h1 = REI_rotl32(h1, 19);
        h1 += h2;
        h1 = h1 * 5 + 0x561ccd1b;

        k2 *= c2;
        k2 = REI_rotl32(k2, 16);
        k2 *= c3;
        h2 ^= k2;

        h2 = REI_rotl32(h2, 17);
        h2 += h3;
        h2 = h2 * 5 + 0x0bcaa747;

        k3 *= c3;
        k3 = REI_rotl32(k3, 17);
        k3 *= c4;
        h3 ^= k3;

        h3 = REI_rotl32(h3, 15);
        h3 += h4;
        h3 = h3 * 5 + 0x96cd1c35;

        k4 *= c4;
        k4 = REI_rotl32(k4, 18);
        k4 *= c1;
        h4 ^= k4;

        h4 = REI_rotl32(h4, 13);
        h4 += h1;
        h4 = h4 * 5 + 0x32ac3b17;
    }

    //----------
    // tail

    const uint8_t* tail = (const uint8_t*)(data + nblocks * 16);

    uint32_t k1 = 0;
    uint32_t k2 = 0;
    uint32_t k3 = 0;
    uint32_t k4 = 0;

    switch (len & 15)
    {
        case 15: k4 ^= tail[14] << 16; [[fallthrough]];
        case 14: k4 ^= tail[13] << 8; [[fallthrough]];
        case 13:
            k4 ^= tail[12] << 0;
            k4 *= c4;
            k4 = REI_rotl32(k4, 18);
            k4 *= c1;
            h4 ^= k4;
            [[fallthrough]];

        case 12: k3 ^= tail[11] << 24; [[fallthrough]];
        case 11: k3 ^= tail[10] << 16; [[fallthrough]];
        case 10: k3 ^= tail[9] << 8; [[fallthrough]];
        case 9:
            k3 ^= tail[8] << 0;
            k3 *= c3;
            k3 = REI_rotl32(k3, 17);
            k3 *= c4;
            h3 ^= k3;
            [[fallthrough]];

        case 8: k2 ^= tail[7] << 24; [[fallthrough]];
        case 7: k2 ^= tail[6] << 16; [[fallthrough]];
        case 6: k2 ^= tail[5] << 8; [[fallthrough]];
        case 5:
            k2 ^= tail[4] << 0;
            k2 *= c2;
            k2 = REI_rotl32(k2, 16);
            k2 *= c3;
            h2 ^= k2;
            [[fallthrough]];

        case 4: k1 ^= tail[3] << 24; [[fallthrough]];
        case 3: k1 ^= tail[2] << 16; [[fallthrough]];
        case 2: k1 ^= tail[1] << 8; [[fallthrough]];
        case 1:
            k1 ^= tail[0] << 0;
            k1 *= c1;
            k1 = REI_rotl32(k1, 15);
            k1 *= c2;
            h1 ^= k1;
    };

    //----------
    // finalization

    h1 ^= len;
    h2 ^= len;
    h3 ^= len;
    h4 ^= len;

    h1 += h2;
    h1 += h3;
    h1 += h4;
    h2 += h1;
    h3 += h1;
    h4 += h1;

    h1 = REI_fmix32(h1);
    h2 = REI_fmix32(h2);
    h3 = REI_fmix32(h3);
    h4 = REI_fmix32(h4);

    h1 += h2;
    h1 += h3;
    h1 += h4;
    h2 += h1;
    h3 += h1;
    h4 += h1;

    ((uint32_t*)out)[0] = h1;
    ((uint32_t*)out)[1] = h2;
    ((uint32_t*)out)[2] = h3;
    ((uint32_t*)out)[3] = h4;
}

static inline void REI_murmurHash3_x64_128(const void* key, const int len, const uint64_t seed, void* out)
{
    const uint8_t* data = (const uint8_t*)key;
    const int      nblocks = len / 16;
    int            i;

    uint64_t h1 = seed;
    uint64_t h2 = seed;

    uint64_t c1 = 0x87c37b91114253d5;
    uint64_t c2 = 0x4cf5ad432745937f;

    //----------
    // body

    const uint64_t* blocks = (const uint64_t*)(data);

    for (i = 0; i < nblocks; i++)
    {
        uint64_t k1 = blocks[i * 2 + 0];
        uint64_t k2 = blocks[i * 2 + 1];

        k1 *= c1;
        k1 = REI_rotl64(k1, 31);
        k1 *= c2;
        h1 ^= k1;

        h1 = REI_rotl64(h1, 27);
        h1 += h2;
        h1 = h1 * 5 + 0x52dce729;

        k2 *= c2;
        k2 = REI_rotl64(k2, 33);
        k2 *= c1;
        h2 ^= k2;

        h2 = REI_rotl64(h2, 31);
        h2 += h1;
        h2 = h2 * 5 + 0x38495ab5;
    }

    //----------
    // tail

    const uint8_t* tail = (const uint8_t*)(data + nblocks * 16);

    uint64_t k1 = 0;
    uint64_t k2 = 0;

    switch (len & 15)
    {
        case 15: k2 ^= (uint64_t)(tail[14]) << 48; [[fallthrough]];
        case 14: k2 ^= (uint64_t)(tail[13]) << 40; [[fallthrough]];
        case 13: k2 ^= (uint64_t)(tail[12]) << 32; [[fallthrough]];
        case 12: k2 ^= (uint64_t)(tail[11]) << 24; [[fallthrough]];
        case 11: k2 ^= (uint64_t)(tail[10]) << 16; [[fallthrough]];
        case 10: k2 ^= (uint64_t)(tail[9]) << 8; [[fallthrough]];
        case 9:
            k2 ^= (uint64_t)(tail[8]) << 0;
            k2 *= c2;
            k2 = REI_rotl64(k2, 33);
            k2 *= c1;
            h2 ^= k2;
            [[fallthrough]];

        case 8: k1 ^= (uint64_t)(tail[7]) << 56; [[fallthrough]];
        case 7: k1 ^= (uint64_t)(tail[6]) << 48; [[fallthrough]];
        case 6: k1 ^= (uint64_t)(tail[5]) << 40; [[fallthrough]];
        case 5: k1 ^= (uint64_t)(tail[4]) << 32; [[fallthrough]];
        case 4: k1 ^= (uint64_t)(tail[3]) << 24; [[fallthrough]];
        case 3: k1 ^= (uint64_t)(tail[2]) << 16; [[fallthrough]];
        case 2: k1 ^= (uint64_t)(tail[1]) << 8; [[fallthrough]];
        case 1:
            k1 ^= (uint64_t)(tail[0]) << 0;
            k1 *= c1;
            k1 = REI_rotl64(k1, 31);
            k1 *= c2;
            h1 ^= k1;
    };

    //----------
    // finalization

    h1 ^= len;
    h2 ^= len;

    h1 += h2;
    h2 += h1;

    h1 = REI_fmix64(h1);
    h2 = REI_fmix64(h2);

    h1 += h2;
    h2 += h1;

    ((uint64_t*)out)[0] = h1;
    ((uint64_t*)out)[1] = h2;
}

static inline uint64_t REI_murmurHash2_x64_64(const void* key, int len, uint64_t seed)
{
    const uint64_t m = 0xc6a4a7935bd1e995;
    const int      r = 47;

    uint64_t h = seed ^ (len * m);

    const uint64_t* data = (const uint64_t*)key;
    const uint64_t* end = data + (len / 8);

    while (data != end)
    {
        uint64_t k = *data++;

        k *= m;
        k ^= k >> r;
        k *= m;

        h ^= k;
        h *= m;
    }

    const unsigned char* data2 = (const unsigned char*)data;

    switch (len & 7)
    {
        case 7: h ^= ((uint64_t)data2[6]) << 48; [[fallthrough]];
        case 6: h ^= ((uint64_t)data2[5]) << 40; [[fallthrough]];
        case 5: h ^= ((uint64_t)data2[4]) << 32; [[fallthrough]];
        case 4: h ^= ((uint64_t)data2[3]) << 24; [[fallthrough]];
        case 3: h ^= ((uint64_t)data2[2]) << 16; [[fallthrough]];
        case 2: h ^= ((uint64_t)data2[1]) << 8; [[fallthrough]];
        case 1: h ^= ((uint64_t)data2[0]); h *= m;
    };

    h ^= h >> r;
    h *= m;
    h ^= h >> r;

    return h;
}

#if REI_PTRSIZE == 8
#    define REI_murmurHash3_128 REI_murmurHash3_x64_128
#elif REI_PTRSIZE == 4
#    define REI_murmurHash3_128 REI_murmurHash3_x86_128
#else
#    define REI_murmurHash3_128
#endif

int               REI_FailedAssert(const char* file, int line, const char* statement, const char* message, ...);
static inline int REI_FailedAssert(const char* file, int line, const char* statement)
{
    return REI_FailedAssert(file, line, statement, 0);
}
void REI_DebugOutput(const char* str);
void REI_Print(const char* str);

#define REI_DEFAULT_MALLOC_ALIGNMENT 8

using REI_MallocPtr = void* (*)(void*, size_t, size_t);
using REI_FreePtr = void (*)(void*, void*);

struct REI_AllocatorCallbacks
{
    void*         pUserData;
    REI_MallocPtr pMalloc;
    REI_FreePtr   pFree;
};

static inline void* REI_calloc(const REI_AllocatorCallbacks& allocator, size_t size)
{
    void* ptr = allocator.pMalloc(allocator.pUserData, size, REI_DEFAULT_MALLOC_ALIGNMENT);
    memset(ptr, 0, size);
    return ptr;
}

template<typename T, typename... Args>
static inline T* REI_new(const REI_AllocatorCallbacks& allocator, Args&&... args)
{
    T* ptr = (T*)REI_calloc(allocator, sizeof(T));
    return new (ptr) T(std::forward<Args>(args)...);
}

template<typename T>
static inline void REI_delete(const REI_AllocatorCallbacks& allocator, T* ptr)
{
    if (ptr)
    {
        ptr->~T();
        allocator.pFree(allocator.pUserData, ptr);
    }
}

static inline void REI_setupAllocatorCallbacks(
    const REI_AllocatorCallbacks* pProvidedCallbacks, REI_AllocatorCallbacks& pResultCallbacks)
{
    bool fallbackOnDefault = pProvidedCallbacks == nullptr ||
                             (pProvidedCallbacks->pMalloc == nullptr && pProvidedCallbacks->pFree == nullptr);

    if (fallbackOnDefault)
    {
        pResultCallbacks.pUserData = nullptr;

#if _MSC_VER
        pResultCallbacks.pMalloc = [](void* userData, size_t size, size_t align)
        { return _aligned_malloc(size, align ? align : REI_DEFAULT_MALLOC_ALIGNMENT); };

        pResultCallbacks.pFree = [](void* userData, void* ptr) { return _aligned_free(ptr); };
#else
        pResultCallbacks.pMalloc = [](void* userData, size_t size, size_t align)
        {
            void* result = nullptr;
            posix_memalign(&result, align ? align : REI_DEFAULT_MALLOC_ALIGNMENT, size);
            return result;
        };

        pResultCallbacks.pFree = [](void* userData, void* ptr) { return free(ptr); };
#endif
    }
    else
    {
        // All callbacks must be set
        REI_ASSERT(pProvidedCallbacks->pMalloc);
        REI_ASSERT(pProvidedCallbacks->pFree);
        pResultCallbacks = *pProvidedCallbacks;
    }
}

static inline void REI_Log(REI_LogType level, const char* message, ...)
{
    const uint32_t BUFFER_SIZE = 1024;
    char           formattedMsg[BUFFER_SIZE + 2];

    const char* levelStr;
    switch (level)
    {
        case REI_LOG_TYPE_INFO: levelStr = "INFO| "; break;
        case REI_LOG_TYPE_WARNING: levelStr = "WARN| "; break;
        case REI_LOG_TYPE_DEBUG: levelStr = " DBG| "; break;
        case REI_LOG_TYPE_ERROR: levelStr = " ERR| "; break;
        default: levelStr = "NONE| "; break;
    }

#ifdef _MSC_VER
    strcpy_s(formattedMsg, levelStr);
#else
    strcpy(formattedMsg, levelStr);
#endif

    uint32_t offset = 6;

    va_list ap;
    va_start(ap, message);
    offset += vsnprintf(formattedMsg + offset, BUFFER_SIZE - offset, message, ap);
    va_end(ap);

    offset = (offset > BUFFER_SIZE) ? BUFFER_SIZE : offset;
    formattedMsg[offset] = '\n';
    formattedMsg[offset + 1] = 0;

    REI_Print(formattedMsg);
    REI_DebugOutput(formattedMsg);
}

#ifdef REI_DISABLE_STD_ALLOC
#    ifndef malloc
#        define malloc(size) static_assert(false, "Please use REI_malloc");
#    endif
#    ifndef calloc
#        define calloc(count, size) static_assert(false, "Please use REI_calloc");
#    endif
#    ifndef memalign
#        define memalign(align, size) static_assert(false, "Please use REI_memalign");
#    endif
#    ifndef realloc
#        define realloc(ptr, size) static_assert(false, "Please use REI_realloc");
#    endif
#    ifndef free
#        define free(ptr) static_assert(false, "Please use REI_free");
#    endif
#    ifndef new
#        define new static_assert(false, "Please use REI_placement_new");
#    endif
#    ifndef delete
#        define delete static_assert(false, "Please use REI_free with explicit destructor call");
#    endif
#endif

template<class TType>
class REI_allocator
{
    public:
    using value_type = TType;
    using size_type = size_t;
    using difference_type = ptrdiff_t;

    const REI_AllocatorCallbacks* pAllocator = nullptr;

    inline constexpr REI_allocator() = default;

    inline constexpr REI_allocator(const REI_AllocatorCallbacks& allocator) noexcept: pAllocator(&allocator) {}

    template<class TOther>
    inline constexpr REI_allocator(const REI_allocator<TOther>& other) noexcept
    {
        pAllocator = other.pAllocator;
    }

    template<class TOther>
    inline constexpr bool operator==(const REI_allocator<TOther>& other) noexcept
    {
        return pAllocator == other.pAllocator;
    }

    template<class TOther>
    inline constexpr bool operator!=(const REI_allocator<TOther>& other) noexcept
    {
        return pAllocator != other.pAllocator;
    }

    inline TType* allocate(const size_t count)
    {
        return reinterpret_cast<TType*>(pAllocator->pMalloc(pAllocator->pUserData, sizeof(TType) * count, 0));
    }

    inline void deallocate(TType* const ptr, const size_t count) { pAllocator->pFree(pAllocator->pUserData, ptr); }
};

template<class TType>
class REI_static_allocator
{
    public:
    using value_type = TType;
    using size_type = size_t;
    using difference_type = ptrdiff_t;

    inline constexpr REI_static_allocator() noexcept = default;

    template<class TOther>
    inline constexpr REI_static_allocator(const REI_static_allocator<TOther>&) noexcept
    {
    }

    template<class TOther>
    inline constexpr bool operator==(const REI_static_allocator<TOther>&) noexcept
    {
        return true;
    }

    template<class TOther>
    inline constexpr bool operator!=(const REI_static_allocator<TOther>&) noexcept
    {
        return false;
    }

    TType* allocate(const size_t count) { return reinterpret_cast<TType*>(malloc(sizeof(TType) * count)); }

    void deallocate(TType* const ptr, const size_t count) { free(ptr); }
};

template<bool dealocateOnDestruction>
struct REI_StackAllocator
{
    void*                         ptr;
    size_t                        size;
    size_t                        offset;
    const REI_AllocatorCallbacks* pAllocator;

    ~REI_StackAllocator()
    {
        if (dealocateOnDestruction)
        {
            if (pAllocator != nullptr)
                pAllocator->pFree(pAllocator->pUserData, ptr);
        }
    }

    template<typename T>
    REI_StackAllocator& reserve(size_t count = 1)
    {
        REI_ASSERT(ptr == nullptr, "Unable to reserve when memory has been allocated");
        size += sizeof(T) * count;
        return *this;
    }

    bool done(const REI_AllocatorCallbacks& allocator)
    {
        REI_ASSERT(ptr == nullptr, "Unable to allocate when memory has been allocated");
        if (size == 0)
            return true;

        if (dealocateOnDestruction)
            pAllocator = &allocator;

        ptr = allocator.pMalloc(allocator.pUserData, size, 0);

        REI_ASSERT(ptr);

        return ptr != nullptr;
    }

    template<typename T>
    T* allocZeroed(size_t count = 1)
    {
        REI_ASSERT(ptr != nullptr);

        size_t allocSize = count * sizeof(T);
        REI_ASSERT(offset + allocSize <= size, "Out of memory");

        T* result = (T*)((uint8_t*)ptr + offset);
        memset(result, 0, allocSize);

        offset += allocSize;

        return result;
    }

    template<typename T>
    T* alloc(size_t count = 1)
    {
        REI_ASSERT(ptr != nullptr);

        size_t allocSize = count * sizeof(T);
        REI_ASSERT(offset + allocSize <= size, "Out of memory");

        T* result = (T*)((uint8_t*)ptr + offset);

        offset += allocSize;

        return result;
    }

    template<typename T, typename... Args>
    T* construct(Args&&... args)
    {
        REI_ASSERT(ptr != nullptr);

        size_t allocSize = sizeof(T);
        REI_ASSERT(offset + allocSize <= size, "Out of memory");

        T* result = (T*)((uint8_t*)ptr + offset);
        new (result) T(std::forward<Args>(args)...);

        offset += allocSize;

        return result;
    }

    template<typename T, typename... Args>
    T* constructZeroed(Args&&... args)
    {
        REI_ASSERT(ptr != nullptr);

        size_t allocSize = sizeof(T);
        REI_ASSERT(offset + allocSize <= size, "Out of memory");

        T* result = (T*)((uint8_t*)ptr + offset);
        memset(result, 0, allocSize);
        new (result) T(std::forward<Args>(args)...);

        offset += allocSize;

        return result;
    }
};

struct REI_string: public std::basic_string<char, std::char_traits<char>, REI_allocator<char>>
{
    using Base = std::basic_string<char, std::char_traits<char>, REI_allocator<char>>;

    REI_string(const REI_string&) = default;
    REI_string& operator=(const REI_string&) = default;

    REI_string(REI_string&&) noexcept = default;
    REI_string& operator=(REI_string&&) noexcept = default;

    REI_string(const REI_allocator<char>& allocator): Base(allocator) {}

    REI_string(const char* str, const REI_allocator<char>& allocator): Base(str, allocator) {}
};

template<typename T>
using REI_vector = std::vector<T, REI_allocator<T>>;

template<typename TKey, typename TVal>
using REI_unordered_map =
    std::unordered_map<TKey, TVal, std::hash<TKey>, std::equal_to<TKey>, REI_allocator<std::pair<const TKey, TVal>>>;

template<typename T>
using REI_deque = std::deque<T, REI_allocator<T>>;

template<typename T>
struct REI_shared_ptr: public std::shared_ptr<T>
{
    using Base = std::shared_ptr<T>;
    template<class TData, class TDeleter>
    REI_shared_ptr(TData* pData, TDeleter deleter, REI_allocator<TData> allocator):
        Base::shared_ptr(pData, deleter, allocator)
    {
    }

    REI_shared_ptr(const REI_shared_ptr&) = default;
    REI_shared_ptr& operator=(const REI_shared_ptr&) = default;

    REI_shared_ptr(REI_shared_ptr&&) noexcept = default;
    REI_shared_ptr& operator=(REI_shared_ptr&&) noexcept = default;

    template<class TData, class TDeleter>
    void reset(TData* pData, TDeleter deleter, REI_allocator<TData> allocator)
    {
        Base::template reset<TData, TDeleter, REI_allocator<TData>>(pData, deleter, allocator);
    }
};

namespace std
{
template<>
struct std::hash<REI_string>
{
    inline std::size_t operator()(const REI_string& str) const
    {
        return std::hash<REI_string::Base>()(static_cast<const REI_string::Base&>(str));
    }
};
}    // namespace std
