#include "X86_64CodeGen.hpp"
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

    bool isAllocaOrBigStructCall = val.isInstruction() &&
        (static_cast<const IRInstruction&>(val).getOpcode() == IROpcode::Alloca ||
         (static_cast<const IRInstruction&>(val).getOpcode() == IROpcode::Call &&
          val.getType()->isStruct() &&
          std::static_pointer_cast<StructType>(val.getType())->getSize() > 8 &&
          static_cast<const CallInst&>(val).getCallee().rfind("sph_", 0) != 0));
    if (isAllocaOrBigStructCall) {
        int32_t offset = getSlotOffset(val);
        m_out << "  lea " << reg << ", [rbp - " << offset << "]\n";
        return;
    }

    int32_t offset = getSlotOffset(val);
    m_out << "  mov " << reg << ", [rbp - " << offset << "]\n";
}

void X86_64CodeGen::storeRegToSlot(const std::string& reg, int32_t offset) {
    m_out << "  mov [rbp - " << offset << "], " << reg << "\n";
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
                if (inst->getOpcode() == IROpcode::Store) {
                    const auto& s = static_cast<const StoreInst&>(*inst);
                    recordString(s.getValue().get());
                } else if (inst->getOpcode() == IROpcode::Call) {
                    const auto& c = static_cast<const CallInst&>(*inst);
                    for (const auto& arg : c.getArgs()) {
                        recordString(arg.get());
                    }
                } else if (inst->getOpcode() == IROpcode::Ret) {
                    const auto& r = static_cast<const RetInst&>(*inst);
                    if (r.hasValue()) recordString(r.getValue().get());
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

    // First pass: allocate slots for all parameters, allocas, and instructions
    const auto& params = fn.getParameters();
    for (const auto& param : params) {
        getSlotOffset(*param);
    }

    size_t maxCallArgs = 4;
    for (const auto& bb : fn.getBlocks()) {
        for (const auto& inst : bb->getInstructions()) {
            if (inst->getOpcode() == IROpcode::Call) {
                const auto& callInst = static_cast<const CallInst&>(*inst);
                bool isSret = callInst.getType()->isStruct() &&
                    std::static_pointer_cast<StructType>(callInst.getType())->getSize() > 8 &&
                    callInst.getCallee().rfind("sph_", 0) != 0;
                size_t callArgCount = callInst.getArgs().size() + (isSret ? 1 : 0);
                if (callArgCount > maxCallArgs) {
                    maxCallArgs = callArgCount;
                }
            }
            if (inst->getOpcode() == IROpcode::Alloca) {
                const auto& allocaInst = static_cast<const AllocaInst&>(*inst);
                int32_t size = 8;
                if (allocaInst.getAllocatedType()->isStruct() || allocaInst.getAllocatedType()->isClass()) {
                    auto structType = std::static_pointer_cast<StructType>(allocaInst.getAllocatedType());
                    size = static_cast<int32_t>(structType->getSize());
                    if (size < 8) size = 8;
                    size = (size + 7) & ~7;
                } else if (allocaInst.getAllocatedType()->isArray()) {
                    auto arrType = std::static_pointer_cast<ArrayType>(allocaInst.getAllocatedType());
                    size = static_cast<int32_t>(arrType->getSize());
                    if (size < 8) size = 8;
                    size = (size + 7) & ~7;
                } else if (allocaInst.getAllocatedType()->isSlice()) {
                    size = 16;
                }
                m_currentOffset += size;
                m_slots[inst.get()] = m_currentOffset;
            } else if (inst->getOpcode() == IROpcode::Call) {
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
                } else {
                    getSlotOffset(*inst);
                }
            } else if (inst->getOpcode() == IROpcode::Load ||
                       inst->getOpcode() == IROpcode::GetFieldPtr ||
                       inst->getOpcode() == IROpcode::GetElementPtr ||
                       inst->getOpcode() == IROpcode::AddressOf ||
                       inst->getOpcode() == IROpcode::SetJmp ||
                       inst->getOpcode() == IROpcode::RcAlloc ||
                       (inst->getOpcode() >= IROpcode::Add && inst->getOpcode() <= IROpcode::Cast)) {
                getSlotOffset(*inst);
            }
        }
    }

    // Allocate stack frame: local variables + shadow space + outgoing call argument space
    size_t maxRegs = getMaxRegisterParams();
    int32_t outgoingSpace = 0;
    if (isSysV()) {
        outgoingSpace = (maxCallArgs > maxRegs ? static_cast<int32_t>((maxCallArgs - maxRegs) * 8) : 0);
    } else {
        outgoingSpace = 32 + (maxCallArgs > 4 ? static_cast<int32_t>((maxCallArgs - 4) * 8) : 0);
    }
    m_frameSize = m_currentOffset + outgoingSpace;
    if (m_frameSize % 16 != 0) {
        m_frameSize += 16 - (m_frameSize % 16);
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

    // Copy incoming parameters from calling convention registers into stack slots
    if (m_currentFnReturnsSret) {
        storeRegToSlot(getParamReg(0), m_currentFnSretSlot);
    }
    if (isSysV()) {
        size_t intRegIdx = m_currentFnReturnsSret ? 1 : 0;
        size_t floatRegIdx = 0;
        size_t stackParamIdx = 0;
        for (size_t i = 0; i < params.size(); ++i) {
            int32_t offset = getSlotOffset(*params[i]);
            bool isFloat = params[i]->getType()->isFloatingPoint();
            if (isFloat && floatRegIdx < 8) {
                if (params[i]->getType()->isF32()) {
                    m_out << "  movss dword ptr [rbp - " << offset << "], xmm" << floatRegIdx++ << "\n";
                } else {
                    m_out << "  movsd qword ptr [rbp - " << offset << "], xmm" << floatRegIdx++ << "\n";
                }
            } else if (!isFloat && intRegIdx < 6) {
                storeRegToSlot(getParamReg(intRegIdx++), offset);
            } else {
                int32_t stackOffset = 16 + static_cast<int32_t>(stackParamIdx++ * 8);
                m_out << "  mov rax, [rbp + " << stackOffset << "]\n";
                storeRegToSlot("rax", offset);
            }
        }
    } else {
        size_t regOffset = m_currentFnReturnsSret ? 1 : 0;
        for (size_t i = 0; i < params.size() && (i + regOffset) < maxRegs; ++i) {
            int32_t offset = getSlotOffset(*params[i]);
            if (params[i]->getType()->isFloatingPoint()) {
                if (params[i]->getType()->isF32()) {
                    m_out << "  movss dword ptr [rbp - " << offset << "], xmm" << (i + regOffset) << "\n";
                } else {
                    m_out << "  movsd qword ptr [rbp - " << offset << "], xmm" << (i + regOffset) << "\n";
                }
            } else {
                storeRegToSlot(getParamReg(i + regOffset), offset);
            }
        }
        for (size_t i = (maxRegs > regOffset ? maxRegs - regOffset : 0); i < params.size(); ++i) {
            int32_t offset = getSlotOffset(*params[i]);
            int32_t stackOffset = 48 + static_cast<int32_t>((i + regOffset - maxRegs) * 8);
            m_out << "  mov rax, [rbp + " << stackOffset << "]\n";
            storeRegToSlot("rax", offset);
        }
    }

    for (const auto& bb : fn.getBlocks()) {
        generateBasicBlock(*bb, fn);
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
            line == "  mov rsi, rsi" || line == "  mov rdi, rdi" || line == "  mov r8, r8" ||
            line == "  mov r9, r9" || line == "  mov r10, r10" || line == "  mov r11, r11") {
            continue;
        }

        if (!out.empty()) {
            const std::string& prev = out.back();
            // Pattern: prev is "  mov [rbp - X], reg"
            //          line is "  mov reg, [rbp - X]"
            if (prev.rfind("  mov [rbp - ", 0) == 0 && line.rfind("  mov ", 0) == 0) {
                size_t commaPrev = prev.find("], ");
                size_t commaLine = line.find(", [rbp - ");
                if (commaPrev != std::string::npos && commaLine != std::string::npos) {
                    std::string slotPrev = prev.substr(6, commaPrev - 6 + 1); // "[rbp - X]"
                    std::string regPrev = prev.substr(commaPrev + 3);          // "rax"
                    std::string regLine = line.substr(6, commaLine - 6);      // "rax"
                    std::string slotLine = line.substr(commaLine + 2);        // "[rbp - X]"

                    if (slotPrev == slotLine && regPrev == regLine &&
                        (regPrev == "rax" || regPrev == "rcx" || regPrev == "rdx" ||
                         regPrev == "r8" || regPrev == "r9" || regPrev == "r10" || regPrev == "r11")) {
                        // Completely redundant load immediately following store!
                        continue;
                    }
                }
            }
        }
        out.push_back(line);
    }
    return out;
}

