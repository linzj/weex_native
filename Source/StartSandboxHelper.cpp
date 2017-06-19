#include "StartSandboxHelper.h"

#include "WeexSandboxLogging.h"
#include "WeexServerPolicy.h"
#include "sandbox/linux/seccomp-bpf/sandbox_bpf.h"

void startSandBox(void)
{
    if (!sandbox::SandboxBPF::SupportsSeccompSandbox(
            sandbox::SandboxBPF::SeccompLevel::MULTI_THREADED)) {
        SANDBOX_LOGE("Required sandbox function is no available in this machine");
        return;
    } else {
        SANDBOX_LOGD("Multithreaded sandbox available");
    }

    sandbox::SandboxBPF sandbox(new WeexServerPolicy());
    if (!sandbox.StartSandbox(
            sandbox::SandboxBPF::SeccompLevel::MULTI_THREADED)) {
        SANDBOX_LOGE("Sandbox failed to start");
    }
    SANDBOX_LOGD("Sandbox starts");
}
