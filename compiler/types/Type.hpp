#pragma once

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace sph {

class ClassType;

enum class TypeKind {
    Void,
    I32,
    I64,
    F32,
    F64,
    Bool,
    String,
    Function,
    Struct,
    Array,
    Slice,
    Pointer,
    Rc,
    Box,
    Arena,
    Vector,
    Result,
    Dynamic,
    Enum,
    Interface,
    Class,
    Error,
    Unknown
};

class Type {
public:
    virtual ~Type() = default;

    [[nodiscard]] TypeKind getKind() const noexcept { return m_kind; }
    [[nodiscard]] virtual std::string toString() const;
    [[nodiscard]] virtual bool isEqual(const Type& other) const;

    [[nodiscard]] bool isVoid() const noexcept { return m_kind == TypeKind::Void; }
    [[nodiscard]] bool isI32() const noexcept { return m_kind == TypeKind::I32; }
    [[nodiscard]] bool isI64() const noexcept { return m_kind == TypeKind::I64; }
    [[nodiscard]] bool isF32() const noexcept { return m_kind == TypeKind::F32; }
    [[nodiscard]] bool isF64() const noexcept { return m_kind == TypeKind::F64; }
    [[nodiscard]] bool isInteger() const noexcept { return m_kind == TypeKind::I32 || m_kind == TypeKind::I64; }
    [[nodiscard]] bool isFloatingPoint() const noexcept { return m_kind == TypeKind::F32 || m_kind == TypeKind::F64; }
    [[nodiscard]] bool isNumeric() const noexcept { return isInteger() || isFloatingPoint(); }
    [[nodiscard]] bool isBoolean() const noexcept { return m_kind == TypeKind::Bool; }
    [[nodiscard]] bool isString() const noexcept { return m_kind == TypeKind::String; }
    [[nodiscard]] bool isError() const noexcept { return m_kind == TypeKind::Error; }
    [[nodiscard]] bool isUnknown() const noexcept { return m_kind == TypeKind::Unknown; }
    [[nodiscard]] bool isFunction() const noexcept { return m_kind == TypeKind::Function; }
    [[nodiscard]] bool isStruct() const noexcept { return m_kind == TypeKind::Struct; }
    [[nodiscard]] bool isArray() const noexcept { return m_kind == TypeKind::Array; }
    [[nodiscard]] bool isSlice() const noexcept { return m_kind == TypeKind::Slice; }
    [[nodiscard]] bool isPointer() const noexcept { return m_kind == TypeKind::Pointer; }
    [[nodiscard]] bool isRc() const noexcept { return m_kind == TypeKind::Rc; }
    [[nodiscard]] bool isBox() const noexcept { return m_kind == TypeKind::Box; }
    [[nodiscard]] bool isArena() const noexcept { return m_kind == TypeKind::Arena; }
    [[nodiscard]] bool isVector() const noexcept { return m_kind == TypeKind::Vector; }
    [[nodiscard]] bool isResult() const noexcept { return m_kind == TypeKind::Result; }
    [[nodiscard]] bool isDynamic() const noexcept { return m_kind == TypeKind::Dynamic; }
    [[nodiscard]] virtual bool isMoveOnly() const noexcept { return isBox() || isArena() || isVector(); }
    [[nodiscard]] virtual bool isCopy() const noexcept {
        return !isMoveOnly();
    }
    [[nodiscard]] bool isEnum() const noexcept { return m_kind == TypeKind::Enum; }
    [[nodiscard]] bool isInterface() const noexcept { return m_kind == TypeKind::Interface; }
    [[nodiscard]] bool isClass() const noexcept { return m_kind == TypeKind::Class; }
    [[nodiscard]] bool isPromise() const noexcept;
    [[nodiscard]] bool isSimd() const noexcept;
    [[nodiscard]] bool isF32x4() const noexcept;
    [[nodiscard]] bool isI32x4() const noexcept;
    [[nodiscard]] bool isF64x2() const noexcept;
    [[nodiscard]] bool isI64x2() const noexcept;
    [[nodiscard]] bool isRecord() const noexcept { return isStruct() || isClass(); }

    [[nodiscard]] virtual size_t getSize() const;
    [[nodiscard]] virtual size_t getAlignment() const;

    static std::shared_ptr<Type> getVoid();
    static std::shared_ptr<Type> getI32();
    static std::shared_ptr<Type> getI64();
    static std::shared_ptr<Type> getF32();
    static std::shared_ptr<Type> getF64();
    static std::shared_ptr<Type> getBool();
    static std::shared_ptr<Type> getString();
    static std::shared_ptr<Type> getDynamic();
    static std::shared_ptr<Type> getError();
    static std::shared_ptr<Type> getUnknown();
    static std::shared_ptr<ClassType> getF32x4();
    static std::shared_ptr<ClassType> getI32x4();
    static std::shared_ptr<ClassType> getF64x2();
    static std::shared_ptr<ClassType> getI64x2();

    static std::shared_ptr<Type> fromString(const std::string& name);

protected:
    explicit Type(TypeKind kind) : m_kind(kind) {}

private:
    TypeKind m_kind;
};

class FunctionType : public Type {
public:
    FunctionType(std::shared_ptr<Type> returnType, std::vector<std::shared_ptr<Type>> paramTypes)
        : Type(TypeKind::Function), m_returnType(std::move(returnType)), m_paramTypes(std::move(paramTypes)) {}

    [[nodiscard]] const std::shared_ptr<Type>& getReturnType() const noexcept { return m_returnType; }
    [[nodiscard]] const std::vector<std::shared_ptr<Type>>& getParamTypes() const noexcept { return m_paramTypes; }

    [[nodiscard]] std::string toString() const override;
    [[nodiscard]] bool isEqual(const Type& other) const override;

    static std::shared_ptr<FunctionType> get(std::shared_ptr<Type> returnType,
                                             std::vector<std::shared_ptr<Type>> paramTypes);

private:
    std::shared_ptr<Type> m_returnType;
    std::vector<std::shared_ptr<Type>> m_paramTypes;
};

struct StructMember {
    std::string name;
    std::shared_ptr<Type> type;
    size_t offset = 0;
};

class StructType : public Type {
public:
    StructType(std::string name, std::vector<StructMember> members, bool isPacked, size_t explicitAlign = 0, TypeKind kind = TypeKind::Struct);
    StructType(std::string name, std::vector<StructMember> members, TypeKind kind = TypeKind::Struct);

    [[nodiscard]] const std::string& getName() const noexcept { return m_name; }
    [[nodiscard]] const std::vector<StructMember>& getMembers() const noexcept { return m_members; }
    [[nodiscard]] const StructMember* findMember(const std::string& name) const;
    [[nodiscard]] int getMemberIndex(const std::string& name) const;
    [[nodiscard]] bool isPacked() const noexcept { return m_isPacked; }
    [[nodiscard]] size_t getExplicitAlignment() const noexcept { return m_explicitAlignment; }

    [[nodiscard]] size_t getSize() const override { return m_size; }
    [[nodiscard]] size_t getAlignment() const override { return m_alignment; }
    [[nodiscard]] std::string toString() const override { return m_name; }
    [[nodiscard]] bool isEqual(const Type& other) const override;
    [[nodiscard]] bool isMoveOnly() const noexcept override {
        for (const auto& m : m_members) {
            if (m.type && m.type->isMoveOnly()) return true;
        }
        return false;
    }

    static std::shared_ptr<StructType> get(std::string name, std::vector<StructMember> members, bool isPacked = false, size_t explicitAlign = 0);

private:
    std::string m_name;
    std::vector<StructMember> m_members;
    size_t m_size = 0;
    size_t m_alignment = 1;
    bool m_isPacked = false;
    size_t m_explicitAlignment = 0;
};

struct ClassMethodInfo {
    std::string name;
    std::shared_ptr<Type> returnType;
    std::vector<std::shared_ptr<Type>> paramTypes;
    bool isMutating = false;
};

class ClassType : public StructType {
public:
    ClassType(std::string name, std::vector<StructMember> fields)
        : StructType(std::move(name), std::move(fields), TypeKind::Class) {}

    void addMethod(std::string name, std::shared_ptr<Type> returnType,
                   std::vector<std::shared_ptr<Type>> paramTypes, bool isMutating = false) {
        m_methods[name] = ClassMethodInfo{name, std::move(returnType), std::move(paramTypes), isMutating};
    }

    [[nodiscard]] const ClassMethodInfo* findMethod(const std::string& name) const {
        auto it = m_methods.find(name);
        if (it != m_methods.end()) {
            return &it->second;
        }
        return nullptr;
    }

    [[nodiscard]] const std::unordered_map<std::string, ClassMethodInfo>& getMethods() const noexcept {
        return m_methods;
    }

    void addImplements(std::string ifaceName) {
        m_implements.push_back(std::move(ifaceName));
    }

    [[nodiscard]] const std::vector<std::string>& getImplements() const noexcept {
        return m_implements;
    }

    [[nodiscard]] bool implementsInterface(const std::string& ifaceName) const {
        for (const auto& i : m_implements) {
            if (i == ifaceName) return true;
        }
        return false;
    }

    static std::shared_ptr<ClassType> get(std::string name, std::vector<StructMember> fields) {
        return std::make_shared<ClassType>(std::move(name), std::move(fields));
    }

private:
    std::unordered_map<std::string, ClassMethodInfo> m_methods;
    std::vector<std::string> m_implements;
};

class PromiseType : public ClassType {
public:
    explicit PromiseType(std::shared_ptr<Type> innerType = nullptr)
        : ClassType("Promise", {{"handle", Type::getI64(), 0}}), m_innerType(std::move(innerType)) {
        addMethod("__construct", Type::getVoid(), {}, true);
        addMethod("resolve", Type::getVoid(), {m_innerType ? m_innerType : Type::getI64()}, true);
        addMethod("reject", Type::getVoid(), {Type::getI64()}, true);
        addMethod("is_ready", Type::getBool(), {}, false);
        addMethod("get_value", m_innerType ? m_innerType : Type::getI64(), {}, false);
        addMethod("await", m_innerType ? m_innerType : Type::getI64(), {}, true);
    }

    [[nodiscard]] const std::shared_ptr<Type>& getInnerType() const noexcept { return m_innerType; }

    [[nodiscard]] std::string toString() const override {
        if (m_innerType) {
            return "Promise<" + m_innerType->toString() + ">";
        }
        return "Promise";
    }

    [[nodiscard]] bool isEqual(const Type& other) const override {
        if (other.getKind() != TypeKind::Class) return false;
        const auto* ct = dynamic_cast<const PromiseType*>(&other);
        if (!ct) {
            const auto* cls = dynamic_cast<const ClassType*>(&other);
            return cls && cls->getName() == "Promise";
        }
        if (!m_innerType && !ct->m_innerType) return true;
        if (!m_innerType || !ct->m_innerType) return true;
        return m_innerType->isEqual(*ct->m_innerType);
    }

    static std::shared_ptr<PromiseType> get(std::shared_ptr<Type> innerType = nullptr) {
        return std::make_shared<PromiseType>(std::move(innerType));
    }

private:
    std::shared_ptr<Type> m_innerType;
};

struct InterfaceMethodInfo {
    std::string name;
    std::shared_ptr<Type> returnType;
    std::vector<std::shared_ptr<Type>> paramTypes;
};

class InterfaceType : public Type {
public:
    InterfaceType(std::string name, std::vector<InterfaceMethodInfo> methods)
        : Type(TypeKind::Interface), m_name(std::move(name)), m_methods(std::move(methods)) {}

    [[nodiscard]] const std::string& getName() const noexcept { return m_name; }
    [[nodiscard]] const std::vector<InterfaceMethodInfo>& getMethods() const noexcept { return m_methods; }

    [[nodiscard]] const InterfaceMethodInfo* findMethod(const std::string& name) const {
        for (const auto& m : m_methods) {
            if (m.name == name) return &m;
        }
        return nullptr;
    }

    [[nodiscard]] int getMethodIndex(const std::string& name) const {
        for (size_t i = 0; i < m_methods.size(); ++i) {
            if (m_methods[i].name == name) return static_cast<int>(i);
        }
        return -1;
    }

    [[nodiscard]] std::string toString() const override { return m_name; }
    [[nodiscard]] bool isEqual(const Type& other) const override {
        if (other.getKind() != TypeKind::Interface) return false;
        const auto& ifaceOther = static_cast<const InterfaceType&>(other);
        return m_name == ifaceOther.m_name;
    }

    [[nodiscard]] size_t getSize() const override { return 16; }
    [[nodiscard]] size_t getAlignment() const override { return 8; }

    static std::shared_ptr<InterfaceType> get(std::string name, std::vector<InterfaceMethodInfo> methods) {
        return std::make_shared<InterfaceType>(std::move(name), std::move(methods));
    }

private:
    std::string m_name;
    std::vector<InterfaceMethodInfo> m_methods;
};

class ArrayType : public Type {
public:
    ArrayType(std::shared_ptr<Type> elementType, size_t elementCount)
        : Type(TypeKind::Array), m_elementType(std::move(elementType)), m_elementCount(elementCount) {}

    [[nodiscard]] const std::shared_ptr<Type>& getElementType() const noexcept { return m_elementType; }
    [[nodiscard]] size_t getElementCount() const noexcept { return m_elementCount; }

    [[nodiscard]] size_t getSize() const override {
        return m_elementType ? m_elementType->getSize() * m_elementCount : 0;
    }
    [[nodiscard]] size_t getAlignment() const override {
        return m_elementType ? m_elementType->getAlignment() : 1;
    }
    [[nodiscard]] std::string toString() const override {
        return "[" + (m_elementType ? m_elementType->toString() : "<?>") + "; " + std::to_string(m_elementCount) + "]";
    }
    [[nodiscard]] bool isEqual(const Type& other) const override {
        if (other.getKind() != TypeKind::Array) return false;
        const auto& arrOther = static_cast<const ArrayType&>(other);
        return m_elementCount == arrOther.m_elementCount &&
               m_elementType && arrOther.m_elementType &&
               m_elementType->isEqual(*arrOther.m_elementType);
    }

    static std::shared_ptr<ArrayType> get(std::shared_ptr<Type> elementType, size_t elementCount) {
        return std::make_shared<ArrayType>(std::move(elementType), elementCount);
    }

private:
    std::shared_ptr<Type> m_elementType;
    size_t m_elementCount = 0;
};

class SliceType : public Type {
public:
    SliceType(std::shared_ptr<Type> elementType)
        : Type(TypeKind::Slice), m_elementType(std::move(elementType)) {}

    [[nodiscard]] const std::shared_ptr<Type>& getElementType() const noexcept { return m_elementType; }

    [[nodiscard]] size_t getSize() const override { return 16; }
    [[nodiscard]] size_t getAlignment() const override { return 8; }

    [[nodiscard]] std::string toString() const override {
        return "&[" + (m_elementType ? m_elementType->toString() : "<?>") + "]";
    }

    [[nodiscard]] bool isEqual(const Type& other) const override {
        if (other.getKind() != TypeKind::Slice) return false;
        const auto& sliceOther = static_cast<const SliceType&>(other);
        return m_elementType && sliceOther.m_elementType &&
               m_elementType->isEqual(*sliceOther.m_elementType);
    }

    static std::shared_ptr<SliceType> get(std::shared_ptr<Type> elementType) {
        return std::make_shared<SliceType>(std::move(elementType));
    }

private:
    std::shared_ptr<Type> m_elementType;
};

struct EnumVariantInfo {
    std::string name;
    int64_t value;
};

class EnumType : public Type {
public:
    EnumType(std::string name, std::vector<EnumVariantInfo> variants)
        : Type(TypeKind::Enum), m_name(std::move(name)), m_variants(std::move(variants)) {}

    [[nodiscard]] const std::string& getName() const noexcept { return m_name; }
    [[nodiscard]] const std::vector<EnumVariantInfo>& getVariants() const noexcept { return m_variants; }

    [[nodiscard]] const EnumVariantInfo* findVariant(const std::string& name) const {
        for (const auto& v : m_variants) {
            if (v.name == name) return &v;
        }
        return nullptr;
    }

    [[nodiscard]] std::string toString() const override { return m_name; }
    [[nodiscard]] bool isEqual(const Type& other) const override {
        if (other.getKind() != TypeKind::Enum) return false;
        const auto& enumOther = static_cast<const EnumType&>(other);
        return m_name == enumOther.m_name;
    }

    [[nodiscard]] size_t getSize() const override { return 4; }
    [[nodiscard]] size_t getAlignment() const override { return 4; }

    static std::shared_ptr<EnumType> get(std::string name, std::vector<EnumVariantInfo> variants) {
        return std::make_shared<EnumType>(std::move(name), std::move(variants));
    }

private:
    std::string m_name;
    std::vector<EnumVariantInfo> m_variants;
};

class PointerType : public Type {
public:
    explicit PointerType(std::shared_ptr<Type> pointeeType)
        : Type(TypeKind::Pointer), m_pointeeType(std::move(pointeeType)) {}

    [[nodiscard]] const std::shared_ptr<Type>& getPointeeType() const noexcept { return m_pointeeType; }

    [[nodiscard]] std::string toString() const override {
        return "*" + (m_pointeeType ? m_pointeeType->toString() : "void");
    }

    [[nodiscard]] bool isEqual(const Type& other) const override {
        if (other.getKind() != getKind()) return false;
        const auto& o = static_cast<const PointerType&>(other);
        if (!m_pointeeType && !o.m_pointeeType) return true;
        if (!m_pointeeType || !o.m_pointeeType) return false;
        return m_pointeeType->isEqual(*o.m_pointeeType);
    }

    [[nodiscard]] size_t getSize() const override { return 8; }
    [[nodiscard]] size_t getAlignment() const override { return 8; }

    static std::shared_ptr<PointerType> get(std::shared_ptr<Type> pointeeType) {
        return std::make_shared<PointerType>(std::move(pointeeType));
    }

private:
    std::shared_ptr<Type> m_pointeeType;
};

class RcType : public Type {
public:
    explicit RcType(std::shared_ptr<Type> innerType)
        : Type(TypeKind::Rc), m_innerType(std::move(innerType)) {}

    [[nodiscard]] const std::shared_ptr<Type>& getInnerType() const noexcept { return m_innerType; }

    [[nodiscard]] std::string toString() const override {
        return "Rc<" + (m_innerType ? m_innerType->toString() : "void") + ">";
    }

    [[nodiscard]] bool isEqual(const Type& other) const override {
        if (other.getKind() != getKind()) return false;
        const auto& o = static_cast<const RcType&>(other);
        if (!m_innerType && !o.m_innerType) return true;
        if (!m_innerType || !o.m_innerType) return false;
        return m_innerType->isEqual(*o.m_innerType);
    }

    [[nodiscard]] size_t getSize() const override { return 8; }
    [[nodiscard]] size_t getAlignment() const override { return 8; }

    static std::shared_ptr<RcType> get(std::shared_ptr<Type> innerType) {
        return std::make_shared<RcType>(std::move(innerType));
    }

private:
    std::shared_ptr<Type> m_innerType;
};

class BoxType : public Type {
public:
    explicit BoxType(std::shared_ptr<Type> innerType)
        : Type(TypeKind::Box), m_innerType(std::move(innerType)) {}

    [[nodiscard]] const std::shared_ptr<Type>& getInnerType() const noexcept { return m_innerType; }

    [[nodiscard]] std::string toString() const override {
        return "Box<" + (m_innerType ? m_innerType->toString() : "void") + ">";
    }

    [[nodiscard]] bool isEqual(const Type& other) const override {
        if (other.getKind() != getKind()) return false;
        const auto& o = static_cast<const BoxType&>(other);
        if (!m_innerType && !o.m_innerType) return true;
        if (!m_innerType || !o.m_innerType) return false;
        return m_innerType->isEqual(*o.m_innerType);
    }

    [[nodiscard]] size_t getSize() const override { return 8; }
    [[nodiscard]] size_t getAlignment() const override { return 8; }
    [[nodiscard]] bool isMoveOnly() const noexcept override { return true; }
    [[nodiscard]] bool isCopy() const noexcept override { return false; }

    static std::shared_ptr<BoxType> get(std::shared_ptr<Type> innerType) {
        return std::make_shared<BoxType>(std::move(innerType));
    }

private:
    std::shared_ptr<Type> m_innerType;
};

class ArenaType : public Type {
public:
    ArenaType() : Type(TypeKind::Arena) {}

    [[nodiscard]] std::string toString() const override { return "Arena"; }
    [[nodiscard]] size_t getSize() const override { return 8; }
    [[nodiscard]] size_t getAlignment() const override { return 8; }
    [[nodiscard]] bool isEqual(const Type& other) const override {
        return other.getKind() == TypeKind::Arena;
    }
    [[nodiscard]] bool isMoveOnly() const noexcept override { return true; }
    [[nodiscard]] bool isCopy() const noexcept override { return false; }

    static std::shared_ptr<ArenaType> get() {
        static auto instance = std::make_shared<ArenaType>();
        return instance;
    }
};

class VectorType : public Type {
public:
    explicit VectorType(std::shared_ptr<Type> elementType)
        : Type(TypeKind::Vector), m_elementType(std::move(elementType)) {}

    [[nodiscard]] const std::shared_ptr<Type>& getElementType() const noexcept { return m_elementType; }

    [[nodiscard]] std::string toString() const override {
        return "Vec<" + (m_elementType ? m_elementType->toString() : "<?>") + ">";
    }

    [[nodiscard]] size_t getSize() const override { return 8; }
    [[nodiscard]] size_t getAlignment() const override { return 8; }

    [[nodiscard]] bool isEqual(const Type& other) const override {
        if (other.getKind() != TypeKind::Vector) return false;
        const auto& otherVec = static_cast<const VectorType&>(other);
        if (!m_elementType || !otherVec.m_elementType) return false;
        return m_elementType->isEqual(*otherVec.m_elementType);
    }

    [[nodiscard]] bool isMoveOnly() const noexcept override { return true; }
    [[nodiscard]] bool isCopy() const noexcept override { return false; }

    static std::shared_ptr<VectorType> get(std::shared_ptr<Type> elementType) {
        return std::make_shared<VectorType>(std::move(elementType));
    }

private:
    std::shared_ptr<Type> m_elementType;
};

class ResultType : public Type {
public:
    ResultType(std::shared_ptr<Type> okType, std::shared_ptr<Type> errType)
        : Type(TypeKind::Result), m_okType(std::move(okType)), m_errType(std::move(errType)) {}

    [[nodiscard]] const std::shared_ptr<Type>& getOkType() const noexcept { return m_okType; }
    [[nodiscard]] const std::shared_ptr<Type>& getErrType() const noexcept { return m_errType; }

    [[nodiscard]] std::string toString() const override {
        std::string okStr = m_okType ? m_okType->toString() : "void";
        std::string errStr = m_errType ? m_errType->toString() : "void";
        return "Result<" + okStr + ", " + errStr + ">";
    }

    [[nodiscard]] size_t getSize() const override { return 8; }
    [[nodiscard]] size_t getAlignment() const override { return 8; }

    [[nodiscard]] bool isEqual(const Type& other) const override {
        if (other.getKind() != TypeKind::Result) return false;
        const auto& otherRes = static_cast<const ResultType&>(other);
        if (!m_okType || !otherRes.m_okType || !m_errType || !otherRes.m_errType) return false;
        return m_okType->isEqual(*otherRes.m_okType) && m_errType->isEqual(*otherRes.m_errType);
    }

    [[nodiscard]] bool isMoveOnly() const noexcept override {
        bool okMove = m_okType && m_okType->isMoveOnly();
        bool errMove = m_errType && m_errType->isMoveOnly();
        return okMove || errMove;
    }
    [[nodiscard]] bool isCopy() const noexcept override { return !isMoveOnly(); }

    static std::shared_ptr<ResultType> get(std::shared_ptr<Type> okType, std::shared_ptr<Type> errType) {
        return std::make_shared<ResultType>(std::move(okType), std::move(errType));
    }

private:
    std::shared_ptr<Type> m_okType;
    std::shared_ptr<Type> m_errType;
};

} // namespace sph