void X86_64CodeGen::generateBasicBlock(const IRBasicBlock& bb, const IRFunction& fn) {
    *m_pOut << getLabel(bb, fn) << ":\n";

    std::ostringstream bbStream;
    std::ostream* savedOut = m_pOut;
    m_pOut = &bbStream;

    const auto& insts = bb.getInstructions();
    for (size_t i = 0; i < insts.size(); ++i) {
        // Compare-Branch Fusion
        if (i + 1 < insts.size() && insts[i+1]->getOpcode() == IROpcode::CondBr) {
            const auto& condBr = static_cast<const CondBranchInst&>(*insts[i+1]);
            if (condBr.getCondition().get() == insts[i].get()) {
                auto op = insts[i]->getOpcode();
                if (op == IROpcode::Equal || op == IROpcode::NotEqual ||
                    op == IROpcode::Less || op == IROpcode::LessEqual ||
                    op == IROpcode::Greater || op == IROpcode::GreaterEqual) {

                    const auto& binInst = static_cast<const BinaryInst&>(*insts[i]);
                    loadValueToReg(*binInst.getLhs(), "rax");
                    loadValueToReg(*binInst.getRhs(), "rcx");

                    if (binInst.getLhs()->getType()->isFloatingPoint()) {
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
                            *m_pOut << "  jmp " << getLabel(*condBr.getTrueTarget(), fn) << "\n";
                        } else if (op == IROpcode::NotEqual) {
                            *m_pOut << "  jp " << getLabel(*condBr.getTrueTarget(), fn) << "\n";
                            *m_pOut << "  jne " << getLabel(*condBr.getTrueTarget(), fn) << "\n";
                            *m_pOut << "  jmp " << getLabel(*condBr.getFalseTarget(), fn) << "\n";
                        } else if (op == IROpcode::Less || op == IROpcode::Greater) {
                            *m_pOut << "  ja " << getLabel(*condBr.getTrueTarget(), fn) << "\n";
                            *m_pOut << "  jmp " << getLabel(*condBr.getFalseTarget(), fn) << "\n";
                        } else if (op == IROpcode::LessEqual || op == IROpcode::GreaterEqual) {
                            *m_pOut << "  jae " << getLabel(*condBr.getTrueTarget(), fn) << "\n";
                            *m_pOut << "  jmp " << getLabel(*condBr.getFalseTarget(), fn) << "\n";
                        }
                    } else {
                        *m_pOut << "  cmp rax, rcx\n";

                        std::string jmpInst;
                        switch (op) {
                            case IROpcode::Equal:        jmpInst = "je"; break;
                            case IROpcode::NotEqual:     jmpInst = "jne"; break;
                            case IROpcode::Less:         jmpInst = "jl"; break;
                            case IROpcode::LessEqual:    jmpInst = "jle"; break;
                            case IROpcode::Greater:      jmpInst = "jg"; break;
                            case IROpcode::GreaterEqual: jmpInst = "jge"; break;
                            default:                     jmpInst = "jne"; break;
                        }
                        *m_pOut << "  " << jmpInst << " " << getLabel(*condBr.getTrueTarget(), fn) << "\n";
                        *m_pOut << "  jmp " << getLabel(*condBr.getFalseTarget(), fn) << "\n";
                    }
                    ++i;
                    continue;
                }
            }
        }
        generateInstruction(*insts[i], fn);
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

void X86_64CodeGen::generateInstruction(const IRInstruction& inst, const IRFunction& fn) {
    if (m_debugInfo && inst.getLocation().isValid()) {
        if (inst.getLocation() != m_lastEmittedLoc) {
            m_out << "  .loc " << inst.getLocation().fileId << " "
                  << inst.getLocation().line << " "
                  << inst.getLocation().column << "\n";
            m_lastEmittedLoc = inst.getLocation();
        }
    }
    switch (inst.getOpcode()) {
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
                        m_out << "  lea rdx, [rbp - " << ptrOffset << "]\n";
                    } else {
                        loadValueToReg(*storeInst.getPointer(), "rdx");
                    }
                    for (int32_t off = 0; off < structSize; off += 8) {
                        m_out << "  mov r10, [rax + " << off << "]\n";
                        m_out << "  mov [rdx + " << off << "], r10\n";
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
                    m_out << "  lea rax, [rbp - " << ptrOffset << "]\n";
                } else {
                    loadValueToReg(*loadInst.getPointer(), "rax");
                }
            } else if (isAlloca) {
                int32_t ptrOffset = getSlotOffset(*loadInst.getPointer());
                m_out << "  mov rax, [rbp - " << ptrOffset << "]\n";
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
            int32_t dstOffset = getSlotOffset(loadInst);
            storeRegToSlot("rax", dstOffset);
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
                m_out << "  lea rax, [rbp - " << fieldStackOffset << "]\n";
            } else {
                loadValueToReg(*gfp.getBasePtr(), "rax");
                if (gfp.getByteOffset() != 0) {
                    m_out << "  add rax, " << gfp.getByteOffset() << "\n";
                }
            }
            int32_t dstOffset = getSlotOffset(gfp);
            storeRegToSlot("rax", dstOffset);
            break;
        }
        case IROpcode::GetElementPtr: {
            const auto& gep = static_cast<const GetElementPtrInst&>(inst);
            loadValueToReg(*gep.getIndex(), "rax");
            if (gep.getElementSize() != 1) {
                m_out << "  imul rax, " << gep.getElementSize() << "\n";
            }
            bool isAlloca = gep.getBasePtr()->isInstruction() &&
                static_cast<const IRInstruction*>(gep.getBasePtr().get())->getOpcode() == IROpcode::Alloca;
            if (isAlloca) {
                int32_t baseOffset = getSlotOffset(*gep.getBasePtr());
                m_out << "  lea rdx, [rbp - " << baseOffset << "]\n";
                m_out << "  add rdx, rax\n";
            } else {
                loadValueToReg(*gep.getBasePtr(), "rdx");
                m_out << "  add rdx, rax\n";
            }
            int32_t dstOffset = getSlotOffset(gep);
            storeRegToSlot("rdx", dstOffset);
            break;
        }
        case IROpcode::AddressOf: {
            const auto& addrInst = static_cast<const AddressOfInst&>(inst);
            loadValueToReg(*addrInst.getTarget(), "rax");
            int32_t dstOffset = getSlotOffset(addrInst);
            storeRegToSlot("rax", dstOffset);
            break;
        }
        case IROpcode::Add:
        case IROpcode::Sub:
        case IROpcode::Mul:
        case IROpcode::Div:
        case IROpcode::Mod: {
            const auto& binInst = static_cast<const BinaryInst&>(inst);
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

            int32_t dstOffset = getSlotOffset(binInst);
            storeRegToSlot("rax", dstOffset);
            break;
        }
        case IROpcode::Equal:
        case IROpcode::NotEqual:
        case IROpcode::Less:
        case IROpcode::LessEqual:
        case IROpcode::Greater:
        case IROpcode::GreaterEqual: {
            const auto& binInst = static_cast<const BinaryInst&>(inst);
            loadValueToReg(*binInst.getLhs(), "rax");
            loadValueToReg(*binInst.getRhs(), "rcx");

            if (binInst.getLhs()->getType()->isFloatingPoint()) {
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
                m_out << "  cmp rax, rcx\n";
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

            int32_t dstOffset = getSlotOffset(binInst);
            storeRegToSlot("rax", dstOffset);
            break;
        }
        case IROpcode::LogicalAnd:
        case IROpcode::LogicalOr:
        case IROpcode::BitwiseAnd:
        case IROpcode::BitwiseOr:
        case IROpcode::BitwiseXor: {
            const auto& binInst = static_cast<const BinaryInst&>(inst);
            loadValueToReg(*binInst.getLhs(), "rax");
            loadValueToReg(*binInst.getRhs(), "rcx");

            if (binInst.getOpcode() == IROpcode::LogicalAnd || binInst.getOpcode() == IROpcode::BitwiseAnd) {
                m_out << "  and rax, rcx\n";
            } else if (binInst.getOpcode() == IROpcode::LogicalOr || binInst.getOpcode() == IROpcode::BitwiseOr) {
                m_out << "  or rax, rcx\n";
            } else if (binInst.getOpcode() == IROpcode::BitwiseXor) {
                m_out << "  xor rax, rcx\n";
            }

            int32_t dstOffset = getSlotOffset(binInst);
            storeRegToSlot("rax", dstOffset);
            break;
        }
        case IROpcode::ShiftLeft:
        case IROpcode::ShiftRight: {
            const auto& binInst = static_cast<const BinaryInst&>(inst);
            loadValueToReg(*binInst.getRhs(), "rcx");
            loadValueToReg(*binInst.getLhs(), "rax");

            if (binInst.getOpcode() == IROpcode::ShiftLeft) {
                m_out << "  shl rax, cl\n";
            } else {
                m_out << "  sar rax, cl\n";
            }

            int32_t dstOffset = getSlotOffset(binInst);
            storeRegToSlot("rax", dstOffset);
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
            int32_t dstOffset = getSlotOffset(unInst);
            storeRegToSlot("rax", dstOffset);
            break;
        }
        case IROpcode::LogicalNot: {
            const auto& unInst = static_cast<const UnaryInst&>(inst);
            loadValueToReg(*unInst.getOperand(), "rax");
            m_out << "  xor rax, 1\n";
            int32_t dstOffset = getSlotOffset(unInst);
            storeRegToSlot("rax", dstOffset);
            break;
        }
        case IROpcode::BitwiseNot: {
            const auto& unInst = static_cast<const UnaryInst&>(inst);
            loadValueToReg(*unInst.getOperand(), "rax");
            m_out << "  not rax\n";
            int32_t dstOffset = getSlotOffset(unInst);
            storeRegToSlot("rax", dstOffset);
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
            int32_t dstOffset = getSlotOffset(castInst);
            storeRegToSlot("rax", dstOffset);
            break;
        }
        case IROpcode::Ret: {
            const auto& retInst = static_cast<const RetInst&>(inst);
            if (retInst.hasValue()) {
                loadValueToReg(*retInst.getValue(), "rax");
                if (m_currentFnReturnsSret) {
                    m_out << "  mov rdi, [rbp - " << m_currentFnSretSlot << "]\n";
                    m_out << "  mov rsi, rax\n";
                    for (int32_t off = 0; off < m_currentFnSretSize; off += 8) {
                        m_out << "  mov r10, [rsi + " << off << "]\n";
                        m_out << "  mov [rdi + " << off << "], r10\n";
                    }
                    m_out << "  mov rax, rdi\n";
                } else if (retInst.getValue()->getType()->isFloatingPoint()) {
                    if (retInst.getValue()->getType()->isF32()) {
                        m_out << "  movd xmm0, eax\n";
                    } else {
                        m_out << "  movq xmm0, rax\n";
                    }
                }
            }
            m_out << "  mov rsp, rbp\n";
            m_out << "  pop rbp\n";
            m_out << "  ret\n";
            break;
        }
        case IROpcode::Br: {
            const auto& brInst = static_cast<const BranchInst&>(inst);
            m_out << "  jmp " << getLabel(*brInst.getTarget(), fn) << "\n";
            break;
        }
        case IROpcode::CondBr: {
            const auto& condBr = static_cast<const CondBranchInst&>(inst);
            loadValueToReg(*condBr.getCondition(), "rax");
            m_out << "  cmp rax, 0\n";
            m_out << "  jne " << getLabel(*condBr.getTrueTarget(), fn) << "\n";
            m_out << "  jmp " << getLabel(*condBr.getFalseTarget(), fn) << "\n";
            break;
        }
        case IROpcode::Call: {
            const auto& callInst = static_cast<const CallInst&>(inst);
            bool isSret = callInst.getType()->isStruct() &&
                std::static_pointer_cast<StructType>(callInst.getType())->getSize() > 8 &&
                callInst.getCallee().rfind("sph_", 0) != 0;
            int32_t dstOffset = getSlotOffset(callInst);

            const auto& args = callInst.getArgs();
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
                    m_out << "  lea " << getParamReg(0) << ", [rbp - " << dstOffset << "]\n";
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
                    m_out << "  lea " << getParamReg(0) << ", [rbp - " << dstOffset << "]\n";
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
                int32_t dstOffset = getSlotOffset(callInst);
                storeRegToSlot("rax", dstOffset);
            }
            break;
        }
        case IROpcode::RcAlloc: {
            const auto& rcAlloc = static_cast<const RcAllocInst&>(inst);
            m_out << "  mov " << getParamReg(0) << ", " << rcAlloc.getAllocSize() << "\n";
            m_out << "  call sph_rc_alloc\n";
            int32_t dstOffset = getSlotOffset(rcAlloc);
            storeRegToSlot("rax", dstOffset);
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
        case IROpcode::SetJmp: {
            const auto& sj = static_cast<const SetJmpInst&>(inst);
            loadValueToReg(*sj.getFramePtr(), getParamReg(0));
            m_out << "  call sph_setjmp\n";
            int32_t dstOffset = getSlotOffset(sj);
            storeRegToSlot("rax", dstOffset);
            break;
        }
    }
}

} // namespace sph

