#include "WeexServerPolicy.h"
#include "WeexSandboxLogging.h"
#include "sandbox/linux/bpf_dsl/bpf_dsl.h"
#include "sandbox/linux/bpf_dsl/trap_registry.h"
#include <errno.h>
#include <sched.h>
#include <sys/prctl.h>
#include <sys/syscall.h>
#include <unistd.h>

#if !defined(PR_SET_VMA)
#define PR_SET_VMA 0x53564d41
#endif

using namespace sandbox;
intptr_t CrashSIGSYS_Handler(const struct arch_seccomp_data& args, void* aux)
{
    uint32_t syscall = args.nr;
    SANDBOX_LOGE("syscall %d is not allowed, arg: %llx %llx %llx %llx %llx %llx"
            , syscall
            , args.args[0]
            , args.args[1]
            , args.args[2]
            , args.args[3]
            , args.args[4]
            , args.args[5]);
    _exit(1);
}

intptr_t SIGSYSCloneFailure(const struct arch_seccomp_data& args, void* aux)
{
    volatile uint64_t clone_flags = args.args[0];

    SANDBOX_LOGE("clone flag %llx is not allowed", clone_flags);
    _exit(1);
}

intptr_t SIGSYSPrctlFailure(const struct arch_seccomp_data& args, void* aux)
{
    volatile uint64_t option = args.args[0];

    SANDBOX_LOGE("prctl option %llu is not allowed", option);
    _exit(1);
}

static bpf_dsl::ResultExpr CrashSIGSYS()
{
    return bpf_dsl::Trap(CrashSIGSYS_Handler, NULL);
}

static bpf_dsl::ResultExpr CrashSIGSYSClone()
{
    return bpf_dsl::Trap(SIGSYSCloneFailure, NULL);
}

static bpf_dsl::ResultExpr CrashSIGSYSPrctl()
{
    return bpf_dsl::Trap(SIGSYSPrctlFailure, NULL);
}

sandbox::bpf_dsl::ResultExpr WeexServerPolicy::EvaluateSyscall(int sysno) const
{
    switch (sysno) {
#if defined(__arm__)
    case __ARM_NR_cacheflush:
#endif
    case __NR_clock_gettime:
    case __NR_exit:
    case __NR_exit_group:
    case __NR_futex:
    case __NR_gettimeofday:
    case __NR_madvise:
    case __NR_mmap2:
    case __NR_mprotect:
    case __NR_munmap:
    case __NR_rt_sigprocmask:
    case __NR_sched_yield:
    case __NR_set_tid_address:
    case __NR_sigaltstack:
    case __NR_brk:
    case __NR_mremap:
    // for crash handler
    case __NR_mincore:
    // for crash handler read maps file.
    case __NR_read:
    // for systrace support.
    case __NR_write:
    case __NR_writev:
        return bpf_dsl::Allow();
    case __NR_clone:
        return ConditionalClone();
    case __NR_prctl:
        return ConditionalPrctl();
    // compatible with logcat.
    case __NR_openat:
    case __NR_close:
#if defined(__arm__)
    case __NR_socket:
#elif defined(__i386__)
    case __NR_socketcall:
#endif
        return bpf_dsl::Error(EPERM);
    default:
        break;
    }
    return CrashSIGSYS();
}

sandbox::bpf_dsl::ResultExpr WeexServerPolicy::ConditionalClone() const
{
    const bpf_dsl::Arg<unsigned long> flags(0);

    // TODO(mdempsky): Extend DSL to support (flags & ~mask1) == mask2.
    const uint64_t kAndroidCloneMask = CLONE_VM | CLONE_FS | CLONE_FILES | CLONE_SIGHAND | CLONE_THREAD | CLONE_SYSVSEM | CLONE_SETTLS | CLONE_PARENT_SETTID | CLONE_CHILD_CLEARTID;
    const uint64_t kObsoleteAndroidCloneMask = kAndroidCloneMask | CLONE_DETACHED;

    const bpf_dsl::BoolExpr android_test = AnyOf(flags == kAndroidCloneMask, flags == kObsoleteAndroidCloneMask);

    return bpf_dsl::If(android_test, bpf_dsl::Allow())
        .ElseIf((flags & (CLONE_VM | CLONE_THREAD)) == 0, bpf_dsl::Error(EPERM))
        .Else(CrashSIGSYSClone());
}

sandbox::bpf_dsl::ResultExpr WeexServerPolicy::ConditionalPrctl() const
{
    // Will need to add seccomp compositing in the future. PR_SET_PTRACER is
    // used by breakpad but not needed anymore.
    const bpf_dsl::Arg<int> option(0);
    return bpf_dsl::Switch(option)
        .CASES((PR_GET_NAME, PR_SET_NAME, PR_GET_DUMPABLE, PR_SET_DUMPABLE, PR_SET_VMA, PR_SET_PTRACER),
            bpf_dsl::Allow())
        .Default(CrashSIGSYSPrctl());
}
