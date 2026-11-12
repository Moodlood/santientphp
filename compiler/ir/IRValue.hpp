#pragma once

#include "compiler/types/Type.hpp"
#include <cstdint>
#include <memory>
#include <string>

namespace sph {

class IRValue {
public:
    virtual ~IRValue() = default;

    [[nodiscard]] const std::shared_ptr<Type>& getType() const noexcept { return m_type; }
    [[nodiscard]] const std::string& getName() const noexcept { return m_name; }
    void setName(std::string name) { m_name = std::move(name); }

    [[nodiscard]] virtual bool isConstant() const noexcept { return false; }
    [[nodiscard]] virtual bool isInstruction() const noexcept { return false; }

protected:
    IRValue(std::shared_ptr<Type> type, std::string name)
        : m_type(std::move(type)), m_name(std::move(name)) {}

private:
    std::shared_ptr<Type> m_type;
    std::string m_name;
};

class ConstantInt : public IRValue {
public:
    ConstantInt(int64_t val, std::shared_ptr<Type> type)
        : IRValue(std::move(type), std::to_string(val)), m_value(val) {}

    [[nodiscard]] int64_t getValue() const noexcept { return m_value; }
    [[nodiscard]] bool isConstant() const noexcept override { return true; }

    static std::shared_ptr<ConstantInt> get(int64_t val, std::shared_ptr<Type> type = Type::getI32()) {
        return std::make_shared<ConstantInt>(val, std::move(type));
    }

private:
    int64_t m_value;
};

class ConstantFloat : public IRValue {
public:
    ConstantFloat(double val, std::shared_ptr<Type> type)
        : IRValue(std::move(type), std::to_string(val)), m_value(val) {}

    [[nodiscard]] double getValue() const noexcept { return m_value; }
    [[nodiscard]] bool isConstant() const noexcept override { return true; }

    static std::shared_ptr<ConstantFloat> get(double val, std::shared_ptr<Type> type = Type::getF64()) {
        return std::make_shared<ConstantFloat>(val, std::move(type));
    }

private:
    double m_value;
};

class ConstantBool : public IRValue {
public:
    explicit ConstantBool(bool val)
        : IRValue(Type::getBool(), val ? "true" : "false"), m_value(val) {}

    [[nodiscard]] bool getValue() const noexcept { return m_value; }
    [[nodiscard]] bool isConstant() const noexcept override { return true; }

    static std::shared_ptr<ConstantBool> get(bool val) {
        return std::make_shared<ConstantBool>(val);
    }

private:
    bool m_value;
};

class ConstantString : public IRValue {
public:
    explicit ConstantString(std::string val)
        : IRValue(Type::getString(), "\"" + val + "\""), m_value(std::move(val)) {}

    [[nodiscard]] const std::string& getValue() const noexcept { return m_value; }
    [[nodiscard]] bool isConstant() const noexcept override { return true; }

    static std::shared_ptr<ConstantString> get(std::string val) {
        return std::make_shared<ConstantString>(std::move(val));
    }

private:
    std::string m_value;
};

class ConstantGlobal : public IRValue {
public:
    ConstantGlobal(std::string name, std::shared_ptr<Type> type)
        : IRValue(std::move(type), "@" + name), m_globalName(std::move(name)) {}

    [[nodiscard]] const std::string& getGlobalName() const noexcept { return m_globalName; }
    [[nodiscard]] bool isConstant() const noexcept override { return true; }

    static std::shared_ptr<ConstantGlobal> get(std::string name, std::shared_ptr<Type> type) {
        return std::make_shared<ConstantGlobal>(std::move(name), std::move(type));
    }

private:
    std::string m_globalName;
};

class IRParameter : public IRValue {
public:
    IRParameter(std::shared_ptr<Type> type, std::string name, size_t index)
        : IRValue(std::move(type), std::move(name)), m_index(index) {}

    [[nodiscard]] size_t getIndex() const noexcept { return m_index; }

private:
    size_t m_index;
};

} // namespace sph

