#include "SemanticAnalyzer.hpp"
#include "ASTCloner.hpp"
#include <unordered_map>
#include <unordered_set>

namespace sph {

static std::string mangleTypePart(const std::string& typeName) {
    std::string result;
    for (char c : typeName) {
        if (std::isalnum(static_cast<unsigned char>(c)) || c == '_') {
            result += c;
        } else if (c == '&') {
            result += "ref_";
        } else if (c == '[' || c == ']' || c == '<' || c == '>' || c == ',' || c == ';' || c == ' ') {
            if (!result.empty() && result.back() != '_') {
                result += '_';
            }
        }
    }
    while (!result.empty() && result.back() == '_') {
        result.pop_back();
    }
    return result;
}

std::string SemanticAnalyzer::getMangledStructName(const std::string& baseName, const std::vector<std::string>& typeArgs) {
    std::string mangled = baseName;
    for (const auto& a : typeArgs) {
        mangled += "_" + mangleTypePart(a);
    }
    return mangled;
}

std::string SemanticAnalyzer::getMangledFunctionName(const std::string& baseName, const std::vector<std::string>& typeArgs) {
    std::string mangled = baseName;
    for (const auto& a : typeArgs) {
        mangled += "_" + mangleTypePart(a);
    }
    return mangled;
}

SemanticAnalyzer::SemanticAnalyzer(DiagnosticEmitter& diagnostics)
    : m_diagnostics(diagnostics) {}

void SemanticAnalyzer::error(SourceSpan span, std::string message) {
    m_diagnostics.emit(Diagnostic::makeError(span, std::move(message)));
}

bool SemanticAnalyzer::checkReturns(const StmtAST& stmt) const {
    if (dynamic_cast<const ReturnStmt*>(&stmt)) {
        return true;
    }

    if (const auto* block = dynamic_cast<const BlockStmt*>(&stmt)) {
        for (const auto& s : block->getStatements()) {
            if (checkReturns(*s)) {
                return true;
            }
        }
        return false;
    }

    if (const auto* unsafeBlock = dynamic_cast<const UnsafeBlockStmt*>(&stmt)) {
        return checkReturns(unsafeBlock->getBlock());
    }

    if (const auto* ifStmt = dynamic_cast<const IfStmt*>(&stmt)) {
        if (ifStmt->hasElse()) {
            return checkReturns(ifStmt->getThenBranch()) && checkReturns(*ifStmt->getElseBranch());
        }
        return false;
    }

    return false;
}

std::shared_ptr<Type> SemanticAnalyzer::resolveType(const std::string& name) {
    std::string cleanName = name;
    if (cleanName == "Atomic<i32>") cleanName = "AtomicI32";
    else if (cleanName == "Atomic<i64>") cleanName = "AtomicI64";
    else if (cleanName == "Atomic<bool>") cleanName = "AtomicBool";

    if (cleanName == "Promise") {
        return PromiseType::get(Type::getI64());
    }
    if (cleanName.rfind("Promise<", 0) == 0 && cleanName.back() == '>') {
        std::string innerName = cleanName.substr(8, cleanName.size() - 9);
        auto innerType = resolveType(innerName);
        if (!innerType) innerType = Type::getI64();
        return PromiseType::get(innerType);
    }

    if (auto t = Type::fromString(cleanName)) {
        return t;
    }

    // Check if array type: e.g. [i32; 4]
    if (name.size() >= 5 && name.front() == '[' && name.back() == ']') {
        std::string inner = name.substr(1, name.size() - 2);
        auto semiPos = inner.find(';');
        if (semiPos != std::string::npos) {
            std::string elemStr = inner.substr(0, semiPos);
            std::string countStr = inner.substr(semiPos + 1);

            // Trim
            while (!elemStr.empty() && (elemStr.front() == ' ' || elemStr.front() == '\t')) elemStr.erase(0, 1);
            while (!elemStr.empty() && (elemStr.back() == ' ' || elemStr.back() == '\t')) elemStr.pop_back();
            while (!countStr.empty() && (countStr.front() == ' ' || countStr.front() == '\t')) countStr.erase(0, 1);
            while (!countStr.empty() && (countStr.back() == ' ' || countStr.back() == '\t')) countStr.pop_back();

            auto elemType = resolveType(elemStr);
            if (elemType) {
                try {
                    size_t count = std::stoull(countStr);
                    return ArrayType::get(elemType, count);
                } catch (...) {
                    return nullptr;
                }
            }
        }
    }

    // Check if pointer type: e.g. *i32, *Point, *void
    if (name.size() >= 2 && name.front() == '*') {
        std::string inner = name.substr(1);
        while (!inner.empty() && (inner.front() == ' ' || inner.front() == '\t')) inner.erase(0, 1);
        while (!inner.empty() && (inner.back() == ' ' || inner.back() == '\t')) inner.pop_back();
        auto pointeeType = resolveType(inner);
        if (pointeeType) {
            return PointerType::get(pointeeType);
        }
        return nullptr;
    }

    // Check if slice type: e.g. &[i32]
    if (name.size() >= 4 && name.substr(0, 2) == "&[" && name.back() == ']') {
        std::string inner = name.substr(2, name.size() - 3);
        while (!inner.empty() && (inner.front() == ' ' || inner.front() == '\t')) inner.erase(0, 1);
        while (!inner.empty() && (inner.back() == ' ' || inner.back() == '\t')) inner.pop_back();
        auto elemType = resolveType(inner);
        if (elemType) {
            return SliceType::get(elemType);
        }
        return nullptr;
    }

    // Check if Rc type: e.g. Rc<Point>
    if (name.size() >= 4 && name.substr(0, 3) == "Rc<" && name.back() == '>') {
        std::string inner = name.substr(3, name.size() - 4);
        while (!inner.empty() && (inner.front() == ' ' || inner.front() == '\t')) inner.erase(0, 1);
        while (!inner.empty() && (inner.back() == ' ' || inner.back() == '\t')) inner.pop_back();
        auto innerType = resolveType(inner);
        if (innerType) {
            return RcType::get(innerType);
        }
        return nullptr;
    }

    // Check if Box type: e.g. Box<Point>
    if (name.size() >= 5 && name.substr(0, 4) == "Box<" && name.back() == '>') {
        std::string inner = name.substr(4, name.size() - 5);
        while (!inner.empty() && (inner.front() == ' ' || inner.front() == '\t')) inner.erase(0, 1);
        while (!inner.empty() && (inner.back() == ' ' || inner.back() == '\t')) inner.pop_back();
        auto innerType = resolveType(inner);
        if (innerType) {
            return BoxType::get(innerType);
        }
        return nullptr;
    }

    // Check if Vec type: e.g. Vec<Point>
    if (name.size() >= 5 && name.substr(0, 4) == "Vec<" && name.back() == '>') {
        std::string inner = name.substr(4, name.size() - 5);
        while (!inner.empty() && (inner.front() == ' ' || inner.front() == '\t')) inner.erase(0, 1);
        while (!inner.empty() && (inner.back() == ' ' || inner.back() == '\t')) inner.pop_back();
        auto innerType = resolveType(inner);
        if (innerType) {
            return VectorType::get(innerType);
        }
        return nullptr;
    }

    // Check if Result type: e.g. Result<i32, string>
    if (name.size() >= 8 && name.substr(0, 7) == "Result<" && name.back() == '>') {
        std::string inner = name.substr(7, name.size() - 8);
        int depth = 0;
        size_t commaPos = std::string::npos;
        for (size_t i = 0; i < inner.size(); ++i) {
            if (inner[i] == '<' || inner[i] == '[') depth++;
            else if (inner[i] == '>' || inner[i] == ']') depth--;
            else if (inner[i] == ',' && depth == 0) {
                commaPos = i;
                break;
            }
        }
        if (commaPos != std::string::npos) {
            std::string okStr = inner.substr(0, commaPos);
            std::string errStr = inner.substr(commaPos + 1);
            while (!okStr.empty() && (okStr.front() == ' ' || okStr.front() == '\t')) okStr.erase(0, 1);
            while (!okStr.empty() && (okStr.back() == ' ' || okStr.back() == '\t')) okStr.pop_back();
            while (!errStr.empty() && (errStr.front() == ' ' || errStr.front() == '\t')) errStr.erase(0, 1);
            while (!errStr.empty() && (errStr.back() == ' ' || errStr.back() == '\t')) errStr.pop_back();

            auto okType = resolveType(okStr);
            auto errType = resolveType(errStr);
            if (okType && errType) {
                return ResultType::get(okType, errType);
            }
        }
        return nullptr;
    }

    // Check if generic type: e.g. Pair<i32, i32>
    auto anglePos = name.find('<');
    if (anglePos != std::string::npos && name.back() == '>') {
        std::string baseName = name.substr(0, anglePos);
        while (!baseName.empty() && (baseName.front() == ' ' || baseName.front() == '\t')) baseName.erase(0, 1);
        while (!baseName.empty() && (baseName.back() == ' ' || baseName.back() == '\t')) baseName.pop_back();

        std::string argsStr = name.substr(anglePos + 1, name.size() - anglePos - 2);
        std::vector<std::string> typeArgs;
        int depth = 0;
        size_t lastStart = 0;
        for (size_t i = 0; i < argsStr.size(); ++i) {
            if (argsStr[i] == '<' || argsStr[i] == '[') depth++;
            else if (argsStr[i] == '>' || argsStr[i] == ']') depth--;
            else if (argsStr[i] == ',' && depth == 0) {
                std::string arg = argsStr.substr(lastStart, i - lastStart);
                while (!arg.empty() && (arg.front() == ' ' || arg.front() == '\t')) arg.erase(0, 1);
                while (!arg.empty() && (arg.back() == ' ' || arg.back() == '\t')) arg.pop_back();
                if (!arg.empty()) typeArgs.push_back(std::move(arg));
                lastStart = i + 1;
            }
        }
        std::string lastArg = argsStr.substr(lastStart);
        while (!lastArg.empty() && (lastArg.front() == ' ' || lastArg.front() == '\t')) lastArg.erase(0, 1);
        while (!lastArg.empty() && (lastArg.back() == ' ' || lastArg.back() == '\t')) lastArg.pop_back();
        if (!lastArg.empty()) typeArgs.push_back(std::move(lastArg));

        if (m_genericStructs.count(baseName)) {
            return instantiateGenericStruct(baseName, typeArgs, SourceSpan());
        }
    }

    const Symbol* sym = m_currentScope ? m_currentScope->lookup(name) : m_globalScope.lookup(name);
    if (sym && (sym->kind == SymbolKind::Struct || sym->kind == SymbolKind::Enum ||
                sym->kind == SymbolKind::Class || sym->kind == SymbolKind::Interface)) {
        return sym->type;
    }
    return nullptr;
}

bool SemanticAnalyzer::isAssignable(const Type& from, const Type& to) const {
    if (from.isEqual(to)) return true;
    if (to.isDynamic() || from.isDynamic()) return true;
    if (to.isInterface() && from.isClass()) {
        const auto& classType = static_cast<const ClassType&>(from);
        const auto& ifaceType = static_cast<const InterfaceType&>(to);
        return classType.implementsInterface(ifaceType.getName());
    }
    return false;
}

std::shared_ptr<Type> SemanticAnalyzer::instantiateGenericStruct(
    const std::string& baseName, const std::vector<std::string>& typeArgs, SourceSpan span) {
    auto it = m_genericStructs.find(baseName);
    if (it == m_genericStructs.end()) {
        error(span, "unknown generic struct '" + baseName + "'");
        return nullptr;
    }
    const StructDecl* templateDecl = it->second;
    if (typeArgs.size() != templateDecl->getTypeParams().size()) {
        error(span, "generic struct '" + baseName + "' expects " +
              std::to_string(templateDecl->getTypeParams().size()) + " type argument(s), found " +
              std::to_string(typeArgs.size()));
        return nullptr;
    }

    std::string mangledName = getMangledStructName(baseName, typeArgs);
    if (const Symbol* sym = m_globalScope.lookup(mangledName)) {
        if (sym->type && sym->type->isStruct()) {
            return sym->type;
        }
    }

    // Build substitutions
    std::unordered_map<std::string, std::string> substs;
    for (size_t i = 0; i < typeArgs.size(); ++i) {
        substs[templateDecl->getTypeParams()[i]] = typeArgs[i];
    }

    ASTCloner cloner(substs);
    std::vector<StructMember> members;
    std::vector<StructField> specializedFields;
    for (const auto& field : templateDecl->getFields()) {
        std::string substTypeStr = cloner.substituteType(field.type);
        auto fType = resolveType(substTypeStr);
        if (!fType) {
            error(field.span, "unknown type '" + substTypeStr + "' for field '" + field.name + "' in struct '" + mangledName + "'");
            fType = Type::getError();
        }
        members.push_back(StructMember{field.name, fType, 0});
        specializedFields.push_back(StructField{field.name, substTypeStr, field.span});
    }

    auto structType = StructType::get(mangledName, std::move(members), templateDecl->isPacked(), templateDecl->getAttribute().explicitAlignment);
    m_globalScope.define(Symbol::makeStruct(mangledName, structType, templateDecl->getSpan()));

    if (m_program && !m_specializedStructs.count(mangledName)) {
        m_specializedStructs.insert(mangledName);
        m_program->getStructs().push_back(
            std::make_unique<StructDecl>(mangledName, std::move(specializedFields), templateDecl->getAttribute(), templateDecl->getSpan()));
    }

    return structType;
}

void SemanticAnalyzer::instantiateGenericFunction(
    const std::string& baseName, const std::vector<std::string>& typeArgs, SourceSpan span) {
    auto it = m_genericFunctions.find(baseName);
    if (it == m_genericFunctions.end()) {
        error(span, "unknown generic function '" + baseName + "'");
        return;
    }
    const FunctionDecl* templateDecl = it->second;
    if (typeArgs.size() != templateDecl->getTypeParams().size()) {
        error(span, "generic function '" + baseName + "' expects " +
              std::to_string(templateDecl->getTypeParams().size()) + " type argument(s), found " +
              std::to_string(typeArgs.size()));
        return;
    }

    std::string mangledName = getMangledFunctionName(baseName, typeArgs);
    if (m_globalScope.lookup(mangledName)) {
        return; // already instantiated
    }

    // Build substitutions
    std::unordered_map<std::string, std::string> substs;
    for (size_t i = 0; i < typeArgs.size(); ++i) {
        substs[templateDecl->getTypeParams()[i]] = typeArgs[i];
    }

    ASTCloner cloner(substs);

    // Substituted parameter types
    std::vector<Parameter> specializedParams;
    std::vector<std::shared_ptr<Type>> paramTypes;
    for (const auto& param : templateDecl->getParams()) {
        std::string substParamType = cloner.substituteType(param.type);
        auto pType = resolveType(substParamType);
        if (!pType) {
            error(param.span, "unknown type '" + substParamType + "' for parameter '" + param.name + "' in specialized function '" + mangledName + "'");
            pType = Type::getError();
        }
        paramTypes.push_back(pType);
        specializedParams.push_back(Parameter{param.name, std::move(substParamType), param.span});
    }

    // Substituted return type
    std::string substRetTypeStr = cloner.substituteType(templateDecl->getReturnType());
    auto retType = resolveType(substRetTypeStr);
    if (!retType) {
        error(templateDecl->getSpan(), "unknown return type '" + substRetTypeStr + "' in specialized function '" + mangledName + "'");
        retType = Type::getError();
    }

    // Register function symbol in global scope
    auto fnType = FunctionType::get(retType, paramTypes);
    m_globalScope.define(Symbol::makeFunction(mangledName, fnType, templateDecl->getSpan()));

    // Clone body with substituted types
    auto clonedBody = std::unique_ptr<BlockStmt>(static_cast<BlockStmt*>(cloner.cloneStmt(templateDecl->getBody()).release()));

    auto specializedFn = std::make_unique<FunctionDecl>(
        mangledName, std::vector<std::string>{}, std::move(specializedParams),
        std::move(substRetTypeStr), std::move(clonedBody), templateDecl->getSpan());

    FunctionDecl* fnPtr = specializedFn.get();
    if (m_program) {
        m_program->getFunctions().push_back(std::move(specializedFn));
    }
    m_specializedFunctions.insert(mangledName);

    // Save and restore context for re-entrancy
    auto* savedScope = m_currentScope;
    const FunctionDecl* savedFn = m_currentFunction;
    auto savedRetType = m_currentFunctionReturnType;
    auto savedClass = m_currentClass;
    bool savedMut = m_currentMethodMutatesThis;

    fnPtr->accept(*this);

    m_currentScope = savedScope;
    m_currentFunction = savedFn;
    m_currentFunctionReturnType = savedRetType;
    m_currentClass = savedClass;
    m_currentMethodMutatesThis = savedMut;
}

bool SemanticAnalyzer::analyze(ProgramAST& program) {
    m_program = &program;
    m_genericStructs.clear();
    m_genericFunctions.clear();
    m_specializedFunctions.clear();
    m_specializedStructs.clear();
    m_functionDecls.clear();

    if (!m_globalScope.lookupCurrent("PDO")) {
        std::vector<EnumVariantInfo> pdoVariants = {
            {"FETCH_ASSOC", 2},
            {"FETCH_NUM", 3},
            {"FETCH_BOTH", 4}
        };
        auto pdoEnumType = EnumType::get("PDO", std::move(pdoVariants));
        m_globalScope.define(Symbol::makeEnum("PDO", pdoEnumType, SourceSpan()));

        auto pdoClassType = std::make_shared<ClassType>("PDO", std::vector<StructMember>{});
        pdoClassType->addMethod("exec", Type::getI64(), {Type::getString()}, false);
        pdoClassType->addMethod("query", Type::getDynamic(), {Type::getString()}, false);
        pdoClassType->addMethod("prepare", Type::getDynamic(), {Type::getString()}, false);
        pdoClassType->addMethod("lastInsertId", Type::getI64(), {}, false);
        m_globalScope.define(Symbol::makeClass("PDO", pdoClassType, SourceSpan()));
    }

    if (!m_globalScope.lookupCurrent("PDOStatement")) {
        auto stmtClassType = std::make_shared<ClassType>("PDOStatement", std::vector<StructMember>{});
        stmtClassType->addMethod("execute", Type::getBool(), {}, false);
        stmtClassType->addMethod("fetch", Type::getDynamic(), {}, false);
        stmtClassType->addMethod("fetchAll", Type::getDynamic(), {}, false);
        stmtClassType->addMethod("rowCount", Type::getI64(), {}, false);
        stmtClassType->addMethod("columnCount", Type::getI32(), {}, false);
        stmtClassType->addMethod("close", Type::getVoid(), {}, false);
        m_globalScope.define(Symbol::makeClass("PDOStatement", stmtClassType, SourceSpan()));
    }

    // Built-in MemoryOrder enum
    if (!m_globalScope.lookupCurrent("MemoryOrder")) {
        std::vector<EnumVariantInfo> variants = {
            {"Relaxed", 0},
            {"Acquire", 1},
            {"Release", 2},
            {"AcqRel", 3},
            {"SeqCst", 4}
        };
        auto memOrderType = EnumType::get("MemoryOrder", std::move(variants));
        m_globalScope.define(Symbol::makeEnum("MemoryOrder", memOrderType, SourceSpan()));
    }

    // Built-in Atomics
    if (!m_globalScope.lookupCurrent("AtomicI32")) {
        std::vector<StructMember> fields = {{"value", Type::getI32(), 0}};
        auto atomicType = std::make_shared<ClassType>("AtomicI32", std::move(fields));
        atomicType->addMethod("__construct", Type::getVoid(), {Type::getI32()}, true);
        atomicType->addMethod("load", Type::getI32(), {}, false);
        atomicType->addMethod("store", Type::getVoid(), {Type::getI32()}, true);
        atomicType->addMethod("fetch_add", Type::getI32(), {Type::getI32()}, true);
        atomicType->addMethod("fetch_sub", Type::getI32(), {Type::getI32()}, true);
        atomicType->addMethod("exchange", Type::getI32(), {Type::getI32()}, true);
        atomicType->addMethod("compare_exchange", Type::getBool(), {Type::getI32(), Type::getI32()}, true);
        m_globalScope.define(Symbol::makeClass("AtomicI32", atomicType, SourceSpan()));
    }

    if (!m_globalScope.lookupCurrent("AtomicI64")) {
        std::vector<StructMember> fields = {{"value", Type::getI64(), 0}};
        auto atomicType = std::make_shared<ClassType>("AtomicI64", std::move(fields));
        atomicType->addMethod("__construct", Type::getVoid(), {Type::getI64()}, true);
        atomicType->addMethod("load", Type::getI64(), {}, false);
        atomicType->addMethod("store", Type::getVoid(), {Type::getI64()}, true);
        atomicType->addMethod("fetch_add", Type::getI64(), {Type::getI64()}, true);
        atomicType->addMethod("fetch_sub", Type::getI64(), {Type::getI64()}, true);
        atomicType->addMethod("exchange", Type::getI64(), {Type::getI64()}, true);
        atomicType->addMethod("compare_exchange", Type::getBool(), {Type::getI64(), Type::getI64()}, true);
        m_globalScope.define(Symbol::makeClass("AtomicI64", atomicType, SourceSpan()));
    }

    if (!m_globalScope.lookupCurrent("AtomicBool")) {
        std::vector<StructMember> fields = {{"value", Type::getBool(), 0}};
        auto atomicType = std::make_shared<ClassType>("AtomicBool", std::move(fields));
        atomicType->addMethod("__construct", Type::getVoid(), {Type::getBool()}, true);
        atomicType->addMethod("load", Type::getBool(), {}, false);
        atomicType->addMethod("store", Type::getVoid(), {Type::getBool()}, true);
        atomicType->addMethod("exchange", Type::getBool(), {Type::getBool()}, true);
        atomicType->addMethod("compare_exchange", Type::getBool(), {Type::getBool(), Type::getBool()}, true);
        m_globalScope.define(Symbol::makeClass("AtomicBool", atomicType, SourceSpan()));
    }

    if (!m_globalScope.lookupCurrent("Promise")) {
        auto promiseType = PromiseType::get(Type::getI64());
        m_globalScope.define(Symbol::makeClass("Promise", promiseType, SourceSpan()));
    }

    if (!m_globalScope.lookupCurrent("f32x4")) {
        m_globalScope.define(Symbol::makeClass("f32x4", Type::getF32x4(), SourceSpan()));
    }
    if (!m_globalScope.lookupCurrent("i32x4")) {
        m_globalScope.define(Symbol::makeClass("i32x4", Type::getI32x4(), SourceSpan()));
    }
    if (!m_globalScope.lookupCurrent("f64x2")) {
        m_globalScope.define(Symbol::makeClass("f64x2", Type::getF64x2(), SourceSpan()));
    }
    if (!m_globalScope.lookupCurrent("i64x2")) {
        m_globalScope.define(Symbol::makeClass("i64x2", Type::getI64x2(), SourceSpan()));
    }

    // Pass 0: Register all struct types
    for (auto& st : program.getStructs()) {
        if (st->isGeneric()) {
            m_genericStructs[st->getName()] = st.get();
            continue;
        }

        if (m_globalScope.lookupCurrent(st->getName())) {
            error(st->getSpan(), "duplicate definition of struct '" + st->getName() + "'");
            continue;
        }

        std::vector<StructMember> members;
        std::unordered_set<std::string> memberNames;

        for (const auto& field : st->getFields()) {
            if (memberNames.count(field.name)) {
                error(field.span, "duplicate field '" + field.name + "' in struct '" + st->getName() + "'");
                continue;
            }
            memberNames.insert(field.name);

            auto fType = resolveType(field.type);
            if (!fType) {
                error(field.span, "unknown type '" + field.type + "' for field '" + field.name + "' in struct '" + st->getName() + "'");
                fType = Type::getError();
            }

            members.push_back(StructMember{field.name, fType, 0});
        }

        auto structType = StructType::get(st->getName(), std::move(members), st->isPacked(), st->getAttribute().explicitAlignment);
        Symbol sym = Symbol::makeStruct(st->getName(), structType, st->getSpan());
        m_globalScope.define(std::move(sym));
    }

    // Pass 0b: Register all enum types
    for (auto& en : program.getEnums()) {
        if (m_globalScope.lookupCurrent(en->getName())) {
            error(en->getSpan(), "duplicate definition of '" + en->getName() + "'");
            continue;
        }

        std::vector<EnumVariantInfo> variants;
        std::unordered_set<std::string> variantNames;
        int64_t nextVal = 0;

        for (const auto& v : en->getVariants()) {
            if (variantNames.count(v.name)) {
                error(v.span, "duplicate variant '" + v.name + "' in enum '" + en->getName() + "'");
                continue;
            }
            variantNames.insert(v.name);

            int64_t actualVal = v.explicitValue.value_or(nextVal);
            variants.push_back(EnumVariantInfo{v.name, actualVal});
            nextVal = actualVal + 1;
        }

        auto enumType = EnumType::get(en->getName(), std::move(variants));
        Symbol sym = Symbol::makeEnum(en->getName(), enumType, en->getSpan());
        m_globalScope.define(std::move(sym));
    }

    // Pass 0c: Register all interface types
    for (auto& iface : program.getInterfaces()) {
        if (m_globalScope.lookupCurrent(iface->getName())) {
            error(iface->getSpan(), "duplicate definition of '" + iface->getName() + "'");
            continue;
        }

        std::vector<InterfaceMethodInfo> methods;
        std::unordered_set<std::string> methodNames;

        for (const auto& sig : iface->getMethods()) {
            if (methodNames.count(sig.name)) {
                error(sig.span, "duplicate method '" + sig.name + "' in interface '" + iface->getName() + "'");
                continue;
            }
            methodNames.insert(sig.name);

            auto retType = resolveType(sig.returnType);
            if (!retType) {
                error(sig.span, "unknown return type '" + sig.returnType + "' in method '" + sig.name + "'");
                retType = Type::getError();
            }

            std::vector<std::shared_ptr<Type>> paramTypes;
            for (const auto& param : sig.params) {
                auto pType = resolveType(param.type);
                if (!pType) {
                    error(param.span, "unknown type '" + param.type + "' for parameter '" + param.name + "' in method '" + sig.name + "'");
                    pType = Type::getError();
                }
                paramTypes.push_back(pType);
            }

            methods.push_back(InterfaceMethodInfo{sig.name, retType, std::move(paramTypes)});
        }

        auto ifaceType = InterfaceType::get(iface->getName(), std::move(methods));
        Symbol sym = Symbol::makeInterface(iface->getName(), ifaceType, iface->getSpan());
        m_globalScope.define(std::move(sym));
    }

    // Pass 0d: Register all class types
    for (auto& cl : program.getClasses()) {
        if (m_globalScope.lookupCurrent(cl->getName())) {
            error(cl->getSpan(), "duplicate definition of class '" + cl->getName() + "'");
            continue;
        }

        std::vector<StructMember> fields;
        std::unordered_set<std::string> fieldNames;

        for (const auto& field : cl->getFields()) {
            if (fieldNames.count(field.name)) {
                error(field.span, "duplicate field '" + field.name + "' in class '" + cl->getName() + "'");
                continue;
            }
            fieldNames.insert(field.name);

            auto fType = resolveType(field.type);
            if (!fType) {
                error(field.span, "unknown type '" + field.type + "' for field '" + field.name + "' in class '" + cl->getName() + "'");
                fType = Type::getError();
            }

            fields.push_back(StructMember{field.name, fType, 0});
        }

        auto classType = ClassType::get(cl->getName(), std::move(fields));
        for (const auto& ifaceName : cl->getImplements()) {
            auto ifaceType = resolveType(ifaceName);
            if (!ifaceType || !ifaceType->isInterface()) {
                error(cl->getSpan(), "class '" + cl->getName() + "' cannot implement unknown interface '" + ifaceName + "'");
            } else {
                classType->addImplements(ifaceName);
            }
        }
        Symbol sym = Symbol::makeClass(cl->getName(), classType, cl->getSpan());
        m_globalScope.define(std::move(sym));
    }

    // Pass 1: Collect top-level function declarations
    for (auto& fn : program.getFunctions()) {
        m_functionDecls[fn->getName()] = fn.get();
        if (fn->isGeneric()) {
            m_genericFunctions[fn->getName()] = fn.get();
            continue;
        }

        auto retType = resolveType(fn->getReturnType());
        if (!retType) {
            error(fn->getSpan(), "unknown return type '" + fn->getReturnType() + "'");
            retType = Type::getError();
        }

        std::vector<std::shared_ptr<Type>> paramTypes;
        for (const auto& param : fn->getParams()) {
            auto pType = resolveType(param.type);
            if (!pType) {
                error(param.span, "unknown type '" + param.type + "' for parameter '" + param.name + "'");
                pType = Type::getError();
            } else if (fn->isExtern() && !pType->isNumeric() && !pType->isBoolean() &&
                       !pType->isString() && !pType->isPointer()) {
                error(param.span, "parameter '" + param.name + "' has type '" + pType->toString() +
                      "' which is not FFI-compatible; use primitive types or raw pointers '*T'");
            }
            paramTypes.push_back(pType);
        }

        if (fn->isExtern() && !retType->isNumeric() && !retType->isBoolean() &&
            !retType->isString() && !retType->isPointer() && !retType->isVoid()) {
            error(fn->getSpan(), "return type '" + retType->toString() +
                  "' is not FFI-compatible; use primitive types, raw pointers '*T', or 'void'");
        }

        auto fnType = FunctionType::get(retType, std::move(paramTypes));
        Symbol sym = Symbol::makeFunction(fn->getName(), fnType, fn->getSpan());

        if (!m_globalScope.define(std::move(sym))) {
            error(fn->getSpan(), "duplicate function definition '" + fn->getName() + "'");
        }
    }

    // Pass 1b: Register class methods on each ClassType
    for (auto& cl : program.getClasses()) {
        const Symbol* sym = m_globalScope.lookupCurrent(cl->getName());
        if (!sym || !sym->type || !sym->type->isClass()) continue;
        auto classType = std::static_pointer_cast<ClassType>(sym->type);

        for (auto& method : cl->getMethods()) {
            if (classType->findMethod(method->getName())) {
                error(method->getSpan(), "duplicate method '" + method->getName() + "' in class '" + cl->getName() + "'");
                continue;
            }

            auto retType = resolveType(method->getReturnType());
            if (!retType) {
                error(method->getSpan(), "unknown return type '" + method->getReturnType() + "' in method '" + method->getName() + "'");
                retType = Type::getError();
            }

            std::vector<std::shared_ptr<Type>> paramTypes;
            for (const auto& param : method->getParams()) {
                auto pType = resolveType(param.type);
                if (!pType) {
                    error(param.span, "unknown type '" + param.type + "' for parameter '" + param.name + "' in method '" + method->getName() + "'");
                    pType = Type::getError();
                }
                paramTypes.push_back(pType);
            }

            classType->addMethod(method->getName(), retType, paramTypes, false);
        }
    }

    // Pass 1c: Verify interface conformance for classes
    for (auto& cl : program.getClasses()) {
        const Symbol* sym = m_globalScope.lookupCurrent(cl->getName());
        if (!sym || !sym->type || !sym->type->isClass()) continue;
        auto classType = std::static_pointer_cast<ClassType>(sym->type);

        for (const auto& ifaceName : classType->getImplements()) {
            const Symbol* ifaceSym = m_globalScope.lookup(ifaceName);
            if (!ifaceSym || !ifaceSym->type || !ifaceSym->type->isInterface()) continue;
            auto ifaceType = std::static_pointer_cast<InterfaceType>(ifaceSym->type);

            for (const auto& ifaceMethod : ifaceType->getMethods()) {
                const auto* classMethod = classType->findMethod(ifaceMethod.name);
                if (!classMethod) {
                    error(cl->getSpan(), "class '" + cl->getName() + "' does not implement interface method '" +
                          ifaceMethod.name + "' required by '" + ifaceName + "'");
                    continue;
                }

                if (classMethod->paramTypes.size() != ifaceMethod.paramTypes.size()) {
                    error(cl->getSpan(), "method '" + ifaceMethod.name + "' in class '" + cl->getName() +
                          "' has " + std::to_string(classMethod->paramTypes.size()) +
                          " parameter(s), expected " + std::to_string(ifaceMethod.paramTypes.size()) +
                          " from interface '" + ifaceName + "'");
                    continue;
                }

                for (size_t i = 0; i < ifaceMethod.paramTypes.size(); ++i) {
                    if (!classMethod->paramTypes[i]->isEqual(*ifaceMethod.paramTypes[i])) {
                        error(cl->getSpan(), "parameter " + std::to_string(i + 1) + " of method '" +
                              ifaceMethod.name + "' in class '" + cl->getName() + "' type mismatch: expected '" +
                              ifaceMethod.paramTypes[i]->toString() + "', found '" +
                              classMethod->paramTypes[i]->toString() + "'");
                    }
                }

                if (!classMethod->returnType->isEqual(*ifaceMethod.returnType)) {
                    error(cl->getSpan(), "return type of method '" + ifaceMethod.name + "' in class '" +
                          cl->getName() + "' mismatch: expected '" + ifaceMethod.returnType->toString() +
                          "', found '" + classMethod->returnType->toString() + "'");
                }
            }
        }
    }

    // Pass 2: Analyze class methods first (to compute mutability), then function bodies
    for (auto& cl : program.getClasses()) {
        cl->accept(*this);
    }
    for (size_t i = 0; i < program.getFunctions().size(); ++i) {
        auto& fn = program.getFunctions()[i];
        if (fn->isGeneric() || m_specializedFunctions.count(fn->getName())) continue;
        fn->accept(*this);
    }

    return !m_diagnostics.hasErrors();
}

void SemanticAnalyzer::visit(ProgramAST& /*node*/) {
    // Top-level traversal is controlled by analyze()
}

void SemanticAnalyzer::visit(ImportDecl& /*node*/) {
    // Processed during parsing/module resolution
}

void SemanticAnalyzer::visit(StructDecl& /*node*/) {
    // Processed during Pass 0 in analyze()
}

void SemanticAnalyzer::visit(EnumDecl& /*node*/) {
    // Processed during Pass 0b in analyze()
}

void SemanticAnalyzer::visit(InterfaceDecl& /*node*/) {
    // Processed during Pass 0c in analyze()
}

void SemanticAnalyzer::visit(ClassDecl& node) {
    const Symbol* sym = m_globalScope.lookup(node.getName());
    if (!sym || !sym->type || !sym->type->isClass()) return;
    auto classType = std::static_pointer_cast<ClassType>(sym->type);

    m_currentClass = classType;

    for (auto& method : node.getMethods()) {
        m_currentMethodMutatesThis = false;
        m_currentFunction = method.get();
        m_currentFunctionReturnType = resolveType(method->getReturnType());
        if (!m_currentFunctionReturnType) {
            m_currentFunctionReturnType = Type::getError();
        }

        Scope methodScope(&m_globalScope);
        m_currentScope = &methodScope;

        // Define 'this' and '$this' in method scope with classType
        Symbol thisSym = Symbol::makeVariable("this", classType, true, method->getSpan());
        methodScope.define(std::move(thisSym));
        Symbol phpThisSym = Symbol::makeVariable("$this", classType, true, method->getSpan());
        methodScope.define(std::move(phpThisSym));

        // Define parameters in method scope
        for (const auto& param : method->getParams()) {
            auto pType = resolveType(param.type);
            if (!pType) pType = Type::getError();

            Symbol paramSym = Symbol::makeVariable(param.name, pType, false, param.span);
            if (!m_currentScope->define(std::move(paramSym))) {
                error(param.span, "duplicate parameter name '" + param.name + "' in method '" + method->getName() + "'");
            }
        }

        // Analyze method body
        method->getBody().accept(*this);

        // Check return paths if return type is not void
        if (!m_currentFunctionReturnType->isVoid()) {
            if (!checkReturns(method->getBody())) {
                error(method->getSpan(), "not all code paths return a value in method '" + method->getName() + "'");
            }
        }

        // Record whether this method mutates 'this'
        if (auto* methodInfo = const_cast<ClassMethodInfo*>(classType->findMethod(method->getName()))) {
            methodInfo->isMutating = m_currentMethodMutatesThis;
        }

        m_currentScope = &m_globalScope;
        m_currentFunction = nullptr;
        m_currentFunctionReturnType = nullptr;
    }

    m_currentClass = nullptr;
}

void SemanticAnalyzer::visit(FunctionDecl& node) {
    if (node.isExtern()) {
        return; // Extern function declaration has no body
    }
    m_currentFunction = &node;
    m_currentFunctionReturnType = resolveType(node.getReturnType());
    if (!m_currentFunctionReturnType) {
        m_currentFunctionReturnType = Type::getError();
    }

    // Function body scope (child of global scope)
    Scope fnScope(&m_globalScope);
    m_currentScope = &fnScope;

    // Define parameters in function scope
    for (const auto& param : node.getParams()) {
        auto pType = resolveType(param.type);
        if (!pType) pType = Type::getError();

        bool isMut = param.isByRef || (!param.name.empty() && param.name[0] == '$');
        Symbol paramSym = Symbol::makeVariable(param.name, pType, isMut, param.span);
        if (!m_currentScope->define(std::move(paramSym))) {
            error(param.span, "duplicate parameter name '" + param.name + "'");
        }
    }

    // Analyze body statements
    for (auto& stmt : node.getBody().getStatements()) {
        stmt->accept(*this);
    }

    // Check return path for non-void functions
    if (!m_currentFunctionReturnType->isVoid() && !m_currentFunctionReturnType->isDynamic() && !m_currentFunctionReturnType->isError()) {
        if (!checkReturns(node.getBody())) {
            error(node.getSpan(), "not all control paths return a value in function '" + node.getName() + "'");
        }
    }

    m_currentScope = &m_globalScope;
    m_currentFunction = nullptr;
    m_currentFunctionReturnType = nullptr;
}

void SemanticAnalyzer::visit(BlockStmt& node) {
    Scope blockScope(m_currentScope);
    m_currentScope = &blockScope;

    for (auto& stmt : node.getStatements()) {
        stmt->accept(*this);
    }

    m_currentScope = m_currentScope->getParent();
}

void SemanticAnalyzer::visit(UnsafeBlockStmt& node) {
    m_unsafeDepth++;
    node.getBlock().accept(*this);
    m_unsafeDepth--;
}

void SemanticAnalyzer::visit(VarDeclStmt& node) {
    std::shared_ptr<Type> annotType = nullptr;
    if (node.getTypeAnnotation().has_value()) {
        annotType = resolveType(*node.getTypeAnnotation());
        if (annotType && annotType->isVector()) {
            if (auto* vecNew = dynamic_cast<VecNewExpr*>(&node.getInitializer())) {
                vecNew->setInferredType(annotType);
            }
        }
    }

    auto oldExpected = m_expectedType;
    if (annotType) {
        m_expectedType = annotType;
    }
    node.getInitializer().accept(*this);
    m_expectedType = oldExpected;

    auto initType = node.getInitializer().getInferredType();

    std::shared_ptr<Type> varType;
    if (node.getTypeAnnotation().has_value()) {
        varType = resolveType(*node.getTypeAnnotation());
        if (!varType) {
            error(node.getSpan(), "unknown type '" + *node.getTypeAnnotation() + "' in variable declaration");
            varType = Type::getError();
        } else if (initType && !initType->isError() && !isAssignable(*initType, *varType)) {
            error(node.getSpan(), "type mismatch: cannot initialize variable '" + node.getName() +
                                  "' of type '" + varType->toString() + "' with expression of type '" +
                                  initType->toString() + "'");
        }
    } else {
        // Type inference
        varType = initType;
        if (!varType || varType->isVoid() || varType->isError()) {
            error(node.getSpan(), "cannot infer variable type for '" + node.getName() + "'");
            varType = Type::getError();
        }
    }

    Symbol sym = Symbol::makeVariable(node.getName(), varType, node.isMut(), node.getSpan());
    if (!m_currentScope->define(std::move(sym))) {
        error(node.getSpan(), "redefinition of variable '" + node.getName() + "' in the same scope");
    }

    if (initType && initType->isMoveOnly()) {
        if (auto* varExpr = dynamic_cast<VariableExpr*>(&node.getInitializer())) {
            Symbol* srcSym = m_currentScope->lookup(varExpr->getName());
            if (srcSym) {
                srcSym->isMoved = true;
            }
        }
    }
}

void SemanticAnalyzer::visit(ReturnStmt& node) {
    if (node.hasValue()) {
        auto oldExpected = m_expectedType;
        m_expectedType = m_currentFunctionReturnType;
        node.getValue()->accept(*this);
        m_expectedType = oldExpected;
        auto valType = node.getValue()->getInferredType();

        if (valType && valType->isMoveOnly()) {
            if (auto* varExpr = dynamic_cast<VariableExpr*>(node.getValue())) {
                Symbol* srcSym = m_currentScope->lookup(varExpr->getName());
                if (srcSym) {
                    srcSym->isMoved = true;
                }
            }
        }

        if (m_currentFunction && m_currentFunction->isAsync() && m_currentFunctionReturnType && m_currentFunctionReturnType->isPromise()) {
            const auto* pt = dynamic_cast<const PromiseType*>(m_currentFunctionReturnType.get());
            auto expectedInner = (pt && pt->getInnerType()) ? pt->getInnerType() : Type::getI64();
            if (!isAssignable(*valType, *m_currentFunctionReturnType) && !isAssignable(*valType, *expectedInner)) {
                error(node.getSpan(), "return type mismatch in async function: expected '" +
                                      expectedInner->toString() + "', found '" +
                                      valType->toString() + "'");
            }
        } else if (m_currentFunctionReturnType && m_currentFunctionReturnType->isVoid()) {
            error(node.getSpan(), "cannot return a value from function with return type 'void'");
        } else if (m_currentFunctionReturnType && valType && !valType->isError()) {
            if (!isAssignable(*valType, *m_currentFunctionReturnType)) {
                error(node.getSpan(), "return type mismatch: expected '" +
                                      m_currentFunctionReturnType->toString() + "', found '" +
                                      valType->toString() + "'");
            }
        }
    } else {
        if (m_currentFunctionReturnType && !m_currentFunctionReturnType->isVoid() &&
            !m_currentFunctionReturnType->isError()) {
            error(node.getSpan(), "expected return value of type '" +
                                  m_currentFunctionReturnType->toString() + "' in non-void function");
        }
    }
}

void SemanticAnalyzer::visit(IfStmt& node) {
    node.getCondition().accept(*this);
    auto condType = node.getCondition().getInferredType();
    if (condType && !condType->isError() && !condType->isBoolean()) {
        error(node.getCondition().getSpan(), "condition in 'if' statement must be of type 'bool', found '" +
                                            condType->toString() + "'");
    }

    node.getThenBranch().accept(*this);
    if (node.hasElse()) {
        node.getElseBranch()->accept(*this);
    }
}

void SemanticAnalyzer::visit(WhileStmt& node) {
    node.getCondition().accept(*this);
    auto condType = node.getCondition().getInferredType();
    if (condType && !condType->isError() && !condType->isBoolean() && !condType->isDynamic()) {
        error(node.getCondition().getSpan(), "condition in 'while' statement must be of type 'bool', found '" +
                                            condType->toString() + "'");
    }

    m_loopDepth++;
    node.getBody().accept(*this);
    m_loopDepth--;
}

void SemanticAnalyzer::visit(ForeachStmt& node) {
    node.getCollection().accept(*this);
    auto collType = node.getCollection().getInferredType();
    std::shared_ptr<Type> keyType = nullptr;
    std::shared_ptr<Type> valType = nullptr;

    if (collType && !collType->isError()) {
        if (collType->isDynamic()) {
            keyType = Type::getDynamic();
            valType = Type::getDynamic();
        } else if (collType->isArray()) {
            auto arrType = std::static_pointer_cast<ArrayType>(collType);
            keyType = Type::getI32();
            valType = arrType->getElementType();
        } else if (collType->isSlice()) {
            auto sliceType = std::static_pointer_cast<SliceType>(collType);
            keyType = Type::getI32();
            valType = sliceType->getElementType();
        } else if (collType->isVector()) {
            auto vecType = std::static_pointer_cast<VectorType>(collType);
            keyType = Type::getI64();
            valType = vecType->getElementType();
        } else {
            error(node.getCollection().getSpan(),
                  "type '" + collType->toString() + "' is not iterable in foreach");
            valType = Type::getError();
        }
    } else {
        valType = Type::getError();
    }

    Scope blockScope(m_currentScope);
    m_currentScope = &blockScope;

    if (node.hasKeyVar() && keyType) {
        m_currentScope->define(Symbol::makeVariable(node.getKeyVar(), keyType, true, node.getSpan()));
    }
    if (valType) {
        m_currentScope->define(Symbol::makeVariable(node.getValVar(), valType, true, node.getSpan()));
    }

    m_loopDepth++;
    for (auto& stmt : node.getBody().getStatements()) {
        stmt->accept(*this);
    }
    m_loopDepth--;

    m_currentScope = m_currentScope->getParent();
}

void SemanticAnalyzer::visit(BreakStmt& node) {
    if (m_loopDepth <= 0) {
        error(node.getSpan(), "'break' statement not within a loop");
    }
}

void SemanticAnalyzer::visit(ContinueStmt& node) {
    if (m_loopDepth <= 0) {
        error(node.getSpan(), "'continue' statement not within a loop");
    }
}

void SemanticAnalyzer::visit(ExprStmt& node) {
    node.getExpr().accept(*this);
}

void SemanticAnalyzer::visit(EchoStmt& node) {
    for (auto& expr : node.getExpressions()) {
        expr->accept(*this);
    }
}

void SemanticAnalyzer::visit(ThrowStmt& node) {
    node.getExpr().accept(*this);
}

void SemanticAnalyzer::visit(TryStmt& node) {
    node.getTryBlock().accept(*this);
    for (auto& clause : node.getCatchClauses()) {
        Scope catchScope(m_currentScope);
        m_currentScope = &catchScope;
        auto exType = resolveType(clause.type);
        if (!exType) exType = Type::getDynamic();
        m_currentScope->define(Symbol::makeVariable(clause.varName, exType, false, clause.span));
        if (clause.body) {
            clause.body->accept(*this);
        }
        m_currentScope = m_currentScope->getParent();
    }
    if (node.hasFinally()) {
        const_cast<BlockStmt*>(node.getFinallyBlock())->accept(*this);
    }
}

void SemanticAnalyzer::visit(DeferStmt& node) {
    node.getStatement().accept(*this);
}

void SemanticAnalyzer::visit(IntegerLiteralExpr& node) {
    node.setInferredType(Type::getI32());
}

void SemanticAnalyzer::visit(FloatLiteralExpr& node) {
    node.setInferredType(Type::getF64());
}

void SemanticAnalyzer::visit(StringLiteralExpr& node) {
    node.setInferredType(Type::getString());
}

void SemanticAnalyzer::visit(BoolLiteralExpr& node) {
    node.setInferredType(Type::getBool());
}

void SemanticAnalyzer::visit(VariableExpr& node) {
    if (node.getName() == "CURLOPT_URL" || node.getName() == "CURLOPT_RETURNTRANSFER" ||
        node.getName() == "CURLOPT_POST" || node.getName() == "CURLOPT_POSTFIELDS" ||
        node.getName() == "CURLOPT_HTTPHEADER" || node.getName() == "CURLOPT_TIMEOUT" ||
        node.getName() == "CURLOPT_CUSTOMREQUEST" || node.getName() == "CURLINFO_HTTP_CODE") {
        node.setInferredType(Type::getI32());
        return;
    }

    Symbol* sym = m_currentScope->lookup(node.getName());
    if (!sym) {
        if (m_isDynamicMode || (!node.getName().empty() && node.getName()[0] == '$')) {
            auto dynType = Type::getDynamic();
            m_currentScope->define(Symbol::makeVariable(node.getName(), dynType, true, node.getSpan()));
            sym = m_currentScope->lookup(node.getName());
        } else {
            error(node.getSpan(), "use of undeclared variable '" + node.getName() + "'");
            node.setInferredType(Type::getError());
            return;
        }
    }

    if (sym->isMoved) {
        error(node.getSpan(), "use of moved value '" + node.getName() + "'");
        node.setInferredType(Type::getError());
        return;
    }

    node.setInferredType(sym->type);
}

void SemanticAnalyzer::visit(AssignExpr& node) {
    auto oldExpected = m_expectedType;
    if (auto* varExpr = dynamic_cast<VariableExpr*>(&node.getTarget())) {
        if (!varExpr->getName().empty() && varExpr->getName()[0] == '$') {
            m_expectedType = Type::getDynamic();
        } else {
            Symbol* s = m_currentScope->lookup(varExpr->getName());
            if (s && s->type) m_expectedType = s->type;
        }
    }
    node.getValue().accept(*this);
    m_expectedType = oldExpected;
    auto valType = node.getValue().getInferredType();

    if (auto* varExpr = dynamic_cast<VariableExpr*>(&node.getTarget())) {
        Symbol* sym = m_currentScope->lookup(varExpr->getName());
        if (!sym) {
            if (m_isDynamicMode || (!varExpr->getName().empty() && varExpr->getName()[0] == '$')) {
                auto dynType = Type::getDynamic();
                m_currentScope->define(Symbol::makeVariable(varExpr->getName(), dynType, true, varExpr->getSpan()));
                sym = m_currentScope->lookup(varExpr->getName());
            } else {
                error(varExpr->getSpan(), "use of undeclared variable '" + varExpr->getName() + "'");
                node.setInferredType(Type::getError());
                return;
            }
        }

        if (!sym->isMut) {
            error(node.getSpan(), "cannot assign to immutable variable '" + varExpr->getName() +
                                  "'; declare it with 'mut' to allow mutation");
        }

        if (sym->type && sym->type->isDynamic()) {
            sym->isMoved = false;
            node.setInferredType(Type::getDynamic());
            return;
        }

        if (valType && !valType->isError() && !isAssignable(*valType, *sym->type)) {
            error(node.getSpan(), "type mismatch in assignment: cannot assign expression of type '" +
                                  valType->toString() + "' to variable of type '" + sym->type->toString() + "'");
        }

        sym->isMoved = false;

        if (valType && valType->isMoveOnly()) {
            if (auto* rhsVar = dynamic_cast<VariableExpr*>(&node.getValue())) {
                Symbol* rhsSym = m_currentScope->lookup(rhsVar->getName());
                if (rhsSym) {
                    rhsSym->isMoved = true;
                }
            }
        }

        node.setInferredType(sym->type);
        return;
    }

    if (auto* memExpr = dynamic_cast<MemberAccessExpr*>(&node.getTarget())) {
        // Trace back root variable to check mutability
        const ExprAST* root = &memExpr->getBase();
        while (const auto* innerMem = dynamic_cast<const MemberAccessExpr*>(root)) {
            root = &innerMem->getBase();
        }

        if (dynamic_cast<const ThisExpr*>(root)) {
            m_currentMethodMutatesThis = true;
        } else if (const auto* rootVar = dynamic_cast<const VariableExpr*>(root)) {
            const Symbol* rootSym = m_currentScope->lookup(rootVar->getName());
            if (rootSym && !rootSym->isMut) {
                error(node.getSpan(), "cannot assign to member of immutable variable '" + rootVar->getName() +
                                      "'; declare it with 'mut' to allow mutation");
            }
        }

        memExpr->accept(*this);
        auto targetType = memExpr->getInferredType();

        if (valType && !valType->isError() && targetType && !targetType->isError() && !isAssignable(*valType, *targetType)) {
            error(node.getSpan(), "type mismatch in member assignment: cannot assign expression of type '" +
                                  valType->toString() + "' to member of type '" + targetType->toString() + "'");
        }

        node.setInferredType(targetType ? targetType : Type::getError());
        return;
    }

    if (auto* idxExpr = dynamic_cast<IndexExpr*>(&node.getTarget())) {
        // Trace back root variable to check mutability
        const ExprAST* root = &idxExpr->getBase();
        while (true) {
            if (const auto* innerIdx = dynamic_cast<const IndexExpr*>(root)) {
                root = &innerIdx->getBase();
            } else if (const auto* innerSlice = dynamic_cast<const SliceExpr*>(root)) {
                root = &innerSlice->getBase();
            } else if (const auto* innerMem = dynamic_cast<const MemberAccessExpr*>(root)) {
                root = &innerMem->getBase();
            } else {
                break;
            }
        }

        if (const auto* rootVar = dynamic_cast<const VariableExpr*>(root)) {
            const Symbol* rootSym = m_currentScope->lookup(rootVar->getName());
            if (rootSym && !rootSym->isMut) {
                error(node.getSpan(), "cannot assign to element of immutable variable '" + rootVar->getName() +
                                      "'; declare it with 'mut' to allow mutation");
            }
        }

        idxExpr->accept(*this);
        auto targetType = idxExpr->getInferredType();

        if (valType && !valType->isError() && targetType && !targetType->isError() && !isAssignable(*valType, *targetType)) {
            error(node.getSpan(), "type mismatch in array element assignment: cannot assign expression of type '" +
                                  valType->toString() + "' to element of type '" + targetType->toString() + "'");
        }

        node.setInferredType(targetType ? targetType : Type::getError());
        return;
    }

    if (auto* unaryExpr = dynamic_cast<UnaryExpr*>(&node.getTarget())) {
        if (unaryExpr->getOp() == UnaryOp::Deref) {
            if (m_unsafeDepth == 0) {
                error(node.getTarget().getSpan(), "dereference of raw pointer requires 'unsafe' block");
            }
            unaryExpr->getOperand().accept(*this);
            auto opType = unaryExpr->getOperand().getInferredType();
            if (!opType || !opType->isPointer()) {
                error(unaryExpr->getSpan(), "cannot dereference non-pointer type '" +
                      (opType ? opType->toString() : "unknown") + "'");
                node.setInferredType(Type::getError());
                return;
            }
            auto ptrType = std::static_pointer_cast<PointerType>(opType);
            auto pointeeType = ptrType->getPointeeType();
            if (valType && !valType->isError() && pointeeType && !pointeeType->isError() && !isAssignable(*valType, *pointeeType)) {
                error(node.getSpan(), "type mismatch in pointer assignment: cannot assign expression of type '" +
                      valType->toString() + "' to pointee of type '" + pointeeType->toString() + "'");
            }
            unaryExpr->setInferredType(pointeeType);
            node.setInferredType(pointeeType ? pointeeType : Type::getError());
            return;
        }
    }

    error(node.getSpan(), "invalid assignment target");
    node.setInferredType(Type::getError());
}

void SemanticAnalyzer::visit(StructInitExpr& node) {
    if (node.hasTypeArgs() || m_genericStructs.count(node.getStructName())) {
        std::vector<std::string> typeArgs = node.getTypeArgs();
        if (typeArgs.empty()) {
            error(node.getSpan(), "generic struct '" + node.getStructName() + "' requires explicit type arguments");
            node.setInferredType(Type::getError());
            return;
        }
        auto stType = instantiateGenericStruct(node.getStructName(), typeArgs, node.getSpan());
        if (!stType) {
            node.setInferredType(Type::getError());
            return;
        }
        std::string mangledName = getMangledStructName(node.getStructName(), typeArgs);
        node.setStructName(mangledName);
    }

    auto structTypePtr = resolveType(node.getStructName());
    if (!structTypePtr || (!structTypePtr->isStruct() && !structTypePtr->isClass())) {
        error(node.getSpan(), "unknown struct or class '" + node.getStructName() + "'");
        node.setInferredType(Type::getError());
        return;
    }

    auto structType = std::static_pointer_cast<StructType>(structTypePtr);
    std::string typeLabel = structTypePtr->isClass() ? "class '" : "struct '";
    std::unordered_map<std::string, const StructMember*> memberMap;
    for (const auto& member : structType->getMembers()) {
        memberMap[member.name] = &member;
    }

    std::unordered_set<std::string> initializedFields;
    for (auto& f : node.getFields()) {
        f.second->accept(*this);
        auto valType = f.second->getInferredType();

        auto it = memberMap.find(f.first);
        if (it == memberMap.end()) {
            error(f.second->getSpan(), typeLabel + node.getStructName() + "' has no field named '" + f.first + "'");
            continue;
        }

        if (initializedFields.count(f.first)) {
            error(f.second->getSpan(), "duplicate initialization for field '" + f.first + "' in " + typeLabel + node.getStructName() + "'");
            continue;
        }
        initializedFields.insert(f.first);

        if (valType && !valType->isError() && !valType->isEqual(*it->second->type)) {
            error(f.second->getSpan(), "type mismatch for field '" + f.first + "': expected '" +
                                       it->second->type->toString() + "', found '" + valType->toString() + "'");
        }
    }

    for (const auto& member : structType->getMembers()) {
        if (!initializedFields.count(member.name)) {
            error(node.getSpan(), "missing field '" + member.name + "' in initialization of " + typeLabel + node.getStructName() + "'");
        }
    }

    node.setInferredType(structType);
}

void SemanticAnalyzer::visit(MemberAccessExpr& node) {
    node.getBase().accept(*this);
    auto baseType = node.getBase().getInferredType();

    if (!baseType || baseType->isError()) {
        node.setInferredType(Type::getError());
        return;
    }

    if (baseType->isDynamic()) {
        node.setInferredType(Type::getDynamic());
        return;
    }

    if (baseType->isSlice()) {
        if (node.getMember() == "len" || node.getMember() == "length") {
            node.setInferredType(Type::getI32());
            return;
        }
        error(node.getSpan(), "slice has no member '" + node.getMember() + "'");
        node.setInferredType(Type::getError());
        return;
    }

    auto effectiveType = baseType;
    if (baseType->isRc()) {
        auto rcType = std::static_pointer_cast<RcType>(baseType);
        effectiveType = rcType->getInnerType();
    } else if (baseType->isBox()) {
        auto boxType = std::static_pointer_cast<BoxType>(baseType);
        effectiveType = boxType->getInnerType();
    } else if (baseType->isPointer()) {
        auto ptrType = std::static_pointer_cast<PointerType>(baseType);
        effectiveType = ptrType->getPointeeType();
    }

    if (!effectiveType || (!effectiveType->isStruct() && !effectiveType->isClass())) {
        error(node.getSpan(), "cannot access member '" + node.getMember() + "' on non-composite type '" + baseType->toString() + "'");
        node.setInferredType(Type::getError());
        return;
    }

    auto structType = std::static_pointer_cast<StructType>(effectiveType);
    const auto* member = structType->findMember(node.getMember());
    if (!member) {
        if (m_isDynamicMode) {
            node.setInferredType(Type::getDynamic());
            return;
        }
        error(node.getSpan(), (effectiveType->isClass() ? "class '" : "struct '") + structType->getName() + "' has no field named '" + node.getMember() + "'");
        node.setInferredType(Type::getError());
        return;
    }

    node.setInferredType(member->type);
}

void SemanticAnalyzer::visit(ScopedAccessExpr& node) {
    const Symbol* sym = m_globalScope.lookup(node.getScopeName());
    if (!sym || sym->kind != SymbolKind::Enum) {
        error(node.getSpan(), "unknown enum '" + node.getScopeName() + "'");
        node.setInferredType(Type::getError());
        return;
    }

    auto enumType = std::dynamic_pointer_cast<EnumType>(sym->type);
    if (!enumType) {
        node.setInferredType(Type::getError());
        return;
    }

    const auto* variant = enumType->findVariant(node.getMemberName());
    if (!variant) {
        error(node.getSpan(), "enum '" + node.getScopeName() + "' has no variant named '" + node.getMemberName() + "'");
        node.setInferredType(Type::getError());
        return;
    }

    node.setInferredType(enumType);
}

void SemanticAnalyzer::visit(ArrayLiteralExpr& node) {
    bool hasAssociativeKey = false;
    for (const auto& elem : node.getElements()) {
        if (elem.key != nullptr) {
            hasAssociativeKey = true;
            break;
        }
    }

    if ((m_isDynamicMode || hasAssociativeKey || (m_expectedType && m_expectedType->isDynamic())) && (!m_expectedType || !m_expectedType->isArray())) {
        for (auto& elem : node.getElements()) {
            if (elem.key) {
                elem.key->accept(*this);
            }
            elem.value->accept(*this);
        }
        node.setInferredType(Type::getDynamic());
        return;
    }

    if (node.getElements().empty()) {
        node.setInferredType(ArrayType::get(Type::getVoid(), 0));
        return;
    }

    std::shared_ptr<Type> elemType = nullptr;
    for (size_t i = 0; i < node.getElements().size(); ++i) {
        auto& elem = node.getElements()[i];
        elem.value->accept(*this);
        auto t = elem.value->getInferredType();

        if (!elemType && t && !t->isError()) {
            elemType = t;
        } else if (elemType && t && !t->isError() && !elemType->isEqual(*t)) {
            if (m_isDynamicMode || !m_expectedType || m_expectedType->isDynamic()) {
                node.setInferredType(Type::getDynamic());
                return;
            }
            error(elem.value->getSpan(), "type mismatch in array literal: expected element of type '" +
                                   elemType->toString() + "', found '" + t->toString() + "'");
        }
    }

    if (!elemType) {
        elemType = Type::getError();
    }

    node.setInferredType(ArrayType::get(elemType, node.getElements().size()));
}

void SemanticAnalyzer::visit(IndexExpr& node) {
    node.getBase().accept(*this);
    auto baseType = node.getBase().getInferredType();

    if (baseType && baseType->isDynamic()) {
        if (node.hasIndex()) {
            node.getIndex().accept(*this);
        }
        node.setInferredType(Type::getDynamic());
        return;
    }

    if (!node.hasIndex()) {
        error(node.getSpan(), "cannot use empty [] on non-dynamic array");
        node.setInferredType(Type::getError());
        return;
    }

    node.getIndex().accept(*this);
    auto indexType = node.getIndex().getInferredType();

    if (indexType && !indexType->isError() && !indexType->isInteger()) {
        error(node.getIndex().getSpan(), "array index must be an integer, found '" + indexType->toString() + "'");
    }

    if (!baseType || baseType->isError()) {
        node.setInferredType(Type::getError());
        return;
    }

    if (baseType->isArray()) {
        auto arrType = std::static_pointer_cast<ArrayType>(baseType);
        node.setInferredType(arrType->getElementType());
    } else if (baseType->isSlice()) {
        auto sliceType = std::static_pointer_cast<SliceType>(baseType);
        node.setInferredType(sliceType->getElementType());
    } else if (baseType->isVector()) {
        auto vecType = std::static_pointer_cast<VectorType>(baseType);
        node.setInferredType(vecType->getElementType());
    } else if (baseType->isF32x4()) {
        node.setInferredType(Type::getF32());
    } else if (baseType->isI32x4()) {
        node.setInferredType(Type::getI32());
    } else if (baseType->isF64x2()) {
        node.setInferredType(Type::getF64());
    } else if (baseType->isI64x2()) {
        node.setInferredType(Type::getI64());
    } else {
        error(node.getBase().getSpan(), "cannot index non-array, non-slice, and non-vector type '" + baseType->toString() + "'");
        node.setInferredType(Type::getError());
    }
}

void SemanticAnalyzer::visit(RangeExpr& node) {
    if (node.hasStart()) {
        const_cast<ExprAST*>(node.getStart())->accept(*this);
        auto startType = node.getStart()->getInferredType();
        if (startType && !startType->isError() && !startType->isInteger()) {
            error(node.getStart()->getSpan(), "range start must be an integer, found '" + startType->toString() + "'");
        }
    }
    if (node.hasEnd()) {
        const_cast<ExprAST*>(node.getEnd())->accept(*this);
        auto endType = node.getEnd()->getInferredType();
        if (endType && !endType->isError() && !endType->isInteger()) {
            error(node.getEnd()->getSpan(), "range end must be an integer, found '" + endType->toString() + "'");
        }
    }
}

void SemanticAnalyzer::visit(SliceExpr& node) {
    node.getBase().accept(*this);
    auto baseType = node.getBase().getInferredType();

    node.getRange().accept(*this);

    if (!baseType || baseType->isError()) {
        node.setInferredType(Type::getError());
        return;
    }

    if (baseType->isArray()) {
        auto arrType = std::static_pointer_cast<ArrayType>(baseType);
        node.setInferredType(SliceType::get(arrType->getElementType()));
    } else if (baseType->isSlice()) {
        auto sliceType = std::static_pointer_cast<SliceType>(baseType);
        node.setInferredType(sliceType);
    } else {
        error(node.getBase().getSpan(), "cannot slice non-array type '" + baseType->toString() + "'");
        node.setInferredType(Type::getError());
    }
}

void SemanticAnalyzer::visit(BinaryExpr& node) {
    node.getLhs().accept(*this);
    node.getRhs().accept(*this);

    auto lhsType = node.getLhs().getInferredType();
    auto rhsType = node.getRhs().getInferredType();

    if (!lhsType || !rhsType || lhsType->isError() || rhsType->isError()) {
        node.setInferredType(Type::getError());
        return;
    }

    if (node.getOp() == BinaryOp::Concat) {
        if (lhsType->isDynamic() || rhsType->isDynamic()) {
            node.setInferredType(Type::getDynamic());
        } else {
            node.setInferredType(Type::getString());
        }
        return;
    }

    if (lhsType->isDynamic() || rhsType->isDynamic()) {
        switch (node.getOp()) {
            case BinaryOp::Add:
            case BinaryOp::Sub:
            case BinaryOp::Mul:
            case BinaryOp::Div:
            case BinaryOp::Mod:
                node.setInferredType(Type::getDynamic());
                return;
            case BinaryOp::Equal:
            case BinaryOp::NotEqual:
            case BinaryOp::Less:
            case BinaryOp::LessEqual:
            case BinaryOp::Greater:
            case BinaryOp::GreaterEqual:
            case BinaryOp::LogicalAnd:
            case BinaryOp::LogicalOr:
                node.setInferredType(Type::getBool());
                return;
            default:
                node.setInferredType(Type::getDynamic());
                return;
        }
    }

    switch (node.getOp()) {
        case BinaryOp::Add: {
            if (lhsType->isString() && rhsType->isString()) {
                node.setInferredType(Type::getString());
            } else if (lhsType->isPointer() && rhsType->isInteger()) {
                if (m_unsafeDepth == 0) {
                    error(node.getSpan(), "pointer arithmetic requires 'unsafe' block");
                }
                node.setInferredType(lhsType);
            } else if (lhsType->isInteger() && rhsType->isPointer()) {
                if (m_unsafeDepth == 0) {
                    error(node.getSpan(), "pointer arithmetic requires 'unsafe' block");
                }
                node.setInferredType(rhsType);
            } else if (lhsType->isNumeric() && rhsType->isNumeric() && lhsType->isEqual(*rhsType)) {
                node.setInferredType(lhsType);
            } else if (lhsType->isSimd() && rhsType->isSimd() && lhsType->isEqual(*rhsType)) {
                node.setInferredType(lhsType);
            } else {
                error(node.getSpan(), "binary operator '+' cannot be applied to types '" +
                                      lhsType->toString() + "' and '" + rhsType->toString() + "'");
                node.setInferredType(Type::getError());
            }
            break;
        }

        case BinaryOp::Sub: {
            if (lhsType->isPointer() && rhsType->isInteger()) {
                if (m_unsafeDepth == 0) {
                    error(node.getSpan(), "pointer arithmetic requires 'unsafe' block");
                }
                node.setInferredType(lhsType);
            } else if (lhsType->isPointer() && rhsType->isPointer()) {
                if (!lhsType->isEqual(*rhsType)) {
                    error(node.getSpan(), "pointer subtraction requires pointers to identical types, found '" +
                                          lhsType->toString() + "' and '" + rhsType->toString() + "'");
                    node.setInferredType(Type::getError());
                } else {
                    if (m_unsafeDepth == 0) {
                        error(node.getSpan(), "pointer arithmetic requires 'unsafe' block");
                    }
                    node.setInferredType(Type::getI64());
                }
            } else if (lhsType->isNumeric() && rhsType->isNumeric() && lhsType->isEqual(*rhsType)) {
                node.setInferredType(lhsType);
            } else if (lhsType->isSimd() && rhsType->isSimd() && lhsType->isEqual(*rhsType)) {
                node.setInferredType(lhsType);
            } else {
                error(node.getSpan(), "binary operator '-' cannot be applied to types '" +
                                      lhsType->toString() + "' and '" + rhsType->toString() + "'");
                node.setInferredType(Type::getError());
            }
            break;
        }

        case BinaryOp::Mul:
        case BinaryOp::Div:
        case BinaryOp::Mod:
            if (lhsType->isSimd() && rhsType->isSimd() && lhsType->isEqual(*rhsType) && node.getOp() != BinaryOp::Mod) {
                node.setInferredType(lhsType);
            } else if (!lhsType->isNumeric() || !rhsType->isNumeric() || !lhsType->isEqual(*rhsType)) {
                error(node.getSpan(), "binary operator '" + std::string(binaryOpToString(node.getOp())) +
                                      "' cannot be applied to types '" + lhsType->toString() +
                                      "' and '" + rhsType->toString() + "'");
                node.setInferredType(Type::getError());
            } else {
                node.setInferredType(lhsType);
            }
            break;

        case BinaryOp::BitwiseAnd:
        case BinaryOp::BitwiseOr:
        case BinaryOp::BitwiseXor:
            if (!lhsType->isInteger() || !rhsType->isInteger() || !lhsType->isEqual(*rhsType)) {
                error(node.getSpan(), "bitwise operator '" + std::string(binaryOpToString(node.getOp())) +
                                      "' cannot be applied to types '" + lhsType->toString() +
                                      "' and '" + rhsType->toString() + "'");
                node.setInferredType(Type::getError());
            } else {
                node.setInferredType(lhsType);
            }
            break;

        case BinaryOp::ShiftLeft:
        case BinaryOp::ShiftRight:
            if (!lhsType->isInteger() || !rhsType->isInteger()) {
                error(node.getSpan(), "shift operator '" + std::string(binaryOpToString(node.getOp())) +
                                      "' requires integer operands, found '" + lhsType->toString() +
                                      "' and '" + rhsType->toString() + "'");
                node.setInferredType(Type::getError());
            } else {
                node.setInferredType(lhsType);
            }
            break;

        case BinaryOp::Equal:
        case BinaryOp::NotEqual:
            if (!lhsType->isEqual(*rhsType)) {
                if (lhsType->isInteger() && rhsType->isInteger()) {
                    node.setInferredType(Type::getBool());
                    break;
                }
                error(node.getSpan(), "comparison operator '" + std::string(binaryOpToString(node.getOp())) +
                                      "' cannot compare incompatible types '" + lhsType->toString() +
                                      "' and '" + rhsType->toString() + "'");
                node.setInferredType(Type::getError());
            } else {
                node.setInferredType(Type::getBool());
            }
            break;

        case BinaryOp::Less:
        case BinaryOp::LessEqual:
        case BinaryOp::Greater:
        case BinaryOp::GreaterEqual:
            if (!lhsType->isNumeric() || !rhsType->isNumeric() || (!lhsType->isEqual(*rhsType) && !(lhsType->isInteger() && rhsType->isInteger()))) {
                error(node.getSpan(), "relational operator '" + std::string(binaryOpToString(node.getOp())) +
                                      "' cannot be applied to types '" + lhsType->toString() +
                                      "' and '" + rhsType->toString() + "'");
                node.setInferredType(Type::getError());
            } else {
                node.setInferredType(Type::getBool());
            }
            break;

        case BinaryOp::LogicalAnd:
        case BinaryOp::LogicalOr:
            if (!lhsType->isBoolean() || !rhsType->isBoolean()) {
                error(node.getSpan(), "logical operator '" + std::string(binaryOpToString(node.getOp())) +
                                      "' requires operands of type 'bool', found '" + lhsType->toString() +
                                      "' and '" + rhsType->toString() + "'");
                node.setInferredType(Type::getError());
            } else {
                node.setInferredType(Type::getBool());
            }
            break;

        case BinaryOp::Concat:
            break;
    }
}

void SemanticAnalyzer::visit(UnaryExpr& node) {
    node.getOperand().accept(*this);
    auto operandType = node.getOperand().getInferredType();

    if (!operandType || operandType->isError()) {
        node.setInferredType(Type::getError());
        return;
    }

    if (node.getOp() == UnaryOp::Negate) {
        if (!operandType->isNumeric()) {
            error(node.getSpan(), "unary operator '-' requires a numeric operand, found '" +
                                  operandType->toString() + "'");
            node.setInferredType(Type::getError());
        } else {
            node.setInferredType(operandType);
        }
    } else if (node.getOp() == UnaryOp::LogicalNot) {
        if (!operandType->isBoolean()) {
            error(node.getSpan(), "unary operator '!' requires a boolean operand, found '" +
                                  operandType->toString() + "'");
            node.setInferredType(Type::getError());
        } else {
            node.setInferredType(Type::getBool());
        }
    } else if (node.getOp() == UnaryOp::BitwiseNot) {
        if (!operandType->isInteger()) {
            error(node.getSpan(), "unary operator '~' requires an integer operand, found '" +
                                  operandType->toString() + "'");
            node.setInferredType(Type::getError());
        } else {
            node.setInferredType(operandType);
        }
    } else if (node.getOp() == UnaryOp::AddressOf) {
        const ExprAST* operand = &node.getOperand();
        bool isLvalue = dynamic_cast<const VariableExpr*>(operand) != nullptr ||
                        dynamic_cast<const MemberAccessExpr*>(operand) != nullptr ||
                        dynamic_cast<const IndexExpr*>(operand) != nullptr ||
                        (dynamic_cast<const UnaryExpr*>(operand) &&
                         dynamic_cast<const UnaryExpr*>(operand)->getOp() == UnaryOp::Deref);
        if (!isLvalue) {
            error(node.getSpan(), "cannot take address of non-lvalue expression");
            node.setInferredType(Type::getError());
        } else {
            node.setInferredType(PointerType::get(operandType));
        }
    } else if (node.getOp() == UnaryOp::Deref) {
        if (m_unsafeDepth == 0) {
            error(node.getSpan(), "dereference of raw pointer requires 'unsafe' block");
        }
        if (!operandType->isPointer()) {
            error(node.getSpan(), "cannot dereference non-pointer type '" + operandType->toString() + "'");
            node.setInferredType(Type::getError());
        } else {
            auto ptrType = std::static_pointer_cast<PointerType>(operandType);
            node.setInferredType(ptrType->getPointeeType());
        }
    }
}

void SemanticAnalyzer::visit(CastExpr& node) {
    node.getExpr().accept(*this);
    auto srcType = node.getExpr().getInferredType();
    auto targetType = resolveType(node.getTargetType());

    if (!targetType) {
        error(node.getSpan(), "unknown target type '" + node.getTargetType() + "' in cast expression");
        node.setInferredType(Type::getError());
        return;
    }

    if (!srcType || srcType->isError()) {
        node.setInferredType(targetType);
        return;
    }

    if (srcType->isEqual(*targetType)) {
        node.setInferredType(targetType);
        return;
    }

    // Dynamic boundary cast: dynamic to concrete or concrete to dynamic
    if (srcType->isDynamic() || targetType->isDynamic()) {
        node.setInferredType(targetType);
        return;
    }

    // Numeric to numeric
    if (srcType->isNumeric() && targetType->isNumeric()) {
        node.setInferredType(targetType);
        return;
    }

    // Pointer to pointer
    if (srcType->isPointer() && targetType->isPointer()) {
        if (m_unsafeDepth == 0) {
            error(node.getSpan(), "pointer-to-pointer cast requires 'unsafe' block");
        }
        node.setInferredType(targetType);
        return;
    }

    // Pointer to integer
    if (srcType->isPointer() && targetType->isInteger()) {
        if (m_unsafeDepth == 0) {
            error(node.getSpan(), "cast from pointer to integer requires 'unsafe' block");
        }
        node.setInferredType(targetType);
        return;
    }

    // Integer to pointer
    if (srcType->isInteger() && targetType->isPointer()) {
        if (m_unsafeDepth == 0) {
            error(node.getSpan(), "cast from integer to pointer requires 'unsafe' block");
        }
        node.setInferredType(targetType);
        return;
    }

    error(node.getSpan(), "cannot cast from '" + srcType->toString() + "' to '" + targetType->toString() + "'");
    node.setInferredType(Type::getError());
}

void SemanticAnalyzer::visit(RcNewExpr& node) {
    node.getValue().accept(*this);
    auto innerType = node.getValue().getInferredType();
    if (!innerType || innerType->isError()) {
        node.setInferredType(Type::getError());
        return;
    }
    node.setInferredType(RcType::get(innerType));
}

void SemanticAnalyzer::visit(RcCountExpr& node) {
    node.getPointer().accept(*this);
    auto ptrType = node.getPointer().getInferredType();
    if (!ptrType || ptrType->isError()) {
        node.setInferredType(Type::getError());
        return;
    }
    if (!ptrType->isRc()) {
        error(node.getSpan(), "Rc::count expects an argument of type 'Rc<T>', but got '" + ptrType->toString() + "'");
        node.setInferredType(Type::getError());
        return;
    }
    node.setInferredType(Type::getI64());
}

void SemanticAnalyzer::visit(BoxNewExpr& node) {
    node.getValue().accept(*this);
    auto innerType = node.getValue().getInferredType();
    if (!innerType || innerType->isError()) {
        node.setInferredType(Type::getError());
        return;
    }
    node.setInferredType(BoxType::get(innerType));
}

void SemanticAnalyzer::visit(ArenaNewExpr& node) {
    if (node.hasCapacity()) {
        node.getCapacity().accept(*this);
        auto capType = node.getCapacity().getInferredType();
        if (capType && !capType->isError() && !capType->isInteger()) {
            error(node.getCapacity().getSpan(), "Arena::new expects integer capacity, found '" + capType->toString() + "'");
        }
    }
    node.setInferredType(ArenaType::get());
}

void SemanticAnalyzer::visit(ArenaAllocExpr& node) {
    node.getArena().accept(*this);
    auto arenaType = node.getArena().getInferredType();
    if (arenaType && !arenaType->isError() && !arenaType->isArena()) {
        error(node.getArena().getSpan(), "Arena::alloc expects first argument of type 'Arena', found '" + arenaType->toString() + "'");
    }

    node.getValue().accept(*this);
    auto valType = node.getValue().getInferredType();
    if (!valType || valType->isError()) {
        node.setInferredType(Type::getError());
        return;
    }
    node.setInferredType(PointerType::get(valType));
}

void SemanticAnalyzer::visit(ArenaResetExpr& node) {
    node.getArena().accept(*this);
    auto arenaType = node.getArena().getInferredType();
    if (arenaType && !arenaType->isError() && !arenaType->isArena()) {
        error(node.getArena().getSpan(), "Arena::reset expects argument of type 'Arena', found '" + arenaType->toString() + "'");
    }
    node.setInferredType(Type::getVoid());
}

void SemanticAnalyzer::visit(ArenaDestroyExpr& node) {
    node.getArena().accept(*this);
    auto arenaType = node.getArena().getInferredType();
    if (arenaType && !arenaType->isError() && !arenaType->isArena()) {
        error(node.getArena().getSpan(), "Arena::destroy expects argument of type 'Arena', found '" + arenaType->toString() + "'");
    }
    if (auto* varExpr = dynamic_cast<VariableExpr*>(&node.getArena())) {
        Symbol* sym = m_currentScope->lookup(varExpr->getName());
        if (sym) {
            sym->isMoved = true;
        }
    }
    node.setInferredType(Type::getVoid());
}

void SemanticAnalyzer::visit(VecNewExpr& node) {
    if (node.hasCapacity()) {
        node.getCapacity().accept(*this);
        auto capType = node.getCapacity().getInferredType();
        if (capType && !capType->isError() && !capType->isInteger()) {
            error(node.getCapacity().getSpan(), "Vec::with_capacity expects integer capacity, found '" + capType->toString() + "'");
        }
    }
    if (node.getInferredType() && node.getInferredType()->isVector()) {
        return;
    }
    if (!node.getElemType().empty()) {
        auto elemType = resolveType(node.getElemType());
        if (!elemType) {
            error(node.getSpan(), "unknown element type '" + node.getElemType() + "' in Vec::new");
            node.setInferredType(Type::getError());
            return;
        }
        node.setInferredType(VectorType::get(elemType));
        return;
    }
    error(node.getSpan(), "cannot infer element type for Vec::new; specify type annotation or use Vec<T>::new()");
    node.setInferredType(Type::getError());
}

void SemanticAnalyzer::visit(CallExpr& node) {
    for (auto& arg : node.getArgs()) {
        arg->accept(*this);
    }

    // Built-in println handling
    if (node.getCallee() == "println") {
        if (node.getArgs().size() != 1) {
            error(node.getSpan(), "built-in function 'println' expects 1 argument, found " +
                                  std::to_string(node.getArgs().size()));
        } else {
            auto argType = node.getArgs()[0]->getInferredType();
            if (argType && !argType->isError()) {
                if (!argType->isNumeric() && !argType->isBoolean() && !argType->isString() && !argType->isSimd()) {
                    error(node.getArgs()[0]->getSpan(),
                          "built-in function 'println' cannot print value of type '" + argType->toString() + "'");
                }
            }
        }
        node.setInferredType(Type::getVoid());
        return;
    }

    // Built-in var_dump handling
    if (node.getCallee() == "var_dump") {
        if (node.getArgs().size() != 1) {
            error(node.getSpan(), "'var_dump' expects 1 argument, found " +
                                  std::to_string(node.getArgs().size()));
        } else {
            node.getArgs()[0]->accept(*this);
        }
        node.setInferredType(Type::getVoid());
        return;
    }

    // Built-in Dynamic-to-Static and Primitive Conversion Intrinsics
    if (node.getCallee() == "intval" || node.getCallee() == "as_i32") {
        if (node.getArgs().size() != 1) {
            error(node.getSpan(), "'" + node.getCallee() + "' expects 1 argument, found " + std::to_string(node.getArgs().size()));
        } else {
            node.getArgs()[0]->accept(*this);
        }
        node.setInferredType(Type::getI32());
        return;
    }

    if (node.getCallee() == "as_i64") {
        if (node.getArgs().size() != 1) {
            error(node.getSpan(), "'as_i64' expects 1 argument, found " + std::to_string(node.getArgs().size()));
        } else {
            node.getArgs()[0]->accept(*this);
        }
        node.setInferredType(Type::getI64());
        return;
    }

    if (node.getCallee() == "floatval" || node.getCallee() == "as_f64") {
        if (node.getArgs().size() != 1) {
            error(node.getSpan(), "'" + node.getCallee() + "' expects 1 argument, found " + std::to_string(node.getArgs().size()));
        } else {
            node.getArgs()[0]->accept(*this);
        }
        node.setInferredType(Type::getF64());
        return;
    }

    if (node.getCallee() == "as_f32") {
        if (node.getArgs().size() != 1) {
            error(node.getSpan(), "'as_f32' expects 1 argument, found " + std::to_string(node.getArgs().size()));
        } else {
            node.getArgs()[0]->accept(*this);
        }
        node.setInferredType(Type::getF32());
        return;
    }

    if (node.getCallee() == "strval" || node.getCallee() == "as_string") {
        if (node.getArgs().size() != 1) {
            error(node.getSpan(), "'" + node.getCallee() + "' expects 1 argument, found " + std::to_string(node.getArgs().size()));
        } else {
            node.getArgs()[0]->accept(*this);
        }
        node.setInferredType(Type::getString());
        return;
    }

    if (node.getCallee() == "boolval" || node.getCallee() == "as_bool") {
        if (node.getArgs().size() != 1) {
            error(node.getSpan(), "'" + node.getCallee() + "' expects 1 argument, found " + std::to_string(node.getArgs().size()));
        } else {
            node.getArgs()[0]->accept(*this);
        }
        node.setInferredType(Type::getBool());
        return;
    }

    // Built-in PHP functions
    if (node.getCallee() == "count") {
        if (node.getArgs().size() != 1) {
            error(node.getSpan(), "'count' expects 1 argument, found " + std::to_string(node.getArgs().size()));
        }
        node.setInferredType(Type::getI64());
        return;
    }

    if (node.getCallee() == "is_array" || node.getCallee() == "is_string" ||
        node.getCallee() == "is_int" || node.getCallee() == "is_float" ||
        node.getCallee() == "is_bool" || node.getCallee() == "is_null" ||
        node.getCallee() == "is_object") {
        if (node.getArgs().size() != 1) {
            error(node.getSpan(), "'" + node.getCallee() + "' expects 1 argument, found " + std::to_string(node.getArgs().size()));
        }
        node.setInferredType(Type::getBool());
        return;
    }

    if (node.getCallee() == "get_class") {
        if (node.getArgs().size() != 1) {
            error(node.getSpan(), "'get_class' expects 1 argument, found " + std::to_string(node.getArgs().size()));
        }
        node.setInferredType(Type::getString());
        return;
    }

    if (node.getCallee() == "property_exists") {
        if (node.getArgs().size() != 2) {
            error(node.getSpan(), "'property_exists' expects 2 arguments ($object, $property), found " + std::to_string(node.getArgs().size()));
        }
        node.setInferredType(Type::getBool());
        return;
    }

    if (node.getCallee() == "array_key_exists") {
        if (node.getArgs().size() != 2) {
            error(node.getSpan(), "'array_key_exists' expects 2 arguments ($key, $array), found " + std::to_string(node.getArgs().size()));
        }
        node.setInferredType(Type::getBool());
        return;
    }

    if (node.getCallee() == "curl_init") {
        node.setInferredType(Type::getDynamic());
        return;
    }
    if (node.getCallee() == "curl_setopt") {
        node.setInferredType(Type::getBool());
        return;
    }
    if (node.getCallee() == "curl_exec") {
        node.setInferredType(Type::getDynamic());
        return;
    }
    if (node.getCallee() == "curl_getinfo") {
        node.setInferredType(Type::getDynamic());
        return;
    }
    if (node.getCallee() == "curl_close") {
        node.setInferredType(Type::getVoid());
        return;
    }
    if (node.getCallee() == "file_get_contents") {
        node.setInferredType(Type::getString());
        return;
    }
    if (node.getCallee() == "file_put_contents") {
        node.setInferredType(Type::getI64());
        return;
    }

    if (node.getCallee() == "array_push") {
        if (node.getArgs().size() != 2) {
            error(node.getSpan(), "'array_push' expects 2 arguments ($array, $value), found " + std::to_string(node.getArgs().size()));
        }
        node.setInferredType(Type::getI64());
        return;
    }

    if (node.getCallee() == "array_pop" || node.getCallee() == "array_shift" ||
        node.getCallee() == "array_keys" || node.getCallee() == "array_values") {
        if (node.getArgs().size() != 1) {
            error(node.getSpan(), "'" + node.getCallee() + "' expects 1 argument ($array), found " + std::to_string(node.getArgs().size()));
        }
        node.setInferredType(Type::getDynamic());
        return;
    }

    if (node.getCallee() == "in_array") {
        if (node.getArgs().size() != 2) {
            error(node.getSpan(), "'in_array' expects 2 arguments ($needle, $haystack), found " + std::to_string(node.getArgs().size()));
        }
        node.setInferredType(Type::getBool());
        return;
    }

    if (node.getCallee() == "strlen") {
        if (node.getArgs().size() != 1) {
            error(node.getSpan(), "'strlen' expects 1 argument, found " + std::to_string(node.getArgs().size()));
        }
        node.setInferredType(Type::getI64());
        return;
    }

    if (node.getCallee() == "substr") {
        if (node.getArgs().size() < 2 || node.getArgs().size() > 3) {
            error(node.getSpan(), "'substr' expects 2 or 3 arguments ($string, $start [, $length]), found " + std::to_string(node.getArgs().size()));
        }
        node.setInferredType(Type::getString());
        return;
    }

    if (node.getCallee() == "strpos") {
        if (node.getArgs().size() != 2) {
            error(node.getSpan(), "'strpos' expects 2 arguments ($haystack, $needle), found " + std::to_string(node.getArgs().size()));
        }
        node.setInferredType(Type::getI64());
        return;
    }

    if (node.getCallee() == "explode") {
        if (node.getArgs().size() != 2) {
            error(node.getSpan(), "'explode' expects 2 arguments ($delimiter, $string), found " + std::to_string(node.getArgs().size()));
        }
        node.setInferredType(Type::getDynamic());
        return;
    }

    if (node.getCallee() == "implode") {
        if (node.getArgs().size() != 2) {
            error(node.getSpan(), "'implode' expects 2 arguments ($glue, $pieces), found " + std::to_string(node.getArgs().size()));
        }
        node.setInferredType(Type::getString());
        return;
    }

    // Built-in File APIs
    if (node.getCallee() == "File::read") {
        if (node.getArgs().size() != 1) {
            error(node.getSpan(), "'File::read' expects 1 argument (path: string), found " +
                                  std::to_string(node.getArgs().size()));
        } else {
            auto argType = node.getArgs()[0]->getInferredType();
            if (argType && !argType->isError() && !argType->isString()) {
                error(node.getArgs()[0]->getSpan(), "'File::read' expects string argument, found '" + argType->toString() + "'");
            }
        }
        node.setInferredType(Type::getString());
        return;
    }
    if (node.getCallee() == "File::write" || node.getCallee() == "File::append") {
        if (node.getArgs().size() != 2) {
            error(node.getSpan(), "'" + node.getCallee() + "' expects 2 arguments (path: string, content: string), found " +
                                  std::to_string(node.getArgs().size()));
        } else {
            auto arg0 = node.getArgs()[0]->getInferredType();
            auto arg1 = node.getArgs()[1]->getInferredType();
            if (arg0 && !arg0->isError() && !arg0->isString()) {
                error(node.getArgs()[0]->getSpan(), "'" + node.getCallee() + "' argument 1 expects string, found '" + arg0->toString() + "'");
            }
            if (arg1 && !arg1->isError() && !arg1->isString()) {
                error(node.getArgs()[1]->getSpan(), "'" + node.getCallee() + "' argument 2 expects string, found '" + arg1->toString() + "'");
            }
        }
        node.setInferredType(Type::getBool());
        return;
    }

    if (node.getCallee() == "File::exists" || node.getCallee() == "File::delete") {
        if (node.getArgs().size() != 1) {
            error(node.getSpan(), "'" + node.getCallee() + "' expects 1 argument (path: string), found " +
                                  std::to_string(node.getArgs().size()));
        } else {
            auto argType = node.getArgs()[0]->getInferredType();
            if (argType && !argType->isError() && !argType->isString()) {
                error(node.getArgs()[0]->getSpan(), "'" + node.getCallee() + "' expects string argument, found '" + argType->toString() + "'");
            }
        }
        node.setInferredType(Type::getBool());
        return;
    }

    // Result constructors: Ok(val), Err(err), Result::ok(val), Result::err(err)
    if (node.getCallee() == "Ok" || node.getCallee() == "Result::ok") {
        if (node.getArgs().size() != 1) {
            error(node.getSpan(), "'" + node.getCallee() + "' expects 1 argument, found " +
                                  std::to_string(node.getArgs().size()));
            node.setInferredType(Type::getError());
            return;
        }
        auto valType = node.getArgs()[0]->getInferredType();
        std::shared_ptr<Type> okType = valType ? valType : Type::getError();
        std::shared_ptr<Type> errType = nullptr;

        if (node.getTypeArgs().size() >= 2) {
            okType = resolveType(node.getTypeArgs()[0]);
            errType = resolveType(node.getTypeArgs()[1]);
        } else if (m_expectedType && m_expectedType->isResult()) {
            auto expRes = std::static_pointer_cast<ResultType>(m_expectedType);
            errType = expRes->getErrType();
            if (valType && !valType->isError() && !isAssignable(*valType, *expRes->getOkType())) {
                error(node.getArgs()[0]->getSpan(), "type mismatch in '" + node.getCallee() + "': expected '" +
                                                    expRes->getOkType()->toString() + "', found '" + valType->toString() + "'");
            }
        } else {
            errType = Type::getString();
        }
        if (!okType) okType = Type::getError();
        if (!errType) errType = Type::getString();
        node.setInferredType(ResultType::get(okType, errType));
        return;
    }

    if (node.getCallee() == "Err" || node.getCallee() == "Result::err") {
        if (node.getArgs().size() != 1) {
            error(node.getSpan(), "'" + node.getCallee() + "' expects 1 argument, found " +
                                  std::to_string(node.getArgs().size()));
            node.setInferredType(Type::getError());
            return;
        }
        auto valType = node.getArgs()[0]->getInferredType();
        std::shared_ptr<Type> okType = nullptr;
        std::shared_ptr<Type> errType = valType ? valType : Type::getError();

        if (node.getTypeArgs().size() >= 2) {
            okType = resolveType(node.getTypeArgs()[0]);
            errType = resolveType(node.getTypeArgs()[1]);
        } else if (m_expectedType && m_expectedType->isResult()) {
            auto expRes = std::static_pointer_cast<ResultType>(m_expectedType);
            okType = expRes->getOkType();
            if (valType && !valType->isError() && !isAssignable(*valType, *expRes->getErrType())) {
                error(node.getArgs()[0]->getSpan(), "type mismatch in '" + node.getCallee() + "': expected '" +
                                                    expRes->getErrType()->toString() + "', found '" + valType->toString() + "'");
            }
        } else {
            okType = Type::getVoid();
        }
        if (!okType) okType = Type::getVoid();
        if (!errType) errType = Type::getError();
        node.setInferredType(ResultType::get(okType, errType));
        return;
    }

    // Built-in Process APIs
    if (node.getCallee() == "Process::exit") {
        if (node.getArgs().size() != 1) {
            error(node.getSpan(), "'Process::exit' expects 1 argument (code: i32), found " +
                                  std::to_string(node.getArgs().size()));
        } else {
            auto argType = node.getArgs()[0]->getInferredType();
            if (argType && !argType->isError() && !argType->isI32()) {
                error(node.getArgs()[0]->getSpan(), "'Process::exit' expects i32 argument, found '" + argType->toString() + "'");
            }
        }
        node.setInferredType(Type::getVoid());
        return;
    }

    // Built-in Env APIs
    if (node.getCallee() == "Env::get") {
        if (node.getArgs().size() != 1) {
            error(node.getSpan(), "'Env::get' expects 1 argument (name: string), found " +
                                  std::to_string(node.getArgs().size()));
        } else {
            auto argType = node.getArgs()[0]->getInferredType();
            if (argType && !argType->isError() && !argType->isString()) {
                error(node.getArgs()[0]->getSpan(), "'Env::get' expects string argument, found '" + argType->toString() + "'");
            }
        }
        node.setInferredType(Type::getString());
        return;
    }
    if (node.getCallee() == "Env::set") {
        if (node.getArgs().size() != 2) {
            error(node.getSpan(), "'Env::set' expects 2 arguments (name: string, value: string), found " +
                                  std::to_string(node.getArgs().size()));
        } else {
            auto arg0 = node.getArgs()[0]->getInferredType();
            auto arg1 = node.getArgs()[1]->getInferredType();
            if (arg0 && !arg0->isError() && !arg0->isString()) {
                error(node.getArgs()[0]->getSpan(), "'Env::set' argument 1 expects string, found '" + arg0->toString() + "'");
            }
            if (arg1 && !arg1->isError() && !arg1->isString()) {
                error(node.getArgs()[1]->getSpan(), "'Env::set' argument 2 expects string, found '" + arg1->toString() + "'");
            }
        }
        node.setInferredType(Type::getBool());
        return;
    }

    // Built-in panic handling
    if (node.getCallee() == "panic") {
        if (node.getArgs().size() != 1) {
            error(node.getSpan(), "'panic' expects 1 argument (message: string), found " +
                                  std::to_string(node.getArgs().size()));
        } else {
            auto argType = node.getArgs()[0]->getInferredType();
            if (argType && !argType->isError() && !argType->isString()) {
                error(node.getArgs()[0]->getSpan(), "'panic' expects string argument, found '" + argType->toString() + "'");
            }
        }
        node.setInferredType(Type::getVoid());
        return;
    }

    if (node.getCallee() == "Thread::spawn") {
        if (node.getArgs().size() != 2) {
            error(node.getSpan(), "'Thread::spawn' expects 2 arguments (func, arg), found " +
                                  std::to_string(node.getArgs().size()));
        }
        node.setInferredType(Type::getI64());
        return;
    }

    if (node.getCallee() == "Thread::join") {
        if (node.getArgs().size() != 1) {
            error(node.getSpan(), "'Thread::join' expects 1 argument (thread_handle), found " +
                                  std::to_string(node.getArgs().size()));
        }
        node.setInferredType(Type::getVoid());
        return;
    }

    if (node.getCallee() == "Thread::sleep_ms") {
        if (node.getArgs().size() != 1) {
            error(node.getSpan(), "'Thread::sleep_ms' expects 1 argument (ms: i32), found " +
                                  std::to_string(node.getArgs().size()));
        }
        node.setInferredType(Type::getVoid());
        return;
    }

    if (node.getCallee() == "EventLoop::run") {
        if (!node.getArgs().empty()) {
            error(node.getSpan(), "'EventLoop::run' expects 0 arguments, found " +
                                  std::to_string(node.getArgs().size()));
        }
        node.setInferredType(Type::getVoid());
        return;
    }

    if (node.getCallee() == "EventLoop::yield_now") {
        if (!node.getArgs().empty()) {
            error(node.getSpan(), "'EventLoop::yield_now' expects 0 arguments, found " +
                                  std::to_string(node.getArgs().size()));
        }
        node.setInferredType(Type::getVoid());
        return;
    }

    if (node.getCallee() == "EventLoop::spawn") {
        if (node.getArgs().size() != 2) {
            error(node.getSpan(), "'EventLoop::spawn' expects 2 arguments (func, arg), found " +
                                  std::to_string(node.getArgs().size()));
            node.setInferredType(Type::getError());
            return;
        }
        node.getArgs()[0]->accept(*this);
        node.getArgs()[1]->accept(*this);
        auto fnType = node.getArgs()[0]->getInferredType();
        std::shared_ptr<Type> retType = nullptr;
        if (fnType && fnType->isFunction()) {
            auto funcType = std::static_pointer_cast<FunctionType>(fnType);
            retType = funcType->getReturnType();
        }
        if (retType) {
            if (retType->isPromise()) {
                node.setInferredType(retType);
            } else {
                node.setInferredType(PromiseType::get(retType));
            }
        } else {
            node.setInferredType(PromiseType::get(Type::getI64()));
        }
        return;
    }

    // SIMD feature detection: Simd::has_*
    if (node.getCallee() == "Simd::has_sse" || node.getCallee() == "Simd::has_sse2" ||
        node.getCallee() == "Simd::has_sse3" || node.getCallee() == "Simd::has_sse41" ||
        node.getCallee() == "Simd::has_avx" || node.getCallee() == "Simd::has_avx2" ||
        node.getCallee() == "Simd::has_neon") {
        if (!node.getArgs().empty()) {
            error(node.getSpan(), "'" + node.getCallee() + "' expects 0 arguments, found " +
                                  std::to_string(node.getArgs().size()));
        }
        node.setInferredType(Type::getBool());
        return;
    }

    // SIMD constructors & static methods
    if (node.getCallee() == "f32x4" || node.getCallee() == "f32x4::zero") {
        node.setInferredType(Type::getF32x4());
        return;
    }
    if (node.getCallee() == "f32x4::splat" || node.getCallee() == "f32x4::load") {
        if (node.getArgs().size() != 1) {
            error(node.getSpan(), "'" + node.getCallee() + "' expects 1 argument, found " + std::to_string(node.getArgs().size()));
        }
        node.setInferredType(Type::getF32x4());
        return;
    }
    if (node.getCallee() == "f32x4::store") {
        if (node.getArgs().size() != 2) {
            error(node.getSpan(), "'f32x4::store' expects 2 arguments (ptr, vec), found " + std::to_string(node.getArgs().size()));
        }
        node.setInferredType(Type::getVoid());
        return;
    }

    if (node.getCallee() == "i32x4" || node.getCallee() == "i32x4::zero") {
        node.setInferredType(Type::getI32x4());
        return;
    }
    if (node.getCallee() == "i32x4::splat" || node.getCallee() == "i32x4::load") {
        if (node.getArgs().size() != 1) {
            error(node.getSpan(), "'" + node.getCallee() + "' expects 1 argument, found " + std::to_string(node.getArgs().size()));
        }
        node.setInferredType(Type::getI32x4());
        return;
    }
    if (node.getCallee() == "i32x4::store") {
        if (node.getArgs().size() != 2) {
            error(node.getSpan(), "'i32x4::store' expects 2 arguments (ptr, vec), found " + std::to_string(node.getArgs().size()));
        }
        node.setInferredType(Type::getVoid());
        return;
    }

    if (node.getCallee() == "f64x2" || node.getCallee() == "f64x2::zero") {
        node.setInferredType(Type::getF64x2());
        return;
    }
    if (node.getCallee() == "f64x2::splat" || node.getCallee() == "f64x2::load") {
        if (node.getArgs().size() != 1) {
            error(node.getSpan(), "'" + node.getCallee() + "' expects 1 argument, found " + std::to_string(node.getArgs().size()));
        }
        node.setInferredType(Type::getF64x2());
        return;
    }
    if (node.getCallee() == "f64x2::store") {
        if (node.getArgs().size() != 2) {
            error(node.getSpan(), "'f64x2::store' expects 2 arguments (ptr, vec), found " + std::to_string(node.getArgs().size()));
        }
        node.setInferredType(Type::getVoid());
        return;
    }

    if (node.getCallee() == "i64x2" || node.getCallee() == "i64x2::zero") {
        node.setInferredType(Type::getI64x2());
        return;
    }
    if (node.getCallee() == "i64x2::splat" || node.getCallee() == "i64x2::load") {
        if (node.getArgs().size() != 1) {
            error(node.getSpan(), "'" + node.getCallee() + "' expects 1 argument, found " + std::to_string(node.getArgs().size()));
        }
        node.setInferredType(Type::getI64x2());
        return;
    }
    if (node.getCallee() == "i64x2::store") {
        if (node.getArgs().size() != 2) {
            error(node.getSpan(), "'i64x2::store' expects 2 arguments (ptr, vec), found " + std::to_string(node.getArgs().size()));
        }
        node.setInferredType(Type::getVoid());
        return;
    }

    // Result constructors: Ok(val), Err(err), Result::ok(val), Result::err(err)
    if (node.getCallee() == "Ok" || node.getCallee() == "Result::ok") {
        if (node.getArgs().size() != 1) {
            error(node.getSpan(), "'" + node.getCallee() + "' expects 1 argument, found " +
                                  std::to_string(node.getArgs().size()));
            node.setInferredType(Type::getError());
            return;
        }
        auto valType = node.getArgs()[0]->getInferredType();
        std::shared_ptr<Type> okType = valType ? valType : Type::getError();
        std::shared_ptr<Type> errType = nullptr;

        if (node.getTypeArgs().size() >= 2) {
            okType = resolveType(node.getTypeArgs()[0]);
            errType = resolveType(node.getTypeArgs()[1]);
        } else if (m_expectedType && m_expectedType->isResult()) {
            auto expRes = std::static_pointer_cast<ResultType>(m_expectedType);
            errType = expRes->getErrType();
            if (valType && !valType->isError() && !isAssignable(*valType, *expRes->getOkType())) {
                error(node.getArgs()[0]->getSpan(), "type mismatch in '" + node.getCallee() + "': expected '" +
                                                    expRes->getOkType()->toString() + "', found '" + valType->toString() + "'");
            }
        } else {
            errType = Type::getString();
        }
        if (!okType) okType = Type::getError();
        if (!errType) errType = Type::getString();
        node.setInferredType(ResultType::get(okType, errType));
        return;
    }

    if (node.getCallee() == "Err" || node.getCallee() == "Result::err") {
        if (node.getArgs().size() != 1) {
            error(node.getSpan(), "'" + node.getCallee() + "' expects 1 argument, found " +
                                  std::to_string(node.getArgs().size()));
            node.setInferredType(Type::getError());
            return;
        }
        auto valType = node.getArgs()[0]->getInferredType();
        std::shared_ptr<Type> okType = nullptr;
        std::shared_ptr<Type> errType = valType ? valType : Type::getError();

        if (node.getTypeArgs().size() >= 2) {
            okType = resolveType(node.getTypeArgs()[0]);
            errType = resolveType(node.getTypeArgs()[1]);
        } else if (m_expectedType && m_expectedType->isResult()) {
            auto expRes = std::static_pointer_cast<ResultType>(m_expectedType);
            okType = expRes->getOkType();
            if (valType && !valType->isError() && !isAssignable(*valType, *expRes->getErrType())) {
                error(node.getArgs()[0]->getSpan(), "type mismatch in '" + node.getCallee() + "': expected '" +
                                                    expRes->getErrType()->toString() + "', found '" + valType->toString() + "'");
            }
        } else {
            okType = Type::getVoid();
        }
        if (!okType) okType = Type::getVoid();
        if (!errType) errType = Type::getError();
        node.setInferredType(ResultType::get(okType, errType));
        return;
    }

    // Check if generic function call
    if (m_genericFunctions.count(node.getCallee())) {
        std::vector<std::string> typeArgs = node.getTypeArgs();
        if (typeArgs.empty()) {
            error(node.getSpan(), "generic function '" + node.getCallee() + "' requires explicit type arguments");
            node.setInferredType(Type::getError());
            return;
        }
        instantiateGenericFunction(node.getCallee(), typeArgs, node.getSpan());
        std::string mangledName = getMangledFunctionName(node.getCallee(), typeArgs);
        node.setCallee(mangledName);
    }

    const Symbol* sym = m_globalScope.lookup(node.getCallee());
    if (!sym) {
        error(node.getSpan(), "call to undeclared function '" + node.getCallee() + "'");
        node.setInferredType(Type::getError());
        return;
    }

    const auto* fnType = dynamic_cast<const FunctionType*>(sym->type.get());
    if (!fnType) {
        error(node.getSpan(), "'" + node.getCallee() + "' is not a function");
        node.setInferredType(Type::getError());
        return;
    }

    const auto& paramTypes = fnType->getParamTypes();
    if (node.getArgs().size() != paramTypes.size()) {
        error(node.getSpan(), "function '" + node.getCallee() + "' expects " +
                              std::to_string(paramTypes.size()) + " arguments, found " +
                              std::to_string(node.getArgs().size()));
        node.setInferredType(fnType->getReturnType());
        return;
    }

    auto fnDeclIt = m_functionDecls.find(node.getCallee());
    const FunctionDecl* targetFnDecl = (fnDeclIt != m_functionDecls.end()) ? fnDeclIt->second : nullptr;

    for (size_t i = 0; i < paramTypes.size(); ++i) {
        if (targetFnDecl && i < targetFnDecl->getParams().size() && targetFnDecl->getParams()[i].isByRef) {
            const auto* argExpr = node.getArgs()[i].get();
            bool isLValue = dynamic_cast<const VariableExpr*>(argExpr) != nullptr ||
                            dynamic_cast<const MemberAccessExpr*>(argExpr) != nullptr ||
                            dynamic_cast<const IndexExpr*>(argExpr) != nullptr;
            if (!isLValue) {
                if (const auto* unary = dynamic_cast<const UnaryExpr*>(argExpr)) {
                    if (unary->getOp() == UnaryOp::Deref) {
                        isLValue = true;
                    }
                }
            }
            if (!isLValue) {
                error(argExpr->getSpan(), "cannot pass non-lvalue expression by reference to parameter '" +
                                          targetFnDecl->getParams()[i].name + "'");
            }
        }

        auto argType = node.getArgs()[i]->getInferredType();
        if (argType && !argType->isError() && !isAssignable(*argType, *paramTypes[i])) {
            error(node.getArgs()[i]->getSpan(), "argument type mismatch in call to '" + node.getCallee() +
                                                "': expected '" + paramTypes[i]->toString() +
                                                "', found '" + argType->toString() + "'");
        } else if (argType && argType->isMoveOnly()) {
            if (auto* varExpr = dynamic_cast<VariableExpr*>(node.getArgs()[i].get())) {
                Symbol* argSym = m_currentScope->lookup(varExpr->getName());
                if (argSym) {
                    argSym->isMoved = true;
                }
            }
        }
    }

    node.setInferredType(fnType->getReturnType());
}

void SemanticAnalyzer::visit(ThisExpr& node) {
    if (!m_currentClass) {
        error(node.getSpan(), "'this' cannot be used outside of a class method");
        node.setInferredType(Type::getError());
        return;
    }
    node.setInferredType(m_currentClass);
}

void SemanticAnalyzer::visit(MethodCallExpr& node) {
    node.getReceiver().accept(*this);
    auto recvType = node.getReceiver().getInferredType();
    if (!recvType || recvType->isError()) {
        node.setInferredType(Type::getError());
        return;
    }

    if (recvType->isInterface()) {
        auto ifaceType = std::static_pointer_cast<InterfaceType>(recvType);
        const auto* methodInfo = ifaceType->findMethod(node.getMethodName());
        if (!methodInfo) {
            error(node.getSpan(), "interface '" + ifaceType->getName() + "' has no method named '" + node.getMethodName() + "'");
            node.setInferredType(Type::getError());
            return;
        }

        const auto& expectedParamTypes = methodInfo->paramTypes;
        if (node.getArgs().size() != expectedParamTypes.size()) {
            error(node.getSpan(), "method '" + node.getMethodName() + "' expects " +
                                  std::to_string(expectedParamTypes.size()) + " argument(s), but got " +
                                  std::to_string(node.getArgs().size()));
        }

        for (size_t i = 0; i < node.getArgs().size(); ++i) {
            node.getArgs()[i]->accept(*this);
            auto argType = node.getArgs()[i]->getInferredType();

            if (i < expectedParamTypes.size()) {
                auto expectedType = expectedParamTypes[i];
                if (argType && !argType->isError() && expectedType && !expectedType->isError() &&
                    !isAssignable(*argType, *expectedType)) {
                    error(node.getArgs()[i]->getSpan(), "argument type mismatch in call to method '" +
                                                        node.getMethodName() + "': expected '" +
                                                        expectedType->toString() + "', but got '" +
                                                        argType->toString() + "'");
                } else if (argType && argType->isMoveOnly()) {
                    if (auto* varExpr = dynamic_cast<VariableExpr*>(node.getArgs()[i].get())) {
                        Symbol* argSym = m_currentScope->lookup(varExpr->getName());
                        if (argSym) {
                            argSym->isMoved = true;
                        }
                    }
                }
            }
        }

        node.setInferredType(methodInfo->returnType);
        return;
    }

    if (recvType->isArena()) {
        if (node.getMethodName() == "reset") {
            if (!node.getArgs().empty()) {
                error(node.getSpan(), "method 'reset' on Arena expects 0 arguments");
            }
            node.setInferredType(Type::getVoid());
            return;
        }
        if (node.getMethodName() == "destroy") {
            if (!node.getArgs().empty()) {
                error(node.getSpan(), "method 'destroy' on Arena expects 0 arguments");
            }
            if (auto* varExpr = dynamic_cast<VariableExpr*>(&node.getReceiver())) {
                Symbol* sym = m_currentScope->lookup(varExpr->getName());
                if (sym) {
                    sym->isMoved = true;
                }
            }
            node.setInferredType(Type::getVoid());
            return;
        }
        if (node.getMethodName() == "alloc") {
            if (node.getArgs().size() != 1) {
                error(node.getSpan(), "method 'alloc' on Arena expects 1 argument");
                node.setInferredType(Type::getError());
                return;
            }
            node.getArgs()[0]->accept(*this);
            auto argType = node.getArgs()[0]->getInferredType();
            if (!argType || argType->isError()) {
                node.setInferredType(Type::getError());
                return;
            }
            node.setInferredType(PointerType::get(argType));
            return;
        }
        error(node.getSpan(), "Arena has no method named '" + node.getMethodName() + "'");
        node.setInferredType(Type::getError());
        return;
    }

    if (recvType->isString()) {
        if (node.getMethodName() == "len") {
            if (!node.getArgs().empty()) {
                error(node.getSpan(), "method 'len' on string expects 0 arguments");
            }
            node.setInferredType(Type::getI64());
            return;
        }
        error(node.getSpan(), "string has no method named '" + node.getMethodName() + "'");
        node.setInferredType(Type::getError());
        return;
    }

    if (recvType->isVector()) {
        auto vecType = std::static_pointer_cast<VectorType>(recvType);
        auto elemType = vecType->getElementType();

        if (node.getMethodName() == "push") {
            if (node.getArgs().size() != 1) {
                error(node.getSpan(), "method 'push' on Vec expects 1 argument");
                node.setInferredType(Type::getError());
                return;
            }
            node.getArgs()[0]->accept(*this);
            auto argType = node.getArgs()[0]->getInferredType();
            if (argType && !argType->isError() && elemType && !isAssignable(*argType, *elemType)) {
                error(node.getArgs()[0]->getSpan(), "type mismatch in Vec::push: expected '" + elemType->toString() + "', found '" + argType->toString() + "'");
            }
            node.setInferredType(Type::getVoid());
            return;
        }
        if (node.getMethodName() == "pop") {
            if (!node.getArgs().empty()) {
                error(node.getSpan(), "method 'pop' on Vec expects 0 arguments");
            }
            node.setInferredType(elemType ? elemType : Type::getError());
            return;
        }
        if (node.getMethodName() == "len") {
            if (!node.getArgs().empty()) {
                error(node.getSpan(), "method 'len' on Vec expects 0 arguments");
            }
            node.setInferredType(Type::getI64());
            return;
        }
        if (node.getMethodName() == "capacity") {
            if (!node.getArgs().empty()) {
                error(node.getSpan(), "method 'capacity' on Vec expects 0 arguments");
            }
            node.setInferredType(Type::getI64());
            return;
        }
        if (node.getMethodName() == "destroy") {
            if (!node.getArgs().empty()) {
                error(node.getSpan(), "method 'destroy' on Vec expects 0 arguments");
            }
            if (auto* varExpr = dynamic_cast<VariableExpr*>(&node.getReceiver())) {
                Symbol* sym = m_currentScope->lookup(varExpr->getName());
                if (sym) {
                    sym->isMoved = true;
                }
            }
            node.setInferredType(Type::getVoid());
            return;
        }
        error(node.getSpan(), "Vec has no method named '" + node.getMethodName() + "'");
        node.setInferredType(Type::getError());
        return;
    }

    if (recvType->isResult()) {
        auto resType = std::static_pointer_cast<ResultType>(recvType);
        if (node.getMethodName() == "is_ok" || node.getMethodName() == "is_err") {
            if (!node.getArgs().empty()) {
                error(node.getSpan(), "method '" + node.getMethodName() + "' on Result expects 0 arguments");
            }
            node.setInferredType(Type::getBool());
            return;
        }
        if (node.getMethodName() == "unwrap") {
            if (!node.getArgs().empty()) {
                error(node.getSpan(), "method 'unwrap' on Result expects 0 arguments");
            }
            node.setInferredType(resType->getOkType());
            return;
        }
        if (node.getMethodName() == "destroy") {
            if (!node.getArgs().empty()) {
                error(node.getSpan(), "method 'destroy' on Result expects 0 arguments");
            }
            if (auto* varExpr = dynamic_cast<VariableExpr*>(&node.getReceiver())) {
                Symbol* sym = m_currentScope->lookup(varExpr->getName());
                if (sym) {
                    sym->isMoved = true;
                }
            }
            node.setInferredType(Type::getVoid());
            return;
        }
        error(node.getSpan(), "Result has no method named '" + node.getMethodName() + "'");
        node.setInferredType(Type::getError());
        return;
    }

    if (recvType->isDynamic()) {
        for (auto& arg : node.getArgs()) {
            arg->accept(*this);
        }
        node.setInferredType(Type::getDynamic());
        return;
    }

    if (recvType->isPointer()) {
        auto ptrType = std::static_pointer_cast<PointerType>(recvType);
        if (ptrType->getPointeeType() && ptrType->getPointeeType()->isClass()) {
            recvType = ptrType->getPointeeType();
        }
    }

    if (!recvType->isClass()) {
        error(node.getSpan(), "cannot call method '" + node.getMethodName() + "' on non-class/non-interface type '" + recvType->toString() + "'");
        node.setInferredType(Type::getError());
        return;
    }

    auto classType = std::static_pointer_cast<ClassType>(recvType);
    const auto* methodInfo = classType->findMethod(node.getMethodName());
    if (!methodInfo) {
        error(node.getSpan(), "class '" + classType->getName() + "' has no method named '" + node.getMethodName() + "'");
        node.setInferredType(Type::getError());
        return;
    }

    // Check argument count
    const auto& expectedParamTypes = methodInfo->paramTypes;
    if (classType->getName() == "AtomicI32" || classType->getName() == "AtomicI64" || classType->getName() == "AtomicBool") {
        for (auto& arg : node.getArgs()) {
            arg->accept(*this);
        }
        const std::string& mName = node.getMethodName();
        if (mName == "load") {
            if (node.getArgs().size() > 1) {
                error(node.getSpan(), "method 'load' expects 0 or 1 argument(s), but got " + std::to_string(node.getArgs().size()));
            }
        } else if (mName == "store" || mName == "fetch_add" || mName == "fetch_sub" || mName == "exchange") {
            if (node.getArgs().size() < 1 || node.getArgs().size() > 2) {
                error(node.getSpan(), "method '" + mName + "' expects 1 or 2 argument(s), but got " + std::to_string(node.getArgs().size()));
            }
        } else if (mName == "compare_exchange") {
            if (node.getArgs().size() < 2 || node.getArgs().size() > 4) {
                error(node.getSpan(), "method 'compare_exchange' expects 2, 3, or 4 argument(s), but got " + std::to_string(node.getArgs().size()));
            }
        }
        node.setInferredType(methodInfo->returnType);
        return;
    } else if (classType->getName() == "Promise") {
        for (auto& arg : node.getArgs()) {
            arg->accept(*this);
        }
        const std::string& mName = node.getMethodName();
        if (mName == "resolve" || mName == "reject") {
            if (node.getArgs().size() != 1) {
                error(node.getSpan(), "method '" + mName + "' expects 1 argument, but got " + std::to_string(node.getArgs().size()));
            }
            node.setInferredType(Type::getVoid());
            return;
        } else if (mName == "is_ready") {
            if (!node.getArgs().empty()) {
                error(node.getSpan(), "method 'is_ready' expects 0 arguments, but got " + std::to_string(node.getArgs().size()));
            }
            node.setInferredType(Type::getBool());
            return;
        } else if (mName == "get_value" || mName == "await") {
            if (!node.getArgs().empty()) {
                error(node.getSpan(), "method '" + mName + "' expects 0 arguments, but got " + std::to_string(node.getArgs().size()));
            }
            const auto* pt = dynamic_cast<const PromiseType*>(classType.get());
            if (pt && pt->getInnerType()) {
                node.setInferredType(pt->getInnerType());
            } else {
                node.setInferredType(Type::getI64());
            }
            return;
        }
    } else if (classType->isSimd()) {
        for (auto& arg : node.getArgs()) {
            arg->accept(*this);
        }
        node.setInferredType(methodInfo->returnType);
        return;
    } else if (classType->getName() == "PDOStatement") {
        if (node.getMethodName() == "execute" || node.getMethodName() == "fetch" || node.getMethodName() == "fetchAll") {
            if (node.getArgs().size() > 1) {
                error(node.getSpan(), "method '" + node.getMethodName() + "' expects 0 or 1 argument(s), but got " + std::to_string(node.getArgs().size()));
            }
        } else if (node.getArgs().size() != expectedParamTypes.size()) {
            error(node.getSpan(), "method '" + node.getMethodName() + "' expects " +
                                  std::to_string(expectedParamTypes.size()) + " argument(s), but got " +
                                  std::to_string(node.getArgs().size()));
        }
        node.setInferredType(methodInfo->returnType);
        return;
    } else if (classType->getName() == "PDO") {
        if (node.getMethodName() == "lastInsertId") {
            if (node.getArgs().size() > 1) {
                error(node.getSpan(), "method '" + node.getMethodName() + "' expects 0 or 1 argument(s), but got " + std::to_string(node.getArgs().size()));
            }
        } else if (node.getArgs().size() != expectedParamTypes.size()) {
            error(node.getSpan(), "method '" + node.getMethodName() + "' expects " +
                                  std::to_string(expectedParamTypes.size()) + " argument(s), but got " +
                                  std::to_string(node.getArgs().size()));
        }
        node.setInferredType(methodInfo->returnType);
        return;
    } else if (node.getArgs().size() != expectedParamTypes.size()) {
        error(node.getSpan(), "method '" + node.getMethodName() + "' expects " +
                              std::to_string(expectedParamTypes.size()) + " argument(s), but got " +
                              std::to_string(node.getArgs().size()));
    }

    // Check argument types
    for (size_t i = 0; i < node.getArgs().size(); ++i) {
        node.getArgs()[i]->accept(*this);
        auto argType = node.getArgs()[i]->getInferredType();

        if (i < expectedParamTypes.size()) {
            auto expectedType = expectedParamTypes[i];
            if (argType && !argType->isError() && expectedType && !expectedType->isError() &&
                !isAssignable(*argType, *expectedType)) {
                error(node.getArgs()[i]->getSpan(), "argument type mismatch in call to method '" +
                                                    node.getMethodName() + "': expected '" +
                                                    expectedType->toString() + "', but got '" +
                                                    argType->toString() + "'");
            } else if (argType && argType->isMoveOnly()) {
                if (auto* varExpr = dynamic_cast<VariableExpr*>(node.getArgs()[i].get())) {
                    Symbol* argSym = m_currentScope->lookup(varExpr->getName());
                    if (argSym) {
                        argSym->isMoved = true;
                    }
                }
            }
        }
    }

    // Mutability check: if method mutates 'this', receiver must be mutable!
    if (methodInfo->isMutating) {
        const ExprAST* root = &node.getReceiver();
        while (const auto* innerMem = dynamic_cast<const MemberAccessExpr*>(root)) {
            root = &innerMem->getBase();
        }

        if (const auto* rootVar = dynamic_cast<const VariableExpr*>(root)) {
            if (rootVar->getName() == "$this" || rootVar->getName() == "this") {
                m_currentMethodMutatesThis = true;
            } else {
                const Symbol* rootSym = m_currentScope->lookup(rootVar->getName());
                if (rootSym && !rootSym->isMut) {
                    error(node.getSpan(), "cannot call mutating method '" + node.getMethodName() +
                                          "' on immutable binding '" + rootVar->getName() +
                                          "'; declare it with 'mut' to allow mutation");
                }
            }
        } else if (dynamic_cast<const ThisExpr*>(root)) {
            // Calling a mutating method on 'this' makes the current method mutating too!
            m_currentMethodMutatesThis = true;
        }
    }

    node.setInferredType(methodInfo->returnType);
}

void SemanticAnalyzer::visit(NewExpr& node) {
    std::string className = node.getClassName();
    if (className == "Atomic<i32>") className = "AtomicI32";
    else if (className == "Atomic<i64>") className = "AtomicI64";
    else if (className == "Atomic<bool>") className = "AtomicBool";

    if (className == "AtomicI32" || className == "AtomicI64" || className == "AtomicBool") {
        if (node.getArgs().size() > 1) {
            error(node.getSpan(), "'" + className + "' constructor expects 0 or 1 argument, found " + std::to_string(node.getArgs().size()));
        }
        for (auto& arg : node.getArgs()) {
            arg->accept(*this);
        }
        const Symbol* sym = m_globalScope.lookup(className);
        if (sym && sym->type) {
            node.setInferredType(sym->type);
        } else {
            node.setInferredType(Type::getError());
        }
        return;
    }

    if (className == "Promise" || (className.rfind("Promise<", 0) == 0 && className.back() == '>')) {
        for (auto& arg : node.getArgs()) {
            arg->accept(*this);
        }
        std::shared_ptr<Type> pType = resolveType(className);
        node.setInferredType(pType ? pType : PromiseType::get(Type::getI64()));
        return;
    }

    if (className == "f32x4" || className == "i32x4" || className == "f64x2" || className == "i64x2") {
        for (auto& arg : node.getArgs()) {
            arg->accept(*this);
        }
        if (className == "f32x4") node.setInferredType(Type::getF32x4());
        else if (className == "i32x4") node.setInferredType(Type::getI32x4());
        else if (className == "f64x2") node.setInferredType(Type::getF64x2());
        else if (className == "i64x2") node.setInferredType(Type::getI64x2());
        return;
    }

    if (node.getClassName() == "PDO") {
        if (node.getArgs().empty() || node.getArgs().size() > 4) {
            error(node.getSpan(), "'PDO' constructor expects 1 to 4 arguments, found " + std::to_string(node.getArgs().size()));
        }
        for (auto& arg : node.getArgs()) {
            arg->accept(*this);
        }
        node.setInferredType(Type::getDynamic());
        return;
    }

    const Symbol* sym = m_currentScope->lookup(node.getClassName());
    if (!sym || !sym->type || !sym->type->isClass()) {
        error(node.getSpan(), "unknown class '" + node.getClassName() + "'");
        node.setInferredType(Type::getError());
        return;
    }

    auto classType = std::static_pointer_cast<ClassType>(sym->type);

    // Look for __construct or ClassName method
    const auto* ctor = classType->findMethod("__construct");
    if (!ctor) {
        ctor = classType->findMethod(node.getClassName());
    }

    if (ctor) {
        const auto& expectedParamTypes = ctor->paramTypes;
        if (node.getArgs().size() != expectedParamTypes.size()) {
            error(node.getSpan(), "constructor for class '" + node.getClassName() + "' expects " +
                                  std::to_string(expectedParamTypes.size()) + " argument(s), but got " +
                                  std::to_string(node.getArgs().size()));
        }

        for (size_t i = 0; i < node.getArgs().size(); ++i) {
            node.getArgs()[i]->accept(*this);
            auto argType = node.getArgs()[i]->getInferredType();

            if (i < expectedParamTypes.size()) {
                auto expectedType = expectedParamTypes[i];
                if (argType && !argType->isError() && expectedType && !expectedType->isError() &&
                    !isAssignable(*argType, *expectedType)) {
                    error(node.getArgs()[i]->getSpan(), "argument type mismatch in constructor for '" +
                                                        node.getClassName() + "': expected '" +
                                                        expectedType->toString() + "', but got '" +
                                                        argType->toString() + "'");
                } else if (argType && argType->isMoveOnly()) {
                    if (auto* varExpr = dynamic_cast<VariableExpr*>(node.getArgs()[i].get())) {
                        Symbol* argSym = m_currentScope->lookup(varExpr->getName());
                        if (argSym) {
                            argSym->isMoved = true;
                        }
                    }
                }
            }
        }
    } else {
        if (!node.getArgs().empty()) {
            error(node.getSpan(), "class '" + node.getClassName() + "' does not define a constructor taking arguments");
        }
    }

    node.setInferredType(classType);
}

void SemanticAnalyzer::visit(AwaitExpr& node) {
    node.getExpr().accept(*this);
    auto exprType = node.getExpr().getInferredType();
    if (!exprType) {
        node.setInferredType(Type::getDynamic());
        return;
    }

    if (exprType->isDynamic()) {
        node.setInferredType(Type::getDynamic());
        return;
    }

    if (exprType->isPromise()) {
        const auto* pt = dynamic_cast<const PromiseType*>(exprType.get());
        if (pt && pt->getInnerType()) {
            node.setInferredType(pt->getInnerType());
        } else {
            node.setInferredType(Type::getI64());
        }
        return;
    }

    error(node.getSpan(), "'await' expects expression of type 'Promise' or 'dynamic', found '" + exprType->toString() + "'");
    node.setInferredType(Type::getError());
}

} // namespace sph

