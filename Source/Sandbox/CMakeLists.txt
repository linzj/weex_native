cmake_minimum_required(VERSION 2.8.12)

add_library(Sandbox
STATIC
sandbox/linux/bpf_dsl/syscall_set.cc
sandbox/linux/bpf_dsl/policy.cc
sandbox/linux/bpf_dsl/verifier.cc
sandbox/linux/bpf_dsl/bpf_dsl.cc
sandbox/linux/bpf_dsl/codegen.cc
sandbox/linux/bpf_dsl/policy_compiler.cc
sandbox/linux/seccomp-bpf/die.cc
sandbox/linux/seccomp-bpf/syscall.cc
sandbox/linux/seccomp-bpf/sandbox_bpf.cc
sandbox/linux/seccomp-bpf/trap.cc
base/scoped_file.cc
)

target_include_directories(Sandbox
PUBLIC
.)
