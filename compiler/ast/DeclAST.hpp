#pragma once

#include "ASTNode.hpp"
#include "ASTVisitor.hpp"
#include "StmtAST.hpp"
#include <memory>
#include <string>
#include <vector>

namespace sph {

struct Parameter {
    std::string name;
    std::string type;
    SourceSpan span;
    bool isByRef = false;
};

struct StructField {
    std::string name;
    std::string type;
    SourceSpan span;
};

enum class StructLayout {
    Default,
    C,
    Packed
};

struct StructAttribute {
    StructLayout layout = StructLayout::Default;
    size_t explicitAlignment = 0;
};

struct EnumVariant {
    std::string name;
    std::optional<int64_t> explicitValue;
    SourceSpan span;
};

class ImportDecl : public ASTNode {
public:
    ImportDecl(std::string path, SourceSpan span)
        : ASTNode(span), m_path(std::move(path)) {}

    [[nodiscard]] const std::string& getPath() const noexcept { return m_path; }

    void accept(ASTVisitor& visitor) override { visitor.visit(*this); }

private:
    std::string m_path;
};

class EnumDecl : public ASTNode {
public:
    EnumDecl(std::string name, std::vector<EnumVariant> variants, SourceSpan span)
        : ASTNode(span), m_name(std::move(name)), m_variants(std::move(variants)) {}

    [[nodiscard]] const std::string& getName() const noexcept { return m_name; }
    [[nodiscard]] const std::vector<EnumVariant>& getVariants() const noexcept { return m_variants; }

    void accept(ASTVisitor& visitor) override { visitor.visit(*this); }

private:
    std::string m_name;
    std::vector<EnumVariant> m_variants;
};

struct ClassField {
    std::string name;
    std::string type;
    SourceSpan span;
};

struct MethodSignature {
    std::string name;
    std::vector<Parameter> params;
    std::string returnType;
    SourceSpan span;
};

class InterfaceDecl : public ASTNode {
public:
    InterfaceDecl(std::string name, std::vector<MethodSignature> methods, SourceSpan span)
        : ASTNode(span), m_name(std::move(name)), m_methods(std::move(methods)) {}

    [[nodiscard]] const std::string& getName() const noexcept { return m_name; }
    [[nodiscard]] const std::vector<MethodSignature>& getMethods() const noexcept { return m_methods; }

    void accept(ASTVisitor& visitor) override { visitor.visit(*this); }

private:
    std::string m_name;
    std::vector<MethodSignature> m_methods;
};

class ClassDecl : public ASTNode {
public:
    ClassDecl(std::string name, std::vector<std::string> implementsInterfaces,
              std::vector<ClassField> fields,
              std::vector<std::unique_ptr<FunctionDecl>> methods, SourceSpan span)
        : ASTNode(span), m_name(std::move(name)), m_implements(std::move(implementsInterfaces)),
          m_fields(std::move(fields)), m_methods(std::move(methods)) {}

    ClassDecl(std::string name, std::vector<ClassField> fields,
              std::vector<std::unique_ptr<FunctionDecl>> methods, SourceSpan span)
        : ASTNode(span), m_name(std::move(name)), m_fields(std::move(fields)),
          m_methods(std::move(methods)) {}

    [[nodiscard]] const std::string& getName() const noexcept { return m_name; }
    [[nodiscard]] const std::vector<std::string>& getImplements() const noexcept { return m_implements; }
    [[nodiscard]] const std::vector<ClassField>& getFields() const noexcept { return m_fields; }
    [[nodiscard]] const std::vector<std::unique_ptr<FunctionDecl>>& getMethods() const noexcept { return m_methods; }
    [[nodiscard]] std::vector<std::unique_ptr<FunctionDecl>>& getMethods() noexcept { return m_methods; }

    void accept(ASTVisitor& visitor) override { visitor.visit(*this); }

private:
    std::string m_name;
    std::vector<std::string> m_implements;
    std::vector<ClassField> m_fields;
    std::vector<std::unique_ptr<FunctionDecl>> m_methods;
};

class StructDecl : public ASTNode {
public:
    StructDecl(std::string name, std::vector<std::string> typeParams,
               std::vector<StructField> fields, StructAttribute attr, SourceSpan span)
        : ASTNode(span), m_name(std::move(name)), m_typeParams(std::move(typeParams)),
          m_fields(std::move(fields)), m_attribute(attr) {}

    StructDecl(std::string name, std::vector<StructField> fields, StructAttribute attr, SourceSpan span)
        : ASTNode(span), m_name(std::move(name)), m_fields(std::move(fields)), m_attribute(attr) {}

