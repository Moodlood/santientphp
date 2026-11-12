#include "IRPrinter.hpp"
#include <iostream>

namespace sph {

std::string IRPrinter::formatValue(const IRValue& val) const {
    if (val.isConstant()) {
        return val.getName();
    }
    // Prefix variables and SSA temporaries with '%' if not already present
    if (!val.getName().empty() && val.getName().front() != '%') {
        return "%" + val.getName();
    }
    return val.getName();
}

void IRPrinter::print(const IRModule& module) {
    m_out << "module \"" << module.getName() << "\"\n\n";

    for (const auto& vt : module.getVTables()) {
        m_out << "vtable @" << vt.name << " = [";
        for (size_t i = 0; i < vt.functionNames.size(); ++i) {
            if (i > 0) m_out << ", ";
            m_out << "@" << vt.functionNames[i];
        }
        m_out << "]\n";
    }
    if (!module.getVTables().empty()) {
        m_out << "\n";
    }

    for (const auto& fn : module.getFunctions()) {
        print(*fn);
        m_out << "\n";
    }
}

void IRPrinter::print(const IRFunction& fn) {
    if (fn.isDeclaration()) {
        m_out << "declare @" << fn.getName() << "(";
        const auto& params = fn.getParameters();
        for (size_t i = 0; i < params.size(); ++i) {
            if (i > 0) m_out << ", ";
            m_out << formatValue(*params[i]) << ": " << params[i]->getType()->toString();
        }
        m_out << "): " << fn.getReturnType()->toString() << "\n";
        return;
    }

    m_out << "fn @" << fn.getName() << "(";
    const auto& params = fn.getParameters();
    for (size_t i = 0; i < params.size(); ++i) {
        if (i > 0) m_out << ", ";
        m_out << formatValue(*params[i]) << ": " << params[i]->getType()->toString();
    }
    m_out << "): " << fn.getReturnType()->toString() << " {\n";

    for (const auto& bb : fn.getBlocks()) {
        print(*bb);
    }

    m_out << "}\n";
}

void IRPrinter::print(const IRBasicBlock& bb) {
    m_out << bb.getName() << ":\n";
    for (const auto& inst : bb.getInstructions()) {
        m_out << "  ";
        print(*inst);
        m_out << "\n";
    }
}

void IRPrinter::print(const IRInstruction& inst) {
    switch (inst.getOpcode()) {
        case IROpcode::Alloca: {
            const auto& allocaInst = static_cast<const AllocaInst&>(inst);
            m_out << formatValue(allocaInst) << " = alloca " << allocaInst.getAllocatedType()->toString();
            break;
        }
        case IROpcode::Load: {
            const auto& loadInst = static_cast<const LoadInst&>(inst);
            m_out << formatValue(loadInst) << " = load " << loadInst.getType()->toString()
                  << ", " << formatValue(*loadInst.getPointer());
            break;
        }
        case IROpcode::Store: {
            const auto& storeInst = static_cast<const StoreInst&>(inst);
            m_out << "store " << storeInst.getValue()->getType()->toString() << " "
                  << formatValue(*storeInst.getValue()) << ", "
                  << formatValue(*storeInst.getPointer());
            break;
        }
        case IROpcode::GetFieldPtr: {
            const auto& gfp = static_cast<const GetFieldPtrInst&>(inst);
            m_out << formatValue(gfp) << " = getfieldptr " << formatValue(*gfp.getBasePtr())
                  << ", field " << gfp.getFieldIndex() << " (offset " << gfp.getByteOffset() << "): "
                  << gfp.getType()->toString();
            break;
        }
        case IROpcode::GetElementPtr: {
            const auto& gep = static_cast<const GetElementPtrInst&>(inst);
            m_out << formatValue(gep) << " = getelementptr " << formatValue(*gep.getBasePtr())
                  << "[" << formatValue(*gep.getIndex()) << "] (elem_size " << gep.getElementSize() << "): "
                  << gep.getType()->toString();
            break;
        }
        case IROpcode::AddressOf: {
            const auto& addrInst = static_cast<const AddressOfInst&>(inst);
            m_out << formatValue(addrInst) << " = addressof " << formatValue(*addrInst.getTarget())
                  << " : " << addrInst.getType()->toString();
            break;
        }
        case IROpcode::Add:
        case IROpcode::Sub:
        case IROpcode::Mul:
        case IROpcode::Div:
        case IROpcode::Mod:
        case IROpcode::Equal:
        case IROpcode::NotEqual:
        case IROpcode::Less:
        case IROpcode::LessEqual:
        case IROpcode::Greater:
        case IROpcode::GreaterEqual:
        case IROpcode::LogicalAnd:
        case IROpcode::LogicalOr:
        case IROpcode::BitwiseAnd:
        case IROpcode::BitwiseOr:
        case IROpcode::BitwiseXor:
        case IROpcode::ShiftLeft:
        case IROpcode::ShiftRight: {
            const auto& binInst = static_cast<const BinaryInst&>(inst);
            m_out << formatValue(binInst) << " = " << opcodeToString(binInst.getOpcode())
                  << " " << binInst.getLhs()->getType()->toString() << " "
                  << formatValue(*binInst.getLhs()) << ", " << formatValue(*binInst.getRhs());
            break;
        }
        case IROpcode::Negate:
        case IROpcode::LogicalNot:
        case IROpcode::BitwiseNot: {
            const auto& unInst = static_cast<const UnaryInst&>(inst);
            m_out << formatValue(unInst) << " = " << opcodeToString(unInst.getOpcode())
                  << " " << unInst.getOperand()->getType()->toString() << " "
                  << formatValue(*unInst.getOperand());
            break;
        }
        case IROpcode::Cast: {
            const auto& castInst = static_cast<const CastInst&>(inst);
            m_out << formatValue(castInst) << " = cast "
                  << castInst.getOperand()->getType()->toString() << " "
                  << formatValue(*castInst.getOperand()) << " to "
                  << castInst.getType()->toString();
            break;
        }
        case IROpcode::Ret: {
            const auto& retInst = static_cast<const RetInst&>(inst);
            if (retInst.hasValue()) {
                m_out << "ret " << retInst.getValue()->getType()->toString() << " "
                      << formatValue(*retInst.getValue());
            } else {
                m_out << "ret void";
            }
            break;
        }
        case IROpcode::Br: {
            const auto& brInst = static_cast<const BranchInst&>(inst);
            m_out << "br label %" << (brInst.getTarget() ? brInst.getTarget()->getName() : "<null>");
            break;
        }
        case IROpcode::CondBr: {
            const auto& condBr = static_cast<const CondBranchInst&>(inst);
            m_out << "cond_br " << formatValue(*condBr.getCondition())
                  << ", label %" << (condBr.getTrueTarget() ? condBr.getTrueTarget()->getName() : "<null>")
                  << ", label %" << (condBr.getFalseTarget() ? condBr.getFalseTarget()->getName() : "<null>");
            break;
        }
        case IROpcode::Call: {
            const auto& callInst = static_cast<const CallInst&>(inst);
            if (!callInst.getType()->isVoid()) {
                m_out << formatValue(callInst) << " = ";
            }
            if (callInst.isIndirect()) {
                m_out << "call " << callInst.getType()->toString() << " " << formatValue(*callInst.getCalleeValue()) << "(";
            } else {
                m_out << "call " << callInst.getType()->toString() << " @" << callInst.getCallee() << "(";
            }
            const auto& args = callInst.getArgs();
            for (size_t i = 0; i < args.size(); ++i) {
                if (i > 0) m_out << ", ";
                m_out << args[i]->getType()->toString() << " " << formatValue(*args[i]);
            }
            m_out << ")";
            break;
        }
        case IROpcode::RcAlloc: {
            const auto& rcAlloc = static_cast<const RcAllocInst&>(inst);
            m_out << formatValue(rcAlloc) << " = rc_alloc " << rcAlloc.getInnerType()->toString()
                  << " (" << rcAlloc.getAllocSize() << " bytes)";
            break;
        }
        case IROpcode::RcRetain: {
            const auto& rcRetain = static_cast<const RcRetainInst&>(inst);
            m_out << "rc_retain " << rcRetain.getPtr()->getType()->toString() << " " << formatValue(*rcRetain.getPtr());
            break;
        }
        case IROpcode::RcRelease: {
            const auto& rcRelease = static_cast<const RcReleaseInst&>(inst);
            m_out << "rc_release " << rcRelease.getPtr()->getType()->toString() << " " << formatValue(*rcRelease.getPtr());
            break;
        }
        case IROpcode::SetJmp: {
            const auto& sj = static_cast<const SetJmpInst&>(inst);
            m_out << "%" << inst.getName() << " = setjmp " << formatValue(*sj.getFramePtr());
            break;
        }
    }
}

} // namespace sph

