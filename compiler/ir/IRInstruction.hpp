#pragma once

#include "IRValue.hpp"
#include "compiler/source/SourceLocation.hpp"
#include <memory>
#include <string>
#include <vector>

namespace sph {

class IRBasicBlock;

enum class IROpcode {
    // Memory
    Alloca,
    Load,
    Store,
    GetFieldPtr,
    GetElementPtr,
    AddressOf,

    // Binary Arithmetic & Logic
    Add,
    Sub,
    Mul,
    Div,
    Mod,
    Equal,
    NotEqual,
    Less,
    LessEqual,
    Greater,
    GreaterEqual,
    LogicalAnd,
    LogicalOr,
    BitwiseAnd,
    BitwiseOr,
    BitwiseXor,
    ShiftLeft,
    ShiftRight,

    // Unary
    Negate,
    LogicalNot,
    BitwiseNot,

    // Cast / Conversion
    Cast,

    // Control Flow (Terminators)
    Ret,
    Br,
    CondBr,

    // Calls
    Call,

    // Reference Counting
    RcAlloc,
    RcRetain,
    RcRelease,

    // Exceptions
    SetJmp
};

[[nodiscard]] const char* opcodeToString(IROpcode op) noexcept;

class IRInstruction : public IRValue {
public:
    IROpcode getOpcode() const noexcept { return m_opcode; }
    void setOpcode(IROpcode op) noexcept { m_opcode = op; }
    IRBasicBlock* getParent() const noexcept { return m_parent; }
    void setParent(IRBasicBlock* parent) noexcept { m_parent = parent; }

    [[nodiscard]] const SourceLocation& getLocation() const noexcept { return m_loc; }
    void setLocation(SourceLocation loc) noexcept { m_loc = loc; }

    [[nodiscard]] bool isInstruction() const noexcept override { return true; }
    [[nodiscard]] virtual bool isTerminator() const noexcept { return false; }

protected:
    IRInstruction(IROpcode op, std::shared_ptr<Type> type, std::string name)
        : IRValue(std::move(type), std::move(name)), m_opcode(op) {}

private:
    IROpcode m_opcode;
    IRBasicBlock* m_parent = nullptr;
    SourceLocation m_loc;
};

// Memory Instructions
class AllocaInst : public IRInstruction {
public:
    AllocaInst(std::shared_ptr<Type> allocatedType, std::string name)
        : IRInstruction(IROpcode::Alloca, allocatedType, std::move(name)),
          m_allocatedType(std::move(allocatedType)) {}

    [[nodiscard]] const std::shared_ptr<Type>& getAllocatedType() const noexcept { return m_allocatedType; }

private:
    std::shared_ptr<Type> m_allocatedType;
};

class LoadInst : public IRInstruction {
public:
    LoadInst(std::shared_ptr<IRValue> pointer, std::string name)
        : IRInstruction(IROpcode::Load, pointer->getType(), std::move(name)),
          m_pointer(std::move(pointer)) {}

    LoadInst(std::shared_ptr<Type> loadedType, std::shared_ptr<IRValue> pointer, std::string name)
        : IRInstruction(IROpcode::Load, std::move(loadedType), std::move(name)),
          m_pointer(std::move(pointer)) {}

    [[nodiscard]] const std::shared_ptr<IRValue>& getPointer() const noexcept { return m_pointer; }
    void setPointer(std::shared_ptr<IRValue> ptr) noexcept { m_pointer = std::move(ptr); }

private:
    std::shared_ptr<IRValue> m_pointer;
};

class AddressOfInst : public IRInstruction {
public:
    AddressOfInst(std::shared_ptr<IRValue> target, std::shared_ptr<Type> ptrType, std::string name)
        : IRInstruction(IROpcode::AddressOf, std::move(ptrType), std::move(name)),
          m_target(std::move(target)) {}