    StructDecl(std::string name, std::vector<std::string> typeParams,
               std::vector<StructField> fields, SourceSpan span)
        : StructDecl(std::move(name), std::move(typeParams), std::move(fields), StructAttribute{}, span) {}

    StructDecl(std::string name, std::vector<StructField> fields, SourceSpan span)
        : StructDecl(std::move(name), std::move(fields), StructAttribute{}, span) {}

    [[nodiscard]] const std::string& getName() const noexcept { return m_name; }
    [[nodiscard]] const std::vector<std::string>& getTypeParams() const noexcept { return m_typeParams; }
    [[nodiscard]] bool isGeneric() const noexcept { return !m_typeParams.empty(); }
    [[nodiscard]] const std::vector<StructField>& getFields() const noexcept { return m_fields; }
    [[nodiscard]] const StructAttribute& getAttribute() const noexcept { return m_attribute; }
    void setAttribute(StructAttribute attr) noexcept { m_attribute = attr; }
    [[nodiscard]] bool isPacked() const noexcept { return m_attribute.layout == StructLayout::Packed; }

    void accept(ASTVisitor& visitor) override { visitor.visit(*this); }

private:
    std::string m_name;
    std::vector<std::string> m_typeParams;
    std::vector<StructField> m_fields;
    StructAttribute m_attribute;
};

class FunctionDecl : public ASTNode {
public:
    FunctionDecl(std::string name, std::vector<std::string> typeParams,
                 std::vector<Parameter> params, std::string returnType,
                 std::unique_ptr<BlockStmt> body, SourceSpan span)
        : ASTNode(span), m_name(std::move(name)), m_typeParams(std::move(typeParams)),
          m_params(std::move(params)), m_returnType(std::move(returnType)),
          m_body(std::move(body)) {}

    FunctionDecl(std::string name, std::vector<Parameter> params, std::string returnType,
                 std::unique_ptr<BlockStmt> body, SourceSpan span)
        : ASTNode(span), m_name(std::move(name)), m_params(std::move(params)),
          m_returnType(std::move(returnType)), m_body(std::move(body)) {}

    FunctionDecl(std::string name, std::string abi, std::vector<Parameter> params,
                 std::string returnType, SourceSpan span)
        : ASTNode(span), m_name(std::move(name)), m_abi(std::move(abi)),
          m_params(std::move(params)), m_returnType(std::move(returnType)),
          m_body(nullptr), m_isExtern(true) {}

    [[nodiscard]] const std::string& getName() const noexcept { return m_name; }
    [[nodiscard]] const std::vector<std::string>& getTypeParams() const noexcept { return m_typeParams; }
    [[nodiscard]] bool isGeneric() const noexcept { return !m_typeParams.empty(); }
    [[nodiscard]] bool isExtern() const noexcept { return m_isExtern; }
    [[nodiscard]] bool isAsync() const noexcept { return m_isAsync; }
    void setAsync(bool isAsync) noexcept { m_isAsync = isAsync; }
    [[nodiscard]] const std::string& getAbi() const noexcept { return m_abi; }
    [[nodiscard]] bool hasBody() const noexcept { return m_body != nullptr; }
    [[nodiscard]] const std::vector<Parameter>& getParams() const noexcept { return m_params; }
    [[nodiscard]] const std::string& getReturnType() const noexcept { return m_returnType; }
    [[nodiscard]] const BlockStmt& getBody() const noexcept { return *m_body; }
    [[nodiscard]] BlockStmt& getBody() noexcept { return *m_body; }

    void accept(ASTVisitor& visitor) override { visitor.visit(*this); }

private:
    std::string m_name;
    std::string m_abi = "C";
    std::vector<std::string> m_typeParams;
    std::vector<Parameter> m_params;
    std::string m_returnType;
    std::unique_ptr<BlockStmt> m_body;
    bool m_isExtern = false;
    bool m_isAsync = false;
};

class ProgramAST : public ASTNode {
public:
    ProgramAST(std::vector<std::unique_ptr<ImportDecl>> imports,
               std::vector<std::unique_ptr<StructDecl>> structs,
               std::vector<std::unique_ptr<EnumDecl>> enums,
               std::vector<std::unique_ptr<InterfaceDecl>> interfaces,
               std::vector<std::unique_ptr<ClassDecl>> classes,
               std::vector<std::unique_ptr<FunctionDecl>> functions, SourceSpan span)
        : ASTNode(span), m_imports(std::move(imports)), m_structs(std::move(structs)),
          m_enums(std::move(enums)), m_interfaces(std::move(interfaces)),
          m_classes(std::move(classes)), m_functions(std::move(functions)) {}

