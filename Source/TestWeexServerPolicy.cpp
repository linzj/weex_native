#include <pthread.h>
#include <stdio.h>
#include <sys/socket.h>

#include "StartSandboxHelper.h"
#include "WeexSandboxLogging.h"

static void* threadEntry(void*)
{
    startSandBox();
    return nullptr;
}

int main()
{
    pthread_t mythread;
    void* result;
    SANDBOX_LOGD("Testing");
    pthread_create(&mythread, nullptr, threadEntry, nullptr);
    pthread_join(mythread, &result);
    pthread_detach(mythread);
    // test open
    if (fopen("/sdcard/1.txt", "r")) {
        SANDBOX_LOGE("should not open");
    }
    int sock;
    sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock != -1) {
        SANDBOX_LOGE("should not open a socket");
    }
}