    [[nodiscard]] const std::shared_ptr<IRValue>& getTarget() const noexcept { return m_target; }
    void setTarget(std::shared_ptr<IRValue> target) noexcept { m_target = std::move(target); }

private:
    std::shared_ptr<IRValue> m_target;
};

class StoreInst : public IRInstruction {
public:
    StoreInst(std::shared_ptr<IRValue> value, std::shared_ptr<IRValue> pointer)
        : IRInstruction(IROpcode::Store, Type::getVoid(), ""),
          m_value(std::move(value)), m_pointer(std::move(pointer)) {}

    [[nodiscard]] const std::shared_ptr<IRValue>& getValue() const noexcept { return m_value; }
    [[nodiscard]] const std::shared_ptr<IRValue>& getPointer() const noexcept { return m_pointer; }
    void setValue(std::shared_ptr<IRValue> val) noexcept { m_value = std::move(val); }
    void setPointer(std::shared_ptr<IRValue> ptr) noexcept { m_pointer = std::move(ptr); }

private:
    std::shared_ptr<IRValue> m_value;
    std::shared_ptr<IRValue> m_pointer;
};

class GetFieldPtrInst : public IRInstruction {
public:
    GetFieldPtrInst(std::shared_ptr<IRValue> basePtr, size_t fieldIndex, size_t byteOffset,
                    std::shared_ptr<Type> fieldType, std::string name)
        : IRInstruction(IROpcode::GetFieldPtr, std::move(fieldType), std::move(name)),
          m_basePtr(std::move(basePtr)), m_fieldIndex(fieldIndex), m_byteOffset(byteOffset) {}

    [[nodiscard]] const std::shared_ptr<IRValue>& getBasePtr() const noexcept { return m_basePtr; }
    [[nodiscard]] size_t getFieldIndex() const noexcept { return m_fieldIndex; }
    [[nodiscard]] size_t getByteOffset() const noexcept { return m_byteOffset; }
    void setBasePtr(std::shared_ptr<IRValue> ptr) noexcept { m_basePtr = std::move(ptr); }

private:
    std::shared_ptr<IRValue> m_basePtr;
    size_t m_fieldIndex;
    size_t m_byteOffset;
};

class GetElementPtrInst : public IRInstruction {
public:
    GetElementPtrInst(std::shared_ptr<IRValue> basePtr, std::shared_ptr<IRValue> index,
                      size_t elementSize, std::shared_ptr<Type> elementType, std::string name)
        : IRInstruction(IROpcode::GetElementPtr, std::move(elementType), std::move(name)),
          m_basePtr(std::move(basePtr)), m_index(std::move(index)), m_elementSize(elementSize) {}

    [[nodiscard]] const std::shared_ptr<IRValue>& getBasePtr() const noexcept { return m_basePtr; }
    [[nodiscard]] const std::shared_ptr<IRValue>& getIndex() const noexcept { return m_index; }
    [[nodiscard]] size_t getElementSize() const noexcept { return m_elementSize; }
    void setBasePtr(std::shared_ptr<IRValue> ptr) noexcept { m_basePtr = std::move(ptr); }
    void setIndex(std::shared_ptr<IRValue> idx) noexcept { m_index = std::move(idx); }

private:
    std::shared_ptr<IRValue> m_basePtr;
    std::shared_ptr<IRValue> m_index;
    size_t m_elementSize = 0;
};

// Binary Instruction
class BinaryInst : public IRInstruction {
public:
    BinaryInst(IROpcode op, std::shared_ptr<IRValue> lhs, std::shared_ptr<IRValue> rhs,
               std::shared_ptr<Type> resultType, std::string name)
        : IRInstruction(op, std::move(resultType), std::move(name)),
          m_lhs(std::move(lhs)), m_rhs(std::move(rhs)) {}

    [[nodiscard]] const std::shared_ptr<IRValue>& getLhs() const noexcept { return m_lhs; }
    [[nodiscard]] const std::shared_ptr<IRValue>& getRhs() const noexcept { return m_rhs; }
    void setLhs(std::shared_ptr<IRValue> lhs) noexcept { m_lhs = std::move(lhs); }
    void setRhs(std::shared_ptr<IRValue> rhs) noexcept { m_rhs = std::move(rhs); }

private:
    std::shared_ptr<IRValue> m_lhs;
    std::shared_ptr<IRValue> m_rhs;
};

// Unary Instruction
class UnaryInst : public IRInstruction {
public:
    UnaryInst(IROpcode op, std::shared_ptr<IRValue> operand, std::shared_ptr<Type> resultType, std::string name)
        : IRInstruction(op, std::move(resultType), std::move(name)),
          m_operand(std::move(operand)) {}