    ProgramAST(std::vector<std::unique_ptr<ImportDecl>> imports,
               std::vector<std::unique_ptr<StructDecl>> structs,
               std::vector<std::unique_ptr<EnumDecl>> enums,
               std::vector<std::unique_ptr<ClassDecl>> classes,
               std::vector<std::unique_ptr<FunctionDecl>> functions, SourceSpan span)
        : ASTNode(span), m_imports(std::move(imports)), m_structs(std::move(structs)),
          m_enums(std::move(enums)), m_classes(std::move(classes)),
          m_functions(std::move(functions)) {}

    ProgramAST(std::vector<std::unique_ptr<ImportDecl>> imports,
               std::vector<std::unique_ptr<StructDecl>> structs,
               std::vector<std::unique_ptr<EnumDecl>> enums,
               std::vector<std::unique_ptr<FunctionDecl>> functions, SourceSpan span)
        : ASTNode(span), m_imports(std::move(imports)), m_structs(std::move(structs)),
          m_enums(std::move(enums)), m_functions(std::move(functions)) {}

    ProgramAST(std::vector<std::unique_ptr<StructDecl>> structs,
               std::vector<std::unique_ptr<EnumDecl>> enums,
               std::vector<std::unique_ptr<FunctionDecl>> functions, SourceSpan span)
        : ASTNode(span), m_structs(std::move(structs)), m_enums(std::move(enums)),
          m_functions(std::move(functions)) {}

    ProgramAST(std::vector<std::unique_ptr<StructDecl>> structs,
               std::vector<std::unique_ptr<FunctionDecl>> functions, SourceSpan span)
        : ASTNode(span), m_structs(std::move(structs)), m_functions(std::move(functions)) {}

    ProgramAST(std::vector<std::unique_ptr<FunctionDecl>> functions, SourceSpan span)
        : ASTNode(span), m_functions(std::move(functions)) {}

    [[nodiscard]] const std::vector<std::unique_ptr<ImportDecl>>& getImports() const noexcept {
        return m_imports;
    }
    [[nodiscard]] std::vector<std::unique_ptr<ImportDecl>>& getImports() noexcept {
        return m_imports;
    }
    [[nodiscard]] const std::vector<std::unique_ptr<StructDecl>>& getStructs() const noexcept {
        return m_structs;
    }
    [[nodiscard]] std::vector<std::unique_ptr<StructDecl>>& getStructs() noexcept {
        return m_structs;
    }
    [[nodiscard]] const std::vector<std::unique_ptr<EnumDecl>>& getEnums() const noexcept {
        return m_enums;
    }
    [[nodiscard]] std::vector<std::unique_ptr<EnumDecl>>& getEnums() noexcept {
        return m_enums;
    }
    [[nodiscard]] const std::vector<std::unique_ptr<InterfaceDecl>>& getInterfaces() const noexcept {
        return m_interfaces;
    }
    [[nodiscard]] std::vector<std::unique_ptr<InterfaceDecl>>& getInterfaces() noexcept {
        return m_interfaces;
    }
    [[nodiscard]] const std::vector<std::unique_ptr<ClassDecl>>& getClasses() const noexcept {
        return m_classes;
    }
    [[nodiscard]] std::vector<std::unique_ptr<ClassDecl>>& getClasses() noexcept {
        return m_classes;
    }
    [[nodiscard]] const std::vector<std::unique_ptr<FunctionDecl>>& getFunctions() const noexcept {
        return m_functions;
    }
    [[nodiscard]] std::vector<std::unique_ptr<FunctionDecl>>& getFunctions() noexcept {
        return m_functions;
    }

    void addStruct(std::unique_ptr<StructDecl> s) { m_structs.push_back(std::move(s)); }
    void addEnum(std::unique_ptr<EnumDecl> e) { m_enums.push_back(std::move(e)); }
    void addInterface(std::unique_ptr<InterfaceDecl> iface) { m_interfaces.push_back(std::move(iface)); }
    void addClass(std::unique_ptr<ClassDecl> c) { m_classes.push_back(std::move(c)); }
    void addFunction(std::unique_ptr<FunctionDecl> f) { m_functions.push_back(std::move(f)); }
    void addImport(std::unique_ptr<ImportDecl> imp) { m_imports.push_back(std::move(imp)); }

    void accept(ASTVisitor& visitor) override { visitor.visit(*this); }

private:
    std::vector<std::unique_ptr<ImportDecl>> m_imports;
    std::vector<std::unique_ptr<StructDecl>> m_structs;
    std::vector<std::unique_ptr<EnumDecl>> m_enums;
    std::vector<std::unique_ptr<InterfaceDecl>> m_interfaces;
    std::vector<std::unique_ptr<ClassDecl>> m_classes;
    std::vector<std::unique_ptr<FunctionDecl>> m_functions;
};

} // namespace sph

