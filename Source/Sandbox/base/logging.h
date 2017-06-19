#ifndef LOGGING_H
#define LOGGING_H
#include <stdlib.h>
#include "build/build_config.h"
class FakeStream
{
};

inline const FakeStream& operator << (const FakeStream& fake, const char*)
{
    return fake;
}

inline const FakeStream& operator << (const FakeStream& fake, int)
{
    return fake;
}

#define CHECK(pred) \
    ({ \
    FakeStream fake; \
    if (__builtin_expect(!(pred), false)) { \
        LOGE(#pred " generates false at %s:%d", __FILE__, __LINE__); \
        abort(); \
    };fake;})

#define CHECK_OP(op, lhs, rhs) \
    ({ \
    FakeStream fake; \
    if (__builtin_expect(!((lhs) op (rhs)), false)) { \
        LOGE(#lhs " is not " #op " rhs at %s:%d", __FILE__, __LINE__); \
        abort(); \
    };fake;})

#define SANDBOX_DEBUG 1
#if defined(SANDBOX_DEBUG) && SANDBOX_DEBUG
#define DCHECK(pred) \
    if (__builtin_expect(!(pred), false)) { \
        LOGE(#pred " generates false at %s:%d", __FILE__, __LINE__); \
        abort(); \
    }
#define DCHECK_OP(op, lhs, rhs) \
    ({ \
    FakeStream fake; \
    if (__builtin_expect(!((lhs) op (rhs)), false)) { \
        LOGE(#lhs " is not " #op " rhs at %s:%d", __FILE__, __LINE__); \
        abort(); \
    };fake;})
#else
#define DCHECK(pred) ({FakeStream fake;})
#define DCHECK_OP(op, lhs, rhs) ({FakeStream fake;})
#endif // SANDBOX_DEBUG

#define CHECK_NE(lhs, rhs) CHECK_OP(!=, lhs, rhs)
#define CHECK_EQ(lhs, rhs) CHECK_OP(==, lhs, rhs)
#define CHECK_LT(lhs, rhs) CHECK_OP(<, lhs, rhs)
#define CHECK_LE(lhs, rhs) CHECK_OP(<=, lhs, rhs)

#define DCHECK_EQ(lhs, rhs) DCHECK_OP(==, lhs, rhs)
#define DCHECK_LE(lhs, rhs) DCHECK_OP(<=, lhs, rhs)
#include <android/log.h>
#define TAG "SANDBOX"
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, TAG, __VA_ARGS__)
#define RAW_LOG(level, msg) __android_log_write(ANDROID_LOG_ERROR, TAG, msg)
#define NOTREACHED() __builtin_unreachable()
#endif /* LOGGING_H */