    [[nodiscard]] const std::shared_ptr<IRValue>& getOperand() const noexcept { return m_operand; }
    void setOperand(std::shared_ptr<IRValue> op) noexcept { m_operand = std::move(op); }

private:
    std::shared_ptr<IRValue> m_operand;
};

// Terminators
class RetInst : public IRInstruction {
public:
    explicit RetInst(std::shared_ptr<IRValue> val = nullptr)
        : IRInstruction(IROpcode::Ret, val ? val->getType() : Type::getVoid(), ""),
          m_value(std::move(val)) {}

    [[nodiscard]] bool hasValue() const noexcept { return m_value != nullptr; }
    [[nodiscard]] const std::shared_ptr<IRValue>& getValue() const noexcept { return m_value; }
    void setValue(std::shared_ptr<IRValue> val) noexcept { m_value = std::move(val); }
    [[nodiscard]] bool isTerminator() const noexcept override { return true; }

private:
    std::shared_ptr<IRValue> m_value;
};

class BranchInst : public IRInstruction {
public:
    explicit BranchInst(IRBasicBlock* target)
        : IRInstruction(IROpcode::Br, Type::getVoid(), ""), m_target(target) {}

    [[nodiscard]] IRBasicBlock* getTarget() const noexcept { return m_target; }
    void setTarget(IRBasicBlock* target) noexcept { m_target = target; }
    [[nodiscard]] bool isTerminator() const noexcept override { return true; }

private:
    IRBasicBlock* m_target;
};

class CondBranchInst : public IRInstruction {
public:
    CondBranchInst(std::shared_ptr<IRValue> cond, IRBasicBlock* trueTarget, IRBasicBlock* falseTarget)
        : IRInstruction(IROpcode::CondBr, Type::getVoid(), ""),
          m_condition(std::move(cond)), m_trueTarget(trueTarget), m_falseTarget(falseTarget) {}

    [[nodiscard]] const std::shared_ptr<IRValue>& getCondition() const noexcept { return m_condition; }
    void setCondition(std::shared_ptr<IRValue> cond) noexcept { m_condition = std::move(cond); }
    [[nodiscard]] IRBasicBlock* getTrueTarget() const noexcept { return m_trueTarget; }
    void setTrueTarget(IRBasicBlock* trueTarget) noexcept { m_trueTarget = trueTarget; }
    [[nodiscard]] IRBasicBlock* getFalseTarget() const noexcept { return m_falseTarget; }
    void setFalseTarget(IRBasicBlock* falseTarget) noexcept { m_falseTarget = falseTarget; }
    [[nodiscard]] bool isTerminator() const noexcept override { return true; }

private:
    std::shared_ptr<IRValue> m_condition;
    IRBasicBlock* m_trueTarget;
    IRBasicBlock* m_falseTarget;
};

// Call Instruction
class CallInst : public IRInstruction {
public:
    CallInst(std::string callee, std::vector<std::shared_ptr<IRValue>> args,
             std::shared_ptr<Type> returnType, std::string name)
        : IRInstruction(IROpcode::Call, std::move(returnType), std::move(name)),
          m_callee(std::move(callee)), m_args(std::move(args)) {}

    CallInst(std::shared_ptr<IRValue> calleeVal, std::vector<std::shared_ptr<IRValue>> args,
             std::shared_ptr<Type> returnType, std::string name)
        : IRInstruction(IROpcode::Call, std::move(returnType), std::move(name)),
          m_calleeVal(std::move(calleeVal)), m_args(std::move(args)) {}

