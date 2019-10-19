//This file contains abstractions for compiler specific things
#pragma once

#include <stdint.h>
#include <stdlib.h>
#include <memory.h>

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
#    define REI_DEBUGBREAK() assert(false)
#endif

#define REI_OFFSETOF(type, mem) ((size_t)(&(((type*)0)->mem)))

#if defined(__ANDROID__) && !defined(REI_ANDROID_LOG_TAG)
#    define REI_ANDROID_LOG_TAG "REI"
#endif

enum
{
    LL_DEBUG,
    LL_INFO,
    LL_WARNING,
    LL_ERROR,
};

#ifdef _DEBUG
#    define REI_ASSERT(b, ...)                                                   \
        do                                                                       \
            if (!(b) && REI_FailedAssert(__FILE__, __LINE__, #b, ##__VA_ARGS__)) \
                REI_DEBUGBREAK();                                                \
        while (0)
#else
#    define REI_ASSERT(b, ...) REI_ASSUME(b)
#endif

#define REI_LOG(stream, msg, ...) REI_LogWrite((LL_##stream), __FILE__, __LINE__, msg, ##__VA_ARGS__)
#define REI_LOG_IF(condition, stream, msg, ...)                                  \
    do                                                                           \
        if (condition)                                                           \
            REI_LogWrite((LL_##stream), __FILE__, __LINE__, msg, ##__VA_ARGS__); \
    while (0)
#define REI_RAWLOG(msg, ...) REI_LogWriteRaw(msg, ##__VA_ARGS__)
#define REI_RAWLOG_IF(condition, msg, ...)       \
    do                                           \
        if (condition)                           \
            REI_LogWriteRaw(msg, ##__VA_ARGS__); \
    while (0)

#ifdef _DEBUG

#    define REI_DLOG(stream, msg, ...) REI_LOG(stream, msg, ##__VA_ARGS__)
#    define REI_DLOG_IF(condition, stream, msg, ...) REI_LOGF_IF(stream, condition, msg, ##__VA_ARGS__)
#    define REI_DRAWLOG(msg, ...) REI_RAWLOG(msg, ##__VA_ARGS__)
#    define REI_DRAWLOG_IF(condition, msg, ...) REI_RAWLOG_IF(condition, msg, ##__VA_ARGS__)

#else

#    define REI_DLOG(stream, msg, ...)
#    define REI_DLOG_IF(condition, stream, msg, ...)
#    define REI_DRAWLOG(msg, ...)
#    define REI_DRAWLOG_IF(condition, msg, ...)

#endif

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

int               REI_FailedAssert(const char* file, int line, const char* statement, const char* message, ...);
static inline int REI_FailedAssert(const char* file, int line, const char* statement)
{
    return REI_FailedAssert(file, line, statement, 0);
}

void REI_LogWrite(uint32_t stream, const char* file, int line, const char* message, ...);
void REI_LogWriteRaw(const char* message, ...);
void REI_LogAddOutput(void* userData, void (*outputFunc)(void* userPtr, const char* msg));
void REI_DebugOutput(const char* str);
void REI_Print(const char* str);

#ifndef REI_malloc
#    define REI_malloc(size) REI_malloc_impl(size, __FILE__, __LINE__, __FUNCTION__)
#endif
#ifndef REI_memalign
#    define REI_memalign(align, size) REI_memalign_impl(align, size, __FILE__, __LINE__, __FUNCTION__)
#endif
#ifndef REI_calloc
#    define REI_calloc(count, size) REI_calloc_impl(count, size, __FILE__, __LINE__, __FUNCTION__)
#endif
#ifndef REI_realloc
#    define REI_realloc(ptr, size) REI_realloc_impl(ptr, size, __FILE__, __LINE__, __FUNCTION__)
#endif
#ifndef REI_free
#    define REI_free(ptr) REI_free_impl(ptr, __FILE__, __LINE__, __FUNCTION__)
#endif
#ifndef REI_new
#    define REI_new(ObjectType, ...) REI_new_impl<ObjectType>(__FILE__, __LINE__, __FUNCTION__, ##__VA_ARGS__)
#endif
#ifndef REI_delete
#    define REI_delete(ptr) REI_delete_impl(ptr, __FILE__, __LINE__, __FUNCTION__)
#endif

#define REI_MIN_MALLOC_ALIGNMENT 8

#ifdef _MSC_VER
static inline void* REI_malloc_impl(size_t size, const char* /*f*/, int /*l*/, const char* /*sf*/)
{
    return _aligned_malloc(size, REI_MIN_MALLOC_ALIGNMENT);
}

static inline void* REI_calloc_impl(size_t count, size_t size, const char* f, int l, const char* sf)
{
    size_t sz = count * size;
    void*  ptr = REI_malloc_impl(sz, f, l, sf);
    memset(ptr, 0, sz);
    return ptr;
}

static inline void* REI_memalign_impl(size_t alignment, size_t size, const char* /*f*/, int /*l*/, const char* /*sf*/)
{
    return _aligned_malloc(size, alignment);
}

static inline void* REI_realloc_impl(void* ptr, size_t size, const char* /*f*/, int /*l*/, const char* /*sf*/)
{
    return _aligned_realloc(ptr, size, REI_MIN_MALLOC_ALIGNMENT);
}

static inline void REI_free_impl(void* ptr, const char* /*f*/, int /*l*/, const char* /*sf*/) { _aligned_free(ptr); }
#else
static inline void* REI_malloc_impl(size_t size, const char* /*f*/, int /*l*/, const char* /*sf*/)
{
    return malloc(size);
}

static inline void* REI_calloc_impl(size_t count, size_t size, const char* /*f*/, int /*l*/, const char* /*sf*/)
{
    return calloc(count, size);
}

static inline void* REI_memalign_impl(size_t alignment, size_t size, const char* /*f*/, int /*l*/, const char* /*sf*/)
{
    void* result;
    if (posix_memalign(&result, alignment, size))
        result = 0;
    return result;
}

static inline void* REI_realloc_impl(void* ptr, size_t size, const char* /*f*/, int /*l*/, const char* /*sf*/)
{
    return realloc(ptr, size);
}

static inline void REI_free_impl(void* ptr, const char* /*f*/, int /*l*/, const char* /*sf*/) { free(ptr); }
#endif

template<typename T, typename... Args>
static T* REI_new_impl(const char* f, int l, const char* sf, Args... args)
{
    T* ptr = (T*)REI_calloc_impl(1, sizeof(T), f, l, sf);
    return new (ptr) T(args...);
}

template<typename T>
static void REI_delete_impl(T* ptr, const char* f, int l, const char* sf)
{
    if (ptr)
    {
        ptr->~T();
        REI_free_impl(ptr, f, l, sf);
    }
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
