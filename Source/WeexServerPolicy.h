#ifndef WEEXSERVERPOLICY_H
#define WEEXSERVERPOLICY_H
#include "sandbox/linux/bpf_dsl/policy.h"
class WeexServerPolicy : public sandbox::bpf_dsl::Policy {
private:
    sandbox::bpf_dsl::ResultExpr EvaluateSyscall(int sysno) const override;
    sandbox::bpf_dsl::ResultExpr ConditionalClone() const;
    sandbox::bpf_dsl::ResultExpr ConditionalPrctl() const;
};
#endif /* WEEXSERVERPOLICY_H */
