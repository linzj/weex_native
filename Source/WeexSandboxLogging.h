#ifndef WEEXSANDBOXLOGGING_H
#define WEEXSANDBOXLOGGING_H
#include <android/log.h>
#define CASES SANDBOX_BPF_DSL_CASES
#define WEEX_SANDBOX_TAG "weex-sandbox"
#define SANDBOX_LOGE(...) __android_log_print(ANDROID_LOG_ERROR, WEEX_SANDBOX_TAG, __VA_ARGS__)
#define SANDBOX_LOGD(...) __android_log_print(ANDROID_LOG_DEBUG, WEEX_SANDBOX_TAG, __VA_ARGS__)
#endif /* WEEXSANDBOXLOGGING_H */
