#include "X86_64CodeGen.hpp"
#include <algorithm>
#include <cstring>
#include <iostream>
#include <sstream>
#include <vector>

#define m_out (*m_pOut)

namespace sph {

const char* X86_64CodeGen::getParamReg(size_t index) const noexcept {
    if (isSysV()) {
        static const char* sysvRegs[] = {"rdi", "rsi", "rdx", "rcx", "r8", "r9"};
        if (index < 6) return sysvRegs[index];
    } else {
        static const char* winRegs[] = {"rcx", "rdx", "r8", "r9"};
        if (index < 4) return winRegs[index];
    }
    return "";
}

std::string X86_64CodeGen::getReg32(const std::string& reg64) const {
    if (reg64 == "rdi") return "edi";
    if (reg64 == "rsi") return "esi";
    if (reg64 == "rdx") return "edx";
    if (reg64 == "rcx") return "ecx";
    if (reg64 == "r8") return "r8d";
    if (reg64 == "r9") return "r9d";
    if (reg64 == "rax") return "eax";
    if (reg64 == "r10") return "r10d";
    return reg64;
}

std::string X86_64CodeGen::memRef(int32_t localOffset) const {
    if (m_useFramePointer) {
        return "[rbp - " + std::to_string(localOffset) + "]";
    }
    return "[rsp + " + std::to_string(m_rspAllocSize - localOffset) + "]";
}

std::string X86_64CodeGen::argStackRef(int32_t rbpPlusOffset) const {
    if (m_useFramePointer) {
        return "[rbp + " + std::to_string(rbpPlusOffset) + "]";
    }
    int32_t rspOff = m_rspAllocSize + static_cast<int32_t>(m_calleePushCount * 8) + (rbpPlusOffset - 8);
    return "[rsp + " + std::to_string(rspOff) + "]";
}

int32_t X86_64CodeGen::getSlotOffset(const IRValue& val) {
    auto it = m_slots.find(&val);
    if (it != m_slots.end()) {
        return it->second;
    }
    m_currentOffset += 8;
    m_slots[&val] = m_currentOffset;
    return m_currentOffset;
}

void X86_64CodeGen::loadValueToReg(const IRValue& val, const std::string& reg) {
    if (val.isConstant()) {
        if (const auto* global = dynamic_cast<const ConstantGlobal*>(&val)) {
            m_out << "  lea " << reg << ", [rip + " << global->getGlobalName() << "]\n";
            return;
        }
        if (val.getType()->isBoolean()) {
            int num = (val.getName() == "true") ? 1 : 0;
            m_out << "  mov " << reg << ", " << num << "\n";
        } else if (val.getType()->isString()) {
            if (const auto* str = dynamic_cast<const ConstantString*>(&val)) {
                auto it = m_stringLabels.find(str->getValue());
                if (it != m_stringLabels.end()) {
                    m_out << "  lea " << reg << ", [rip + " << it->second << "]\n";
                    return;
                }
            }
            m_out << "  mov " << reg << ", 0\n";
        } else if (val.getType()->isFloatingPoint()) {
            if (val.getType()->isF32()) {
                float f = 0.0f;
                try { f = std::stof(val.getName()); } catch (...) {}
                uint32_t rawBits = 0;
                std::memcpy(&rawBits, &f, sizeof(f));
                m_out << "  mov " << reg << ", " << rawBits << "\n";
            } else {
                double d = 0.0;
                try { d = std::stod(val.getName()); } catch (...) {}
                uint64_t rawBits = 0;
                std::memcpy(&rawBits, &d, sizeof(d));
                m_out << "  mov " << reg << ", " << rawBits << "\n";
            }
        } else {
            m_out << "  mov " << reg << ", " << val.getName() << "\n";
        }
        return;
    }

    if (m_lsra && m_lsra->hasRegister(&val)) {
        const auto& physReg = m_lsra->getRegister(&val);
        if (physReg != reg) {
            m_out << "  mov " << reg << ", " << physReg << "\n";
        }
        return;
    }

    bool isAllocaOrBigStructCall = val.isInstruction() &&
        (static_cast<const IRInstruction&>(val).getOpcode() == IROpcode::Alloca ||
         (static_cast<const IRInstruction&>(val).getOpcode() == IROpcode::Call &&
          val.getType()->isStruct() &&
          std::static_pointer_cast<StructType>(val.getType())->getSize() > 8 &&
          static_cast<const CallInst&>(val).getCallee().rfind("sph_", 0) != 0));
    if (isAllocaOrBigStructCall) {
        int32_t offset = getSlotOffset(val);
        m_out << "  lea " << reg << ", " << memRef(offset) << "\n";
        return;
    }

    int32_t offset = getSlotOffset(val);
    m_out << "  mov " << reg << ", " << memRef(offset) << "\n";
}

void X86_64CodeGen::storeRegToSlot(const std::string& reg, int32_t offset) {
    m_out << "  mov " << memRef(offset) << ", " << reg << "\n";
}

void X86_64CodeGen::storeRegToValue(const std::string& reg, const IRValue& val) {
    if (m_lsra && m_lsra->hasRegister(&val)) {
        const auto& physReg = m_lsra->getRegister(&val);
        if (physReg != reg) {
            m_out << "  mov " << physReg << ", " << reg << "\n";
        }
        return;
    }
    int32_t offset = getSlotOffset(val);
    storeRegToSlot(reg, offset);
}

void X86_64CodeGen::emitEpilogue(const IRFunction& /*fn*/) {
    if (m_useFramePointer) {
        for (const auto& [reg, offset] : m_calleeSavedSlots) {
            m_out << "  mov " << reg << ", [rbp - " << offset << "]\n";
        }
        m_out << "  mov rsp, rbp\n";
        m_out << "  pop rbp\n";
        m_out << "  ret\n";
    } else {
        if (m_rspAllocSize > 0) {
            m_out << "  add rsp, " << m_rspAllocSize << "\n";
        }
        for (auto it = m_usedCalleeSaved.rbegin(); it != m_usedCalleeSaved.rend(); ++it) {
            m_out << "  pop " << *it << "\n";
        }
        m_out << "  ret\n";
    }
}

std::string X86_64CodeGen::getLabel(const IRBasicBlock& bb, const IRFunction& fn) const {
    return "." + fn.getName() + "_" + bb.getName();
}

void X86_64CodeGen::generate(const IRModule& module) {
    m_out << ".intel_syntax noprefix\n";

    if (m_debugInfo) {
        for (const auto& [fileId, path] : module.getSourceFiles()) {
            std::string normPath = path;
            for (char& c : normPath) {
                if (c == '\\') c = '/';
            }
            m_out << "  .file " << fileId << " \"" << normPath << "\"\n";
        }
    }

    m_stringLabels.clear();
    int strCounter = 0;
    auto recordString = [&](const IRValue* val) {
        if (!val) return;
        if (const auto* str = dynamic_cast<const ConstantString*>(val)) {
            if (m_stringLabels.find(str->getValue()) == m_stringLabels.end()) {
                m_stringLabels[str->getValue()] = ".Lstr_" + std::to_string(strCounter++);
            }
        }
    };

    for (const auto& fn : module.getFunctions()) {
        for (const auto& bb : fn->getBlocks()) {
            for (const auto& inst : bb->getInstructions()) {
                for (const auto* op : getInstructionOperands(*inst)) {
                    recordString(op);
                }
            }
        }
    }

    if (!module.getVTables().empty() || !m_stringLabels.empty()) {
        m_out << m_target.getRoDataSectionDirective() << "\n";
        for (const auto& vt : module.getVTables()) {
            m_out << vt.name << ":\n";
            for (const auto& fnName : vt.functionNames) {
                m_out << "  .quad " << fnName << "\n";
            }
        }
        for (const auto& [strVal, lbl] : m_stringLabels) {
            m_out << lbl << ":\n";
            m_out << "  .asciz \"";
            for (char c : strVal) {
                if (c == '"') m_out << "\\\"";
                else if (c == '\\') m_out << "\\\\";
                else if (c == '\n') m_out << "\\n";
                else if (c == '\t') m_out << "\\t";
                else if (c == '\r') m_out << "\\r";
                else m_out << c;
            }
            m_out << "\"\n";
        }
        m_out << "\n";
    }

    m_out << ".text\n\n";

    for (const auto& fn : module.getFunctions()) {
        if (!fn->isDeclaration()) {
            m_out << ".globl " << fn->getName() << "\n";
            std::string typeDir = m_target.getFunctionTypeDirective(fn->getName());
            if (!typeDir.empty()) {
                m_out << typeDir << "\n";
            }
        }
    }
    m_out << "\n";

    for (const auto& fn : module.getFunctions()) {
        if (!fn->isDeclaration()) {
            generateFunction(*fn);
            m_out << "\n";
        }
    }
}

void X86_64CodeGen::generateFunction(const IRFunction& fn) {
    m_slots.clear();
    m_currentOffset = 0;
    m_useFramePointer = (m_optLevel == OptimizationLevel::O0);
    m_rspAllocSize = 0;
    m_calleePushCount = 0;

    m_lsra = std::make_unique<LinearScanAllocator>(fn, isSysV());
    m_lsra->allocate();
    m_usedCalleeSaved = m_lsra->getUsedCalleeSavedRegs();
    m_calleeSavedSlots.clear();
    if (m_useFramePointer) {
        for (const auto& reg : m_usedCalleeSaved) {
            m_currentOffset += 8;
            m_calleeSavedSlots[reg] = m_currentOffset;
        }
    }

    m_currentFnReturnsSret = false;
    m_currentFnSretSlot = 0;
    m_currentFnSretSize = 0;
    if (fn.getReturnType() && fn.getReturnType()->isStruct()) {
        auto structType = std::static_pointer_cast<StructType>(fn.getReturnType());
        if (structType->getSize() > 8) {
            m_currentFnReturnsSret = true;
            m_currentFnSretSize = static_cast<int32_t>(structType->getSize());
            m_currentOffset += 8;
            m_currentFnSretSlot = m_currentOffset;
        }
    }

    // Reserve stack slots only for parameters not permanently assigned a register
    const auto& params = fn.getParameters();
    for (const auto& param : params) {
        if (param->getType()->isFloatingPoint() || !m_lsra || !m_lsra->hasRegister(param.get())) {
            getSlotOffset(*param);
        }
    }

    bool hasCalls = false;
    bool hasTco = false;
    size_t maxCallArgs = 0;
    size_t maxPhiCopies = 0;
    for (const auto& bb : fn.getBlocks()) {
        size_t bbPhiCount = 0;
        for (const auto& inst : bb->getInstructions()) {
            auto op = inst->getOpcode();
            if (op == IROpcode::Phi) {
                bbPhiCount++;
                if (inst->getType()->isFloatingPoint() || !m_lsra || !m_lsra->hasRegister(inst.get())) {
                    getSlotOffset(*inst);
                }
                continue;
            }
            if (op == IROpcode::Call) {
                const auto& callInst = static_cast<const CallInst&>(*inst);
                if (callInst.isTailCall() && !callInst.isIndirect() && callInst.getCallee() == fn.getName()) {
                    hasTco = true;
                } else {
                    hasCalls = true;
                }
                bool isSret = callInst.getType()->isStruct() &&
                    std::static_pointer_cast<StructType>(callInst.getType())->getSize() > 8 &&
                    callInst.getCallee().rfind("sph_", 0) != 0;
                size_t callArgCount = callInst.getArgs().size() + (isSret ? 1 : 0);
                if (callArgCount > maxCallArgs) {
                    maxCallArgs = callArgCount;
                }
            } else if (op == IROpcode::RcAlloc || op == IROpcode::RcRetain ||
                       op == IROpcode::RcRelease || op == IROpcode::ArcAlloc ||
                       op == IROpcode::ArcRetain || op == IROpcode::ArcRelease ||
                       op == IROpcode::SetJmp) {
                hasCalls = true;
                if (maxCallArgs < 1) maxCallArgs = 1;
            }
            if (op == IROpcode::Alloca) {
                const auto& allocaInst = static_cast<const AllocaInst&>(*inst);
                const auto& allocTy = allocaInst.getAllocatedType();
                int32_t size = allocTy ? static_cast<int32_t>(allocTy->getSize()) : 8;
                if (allocTy && (allocTy->isSlice() || allocTy->isInterface()) && size < 16) {
                    size = 16;
                }
                if (size < 8) size = 8;
                size = (size + 7) & ~7;
                m_currentOffset += size;
                m_slots[inst.get()] = m_currentOffset;
            } else if (op == IROpcode::Call) {
                const auto& callInst = static_cast<const CallInst&>(*inst);
                bool isSret = callInst.getType()->isStruct() &&
                    std::static_pointer_cast<StructType>(callInst.getType())->getSize() > 8 &&
                    callInst.getCallee().rfind("sph_", 0) != 0;
                if (isSret) {
                    auto structType = std::static_pointer_cast<StructType>(callInst.getType());
                    int32_t size = static_cast<int32_t>(structType->getSize());
                    size = (size + 7) & ~7;
                    m_currentOffset += size;
                    m_slots[inst.get()] = m_currentOffset;
                } else if (!inst->getType()->isVoid() &&
                           (inst->getType()->isFloatingPoint() || !m_lsra || !m_lsra->hasRegister(inst.get()))) {
                    getSlotOffset(*inst);
                }
            } else if (op == IROpcode::Load || op == IROpcode::GetFieldPtr ||
                       op == IROpcode::GetElementPtr || op == IROpcode::AddressOf ||
                       op == IROpcode::SetJmp || op == IROpcode::RcAlloc ||
                       op == IROpcode::ArcAlloc ||
                       (op >= IROpcode::Add && op <= IROpcode::Cast)) {
                if (inst->getType()->isFloatingPoint() || !m_lsra || !m_lsra->hasRegister(inst.get())) {
                    getSlotOffset(*inst);
                }
            }
        }
        if (bbPhiCount > maxPhiCopies) {
            maxPhiCopies = bbPhiCount;
        }
    }

    m_tcoScratchOffset = m_currentOffset;
    size_t scratchSlots = 0;
    if (hasTco) {
        scratchSlots = std::max<size_t>(std::max<size_t>(6, params.size()), maxPhiCopies);
    } else if (maxPhiCopies > 1) {
        scratchSlots = maxPhiCopies;
    }
    m_currentOffset += static_cast<int32_t>(scratchSlots * 8);

    size_t maxRegs = getMaxRegisterParams();
    int32_t outgoingSpace = 0;
    if (hasCalls) {
        if (isSysV()) {
            outgoingSpace = (maxCallArgs > maxRegs ? static_cast<int32_t>((maxCallArgs - maxRegs) * 8) : 0);
        } else if (isInternalFastCC(fn)) {
            outgoingSpace = (maxCallArgs > 6 ? static_cast<int32_t>((maxCallArgs - 6) * 8) : 0);
        } else {
            outgoingSpace = 32 + (maxCallArgs > 4 ? static_cast<int32_t>((maxCallArgs - 4) * 8) : 0);
        }
    }

    m_out << fn.getName() << ":\n";
    if (m_debugInfo && fn.getLocation().isValid()) {
        m_out << "  .loc " << fn.getLocation().fileId << " "
              << fn.getLocation().line << " "
              << fn.getLocation().column << "\n";
        m_lastEmittedLoc = fn.getLocation();
    }
    if (m_debugInfo) {
        m_out << "  .cfi_startproc\n";
    }

    if (m_useFramePointer) {
        m_frameSize = m_currentOffset + outgoingSpace;
        if (m_frameSize % 16 != 0) {
            m_frameSize += 16 - (m_frameSize % 16);
        }
        m_out << "  push rbp\n";
        if (m_debugInfo) {
            m_out << "  .cfi_def_cfa_offset 16\n";
            m_out << "  .cfi_offset rbp, -16\n";
        }
        m_out << "  mov rbp, rsp\n";
        if (m_debugInfo) {
            m_out << "  .cfi_def_cfa_register rbp\n";
        }
        m_out << "  sub rsp, " << m_frameSize << "\n";

        for (const auto& [reg, offset] : m_calleeSavedSlots) {
            m_out << "  mov [rbp - " << offset << "], " << reg << "\n";
        }
    } else {
        m_calleePushCount = m_usedCalleeSaved.size();
        for (const auto& reg : m_usedCalleeSaved) {
            m_out << "  push " << reg << "\n";
        }
        m_rspAllocSize = m_currentOffset + outgoingSpace;
        if (hasCalls || m_rspAllocSize > 0) {
            size_t totalDepth = 8 + m_calleePushCount * 8 + static_cast<size_t>(m_rspAllocSize);
            if (totalDepth % 16 != 0) {
                m_rspAllocSize += static_cast<int32_t>(16 - (totalDepth % 16));
            }
        }
        if (m_rspAllocSize > 0) {
            m_out << "  sub rsp, " << m_rspAllocSize << "\n";
        }
    }

    // Copy incoming parameters into slots or directly into LSRA registers (conflict-free order)
    if (m_currentFnReturnsSret) {
        storeRegToSlot(getParamReg(0), m_currentFnSretSlot);
    }
    struct RegMove {
        std::string dst;
        std::string src;
    };
    std::vector<RegMove> pendingParamMoves;
    auto resolveParamMoves = [&]() {
        while (!pendingParamMoves.empty()) {
            auto it = std::find_if(pendingParamMoves.begin(), pendingParamMoves.end(), [&](const RegMove& m) {
                for (const auto& other : pendingParamMoves) {
                    if (&other != &m && other.src == m.dst) return false;
                }
                return true;
            });
            if (it != pendingParamMoves.end()) {
                if (it->dst != it->src) {
                    m_out << "  mov " << it->dst << ", " << it->src << "\n";
                }
                pendingParamMoves.erase(it);
            } else {
                // Break permutation cycle (e.g. r8 <-> r9) via rax
                std::string cycleSrc = pendingParamMoves[0].src;
                std::string cycleDst = pendingParamMoves[0].dst;
                m_out << "  mov rax, " << cycleDst << "\n";
                m_out << "  mov " << cycleDst << ", " << cycleSrc << "\n";
                for (auto& rem : pendingParamMoves) {
                    if (rem.src == cycleDst) rem.src = "rax";
                }
                pendingParamMoves.erase(pendingParamMoves.begin());
            }
        }
    };

    if (isSysV()) {
        struct PendingStackParam {
            const IRValue* val;
            int32_t stackOffset;
            bool isFloat;
        };
        std::vector<PendingStackParam> pendingStackParams;
        size_t intRegIdx = m_currentFnReturnsSret ? 1 : 0;
        size_t floatRegIdx = 0;
        size_t stackParamIdx = 0;
        for (size_t i = 0; i < params.size(); ++i) {
            bool isFloat = params[i]->getType()->isFloatingPoint();
            if (isFloat && floatRegIdx < 8) {
                int32_t offset = getSlotOffset(*params[i]);
                std::string destMem = memRef(offset);
                if (params[i]->getType()->isF32()) {
                    m_out << "  movss dword ptr " << destMem << ", xmm" << floatRegIdx++ << "\n";
                } else {
                    m_out << "  movsd qword ptr " << destMem << ", xmm" << floatRegIdx++ << "\n";
                }
            } else if (!isFloat && intRegIdx < 6) {
                const char* srcReg = getParamReg(intRegIdx++);
                if (m_lsra && m_lsra->hasRegister(params[i].get())) {
                    pendingParamMoves.push_back({m_lsra->getRegister(params[i].get()), srcReg});
                } else {
                    storeRegToSlot(srcReg, getSlotOffset(*params[i]));
                }
            } else {
                int32_t stackOffset = 16 + static_cast<int32_t>(stackParamIdx++ * 8);
                pendingStackParams.push_back({params[i].get(), stackOffset, isFloat});
            }
        }
        resolveParamMoves();
        // Load stack arguments only after incoming register params (e.g. r9) are moved
        // to their allocated destinations so stack-param register assignments don't clobber them.
        for (const auto& sp : pendingStackParams) {
            if (m_lsra && !sp.isFloat && m_lsra->hasRegister(sp.val)) {
                m_out << "  mov " << m_lsra->getRegister(sp.val) << ", " << argStackRef(sp.stackOffset) << "\n";
            } else {
                m_out << "  mov rax, " << argStackRef(sp.stackOffset) << "\n";
                storeRegToSlot("rax", getSlotOffset(*sp.val));
            }
        }
    } else {
        size_t regOffset = m_currentFnReturnsSret ? 1 : 0;
        for (size_t i = 0; i < params.size() && (i + regOffset) < maxRegs; ++i) {
            bool isFloat = params[i]->getType()->isFloatingPoint();
            if (isFloat) {
                int32_t offset = getSlotOffset(*params[i]);
                std::string destMem = memRef(offset);
                if (params[i]->getType()->isF32()) {
                    m_out << "  movss dword ptr " << destMem << ", xmm" << (i + regOffset) << "\n";
                } else {
                    m_out << "  movsd qword ptr " << destMem << ", xmm" << (i + regOffset) << "\n";
                }
            } else {
                const char* srcReg = getParamReg(i + regOffset);
                if (m_lsra && m_lsra->hasRegister(params[i].get())) {
                    pendingParamMoves.push_back({m_lsra->getRegister(params[i].get()), srcReg});
                } else {
                    storeRegToSlot(srcReg, getSlotOffset(*params[i]));
                }
            }
        }
        resolveParamMoves();
        for (size_t i = (maxRegs > regOffset ? maxRegs - regOffset : 0); i < params.size(); ++i) {
            int32_t stackOffset = 48 + static_cast<int32_t>((i + regOffset - maxRegs) * 8);
            if (m_lsra && !params[i]->getType()->isFloatingPoint() && m_lsra->hasRegister(params[i].get())) {
                m_out << "  mov " << m_lsra->getRegister(params[i].get()) << ", " << argStackRef(stackOffset) << "\n";
            } else {
                m_out << "  mov rax, " << argStackRef(stackOffset) << "\n";
                storeRegToSlot("rax", getSlotOffset(*params[i]));
            }
        }
    }

    // Entry loop label for self-recursion TCO
    m_out << "." << fn.getName() << "_entry_loop:\n";

    const auto& blocks = fn.getBlocks();
    for (size_t bIdx = 0; bIdx < blocks.size(); ++bIdx) {
        const IRBasicBlock* nextBlock = (bIdx + 1 < blocks.size()) ? blocks[bIdx + 1].get() : nullptr;
        generateBasicBlock(*blocks[bIdx], fn, nextBlock);
    }

    if (m_debugInfo) {
        m_out << "  .cfi_endproc\n";
    }
}

static std::vector<std::string> applyPeephole(const std::vector<std::string>& lines) {
    std::vector<std::string> out;
    for (const auto& rawLine : lines) {
        std::string line = rawLine;
        while (!line.empty() && (line.back() == '\r' || line.back() == '\n' || line.back() == ' ')) {
            line.pop_back();
        }

        // Drop redundant 64-bit self-moves: e.g. "  mov rax, rax"
        if (line == "  mov rax, rax" || line == "  mov rcx, rcx" || line == "  mov rdx, rdx" ||
            line == "  mov rbx, rbx" || line == "  mov r12, r12" || line == "  mov r13, r13" ||
            line == "  mov r14, r14" || line == "  mov r15, r15" ||
            line == "  mov rsi, rsi" || line == "  mov rdi, rdi" || line == "  mov r8, r8" ||
            line == "  mov r9, r9" || line == "  mov r10, r10" || line == "  mov r11, r11") {
            continue;
        }

        if (!out.empty()) {
            const std::string& prev = out.back();
            // Pattern: prev is "  mov [rbp - X], reg" or "  mov [rsp + X], reg"
            //          line is "  mov reg, [rbp - X]" or "  mov reg, [rsp + X]"
            if ((prev.rfind("  mov [rbp - ", 0) == 0 || prev.rfind("  mov [rsp + ", 0) == 0) &&
                line.rfind("  mov ", 0) == 0) {
                size_t commaPrev = prev.find("], ");
                size_t commaLine = line.find(", [");
                if (commaPrev != std::string::npos && commaLine != std::string::npos) {
                    std::string slotPrev = prev.substr(6, commaPrev - 6 + 1);
                    std::string regPrev = prev.substr(commaPrev + 3);
                    std::string regLine = line.substr(6, commaLine - 6);
                    std::string slotLine = line.substr(commaLine + 2);

                    if (slotPrev == slotLine && regPrev == regLine &&
                        (regPrev == "rax" || regPrev == "rcx" || regPrev == "rdx" ||
                         regPrev == "r8" || regPrev == "r9" || regPrev == "r10" || regPrev == "r11")) {
                        continue;
                    }
                }
            }
        }
        out.push_back(line);
    }
    return out;
}

static bool isImm32Value(const IRValue* val, int64_t& outImm) {
    if (!val || !val->isConstant()) return false;
    if (const auto* ci = dynamic_cast<const ConstantInt*>(val)) {
        int64_t v = ci->getValue();
        if (v >= -2147483648LL && v <= 2147483647LL) {
            outImm = v;
            return true;
        }
    } else if (val->getType()->isBoolean()) {
        outImm = (val->getName() == "true") ? 1 : 0;
        return true;
    }
    return false;
}

void X86_64CodeGen::generateBasicBlock(const IRBasicBlock& bb, const IRFunction& fn, const IRBasicBlock* nextBlock) {
    *m_pOut << getLabel(bb, fn) << ":\n";

    std::ostringstream bbStream;
    std::ostream* savedOut = m_pOut;
    m_pOut = &bbStream;

    auto edgeHasNoPhiMoves = [&](const IRBasicBlock* target) -> bool {
        if (!target) return true;
        for (const auto& tInst : target->getInstructions()) {
            if (tInst->getOpcode() != IROpcode::Phi) break;
            const auto& phi = static_cast<const PhiInst&>(*tInst);
            auto val = phi.getIncomingValue(&bb);
            if (!val || val.get() == &phi) continue;
            if (m_lsra && m_lsra->hasRegister(&phi) && m_lsra->hasRegister(val.get()) &&
                m_lsra->getRegister(&phi) == m_lsra->getRegister(val.get())) {
                continue;
            }
            return false;
        }
        return true;
    };

    const auto& insts = bb.getInstructions();
    for (size_t i = 0; i < insts.size(); ++i) {
        // Compare-Branch Fusion (safe when target edges require no materialized Phi copies)
        if (i + 1 < insts.size() && insts[i+1]->getOpcode() == IROpcode::CondBr) {
            const auto& condBr = static_cast<const CondBranchInst&>(*insts[i+1]);
            if (condBr.getCondition().get() == insts[i].get() &&
                edgeHasNoPhiMoves(condBr.getTrueTarget()) &&
                edgeHasNoPhiMoves(condBr.getFalseTarget())) {
                auto op = insts[i]->getOpcode();
                if (op == IROpcode::Equal || op == IROpcode::NotEqual ||
                    op == IROpcode::Less || op == IROpcode::LessEqual ||
                    op == IROpcode::Greater || op == IROpcode::GreaterEqual) {

                    const auto& binInst = static_cast<const BinaryInst&>(*insts[i]);

                    if (binInst.getLhs()->getType()->isFloatingPoint()) {
                        loadValueToReg(*binInst.getLhs(), "rax");
                        loadValueToReg(*binInst.getRhs(), "rcx");
                        if (binInst.getLhs()->getType()->isF32()) {
                            *m_pOut << "  movd xmm0, eax\n";
                            *m_pOut << "  movd xmm1, ecx\n";
                            if (op == IROpcode::Less || op == IROpcode::LessEqual) {
                                *m_pOut << "  ucomiss xmm1, xmm0\n";
                            } else {
                                *m_pOut << "  ucomiss xmm0, xmm1\n";
                            }
                        } else {
                            *m_pOut << "  movq xmm0, rax\n";
                            *m_pOut << "  movq xmm1, rcx\n";
                            if (op == IROpcode::Less || op == IROpcode::LessEqual) {
                                *m_pOut << "  ucomisd xmm1, xmm0\n";
                            } else {
                                *m_pOut << "  ucomisd xmm0, xmm1\n";
                            }
                        }
                        if (op == IROpcode::Equal) {
                            *m_pOut << "  jp " << getLabel(*condBr.getFalseTarget(), fn) << "\n";
                            *m_pOut << "  jne " << getLabel(*condBr.getFalseTarget(), fn) << "\n";
                            if (condBr.getTrueTarget() != nextBlock) {
                                *m_pOut << "  jmp " << getLabel(*condBr.getTrueTarget(), fn) << "\n";
                            }
                        } else if (op == IROpcode::NotEqual) {
                            *m_pOut << "  jp " << getLabel(*condBr.getTrueTarget(), fn) << "\n";
                            *m_pOut << "  jne " << getLabel(*condBr.getTrueTarget(), fn) << "\n";
                            if (condBr.getFalseTarget() != nextBlock) {
                                *m_pOut << "  jmp " << getLabel(*condBr.getFalseTarget(), fn) << "\n";
                            }
                        } else if (op == IROpcode::Less || op == IROpcode::Greater) {
                            *m_pOut << "  ja " << getLabel(*condBr.getTrueTarget(), fn) << "\n";
                            if (condBr.getFalseTarget() != nextBlock) {
                                *m_pOut << "  jmp " << getLabel(*condBr.getFalseTarget(), fn) << "\n";
                            }
                        } else if (op == IROpcode::LessEqual || op == IROpcode::GreaterEqual) {
                            *m_pOut << "  jae " << getLabel(*condBr.getTrueTarget(), fn) << "\n";
                            if (condBr.getFalseTarget() != nextBlock) {
                                *m_pOut << "  jmp " << getLabel(*condBr.getFalseTarget(), fn) << "\n";
                            }
                        }
                    } else {
                        const IRValue* lhs = binInst.getLhs().get();
                        const IRValue* rhs = binInst.getRhs().get();
                        std::string lhsOp = "rax";
                        if (m_lsra && m_lsra->hasRegister(lhs)) {
                            lhsOp = m_lsra->getRegister(lhs);
                        } else {
                            loadValueToReg(*lhs, "rax");
                        }

                        int64_t rhsImm = 0;
                        if (isImm32Value(rhs, rhsImm)) {
                            *m_pOut << "  cmp " << lhsOp << ", " << rhsImm << "\n";
                        } else if (m_lsra && m_lsra->hasRegister(rhs)) {
                            *m_pOut << "  cmp " << lhsOp << ", " << m_lsra->getRegister(rhs) << "\n";
                        } else {
                            loadValueToReg(*rhs, "rcx");
                            *m_pOut << "  cmp " << lhsOp << ", rcx\n";
                        }

                        auto invertCc = [](IROpcode c) {
                            switch (c) {
                                case IROpcode::Equal:        return "jne";
                                case IROpcode::NotEqual:     return "je";
                                case IROpcode::Less:         return "jge";
                                case IROpcode::LessEqual:    return "jg";
                                case IROpcode::Greater:      return "jle";
                                case IROpcode::GreaterEqual: return "jl";
                                default:                     return "je";
                            }
                        };
                        auto directCc = [](IROpcode c) {
                            switch (c) {
                                case IROpcode::Equal:        return "je";
                                case IROpcode::NotEqual:     return "jne";
                                case IROpcode::Less:         return "jl";
                                case IROpcode::LessEqual:    return "jle";
                                case IROpcode::Greater:      return "jg";
                                case IROpcode::GreaterEqual: return "jge";
                                default:                     return "jne";
                            }
                        };

                        if (condBr.getTrueTarget() == nextBlock && condBr.getFalseTarget() != nextBlock) {
                            *m_pOut << "  " << invertCc(op) << " " << getLabel(*condBr.getFalseTarget(), fn) << "\n";
                        } else {
                            *m_pOut << "  " << directCc(op) << " " << getLabel(*condBr.getTrueTarget(), fn) << "\n";
                            if (condBr.getFalseTarget() != nextBlock) {
                                *m_pOut << "  jmp " << getLabel(*condBr.getFalseTarget(), fn) << "\n";
                            }
                        }
                    }
                    ++i;
                    continue;
                }
            }
        }
        generateInstruction(*insts[i], fn, nextBlock);
    }

    m_pOut = savedOut;

    // Apply peephole pass
    std::istringstream iss(bbStream.str());
    std::string line;
    std::vector<std::string> lines;
    while (std::getline(iss, line)) {
        lines.push_back(line);
    }

    auto optLines = applyPeephole(lines);
    optLines = applyPeephole(optLines);

    for (const auto& l : optLines) {
        *m_pOut << l << "\n";
    }
}

void X86_64CodeGen::emitParallelPhiCopies(const IRBasicBlock& target, const IRBasicBlock& fromBlock) {
    auto valueLocKey = [&](const IRValue* v) -> std::string {
        if (!v) return "";
        if (m_lsra && m_lsra->hasRegister(v)) {
            return "reg:" + m_lsra->getRegister(v);
        }
        if (v->isConstant()) return "";
        return "val:" + std::to_string(reinterpret_cast<uintptr_t>(v));
    };

    struct CopyItem {
        const PhiInst* dst;
        const IRValue* src;
        std::string dstLoc;
        std::string srcLoc;
    };
    std::vector<CopyItem> pending;

    for (const auto& inst : target.getInstructions()) {
        if (inst->getOpcode() == IROpcode::Phi) {
            const auto& phi = static_cast<const PhiInst&>(*inst);
            auto val = phi.getIncomingValue(&fromBlock);
            if (val && val.get() != &phi) {
                std::string dLoc = valueLocKey(&phi);
                std::string sLoc = valueLocKey(val.get());
                // Skip no-op copies when coalesced to the exact same physical location
                if (!dLoc.empty() && dLoc == sLoc) continue;
                pending.push_back({&phi, val.get(), std::move(dLoc), std::move(sLoc)});
            }
        } else {
            break;
        }
    }

    if (pending.empty()) return;

    auto emitDirectCopy = [&](const IRValue* src, const PhiInst* dst) {
        if (m_lsra && m_lsra->hasRegister(dst)) {
            loadValueToReg(*src, m_lsra->getRegister(dst));
        } else {
            loadValueToReg(*src, "rax");
            storeRegToValue("rax", *dst);
        }
    };

    if (pending.size() == 1) {
        emitDirectCopy(pending[0].src, pending[0].dst);
        return;
    }

    // Parallel copy resolution with cycle breaking keyed by physical location
    std::unordered_map<std::string, int32_t> scratchMap;
    int32_t scratchCounter = 0;

    while (!pending.empty()) {
        auto it = std::find_if(pending.begin(), pending.end(), [&](const CopyItem& item) {
            for (const auto& other : pending) {
                if (&other != &item && !other.srcLoc.empty() && other.srcLoc == item.dstLoc) {
                    return false;
                }
            }
            return true;
        });

        if (it != pending.end()) {
            const auto* srcVal = it->src;
            auto scIt = !it->srcLoc.empty() ? scratchMap.find(it->srcLoc) : scratchMap.end();
            if (scIt != scratchMap.end()) {
                if (m_lsra && m_lsra->hasRegister(it->dst)) {
                    m_out << "  mov " << m_lsra->getRegister(it->dst) << ", " << memRef(scIt->second) << "\n";
                } else {
                    m_out << "  mov rax, " << memRef(scIt->second) << "\n";
                    storeRegToValue("rax", *it->dst);
                }
            } else {
                emitDirectCopy(srcVal, it->dst);
            }
            pending.erase(it);
        } else {
            // Cycle detected! Stage cycleDst's pre-overwrite value by physical location
            const auto* cycleDst = pending[0].dst;
            const std::string cycleDstLoc = pending[0].dstLoc;
            scratchCounter++;
            int32_t scratchSlot = m_tcoScratchOffset + scratchCounter * 8;

            loadValueToReg(*cycleDst, "rax");
            storeRegToSlot("rax", scratchSlot);
            if (!cycleDstLoc.empty()) {
                scratchMap[cycleDstLoc] = scratchSlot;
            }

            const auto* cycleSrc = pending[0].src;
            auto scIt = !pending[0].srcLoc.empty() ? scratchMap.find(pending[0].srcLoc) : scratchMap.end();
            if (scIt != scratchMap.end()) {
                if (m_lsra && m_lsra->hasRegister(cycleDst)) {
                    m_out << "  mov " << m_lsra->getRegister(cycleDst) << ", " << memRef(scIt->second) << "\n";
                } else {
                    m_out << "  mov rax, " << memRef(scIt->second) << "\n";
                    storeRegToValue("rax", *cycleDst);
                }
            } else {
                emitDirectCopy(cycleSrc, cycleDst);
            }
            pending.erase(pending.begin());
        }
    }
}

bool X86_64CodeGen::tryDirectRegisterBinaryOp(const BinaryInst& binInst) {
    if (binInst.getType()->isFloatingPoint()) return false;
    if (!m_lsra || !m_lsra->hasRegister(&binInst)) return false;

    auto op = binInst.getOpcode();
    if (op != IROpcode::Add && op != IROpcode::Sub && op != IROpcode::Mul &&
        op != IROpcode::BitwiseAnd && op != IROpcode::BitwiseOr && op != IROpcode::BitwiseXor) {
        return false;
    }

    std::string dstReg = m_lsra->getRegister(&binInst);
    const auto* lhs = binInst.getLhs().get();
    const auto* rhs = binInst.getRhs().get();

    bool rhsInDst = (m_lsra->hasRegister(rhs) && m_lsra->getRegister(rhs) == dstReg);
    bool lhsInDst = (m_lsra->hasRegister(lhs) && m_lsra->getRegister(lhs) == dstReg);

    // Non-destructive 3-operand lea lowering when enabled (-O1+)
    if (m_optLevel >= OptimizationLevel::O1 && !lhsInDst && !rhsInDst) {
        if ((op == IROpcode::Add || op == IROpcode::Sub) && m_lsra->hasRegister(lhs)) {
            const std::string& lhsReg = m_lsra->getRegister(lhs);
            int64_t immVal = 0;
            if (isImm32Value(rhs, immVal)) {
                if (op == IROpcode::Add || immVal > -2147483648LL) {
                    int64_t adj = (op == IROpcode::Add) ? immVal : -immVal;
                    if (adj == 0) {
                        m_out << "  mov " << dstReg << ", " << lhsReg << "\n";
                    } else if (adj > 0) {
                        m_out << "  lea " << dstReg << ", [" << lhsReg << " + " << adj << "]\n";
                    } else {
                        m_out << "  lea " << dstReg << ", [" << lhsReg << " - " << (-adj) << "]\n";
                    }
                    return true;
                }
            }
            if (op == IROpcode::Add && m_lsra->hasRegister(rhs)) {
                const std::string& rhsReg = m_lsra->getRegister(rhs);
                m_out << "  lea " << dstReg << ", [" << lhsReg << " + " << rhsReg << "]\n";
                return true;
            }
        }
    }

    if (rhsInDst && !lhsInDst) {
        if (op == IROpcode::Add || op == IROpcode::Mul ||
            op == IROpcode::BitwiseAnd || op == IROpcode::BitwiseOr || op == IROpcode::BitwiseXor) {
            std::swap(lhs, rhs);
            lhsInDst = true;
        } else {
            return false;
        }
    }

    if (!lhsInDst) {
        loadValueToReg(*lhs, dstReg);
    }

    std::string opName;
    switch (op) {
        case IROpcode::Add: opName = "add"; break;
        case IROpcode::Sub: opName = "sub"; break;
        case IROpcode::Mul: opName = "imul"; break;
        case IROpcode::BitwiseAnd: opName = "and"; break;
        case IROpcode::BitwiseOr: opName = "or"; break;
        case IROpcode::BitwiseXor: opName = "xor"; break;
        default: return false;
    }

    if (rhs->isConstant()) {
        if (auto ci = dynamic_cast<const ConstantInt*>(rhs)) {
            int64_t val = ci->getValue();
            if (val >= -2147483648LL && val <= 2147483647LL) {
                if (op == IROpcode::Mul) {
                    m_out << "  imul " << dstReg << ", " << dstReg << ", " << val << "\n";
                } else {
                    m_out << "  " << opName << " " << dstReg << ", " << val << "\n";
                }
                return true;
            }
        }
    }

    if (m_lsra->hasRegister(rhs)) {
        std::string rhsReg = m_lsra->getRegister(rhs);
        m_out << "  " << opName << " " << dstReg << ", " << rhsReg << "\n";
        return true;
    }

    int32_t offset = getSlotOffset(*rhs);
    m_out << "  " << opName << " " << dstReg << ", " << memRef(offset) << "\n";
    return true;
}

void X86_64CodeGen::generateInstruction(const IRInstruction& inst, const IRFunction& fn, const IRBasicBlock* nextBlock) {
    if (m_debugInfo && inst.getLocation().isValid()) {
        if (inst.getLocation() != m_lastEmittedLoc) {
            m_out << "  .loc " << inst.getLocation().fileId << " "
                  << inst.getLocation().line << " "
                  << inst.getLocation().column << "\n";
            m_lastEmittedLoc = inst.getLocation();
        }
    }
    switch (inst.getOpcode()) {
        case IROpcode::Phi: {
            break;
        }
        case IROpcode::Alloca: {
            // Already allocated a stack slot in m_slots
            break;
        }
        case IROpcode::Store: {
            const auto& storeInst = static_cast<const StoreInst&>(inst);
            loadValueToReg(*storeInst.getValue(), "rax");
            bool isAlloca = storeInst.getPointer()->isInstruction() &&
                static_cast<const IRInstruction*>(storeInst.getPointer().get())->getOpcode() == IROpcode::Alloca;
            if (storeInst.getValue()->getType()->isStruct()) {
                auto structType = std::static_pointer_cast<StructType>(storeInst.getValue()->getType());
                int32_t structSize = static_cast<int32_t>(structType->getSize());
                if (structSize > 8) {
                    if (isAlloca) {
                        int32_t ptrOffset = getSlotOffset(*storeInst.getPointer());
                        m_out << "  lea rdx, " << memRef(ptrOffset) << "\n";
                    } else {
                        loadValueToReg(*storeInst.getPointer(), "rdx");
                    }
                    for (int32_t off = 0; off < structSize; off += 8) {
                        m_out << "  mov rcx, [rax + " << off << "]\n";
                        m_out << "  mov [rdx + " << off << "], rcx\n";
                    }
                    break;
                }
            }
            if (isAlloca) {
                int32_t ptrOffset = getSlotOffset(*storeInst.getPointer());
                storeRegToSlot("rax", ptrOffset);
            } else {
                loadValueToReg(*storeInst.getPointer(), "rdx");
                bool isPtrVal = storeInst.getValue()->getType()->isPointer() ||
                    storeInst.getValue()->getType()->isRc() ||
                    storeInst.getValue()->getType()->isArc() ||
                    storeInst.getValue()->getType()->isBox() ||
                    storeInst.getValue()->getType()->isArena() ||
                    storeInst.getValue()->getType()->isVector() ||
                    (storeInst.getValue()->isInstruction() &&
                    (static_cast<const IRInstruction*>(storeInst.getValue().get())->getOpcode() == IROpcode::Alloca ||
                     static_cast<const IRInstruction*>(storeInst.getValue().get())->getOpcode() == IROpcode::GetFieldPtr ||
                     static_cast<const IRInstruction*>(storeInst.getValue().get())->getOpcode() == IROpcode::GetElementPtr));

                if (!isPtrVal && storeInst.getValue()->getType()->isBoolean()) {
                    m_out << "  mov byte ptr [rdx], al\n";
                } else if (!isPtrVal && (storeInst.getValue()->getType()->isI32() || storeInst.getValue()->getType()->isEnum() || storeInst.getValue()->getType()->isF32())) {
                    m_out << "  mov [rdx], eax\n";
                } else {
                    m_out << "  mov [rdx], rax\n";
                }
            }
            break;
        }
        case IROpcode::Load: {
            const auto& loadInst = static_cast<const LoadInst&>(inst);
            bool isAlloca = loadInst.getPointer()->isInstruction() &&
                static_cast<const IRInstruction*>(loadInst.getPointer().get())->getOpcode() == IROpcode::Alloca;
            if (loadInst.getType()->isStruct() &&
                std::static_pointer_cast<StructType>(loadInst.getType())->getSize() > 8) {
                if (isAlloca) {
                    int32_t ptrOffset = getSlotOffset(*loadInst.getPointer());
                    m_out << "  lea rax, " << memRef(ptrOffset) << "\n";
                } else {
                    loadValueToReg(*loadInst.getPointer(), "rax");
                }
            } else if (isAlloca) {
                int32_t ptrOffset = getSlotOffset(*loadInst.getPointer());
                m_out << "  mov rax, " << memRef(ptrOffset) << "\n";
            } else {
                loadValueToReg(*loadInst.getPointer(), "rdx");
                if (loadInst.getType()->isBoolean()) {
                    m_out << "  movzx rax, byte ptr [rdx]\n";
                } else if (loadInst.getType()->isI32() || loadInst.getType()->isEnum()) {
                    m_out << "  movsxd rax, dword ptr [rdx]\n";
                } else if (loadInst.getType()->isF32()) {
                    m_out << "  mov eax, dword ptr [rdx]\n";
                } else {
                    m_out << "  mov rax, [rdx]\n";
                }
            }
            storeRegToValue("rax", loadInst);
            break;
        }
        case IROpcode::GetFieldPtr: {
            const auto& gfp = static_cast<const GetFieldPtrInst&>(inst);
            bool isAlloca = gfp.getBasePtr()->isInstruction() &&
                static_cast<const IRInstruction*>(gfp.getBasePtr().get())->getOpcode() == IROpcode::Alloca;
            bool isCall = gfp.getBasePtr()->isInstruction() &&
                static_cast<const IRInstruction*>(gfp.getBasePtr().get())->getOpcode() == IROpcode::Call &&
                gfp.getBasePtr()->getType()->isStruct() &&
                std::static_pointer_cast<StructType>(gfp.getBasePtr()->getType())->getSize() > 8 &&
                static_cast<const CallInst*>(gfp.getBasePtr().get())->getCallee().rfind("sph_", 0) != 0;
            if (isAlloca || isCall) {
                int32_t baseOffset = getSlotOffset(*gfp.getBasePtr());
                int32_t fieldStackOffset = baseOffset - static_cast<int32_t>(gfp.getByteOffset());
                m_out << "  lea rax, " << memRef(fieldStackOffset) << "\n";
            } else {
                loadValueToReg(*gfp.getBasePtr(), "rax");
                if (gfp.getByteOffset() != 0) {
                    m_out << "  add rax, " << gfp.getByteOffset() << "\n";
                }
            }
            storeRegToValue("rax", gfp);
            break;
        }
        case IROpcode::GetElementPtr: {
            const auto& gep = static_cast<const GetElementPtrInst&>(inst);
            bool isAlloca = gep.getBasePtr()->isInstruction() &&
                static_cast<const IRInstruction*>(gep.getBasePtr().get())->getOpcode() == IROpcode::Alloca;
            size_t elemSize = gep.getElementSize();
            bool validScale = (elemSize == 1 || elemSize == 2 || elemSize == 4 || elemSize == 8);

            if (!isAlloca && validScale) {
                std::string bReg = (m_lsra && m_lsra->hasRegister(gep.getBasePtr().get()))
                    ? m_lsra->getRegister(gep.getBasePtr().get()) : "rdx";
                if (bReg == "rdx") {
                    loadValueToReg(*gep.getBasePtr(), "rdx");
                }
                int64_t constIdx = 0;
                if (isImm32Value(gep.getIndex().get(), constIdx)) {
                    int64_t byteOff = constIdx * static_cast<int64_t>(elemSize);
                    std::string dstReg = (m_lsra && m_lsra->hasRegister(&gep))
                        ? m_lsra->getRegister(&gep) : "rdx";
                    if (byteOff == 0) {
                        if (dstReg != bReg) m_out << "  mov " << dstReg << ", " << bReg << "\n";
                    } else if (byteOff > 0 && byteOff <= 2147483647LL) {
                        m_out << "  lea " << dstReg << ", [" << bReg << " + " << byteOff << "]\n";
                    } else if (byteOff < 0 && -byteOff <= 2147483647LL) {
                        m_out << "  lea " << dstReg << ", [" << bReg << " - " << (-byteOff) << "]\n";
                    } else {
                        loadValueToReg(*gep.getIndex(), "rax");
                        if (elemSize > 1) m_out << "  imul rax, " << elemSize << "\n";
                        m_out << "  lea " << dstReg << ", [" << bReg << " + rax]\n";
                    }
                    if (!m_lsra || !m_lsra->hasRegister(&gep)) {
                        storeRegToValue(dstReg, gep);
                    }
                    break;
                }
                std::string iReg = (m_lsra && m_lsra->hasRegister(gep.getIndex().get()) &&
                                    m_lsra->getRegister(gep.getIndex().get()) != bReg)
                    ? m_lsra->getRegister(gep.getIndex().get()) : "rax";
                if (iReg == "rax") {
                    loadValueToReg(*gep.getIndex(), "rax");
                }
                std::string dstReg = (m_lsra && m_lsra->hasRegister(&gep))
                    ? m_lsra->getRegister(&gep) : "rdx";
                if (elemSize == 1) {
                    m_out << "  lea " << dstReg << ", [" << bReg << " + " << iReg << "]\n";
                } else {
                    m_out << "  lea " << dstReg << ", [" << bReg << " + " << iReg << " * " << elemSize << "]\n";
                }
                if (!m_lsra || !m_lsra->hasRegister(&gep)) {
                    storeRegToValue(dstReg, gep);
                }
                break;
            }

            loadValueToReg(*gep.getIndex(), "rax");
            if (elemSize != 1) {
                m_out << "  imul rax, " << elemSize << "\n";
            }
            if (isAlloca) {
                int32_t baseOffset = getSlotOffset(*gep.getBasePtr());
                m_out << "  lea rdx, " << memRef(baseOffset) << "\n";
                m_out << "  add rdx, rax\n";
            } else {
                loadValueToReg(*gep.getBasePtr(), "rdx");
                m_out << "  add rdx, rax\n";
            }
            storeRegToValue("rdx", gep);
            break;
        }
        case IROpcode::AddressOf: {
            const auto& addrInst = static_cast<const AddressOfInst&>(inst);
            loadValueToReg(*addrInst.getTarget(), "rax");
            storeRegToValue("rax", addrInst);
            break;
        }
        case IROpcode::Add:
        case IROpcode::Sub:
        case IROpcode::Mul:
        case IROpcode::Div:
        case IROpcode::Mod: {
            const auto& binInst = static_cast<const BinaryInst&>(inst);
            if (tryDirectRegisterBinaryOp(binInst)) {
                break;
            }
            loadValueToReg(*binInst.getLhs(), "rax");
            loadValueToReg(*binInst.getRhs(), "rcx");

            if (binInst.getType()->isFloatingPoint()) {
                if (binInst.getType()->isF32()) {
                    m_out << "  movd xmm0, eax\n";
                    m_out << "  movd xmm1, ecx\n";
                    switch (binInst.getOpcode()) {
                        case IROpcode::Add: m_out << "  addss xmm0, xmm1\n"; break;
                        case IROpcode::Sub: m_out << "  subss xmm0, xmm1\n"; break;
                        case IROpcode::Mul: m_out << "  mulss xmm0, xmm1\n"; break;
                        case IROpcode::Div: m_out << "  divss xmm0, xmm1\n"; break;
                        default: break;
                    }
                    m_out << "  movd eax, xmm0\n";
                } else {
                    m_out << "  movq xmm0, rax\n";
                    m_out << "  movq xmm1, rcx\n";
                    switch (binInst.getOpcode()) {
                        case IROpcode::Add: m_out << "  addsd xmm0, xmm1\n"; break;
                        case IROpcode::Sub: m_out << "  subsd xmm0, xmm1\n"; break;
                        case IROpcode::Mul: m_out << "  mulsd xmm0, xmm1\n"; break;
                        case IROpcode::Div: m_out << "  divsd xmm0, xmm1\n"; break;
                        default: break;
                    }
                    m_out << "  movq rax, xmm0\n";
                }
            } else {
                switch (binInst.getOpcode()) {
                    case IROpcode::Add:
                        m_out << "  add rax, rcx\n";
                        break;
                    case IROpcode::Sub:
                        m_out << "  sub rax, rcx\n";
                        break;
                    case IROpcode::Mul:
                        m_out << "  imul rax, rcx\n";
                        break;
                    case IROpcode::Div:
                        m_out << "  cqo\n";
                        m_out << "  idiv rcx\n";
                        break;
                    case IROpcode::Mod:
                        m_out << "  cqo\n";
                        m_out << "  idiv rcx\n";
                        m_out << "  mov rax, rdx\n";
                        break;
                    default:
                        break;
                }
            }

            storeRegToValue("rax", binInst);
            break;
        }
        case IROpcode::Equal:
        case IROpcode::NotEqual:
        case IROpcode::Less:
        case IROpcode::LessEqual:
        case IROpcode::Greater:
        case IROpcode::GreaterEqual: {
            const auto& binInst = static_cast<const BinaryInst&>(inst);

            if (binInst.getLhs()->getType()->isFloatingPoint()) {
                loadValueToReg(*binInst.getLhs(), "rax");
                loadValueToReg(*binInst.getRhs(), "rcx");
                if (binInst.getLhs()->getType()->isF32()) {
                    m_out << "  movd xmm0, eax\n";
                    m_out << "  movd xmm1, ecx\n";
                    if (binInst.getOpcode() == IROpcode::Less || binInst.getOpcode() == IROpcode::LessEqual) {
                        m_out << "  ucomiss xmm1, xmm0\n";
                    } else {
                        m_out << "  ucomiss xmm0, xmm1\n";
                    }
                } else {
                    m_out << "  movq xmm0, rax\n";
                    m_out << "  movq xmm1, rcx\n";
                    if (binInst.getOpcode() == IROpcode::Less || binInst.getOpcode() == IROpcode::LessEqual) {
                        m_out << "  ucomisd xmm1, xmm0\n";
                    } else {
                        m_out << "  ucomisd xmm0, xmm1\n";
                    }
                }
                switch (binInst.getOpcode()) {
                    case IROpcode::Equal:
                        m_out << "  sete al\n";
                        m_out << "  setnp dl\n";
                        m_out << "  and al, dl\n";
                        break;
                    case IROpcode::NotEqual:
                        m_out << "  setne al\n";
                        m_out << "  setp dl\n";
                        m_out << "  or al, dl\n";
                        break;
                    case IROpcode::Less:
                    case IROpcode::Greater:
                        m_out << "  seta al\n";
                        break;
                    case IROpcode::LessEqual:
                    case IROpcode::GreaterEqual:
                        m_out << "  setae al\n";
                        break;
                    default:
                        m_out << "  sete al\n";
                        break;
                }
            } else {
                const IRValue* lhs = binInst.getLhs().get();
                const IRValue* rhs = binInst.getRhs().get();
                std::string lhsOp = "rax";
                if (m_lsra && m_lsra->hasRegister(lhs)) {
                    lhsOp = m_lsra->getRegister(lhs);
                } else {
                    loadValueToReg(*lhs, "rax");
                }
                int64_t rhsImm = 0;
                if (isImm32Value(rhs, rhsImm)) {
                    m_out << "  cmp " << lhsOp << ", " << rhsImm << "\n";
                } else if (m_lsra && m_lsra->hasRegister(rhs)) {
                    m_out << "  cmp " << lhsOp << ", " << m_lsra->getRegister(rhs) << "\n";
                } else {
                    loadValueToReg(*rhs, "rcx");
                    m_out << "  cmp " << lhsOp << ", rcx\n";
                }
                std::string setInst;
                switch (binInst.getOpcode()) {
                    case IROpcode::Equal:        setInst = "sete"; break;
                    case IROpcode::NotEqual:     setInst = "setne"; break;
                    case IROpcode::Less:         setInst = "setl"; break;
                    case IROpcode::LessEqual:    setInst = "setle"; break;
                    case IROpcode::Greater:      setInst = "setg"; break;
                    case IROpcode::GreaterEqual: setInst = "setge"; break;
                    default:                     setInst = "sete"; break;
                }
                m_out << "  " << setInst << " al\n";
            }
            m_out << "  movzx rax, al\n";

            storeRegToValue("rax", binInst);
            break;
        }
        case IROpcode::LogicalAnd:
        case IROpcode::LogicalOr:
        case IROpcode::BitwiseAnd:
        case IROpcode::BitwiseOr:
        case IROpcode::BitwiseXor: {
            const auto& binInst = static_cast<const BinaryInst&>(inst);
            if (tryDirectRegisterBinaryOp(binInst)) {
                break;
            }
            loadValueToReg(*binInst.getLhs(), "rax");
            loadValueToReg(*binInst.getRhs(), "rcx");

            if (binInst.getOpcode() == IROpcode::LogicalAnd || binInst.getOpcode() == IROpcode::BitwiseAnd) {
                m_out << "  and rax, rcx\n";
            } else if (binInst.getOpcode() == IROpcode::LogicalOr || binInst.getOpcode() == IROpcode::BitwiseOr) {
                m_out << "  or rax, rcx\n";
            } else if (binInst.getOpcode() == IROpcode::BitwiseXor) {
                m_out << "  xor rax, rcx\n";
            }

            storeRegToValue("rax", binInst);
            break;
        }
        case IROpcode::ShiftLeft:
        case IROpcode::ShiftRight: {
            const auto& binInst = static_cast<const BinaryInst&>(inst);
            int64_t shiftImm = 0;
            if (isImm32Value(binInst.getRhs().get(), shiftImm) && shiftImm >= 0 && shiftImm < 64) {
                const char* shiftOp = (binInst.getOpcode() == IROpcode::ShiftLeft) ? "shl" : "sar";
                if (m_lsra && m_lsra->hasRegister(&binInst)) {
                    std::string dstReg = m_lsra->getRegister(&binInst);
                    loadValueToReg(*binInst.getLhs(), dstReg);
                    m_out << "  " << shiftOp << " " << dstReg << ", " << shiftImm << "\n";
                } else {
                    loadValueToReg(*binInst.getLhs(), "rax");
                    m_out << "  " << shiftOp << " rax, " << shiftImm << "\n";
                    storeRegToValue("rax", binInst);
                }
                break;
            }
            loadValueToReg(*binInst.getRhs(), "rcx");
            loadValueToReg(*binInst.getLhs(), "rax");

            if (binInst.getOpcode() == IROpcode::ShiftLeft) {
                m_out << "  shl rax, cl\n";
            } else {
                m_out << "  sar rax, cl\n";
            }

            storeRegToValue("rax", binInst);
            break;
        }
        case IROpcode::Negate: {
            const auto& unInst = static_cast<const UnaryInst&>(inst);
            loadValueToReg(*unInst.getOperand(), "rax");
            if (unInst.getType()->isF32()) {
                m_out << "  btc eax, 31\n";
            } else if (unInst.getType()->isFloatingPoint()) {
                m_out << "  btc rax, 63\n";
            } else {
                m_out << "  neg rax\n";
            }
            storeRegToValue("rax", unInst);
            break;
        }
        case IROpcode::LogicalNot: {
            const auto& unInst = static_cast<const UnaryInst&>(inst);
            loadValueToReg(*unInst.getOperand(), "rax");
            m_out << "  xor rax, 1\n";
            storeRegToValue("rax", unInst);
            break;
        }
        case IROpcode::BitwiseNot: {
            const auto& unInst = static_cast<const UnaryInst&>(inst);
            loadValueToReg(*unInst.getOperand(), "rax");
            m_out << "  not rax\n";
            storeRegToValue("rax", unInst);
            break;
        }
        case IROpcode::Cast: {
            const auto& castInst = static_cast<const CastInst&>(inst);
            loadValueToReg(*castInst.getOperand(), "rax");
            if (castInst.getOperand()->getType()->isI32() && castInst.getType()->isI64()) {
                m_out << "  movsxd rax, eax\n";
            } else if (castInst.getOperand()->getType()->isI64() && castInst.getType()->isI32()) {
                m_out << "  movsxd rax, eax\n";
            } else if (castInst.getOperand()->getType()->isF64() && castInst.getType()->isF32()) {
                m_out << "  movq xmm0, rax\n";
                m_out << "  cvtsd2ss xmm0, xmm0\n";
                m_out << "  movd eax, xmm0\n";
            } else if (castInst.getOperand()->getType()->isF32() && castInst.getType()->isF64()) {
                m_out << "  movd xmm0, eax\n";
                m_out << "  cvtss2sd xmm0, xmm0\n";
                m_out << "  movq rax, xmm0\n";
            } else if (castInst.getOperand()->getType()->isInteger() && castInst.getType()->isF32()) {
                m_out << "  cvtsi2ss xmm0, eax\n";
                m_out << "  movd eax, xmm0\n";
            } else if (castInst.getOperand()->getType()->isInteger() && castInst.getType()->isF64()) {
                m_out << "  cvtsi2sd xmm0, rax\n";
                m_out << "  movq rax, xmm0\n";
            } else if (castInst.getOperand()->getType()->isF32() && castInst.getType()->isInteger()) {
                m_out << "  movd xmm0, eax\n";
                m_out << "  cvttss2si eax, xmm0\n";
            } else if (castInst.getOperand()->getType()->isF64() && castInst.getType()->isInteger()) {
                m_out << "  movq xmm0, rax\n";
                m_out << "  cvttsd2si rax, xmm0\n";
            }
            storeRegToValue("rax", castInst);
            break;
        }
        case IROpcode::Ret: {
            const auto& retInst = static_cast<const RetInst&>(inst);
            if (retInst.hasValue()) {
                loadValueToReg(*retInst.getValue(), "rax");
                if (m_currentFnReturnsSret) {
                    m_out << "  mov r11, " << memRef(m_currentFnSretSlot) << "\n";
                    for (int32_t off = 0; off < m_currentFnSretSize; off += 8) {
                        m_out << "  mov r10, [rax + " << off << "]\n";
                        m_out << "  mov [r11 + " << off << "], r10\n";
                    }
                    m_out << "  mov rax, r11\n";
                } else if (retInst.getValue()->getType()->isFloatingPoint()) {
                    if (retInst.getValue()->getType()->isF32()) {
                        m_out << "  movd xmm0, eax\n";
                    } else {
                        m_out << "  movq xmm0, rax\n";
                    }
                }
            }
            emitEpilogue(fn);
            break;
        }
        case IROpcode::Br: {
            const auto& brInst = static_cast<const BranchInst&>(inst);
            if (brInst.getTarget()) {
                emitParallelPhiCopies(*brInst.getTarget(), *inst.getParent());
            }
            if (brInst.getTarget() != nextBlock) {
                m_out << "  jmp " << getLabel(*brInst.getTarget(), fn) << "\n";
            }
            break;
        }
        case IROpcode::CondBr: {
            const auto& condBr = static_cast<const CondBranchInst&>(inst);
            if (m_lsra && m_lsra->hasRegister(condBr.getCondition().get())) {
                const std::string& condReg = m_lsra->getRegister(condBr.getCondition().get());
                m_out << "  test " << condReg << ", " << condReg << "\n";
            } else {
                loadValueToReg(*condBr.getCondition(), "rax");
                m_out << "  cmp rax, 0\n";
            }
            bool trueHasPhis = false;
            if (condBr.getTrueTarget()) {
                for (const auto& tInst : condBr.getTrueTarget()->getInstructions()) {
                    if (tInst->getOpcode() == IROpcode::Phi) { trueHasPhis = true; break; }
                }
            }
            bool falseHasPhis = false;
            if (condBr.getFalseTarget()) {
                for (const auto& fInst : condBr.getFalseTarget()->getInstructions()) {
                    if (fInst->getOpcode() == IROpcode::Phi) { falseHasPhis = true; break; }
                }
            }

            if (!trueHasPhis && !falseHasPhis) {
                if (condBr.getTrueTarget() == nextBlock && condBr.getFalseTarget() != nextBlock) {
                    m_out << "  je " << getLabel(*condBr.getFalseTarget(), fn) << "\n";
                } else {
                    m_out << "  jne " << getLabel(*condBr.getTrueTarget(), fn) << "\n";
                    if (condBr.getFalseTarget() != nextBlock) {
                        m_out << "  jmp " << getLabel(*condBr.getFalseTarget(), fn) << "\n";
                    }
                }
            } else {
                static size_t condEdgeCounter = 0;
                std::string falseEdge = ".L_edge_f_" + std::to_string(condEdgeCounter++);
                m_out << "  je " << falseEdge << "\n";
                if (condBr.getTrueTarget()) {
                    emitParallelPhiCopies(*condBr.getTrueTarget(), *inst.getParent());
                }
                m_out << "  jmp " << getLabel(*condBr.getTrueTarget(), fn) << "\n";
                m_out << falseEdge << ":\n";
                if (condBr.getFalseTarget()) {
                    emitParallelPhiCopies(*condBr.getFalseTarget(), *inst.getParent());
                }
                if (condBr.getFalseTarget() != nextBlock) {
                    m_out << "  jmp " << getLabel(*condBr.getFalseTarget(), fn) << "\n";
                }
            }
            break;
        }
        case IROpcode::Call: {
            const auto& callInst = static_cast<const CallInst&>(inst);
            const auto& args = callInst.getArgs();
            if (callInst.isTailCall() && !callInst.isIndirect()) {
                if (callInst.getCallee() == fn.getName()) {
                    const auto& fnParams = fn.getParameters();
                    size_t count = std::min(args.size(), fnParams.size());
                    // Phase 1: Evaluate all arguments into temporary frame scratch slots
                    // to prevent cyclic parameter clobbering (e.g. a, b -> b, a or cyclic permutations)
                    for (size_t aIdx = 0; aIdx < count; ++aIdx) {
                        loadValueToReg(*args[aIdx], "rax");
                        int32_t slotOff = m_tcoScratchOffset + static_cast<int32_t>((aIdx + 1) * 8);
                        storeRegToSlot("rax", slotOff);
                    }
                    // Phase 2: Copy from scratch slots into destination parameter registers/slots
                    for (size_t aIdx = 0; aIdx < count; ++aIdx) {
                        int32_t slotOff = m_tcoScratchOffset + static_cast<int32_t>((aIdx + 1) * 8);
                        m_out << "  mov rax, " << memRef(slotOff) << "\n";
                        storeRegToValue("rax", *fnParams[aIdx]);
                    }
                    m_out << "  jmp ." << fn.getName() << "_entry_loop\n";
                    break;
                }
            }

            bool isSret = callInst.getType()->isStruct() &&
                std::static_pointer_cast<StructType>(callInst.getType())->getSize() > 8 &&
                callInst.getCallee().rfind("sph_", 0) != 0;
            int32_t dstOffset = isSret ? getSlotOffset(callInst) : 0;

            size_t maxRegs = getMaxRegisterParams();

            if (isSysV()) {
                size_t intIdx = isSret ? 1 : 0;
                size_t floatIdx = 0;
                size_t stackIdx = 0;
                for (size_t i = 0; i < args.size(); ++i) {
                    bool isFloat = args[i]->getType()->isFloatingPoint();
                    if ((isFloat && floatIdx >= 8) || (!isFloat && intIdx >= 6)) {
                        loadValueToReg(*args[i], "rax");
                        int32_t stackOffset = static_cast<int32_t>(stackIdx++ * 8);
                        m_out << "  mov [rsp + " << stackOffset << "], rax\n";
                    } else if (isFloat) {
                        floatIdx++;
                    } else {
                        intIdx++;
                    }
                }

                intIdx = isSret ? 1 : 0;
                floatIdx = 0;
                if (isSret) {
                    m_out << "  lea " << getParamReg(0) << ", " << memRef(dstOffset) << "\n";
                }
                for (size_t i = 0; i < args.size(); ++i) {
                    bool isFloat = args[i]->getType()->isFloatingPoint();
                    if (isFloat && floatIdx < 8) {
                        loadValueToReg(*args[i], "rax");
                        if (args[i]->getType()->isF32()) {
                            m_out << "  movd xmm" << floatIdx++ << ", eax\n";
                        } else {
                            m_out << "  movq xmm" << floatIdx++ << ", rax\n";
                        }
                    } else if (!isFloat && intIdx < 6) {
                        const char* regName = getParamReg(intIdx++);
                        loadValueToReg(*args[i], regName);
                    }
                }
                m_out << "  mov al, " << floatIdx << "\n";
            } else {
                size_t regOffset = isSret ? 1 : 0;
                size_t totalArgs = args.size() + regOffset;
                // Store outgoing stack arguments beyond register capacity
                for (size_t i = maxRegs; i < totalArgs; ++i) {
                    size_t userArgIdx = i - regOffset;
                    loadValueToReg(*args[userArgIdx], "rax");
                    int32_t stackOffset = 32 + static_cast<int32_t>((i - maxRegs) * 8);
                    m_out << "  mov [rsp + " << stackOffset << "], rax\n";
                }

                if (isSret) {
                    m_out << "  lea " << getParamReg(0) << ", " << memRef(dstOffset) << "\n";
                }

                // Load arguments into register parameters according to calling convention
                for (size_t i = 0; i < args.size() && (i + regOffset) < maxRegs; ++i) {
                    const char* regName = getParamReg(i + regOffset);
                    loadValueToReg(*args[i], regName);
                    if (args[i]->getType()->isFloatingPoint()) {
                        if (args[i]->getType()->isF32()) {
                            std::string r32 = getReg32(regName);
                            m_out << "  movd xmm" << (i + regOffset) << ", " << r32 << "\n";
                        } else {
                            m_out << "  movq xmm" << (i + regOffset) << ", " << regName << "\n";
                        }
                    }
                }
            }

            if (callInst.isIndirect()) {
                loadValueToReg(*callInst.getCalleeValue(), "r10");
                m_out << "  call r10\n";
            } else {
                std::string callee = callInst.getCallee();

                // Built-in var_dump dispatch
                if (callee == "var_dump") {
                    callee = "sph_var_dump";
                }

                // Built-in println dispatch
                if (callee == "println" && !callInst.getArgs().empty()) {
                    auto argType = callInst.getArgs()[0]->getType();
                    if (argType->isF32x4()) callee = "sph_println_f32x4";
                    else if (argType->isI32x4()) callee = "sph_println_i32x4";
                    else if (argType->isF64x2()) callee = "sph_println_f64x2";
                    else if (argType->isI64x2()) callee = "sph_println_i64x2";
                    else if (argType->isI64()) callee = "sph_println_i64";
                    else if (argType->isF32()) callee = "sph_println_f32";
                    else if (argType->isFloatingPoint()) callee = "sph_println_f64";
                    else if (argType->isBoolean()) callee = "sph_println_bool";
                    else if (argType->isString()) callee = "sph_println_string";
                    else callee = "sph_println_i32";
                }

                m_out << "  call " << callee << "\n";
            }

            if (!callInst.getType()->isVoid() && !isSret) {
                if (callInst.getType()->isFloatingPoint()) {
                    if (callInst.getType()->isF32()) {
                        m_out << "  movd eax, xmm0\n";
                    } else {
                        m_out << "  movq rax, xmm0\n";
                    }
                }
                storeRegToValue("rax", callInst);
            }
            break;
        }
        case IROpcode::RcAlloc: {
            const auto& rcAlloc = static_cast<const RcAllocInst&>(inst);
            m_out << "  mov " << getParamReg(0) << ", " << rcAlloc.getAllocSize() << "\n";
            m_out << "  call sph_rc_alloc\n";
            storeRegToValue("rax", rcAlloc);
            break;
        }
        case IROpcode::RcRetain: {
            const auto& rcRetain = static_cast<const RcRetainInst&>(inst);
            loadValueToReg(*rcRetain.getPtr(), getParamReg(0));
            m_out << "  call sph_rc_retain\n";
            break;
        }
        case IROpcode::RcRelease: {
            const auto& rcRelease = static_cast<const RcReleaseInst&>(inst);
            loadValueToReg(*rcRelease.getPtr(), getParamReg(0));
            m_out << "  call sph_rc_release\n";
            break;
        }
        case IROpcode::ArcAlloc: {
            const auto& arcAlloc = static_cast<const ArcAllocInst&>(inst);
            m_out << "  mov " << getParamReg(0) << ", " << arcAlloc.getAllocSize() << "\n";
            m_out << "  call sph_arc_alloc\n";
            storeRegToValue("rax", arcAlloc);
            break;
        }
        case IROpcode::ArcRetain: {
            const auto& arcRetain = static_cast<const ArcRetainInst&>(inst);
            loadValueToReg(*arcRetain.getPtr(), getParamReg(0));
            m_out << "  call sph_arc_retain\n";
            break;
        }
        case IROpcode::ArcRelease: {
            const auto& arcRelease = static_cast<const ArcReleaseInst&>(inst);
            loadValueToReg(*arcRelease.getPtr(), getParamReg(0));
            m_out << "  call sph_arc_release\n";
            break;
        }
        case IROpcode::SetJmp: {
            const auto& sj = static_cast<const SetJmpInst&>(inst);
            loadValueToReg(*sj.getFramePtr(), getParamReg(0));
            m_out << "  call sph_setjmp\n";
            storeRegToValue("rax", sj);
            break;
        }
    }
}

} // namespace sph

