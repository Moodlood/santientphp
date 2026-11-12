#include "Type.hpp"
#include <sstream>

namespace sph {

std::string Type::toString() const {
    switch (m_kind) {
        case TypeKind::Void:     return "void";
        case TypeKind::I32:      return "i32";
        case TypeKind::I64:      return "i64";
        case TypeKind::F32:      return "f32";
        case TypeKind::F64:      return "f64";
        case TypeKind::Bool:     return "bool";
        case TypeKind::String:   return "string";
        case TypeKind::Error:    return "<error>";
        case TypeKind::Unknown:  return "<unknown>";
        case TypeKind::Function: return "function";
        case TypeKind::Struct:   return "struct";
        case TypeKind::Array:    return "array";
        case TypeKind::Slice:     return "slice";
        case TypeKind::Pointer:   return "pointer";
        case TypeKind::Rc:        return "rc";
        case TypeKind::Box:       return "box";
        case TypeKind::Arena:     return "arena";
        case TypeKind::Vector:    return "vector";
        case TypeKind::Result:    return "result";
        case TypeKind::Dynamic:   return "dynamic";
        case TypeKind::Enum:      return "enum";
        case TypeKind::Interface: return "interface";
        case TypeKind::Class:     return "class";
    }
    return "unknown";
}

bool Type::isEqual(const Type& other) const {
    return m_kind == other.m_kind;
}

bool Type::isPromise() const noexcept {
    if (m_kind != TypeKind::Class) return false;
    const auto* ct = dynamic_cast<const ClassType*>(this);
    return ct && ct->getName() == "Promise";
}

bool Type::isSimd() const noexcept {
    return isF32x4() || isI32x4() || isF64x2() || isI64x2();
}

bool Type::isF32x4() const noexcept {
    if (m_kind != TypeKind::Class) return false;
    const auto* ct = dynamic_cast<const ClassType*>(this);
    return ct && ct->getName() == "f32x4";
}

bool Type::isI32x4() const noexcept {
    if (m_kind != TypeKind::Class) return false;
    const auto* ct = dynamic_cast<const ClassType*>(this);
    return ct && ct->getName() == "i32x4";
}

bool Type::isF64x2() const noexcept {
    if (m_kind != TypeKind::Class) return false;
    const auto* ct = dynamic_cast<const ClassType*>(this);
    return ct && ct->getName() == "f64x2";
}

bool Type::isI64x2() const noexcept {
    if (m_kind != TypeKind::Class) return false;
    const auto* ct = dynamic_cast<const ClassType*>(this);
    return ct && ct->getName() == "i64x2";
}

size_t Type::getSize() const {
    switch (m_kind) {
        case TypeKind::Void:     return 0;
        case TypeKind::Bool:     return 1;
        case TypeKind::I32:      return 4;
        case TypeKind::I64:      return 8;
        case TypeKind::F32:      return 4;
        case TypeKind::F64:      return 8;
        case TypeKind::String:   return 8;
        case TypeKind::Function: return 8;
        case TypeKind::Pointer:  return 8;
        case TypeKind::Rc:       return 8;
        case TypeKind::Box:      return 8;
        case TypeKind::Arena:    return 8;
        case TypeKind::Vector:   return 8;
        case TypeKind::Result:   return 8;
        case TypeKind::Dynamic:  return 8;
        case TypeKind::Slice:     return 16;
        case TypeKind::Interface: return 16;
        case TypeKind::Enum:      return 4;
        default:                 return 0;
    }
}

size_t Type::getAlignment() const {
    switch (m_kind) {
        case TypeKind::Void:     return 1;
        case TypeKind::Bool:     return 1;
        case TypeKind::I32:      return 4;
        case TypeKind::I64:      return 8;
        case TypeKind::F32:      return 4;
        case TypeKind::F64:      return 8;
        case TypeKind::String:   return 8;
        case TypeKind::Function: return 8;
        case TypeKind::Pointer:  return 8;
        case TypeKind::Rc:       return 8;
        case TypeKind::Box:      return 8;
        case TypeKind::Arena:    return 8;
        case TypeKind::Vector:   return 8;
        case TypeKind::Result:   return 8;
        case TypeKind::Dynamic:  return 8;
        case TypeKind::Slice:     return 8;
        case TypeKind::Interface: return 8;
        case TypeKind::Enum:      return 4;
        default:                 return 1;
    }
}

std::shared_ptr<Type> Type::getVoid() {
    static auto instance = std::shared_ptr<Type>(new Type(TypeKind::Void));
    return instance;
}

std::shared_ptr<Type> Type::getI32() {
    static auto instance = std::shared_ptr<Type>(new Type(TypeKind::I32));
    return instance;
}

std::shared_ptr<Type> Type::getI64() {
    static auto instance = std::shared_ptr<Type>(new Type(TypeKind::I64));
    return instance;
}

std::shared_ptr<Type> Type::getF32() {
    static auto instance = std::shared_ptr<Type>(new Type(TypeKind::F32));
    return instance;
}

std::shared_ptr<Type> Type::getF64() {
    static auto instance = std::shared_ptr<Type>(new Type(TypeKind::F64));
    return instance;
}

std::shared_ptr<Type> Type::getBool() {
    static auto instance = std::shared_ptr<Type>(new Type(TypeKind::Bool));
    return instance;
}

std::shared_ptr<Type> Type::getString() {
    static auto instance = std::shared_ptr<Type>(new Type(TypeKind::String));
    return instance;
}

std::shared_ptr<Type> Type::getDynamic() {
    static auto instance = std::shared_ptr<Type>(new Type(TypeKind::Dynamic));
    return instance;
}

std::shared_ptr<Type> Type::getError() {
    static auto instance = std::shared_ptr<Type>(new Type(TypeKind::Error));
    return instance;
}

std::shared_ptr<Type> Type::getUnknown() {
    static auto instance = std::shared_ptr<Type>(new Type(TypeKind::Unknown));
    return instance;
}

std::shared_ptr<ClassType> Type::getF32x4() {
    static auto instance = []() {
        std::vector<StructMember> fields = {
            {"x", Type::getF32(), 0},
            {"y", Type::getF32(), 4},
            {"z", Type::getF32(), 8},
            {"w", Type::getF32(), 12}
        };
        auto t = std::make_shared<ClassType>("f32x4", std::move(fields));
        t->addMethod("__construct", Type::getVoid(), {Type::getF32(), Type::getF32(), Type::getF32(), Type::getF32()}, true);
        t->addMethod("get", Type::getF32(), {Type::getI32()}, false);
        t->addMethod("set", Type::getVoid(), {Type::getI32(), Type::getF32()}, true);
        t->addMethod("add", t, {t}, false);
        t->addMethod("sub", t, {t}, false);
        t->addMethod("mul", t, {t}, false);
        t->addMethod("div", t, {t}, false);
        t->addMethod("sqrt", t, {}, false);
        t->addMethod("abs", t, {}, false);
        t->addMethod("min", t, {t}, false);
        t->addMethod("max", t, {t}, false);
        t->addMethod("sum", Type::getF32(), {}, false);
        t->addMethod("dot", Type::getF32(), {t}, false);
        t->addMethod("store", Type::getVoid(), {PointerType::get(Type::getF32())}, false);
        t->addMethod("to_string", Type::getString(), {}, false);
        return t;
    }();
    return instance;
}

std::shared_ptr<ClassType> Type::getI32x4() {
    static auto instance = []() {
        std::vector<StructMember> fields = {
            {"x", Type::getI32(), 0},
            {"y", Type::getI32(), 4},
            {"z", Type::getI32(), 8},
            {"w", Type::getI32(), 12}
        };
        auto t = std::make_shared<ClassType>("i32x4", std::move(fields));
        t->addMethod("__construct", Type::getVoid(), {Type::getI32(), Type::getI32(), Type::getI32(), Type::getI32()}, true);
        t->addMethod("get", Type::getI32(), {Type::getI32()}, false);
        t->addMethod("set", Type::getVoid(), {Type::getI32(), Type::getI32()}, true);
        t->addMethod("add", t, {t}, false);
        t->addMethod("sub", t, {t}, false);
        t->addMethod("mul", t, {t}, false);
        t->addMethod("div", t, {t}, false);
        t->addMethod("abs", t, {}, false);
        t->addMethod("min", t, {t}, false);
        t->addMethod("max", t, {t}, false);
        t->addMethod("sum", Type::getI32(), {}, false);
        t->addMethod("dot", Type::getI32(), {t}, false);
        t->addMethod("store", Type::getVoid(), {PointerType::get(Type::getI32())}, false);
        t->addMethod("to_string", Type::getString(), {}, false);
        return t;
    }();
    return instance;
}

std::shared_ptr<ClassType> Type::getF64x2() {
    static auto instance = []() {
        std::vector<StructMember> fields = {
            {"x", Type::getF64(), 0},
            {"y", Type::getF64(), 8}
        };
        auto t = std::make_shared<ClassType>("f64x2", std::move(fields));
        t->addMethod("__construct", Type::getVoid(), {Type::getF64(), Type::getF64()}, true);
        t->addMethod("get", Type::getF64(), {Type::getI32()}, false);
        t->addMethod("set", Type::getVoid(), {Type::getI32(), Type::getF64()}, true);
        t->addMethod("add", t, {t}, false);
        t->addMethod("sub", t, {t}, false);
        t->addMethod("mul", t, {t}, false);
        t->addMethod("div", t, {t}, false);
        t->addMethod("sqrt", t, {}, false);
        t->addMethod("abs", t, {}, false);
        t->addMethod("min", t, {t}, false);
        t->addMethod("max", t, {t}, false);
        t->addMethod("sum", Type::getF64(), {}, false);
        t->addMethod("dot", Type::getF64(), {t}, false);
        t->addMethod("store", Type::getVoid(), {PointerType::get(Type::getF64())}, false);
        t->addMethod("to_string", Type::getString(), {}, false);
        return t;
    }();
    return instance;
}

std::shared_ptr<ClassType> Type::getI64x2() {
    static auto instance = []() {
        std::vector<StructMember> fields = {
            {"x", Type::getI64(), 0},
            {"y", Type::getI64(), 8}
        };
        auto t = std::make_shared<ClassType>("i64x2", std::move(fields));
        t->addMethod("__construct", Type::getVoid(), {Type::getI64(), Type::getI64()}, true);
        t->addMethod("get", Type::getI64(), {Type::getI32()}, false);
        t->addMethod("set", Type::getVoid(), {Type::getI32(), Type::getI64()}, true);
        t->addMethod("add", t, {t}, false);
        t->addMethod("sub", t, {t}, false);
        t->addMethod("mul", t, {t}, false);
        t->addMethod("div", t, {t}, false);
        t->addMethod("abs", t, {}, false);
        t->addMethod("min", t, {t}, false);
        t->addMethod("max", t, {t}, false);
        t->addMethod("sum", Type::getI64(), {}, false);
        t->addMethod("dot", Type::getI64(), {t}, false);
        t->addMethod("store", Type::getVoid(), {PointerType::get(Type::getI64())}, false);
        t->addMethod("to_string", Type::getString(), {}, false);
        return t;
    }();
    return instance;
}

std::shared_ptr<Type> Type::fromString(const std::string& name) {
    if (name == "i32" || name == "int")     return getI32();
    if (name == "i64")                      return getI64();
    if (name == "f32")                      return getF32();
    if (name == "f64" || name == "float")   return getF64();
    if (name == "bool" || name == "boolean") return getBool();
    if (name == "string") return getString();
    if (name == "void")   return getVoid();
    if (name == "dynamic" || name == "Value") return getDynamic();
    if (name == "Arena")  return ArenaType::get();
    if (name == "f32x4")  return getF32x4();
    if (name == "i32x4")  return getI32x4();
    if (name == "f64x2")  return getF64x2();
    if (name == "i64x2")  return getI64x2();
    return nullptr;
}

std::string FunctionType::toString() const {
    std::ostringstream ss;
    ss << "fn(";
    for (size_t i = 0; i < m_paramTypes.size(); ++i) {
        if (i > 0) ss << ", ";
        ss << m_paramTypes[i]->toString();
    }
    ss << "): " << m_returnType->toString();
    return ss.str();
}

bool FunctionType::isEqual(const Type& other) const {
    if (other.getKind() != TypeKind::Function) {
        return false;
    }
    const auto& fnOther = static_cast<const FunctionType&>(other);
    if (!m_returnType->isEqual(*fnOther.m_returnType)) {
        return false;
    }
    if (m_paramTypes.size() != fnOther.m_paramTypes.size()) {
        return false;
    }
    for (size_t i = 0; i < m_paramTypes.size(); ++i) {
        if (!m_paramTypes[i]->isEqual(*fnOther.m_paramTypes[i])) {
            return false;
        }
    }
    return true;
}

std::shared_ptr<FunctionType> FunctionType::get(std::shared_ptr<Type> returnType,
                                               std::vector<std::shared_ptr<Type>> paramTypes) {
    return std::make_shared<FunctionType>(std::move(returnType), std::move(paramTypes));
}

StructType::StructType(std::string name, std::vector<StructMember> members, bool isPacked, size_t explicitAlign, TypeKind kind)
    : Type(kind), m_name(std::move(name)), m_members(std::move(members)),
      m_isPacked(isPacked), m_explicitAlignment(explicitAlign) {
    size_t currentOffset = 0;
    size_t maxAlign = 1;

    for (auto& member : m_members) {
        size_t align = (m_isPacked) ? 1 : (member.type ? member.type->getAlignment() : 1);
        size_t size = member.type ? member.type->getSize() : 0;
        if (align > maxAlign) {
            maxAlign = align;
        }
        if (align > 0) {
            currentOffset = (currentOffset + align - 1) & ~(align - 1);
        }
        member.offset = currentOffset;
        currentOffset += size;
    }

    if (m_explicitAlignment > maxAlign) {
        maxAlign = m_explicitAlignment;
    }

    if (maxAlign > 0) {
        currentOffset = (currentOffset + maxAlign - 1) & ~(maxAlign - 1);
    }
    m_alignment = maxAlign;
    m_size = currentOffset;
}

StructType::StructType(std::string name, std::vector<StructMember> members, TypeKind kind)
    : StructType(std::move(name), std::move(members), false, 0, kind) {}

const StructMember* StructType::findMember(const std::string& name) const {
    for (const auto& m : m_members) {
        if (m.name == name) return &m;
    }
    return nullptr;
}

int StructType::getMemberIndex(const std::string& name) const {
    for (size_t i = 0; i < m_members.size(); ++i) {
        if (m_members[i].name == name) return static_cast<int>(i);
    }
    return -1;
}

bool StructType::isEqual(const Type& other) const {
    if (other.getKind() != getKind()) return false;
    const auto& structOther = static_cast<const StructType&>(other);
    return m_name == structOther.m_name;
}

std::shared_ptr<StructType> StructType::get(std::string name, std::vector<StructMember> members, bool isPacked, size_t explicitAlign) {
    return std::make_shared<StructType>(std::move(name), std::move(members), isPacked, explicitAlign);
}

} // namespace sph

