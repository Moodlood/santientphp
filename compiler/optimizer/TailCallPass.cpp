#include "TailCallPass.hpp"
#include "compiler/ir/IRBasicBlock.hpp"
#include "compiler/ir/IRInstruction.hpp"

namespace sph {

bool TailCallPass::runOnFunction(IRFunction& function) {
    bool changed = false;

    for (const auto& bb : function.getBlocks()) {
        const auto& insts = bb->getInstructions();
        if (insts.size() < 2) continue;

        // Check if last instruction is RetInst
        const auto& lastInst = insts.back();
        if (lastInst->getOpcode() != IROpcode::Ret) continue;

        const auto& prevInst = insts[insts.size() - 2];
        if (prevInst->getOpcode() != IROpcode::Call) continue;

        const auto& ret = static_cast<const RetInst&>(*lastInst);
        auto call = std::static_pointer_cast<CallInst>(prevInst);

        if (call->isTailCall()) continue;

        // If function returns void, call must be void
        if (function.getReturnType()->isVoid()) {
            call->setTailCall(true);
            changed = true;
        } else if (ret.hasValue() && ret.getValue().get() == call.get()) {
            // Returned value is exactly the call instruction's result
            call->setTailCall(true);
            changed = true;
        }
    }

    return changed;
}

} // namespace sph