    [[nodiscard]] bool isIndirect() const noexcept { return m_calleeVal != nullptr; }
    [[nodiscard]] const std::shared_ptr<IRValue>& getCalleeValue() const noexcept { return m_calleeVal; }
    void setCalleeValue(std::shared_ptr<IRValue> val) noexcept { m_calleeVal = std::move(val); }
    [[nodiscard]] const std::string& getCallee() const noexcept { return m_callee; }
    void setCallee(std::string callee) noexcept { m_callee = std::move(callee); }
    [[nodiscard]] const std::vector<std::shared_ptr<IRValue>>& getArgs() const noexcept { return m_args; }
    [[nodiscard]] std::vector<std::shared_ptr<IRValue>>& getArgsMutable() noexcept { return m_args; }
    void setArg(size_t index, std::shared_ptr<IRValue> arg) noexcept {
        if (index < m_args.size()) m_args[index] = std::move(arg);
    }

private:
    std::string m_callee;
    std::shared_ptr<IRValue> m_calleeVal;
    std::vector<std::shared_ptr<IRValue>> m_args;
};

// Cast Instruction
class CastInst : public IRInstruction {
public:
    CastInst(std::shared_ptr<IRValue> operand, std::shared_ptr<Type> targetType, std::string name)
        : IRInstruction(IROpcode::Cast, std::move(targetType), std::move(name)),
          m_operand(std::move(operand)) {}

    [[nodiscard]] const std::shared_ptr<IRValue>& getOperand() const noexcept { return m_operand; }
    void setOperand(std::shared_ptr<IRValue> operand) noexcept { m_operand = std::move(operand); }

private:
    std::shared_ptr<IRValue> m_operand;
};

// Reference Counting Instructions
class RcAllocInst : public IRInstruction {
public:
    RcAllocInst(std::shared_ptr<Type> innerType, size_t allocSize, std::string name)
        : IRInstruction(IROpcode::RcAlloc, RcType::get(innerType), std::move(name)),
          m_innerType(std::move(innerType)), m_allocSize(allocSize) {}

    [[nodiscard]] const std::shared_ptr<Type>& getInnerType() const noexcept { return m_innerType; }
    [[nodiscard]] size_t getAllocSize() const noexcept { return m_allocSize; }

private:
    std::shared_ptr<Type> m_innerType;
    size_t m_allocSize;
};

class RcRetainInst : public IRInstruction {
public:
    explicit RcRetainInst(std::shared_ptr<IRValue> ptr)
        : IRInstruction(IROpcode::RcRetain, Type::getVoid(), ""),
          m_ptr(std::move(ptr)) {}

    [[nodiscard]] const std::shared_ptr<IRValue>& getPtr() const noexcept { return m_ptr; }
    void setPtr(std::shared_ptr<IRValue> ptr) noexcept { m_ptr = std::move(ptr); }

private:
    std::shared_ptr<IRValue> m_ptr;
};

class RcReleaseInst : public IRInstruction {
public:
    explicit RcReleaseInst(std::shared_ptr<IRValue> ptr)
        : IRInstruction(IROpcode::RcRelease, Type::getVoid(), ""),
          m_ptr(std::move(ptr)) {}

    [[nodiscard]] const std::shared_ptr<IRValue>& getPtr() const noexcept { return m_ptr; }
    void setPtr(std::shared_ptr<IRValue> ptr) noexcept { m_ptr = std::move(ptr); }

private:
    std::shared_ptr<IRValue> m_ptr;
};

// Exception SetJmp Instruction
class SetJmpInst : public IRInstruction {
public:
    SetJmpInst(std::shared_ptr<IRValue> framePtr, std::string name)
        : IRInstruction(IROpcode::SetJmp, Type::getI32(), std::move(name)),
          m_framePtr(std::move(framePtr)) {}

    [[nodiscard]] const std::shared_ptr<IRValue>& getFramePtr() const noexcept { return m_framePtr; }
    void setFramePtr(std::shared_ptr<IRValue> ptr) noexcept { m_framePtr = std::move(ptr); }

private:
    std::shared_ptr<IRValue> m_framePtr;
};

// Instruction Operand Substitution and Query Utilities
bool replaceOperandInInstruction(IRInstruction& inst, const IRValue* oldVal, std::shared_ptr<IRValue> newVal);
std::vector<const IRValue*> getInstructionOperands(const IRInstruction& inst);

} // namespace sph

