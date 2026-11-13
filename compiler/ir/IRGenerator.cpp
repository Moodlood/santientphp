#include "IRGenerator.hpp"
#include "compiler/source/SourceManager.hpp"

namespace sph {

namespace {

struct LocGuard {
    IRGenerator& gen;
    SourceLocation prev;
    LocGuard(IRGenerator& g, SourceLocation loc) : gen(g), prev(g.getCurrentLoc()) {
        if (loc.isValid()) gen.setCurrentLoc(loc);
    }
    ~LocGuard() { gen.setCurrentLoc(prev); }
};

} // namespace

void IRGenerator::emitInst(std::shared_ptr<IRInstruction> inst) {
    if (inst && m_currentBlock) {
        if (!inst->getLocation().isValid()) {
            inst->setLocation(m_currentLoc);
        }
        m_currentBlock->addInstruction(std::move(inst));
    }
}

std::string IRGenerator::nextTemp() {
    return "%t" + std::to_string(m_tempCounter++);
}

void IRGenerator::pushScope() {
    m_variableScopes.emplace_back();
    m_activeBoxes.emplace_back();
    m_deferredStmts.emplace_back();
}

void IRGenerator::executeDeferredInCurrentScope() {
    if (!m_deferredStmts.empty()) {
        auto stmts = std::move(m_deferredStmts.back());
        m_deferredStmts.back().clear();
        for (auto it = stmts.rbegin(); it != stmts.rend(); ++it) {
            if (m_currentBlock && !m_currentBlock->hasTerminator()) {
                (*it)->accept(*this);
            }
        }
    }
}

void IRGenerator::executeDeferredAcrossAllScopes() {
    for (auto sIt = m_deferredStmts.rbegin(); sIt != m_deferredStmts.rend(); ++sIt) {
        auto stmts = *sIt;
        for (auto it = stmts.rbegin(); it != stmts.rend(); ++it) {
            if (m_currentBlock && !m_currentBlock->hasTerminator()) {
                (*it)->accept(*this);
            }
        }
    }
}

void IRGenerator::popScope() {
    if (!m_variableScopes.empty()) {
        if (m_currentBlock && !m_currentBlock->hasTerminator()) {
            executeDeferredInCurrentScope();
            for (const auto& [name, slot] : m_variableScopes.back()) {
                if (slot->getType()->isRc()) {
                    auto loadVal = std::make_shared<LoadInst>(slot, nextTemp());
                    emitInst(loadVal);
                    emitInst(std::make_shared<RcReleaseInst>(loadVal));
                }
            }
            if (!m_activeBoxes.empty()) {
                for (const auto& name : m_activeBoxes.back()) {
                    auto slot = getVariable(name);
                    if (slot) {
                        auto loadVal = std::make_shared<LoadInst>(slot, nextTemp());
                        emitInst(loadVal);
                        emitInst(std::make_shared<CallInst>("sph_free", std::vector<std::shared_ptr<IRValue>>{loadVal}, Type::getVoid(), nextTemp()));
                    }
                }
            }
        }
        m_variableScopes.pop_back();
        if (!m_activeBoxes.empty()) {
            m_activeBoxes.pop_back();
        }
        if (!m_deferredStmts.empty()) {
            m_deferredStmts.pop_back();
        }
    }
}

void IRGenerator::setVariable(const std::string& name, std::shared_ptr<IRValue> slot) {
    if (!m_variableScopes.empty()) {
        m_variableScopes.back()[name] = std::move(slot);
    }
}

std::shared_ptr<IRValue> IRGenerator::getVariable(const std::string& name) const {
    for (auto it = m_variableScopes.rbegin(); it != m_variableScopes.rend(); ++it) {
        auto found = it->find(name);
        if (found != it->end()) {
            return found->second;
        }
    }
    return nullptr;
}

std::shared_ptr<Type> IRGenerator::resolveType(const std::string& rawName) const {
    std::string name = rawName;
    if (name == "Atomic<i32>") name = "AtomicI32";
    else if (name == "Atomic<i64>") name = "AtomicI64";
    else if (name == "Atomic<bool>") name = "AtomicBool";

    if (name == "Promise") {
        return PromiseType::get(Type::getI64());
    }
    if (name.rfind("Promise<", 0) == 0 && name.back() == '>') {
        std::string innerName = name.substr(8, name.size() - 9);
        auto innerType = resolveType(innerName);
        if (!innerType) innerType = Type::getI64();
        return PromiseType::get(innerType);
    }

    if (auto t = Type::fromString(name)) {
        return t;
    }

    // Check if array type: e.g. [i32; 4]
    if (name.size() >= 5 && name.front() == '[' && name.back() == ']') {
        std::string inner = name.substr(1, name.size() - 2);
        auto semiPos = inner.find(';');
        if (semiPos != std::string::npos) {
            std::string elemStr = inner.substr(0, semiPos);
            std::string countStr = inner.substr(semiPos + 1);

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
                    return Type::getVoid();
                }
            }
        }
    }

    // Check if pointer type: e.g. *i32, *Point, *void
    if (name.size() >= 2 && name.front() == '*') {
        std::string inner = name.substr(1);
        while (!inner.empty() && (inner.front() == ' ' || inner.front() == '\t')) inner.erase(0, 1);
        while (!inner.empty() && (inner.back() == ' ' || inner.back() == '\t')) inner.pop_back();
        auto elemType = resolveType(inner);
        if (elemType) {
            return PointerType::get(elemType);
        }
        return Type::getVoid();
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
        return Type::getVoid();
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
        return Type::getVoid();
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
        return Type::getVoid();
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
        return Type::getVoid();
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
        return Type::getVoid();
    }

    // Check if generic type: e.g. Pair<i32, i32>
    auto anglePos = name.find('<');
    if (anglePos != std::string::npos && name.back() == '>') {
        std::string mangled = name.substr(0, anglePos);
        std::string argsStr = name.substr(anglePos + 1, name.size() - anglePos - 2);
        int depth = 0;
        size_t lastStart = 0;
        for (size_t i = 0; i < argsStr.size(); ++i) {
            if (argsStr[i] == '<' || argsStr[i] == '[') depth++;
            else if (argsStr[i] == '>' || argsStr[i] == ']') depth--;
            else if (argsStr[i] == ',' && depth == 0) {
                std::string arg = argsStr.substr(lastStart, i - lastStart);
                while (!arg.empty() && (arg.front() == ' ' || arg.front() == '\t')) arg.erase(0, 1);
                while (!arg.empty() && (arg.back() == ' ' || arg.back() == '\t')) arg.pop_back();
                if (!arg.empty()) mangled += "_" + arg;
                lastStart = i + 1;
            }
        }
        std::string lastArg = argsStr.substr(lastStart);
        while (!lastArg.empty() && (lastArg.front() == ' ' || lastArg.front() == '\t')) lastArg.erase(0, 1);
        while (!lastArg.empty() && (lastArg.back() == ' ' || lastArg.back() == '\t')) lastArg.pop_back();
        if (!lastArg.empty()) mangled += "_" + lastArg;

        auto it = m_types.find(mangled);
        if (it != m_types.end()) {
            return it->second;
        }
    }

    auto it = m_types.find(name);
    if (it != m_types.end()) {
        return it->second;
    }
    return Type::getVoid();
}

std::unique_ptr<IRModule> IRGenerator::generate(ProgramAST& program, const std::string& moduleName, const SourceManager* sm) {
    m_module = std::make_unique<IRModule>(moduleName);
    if (sm) {
        for (uint32_t i = 1; i <= sm->getFileCount(); ++i) {
            const auto* sf = sm->getFile(i);
            if (sf) {
                m_module->addSourceFile(sf->getId(), sf->getFilename());
            }
        }
    }
    m_types.clear();
    m_functionParamTypes.clear();
    m_functionParamByRef.clear();
    m_asyncFunctions.clear();
    m_activeExceptionFrames.clear();
    m_enumVariants.clear();

    m_types["Promise"] = PromiseType::get(Type::getI64());
    m_types["f32x4"] = Type::getF32x4();
    m_types["i32x4"] = Type::getI32x4();
    m_types["f64x2"] = Type::getF64x2();
    m_types["i64x2"] = Type::getI64x2();

    m_enumVariants["MemoryOrder"]["Relaxed"] = 0;
    m_enumVariants["MemoryOrder"]["Acquire"] = 1;
    m_enumVariants["MemoryOrder"]["Release"] = 2;
    m_enumVariants["MemoryOrder"]["AcqRel"] = 3;
    m_enumVariants["MemoryOrder"]["SeqCst"] = 4;

    {
        std::vector<StructMember> fields = {{"value", Type::getI32(), 0}};
        m_types["AtomicI32"] = ClassType::get("AtomicI32", std::move(fields));
    }
    {
        std::vector<StructMember> fields = {{"value", Type::getI64(), 0}};
        m_types["AtomicI64"] = ClassType::get("AtomicI64", std::move(fields));
    }
    {
        std::vector<StructMember> fields = {{"value", Type::getBool(), 0}};
        m_types["AtomicBool"] = ClassType::get("AtomicBool", std::move(fields));
    }

    for (const auto& en : program.getEnums()) {
        std::vector<EnumVariantInfo> variants;
        int64_t nextVal = 0;
        for (const auto& v : en->getVariants()) {
            int64_t actualVal = v.explicitValue.value_or(nextVal);
            variants.push_back(EnumVariantInfo{v.name, actualVal});
            m_enumVariants[en->getName()][v.name] = actualVal;
            nextVal = actualVal + 1;
        }
        m_types[en->getName()] = EnumType::get(en->getName(), std::move(variants));
    }

    for (const auto& st : program.getStructs()) {
        if (st->isGeneric()) continue;
        m_types[st->getName()] = StructType::get(st->getName(), {});
    }

    for (const auto& st : program.getStructs()) {
        if (st->isGeneric()) continue;
        std::vector<StructMember> members;
        for (const auto& f : st->getFields()) {
            auto fType = resolveType(f.type);
            members.push_back(StructMember{f.name, fType, 0});
        }
        m_types[st->getName()] = StructType::get(st->getName(), std::move(members), st->isPacked(), st->getAttribute().explicitAlignment);
    }

    for (const auto& iface : program.getInterfaces()) {
        std::vector<InterfaceMethodInfo> methods;
        for (const auto& m : iface->getMethods()) {
            auto retType = resolveType(m.returnType);
            std::vector<std::shared_ptr<Type>> paramTypes;
            for (const auto& p : m.params) {
                paramTypes.push_back(resolveType(p.type));
            }
            methods.push_back(InterfaceMethodInfo{m.name, retType, std::move(paramTypes)});
        }
        m_types[iface->getName()] = InterfaceType::get(iface->getName(), std::move(methods));
    }

    for (const auto& cl : program.getClasses()) {
        m_types[cl->getName()] = ClassType::get(cl->getName(), {});
    }

    for (const auto& cl : program.getClasses()) {
        std::vector<StructMember> fields;
        for (const auto& f : cl->getFields()) {
            auto fType = resolveType(f.type);
            fields.push_back(StructMember{f.name, fType, 0});
        }
        auto classType = ClassType::get(cl->getName(), std::move(fields));
        for (const auto& ifaceName : cl->getImplements()) {
            classType->addImplements(ifaceName);
        }
        for (const auto& m : cl->getMethods()) {
            auto retType = resolveType(m->getReturnType());
            std::vector<std::shared_ptr<Type>> paramTypes;
            for (const auto& p : m->getParams()) {
                paramTypes.push_back(resolveType(p.type));
            }
            classType->addMethod(m->getName(), retType, paramTypes, false);
        }
        m_types[cl->getName()] = classType;
    }

    for (const auto& cl : program.getClasses()) {
        for (const auto& ifaceName : cl->getImplements()) {
            auto it = m_types.find(ifaceName);
            if (it != m_types.end() && it->second->isInterface()) {
                auto ifaceType = std::static_pointer_cast<InterfaceType>(it->second);
                IRVTable vt;
                vt.name = cl->getName() + "_" + ifaceName + "_vtable";
                for (const auto& m : ifaceType->getMethods()) {
                    vt.functionNames.push_back(cl->getName() + "_" + m.name);
                }
                m_module->addVTable(std::move(vt));
            }
        }
    }

    for (const auto& fn : program.getFunctions()) {
        if (fn->isGeneric()) continue;
        if (fn->isAsync()) {
            m_asyncFunctions.insert(fn->getName());
        }
        std::vector<std::shared_ptr<Type>> pTypes;
        std::vector<bool> byRefFlags;
        for (const auto& p : fn->getParams()) {
            pTypes.push_back(resolveType(p.type));
            byRefFlags.push_back(p.isByRef);
        }
        m_functionParamTypes[fn->getName()] = std::move(pTypes);
        m_functionParamByRef[fn->getName()] = std::move(byRefFlags);
    }

    program.accept(*this);
    return std::move(m_module);
}

void IRGenerator::visit(ProgramAST& node) {
    for (auto& cl : node.getClasses()) {
        cl->accept(*this);
    }
    for (auto& fn : node.getFunctions()) {
        if (fn->isGeneric()) continue;
        fn->accept(*this);
    }
}

void IRGenerator::visit(FunctionDecl& node) {
    if (node.isGeneric()) return;
    m_tempCounter = 0;
    auto returnType = resolveType(node.getReturnType());
    if (!returnType) returnType = Type::getVoid();
    if (node.isAsync()) {
        const auto* pt = dynamic_cast<const PromiseType*>(returnType.get());
        if (pt && pt->getInnerType()) {
            returnType = pt->getInnerType();
        } else {
            returnType = Type::getI64();
        }
    }

    std::vector<std::shared_ptr<IRParameter>> params;
    for (size_t i = 0; i < node.getParams().size(); ++i) {
        const auto& p = node.getParams()[i];
        auto pType = resolveType(p.type);
        if (p.isByRef) {
            pType = PointerType::get(pType);
        }
        params.push_back(std::make_shared<IRParameter>(pType, "%arg_" + p.name, i));
    }

    auto irFn = std::make_unique<IRFunction>(node.getName(), returnType, params);
    irFn->setLocation(node.getSpan().start);
    if (node.isExtern()) {
        m_module->addFunction(std::move(irFn));
        return;
    }
    m_currentFunction = irFn.get();
    m_module->addFunction(std::move(irFn));

    m_currentBlock = m_currentFunction->createBlock("entry");

    pushScope();

    // Allocate stack slots for each parameter and copy parameter value into slot
    for (size_t i = 0; i < params.size(); ++i) {
        const auto& param = params[i];
        const std::string& origName = node.getParams()[i].name;

        if (node.getParams()[i].isByRef) {
            m_byRefVars.insert(origName);
            setVariable(origName, param);
        } else if (param->getType()->isRc()) {
            auto allocaInst = std::make_shared<AllocaInst>(param->getType(), "%var_" + origName);
            emitInst(allocaInst);
            emitInst(std::make_shared<RcRetainInst>(param));
            emitInst(std::make_shared<StoreInst>(param, allocaInst));
            setVariable(origName, allocaInst);
        } else if (param->getType()->isBox()) {
            auto allocaInst = std::make_shared<AllocaInst>(param->getType(), "%var_" + origName);
            emitInst(allocaInst);
            emitInst(std::make_shared<StoreInst>(param, allocaInst));
            setVariable(origName, allocaInst);
            if (!m_activeBoxes.empty()) {
                m_activeBoxes.back().insert(origName);
            }
        } else if (param->getType()->isStruct() || param->getType()->isArray() || param->getType()->isSlice() || param->getType()->isClass() || param->getType()->isInterface()) {
            setVariable(origName, param);
        } else {
            auto allocaInst = std::make_shared<AllocaInst>(param->getType(), "%var_" + origName);
            emitInst(allocaInst);

            emitInst(std::make_shared<StoreInst>(param, allocaInst));
            setVariable(origName, allocaInst);
        }
    }

    // Generate body
    node.getBody().accept(*this);

    // If block doesn't terminate, add return void or default
    if (m_currentBlock && !m_currentBlock->hasTerminator()) {
        if (returnType->isVoid()) {
            emitInst(std::make_shared<RetInst>());
        } else {
            emitInst(std::make_shared<RetInst>(ConstantInt::get(0, returnType)));
        }
    }

    popScope();
    for (const auto& p : node.getParams()) {
        if (p.isByRef) {
            m_byRefVars.erase(p.name);
        }
    }
    m_currentFunction = nullptr;
    m_currentBlock = nullptr;
}

void IRGenerator::visit(BlockStmt& node) {
    pushScope();
    for (auto& stmt : node.getStatements()) {
        if (m_currentBlock && m_currentBlock->hasTerminator()) {
            break;
        }
        stmt->accept(*this);
    }
    popScope();
}

void IRGenerator::visit(UnsafeBlockStmt& node) {
    node.getBlock().accept(*this);
}

void IRGenerator::visit(DeferStmt& node) {
    if (!m_deferredStmts.empty()) {
        m_deferredStmts.back().push_back(&node.getStatement());
    }
}

std::shared_ptr<IRValue> IRGenerator::coerceToInterface(std::shared_ptr<IRValue> val, const std::shared_ptr<Type>& targetType) {
    if (!targetType || !targetType->isInterface() || !val || !val->getType()->isClass()) {
        return val;
    }

    auto ifaceType = std::static_pointer_cast<InterfaceType>(targetType);
    auto classType = std::static_pointer_cast<ClassType>(val->getType());
    std::string vtableName = classType->getName() + "_" + ifaceType->getName() + "_vtable";

    // Allocate 16-byte interface descriptor: { ptr, ptr }
    auto ifaceSlot = std::make_shared<AllocaInst>(ifaceType, nextTemp());
    emitInst(ifaceSlot);

    // Offset 0: data pointer (pointer to class instance)
    auto dataField = std::make_shared<GetFieldPtrInst>(ifaceSlot, 0, 0, Type::getI64(), nextTemp());
    emitInst(dataField);
    emitInst(std::make_shared<StoreInst>(val, dataField));

    // Offset 8: vtable pointer (pointer to global vtable)
    auto vtableField = std::make_shared<GetFieldPtrInst>(ifaceSlot, 1, 8, Type::getI64(), nextTemp());
    emitInst(vtableField);
    auto vtableSym = ConstantGlobal::get(vtableName, Type::getI64());
    emitInst(std::make_shared<StoreInst>(vtableSym, vtableField));

    return ifaceSlot;
}

std::shared_ptr<IRValue> IRGenerator::boxToDynamic(std::shared_ptr<IRValue> val) {
    if (!val) {
        auto call = std::make_shared<CallInst>("sph_value_create_null",
                                              std::vector<std::shared_ptr<IRValue>>{},
                                              Type::getDynamic(), nextTemp());
        emitInst(call);
        return call;
    }
    if (val->getType()->isDynamic()) {
        return val;
    }
    if (val->getType()->isI32()) {
        auto ext = std::make_shared<CastInst>(val, Type::getI64(), nextTemp());
        emitInst(ext);
        auto call = std::make_shared<CallInst>("sph_value_create_int",
                                              std::vector<std::shared_ptr<IRValue>>{ext},
                                              Type::getDynamic(), nextTemp());
        emitInst(call);
        return call;
    }
    if (val->getType()->isI64()) {
        auto call = std::make_shared<CallInst>("sph_value_create_int",
                                              std::vector<std::shared_ptr<IRValue>>{val},
                                              Type::getDynamic(), nextTemp());
        emitInst(call);
        return call;
    }
    if (val->getType()->isF32()) {
        auto ext = std::make_shared<CastInst>(val, Type::getF64(), nextTemp());
        emitInst(ext);
        auto call = std::make_shared<CallInst>("sph_value_create_float",
                                              std::vector<std::shared_ptr<IRValue>>{ext},
                                              Type::getDynamic(), nextTemp());
        emitInst(call);
        return call;
    }
    if (val->getType()->isF64()) {
        auto call = std::make_shared<CallInst>("sph_value_create_float",
                                              std::vector<std::shared_ptr<IRValue>>{val},
                                              Type::getDynamic(), nextTemp());
        emitInst(call);
        return call;
    }
    if (val->getType()->isBoolean()) {
        auto call = std::make_shared<CallInst>("sph_value_create_bool",
                                              std::vector<std::shared_ptr<IRValue>>{val},
                                              Type::getDynamic(), nextTemp());
        emitInst(call);
        return call;
    }
    if (val->getType()->isString()) {
        auto call = std::make_shared<CallInst>("sph_value_create_string",
                                              std::vector<std::shared_ptr<IRValue>>{val},
                                              Type::getDynamic(), nextTemp());
        emitInst(call);
        return call;
    }
    if (val->getType()->isClass()) {
        auto classType = std::static_pointer_cast<ClassType>(val->getType());
        auto clsName = ConstantString::get(classType->getName());
        auto call = std::make_shared<CallInst>("sph_value_create_object",
                                              std::vector<std::shared_ptr<IRValue>>{clsName, val},
                                              Type::getDynamic(), nextTemp());
        emitInst(call);

        for (size_t i = 0; i < classType->getMembers().size(); ++i) {
            const auto& m = classType->getMembers()[i];
            auto fieldPtr = std::make_shared<GetFieldPtrInst>(val, static_cast<int>(i), m.offset, m.type, nextTemp());
            emitInst(fieldPtr);
            auto loadVal = std::make_shared<LoadInst>(fieldPtr, nextTemp());
            emitInst(loadVal);
            auto boxedProp = boxToDynamic(loadVal);
            auto propName = ConstantString::get(m.name);
            auto setCall = std::make_shared<CallInst>("sph_object_set_property",
                                                      std::vector<std::shared_ptr<IRValue>>{call, propName, boxedProp},
                                                      Type::getVoid(), nextTemp());
            emitInst(setCall);
        }
        return call;
    }
    auto call = std::make_shared<CallInst>("sph_value_create_null",
                                          std::vector<std::shared_ptr<IRValue>>{},
                                          Type::getDynamic(), nextTemp());
    emitInst(call);
    return call;
}

std::shared_ptr<IRValue> IRGenerator::unboxFromDynamic(std::shared_ptr<IRValue> val, const std::shared_ptr<Type>& targetType) {
    if (!val || !val->getType()->isDynamic() || !targetType || targetType->isDynamic()) {
        return val;
    }

    if (targetType->isI64()) {
        auto unbox = std::make_shared<CallInst>("sph_value_to_int", std::vector<std::shared_ptr<IRValue>>{val}, Type::getI64(), nextTemp());
        emitInst(unbox);
        return unbox;
    }
    if (targetType->isI32()) {
        auto unbox = std::make_shared<CallInst>("sph_value_to_int", std::vector<std::shared_ptr<IRValue>>{val}, Type::getI64(), nextTemp());
        emitInst(unbox);
        auto cast = std::make_shared<CastInst>(unbox, Type::getI32(), nextTemp());
        emitInst(cast);
        return cast;
    }
    if (targetType->isF64()) {
        auto unbox = std::make_shared<CallInst>("sph_value_to_float", std::vector<std::shared_ptr<IRValue>>{val}, Type::getF64(), nextTemp());
        emitInst(unbox);
        return unbox;
    }
    if (targetType->isF32()) {
        auto unbox = std::make_shared<CallInst>("sph_value_to_float", std::vector<std::shared_ptr<IRValue>>{val}, Type::getF64(), nextTemp());
        emitInst(unbox);
        auto cast = std::make_shared<CastInst>(unbox, Type::getF32(), nextTemp());
        emitInst(cast);
        return cast;
    }
    if (targetType->isBoolean()) {
        auto unbox = std::make_shared<CallInst>("sph_value_to_bool", std::vector<std::shared_ptr<IRValue>>{val}, Type::getBool(), nextTemp());
        emitInst(unbox);
        return unbox;
    }
    if (targetType->isString()) {
        auto unbox = std::make_shared<CallInst>("sph_value_to_string", std::vector<std::shared_ptr<IRValue>>{val}, Type::getString(), nextTemp());
        emitInst(unbox);
        return unbox;
    }
    return val;
}

void IRGenerator::visit(VarDeclStmt& node) {
    LocGuard locGuard(*this, node.getSpan().start);
    node.getInitializer().accept(*this);
    auto initVal = m_lastValue;

    std::shared_ptr<Type> annotType = nullptr;
    if (node.getTypeAnnotation().has_value()) {
        annotType = resolveType(*node.getTypeAnnotation());
    }

    if (annotType && !annotType->isDynamic() && initVal && initVal->getType()->isDynamic()) {
        initVal = unboxFromDynamic(initVal, annotType);
    }

    if (annotType && annotType->isInterface() && initVal->getType()->isClass()) {
        initVal = coerceToInterface(initVal, annotType);
    }

    if (annotType && annotType->isDynamic()) {
        if (!initVal->getType()->isDynamic()) {
            initVal = boxToDynamic(initVal);
        }
        auto allocaInst = std::make_shared<AllocaInst>(Type::getDynamic(), "%var_" + node.getName());
        emitInst(allocaInst);
        emitInst(std::make_shared<StoreInst>(initVal, allocaInst));
        setVariable(node.getName(), allocaInst);
        return;
    }

    if (initVal->getType()->isInterface()) {
        if (dynamic_cast<VariableExpr*>(&node.getInitializer())) {
            auto ifaceType = std::static_pointer_cast<InterfaceType>(initVal->getType());
            auto newAlloca = std::make_shared<AllocaInst>(ifaceType, "%var_" + node.getName());
            emitInst(newAlloca);

            // Copy data ptr (offset 0)
            auto srcDataField = std::make_shared<GetFieldPtrInst>(initVal, 0, 0, Type::getI64(), nextTemp());
            emitInst(srcDataField);
            auto dataVal = std::make_shared<LoadInst>(srcDataField, nextTemp());
            emitInst(dataVal);
            auto dstDataField = std::make_shared<GetFieldPtrInst>(newAlloca, 0, 0, Type::getI64(), nextTemp());
            emitInst(dstDataField);
            emitInst(std::make_shared<StoreInst>(dataVal, dstDataField));

            // Copy vtable ptr (offset 8)
            auto srcVtField = std::make_shared<GetFieldPtrInst>(initVal, 1, 8, Type::getI64(), nextTemp());
            emitInst(srcVtField);
            auto vtVal = std::make_shared<LoadInst>(srcVtField, nextTemp());
            emitInst(vtVal);
            auto dstVtField = std::make_shared<GetFieldPtrInst>(newAlloca, 1, 8, Type::getI64(), nextTemp());
            emitInst(dstVtField);
            emitInst(std::make_shared<StoreInst>(vtVal, dstVtField));

            setVariable(node.getName(), newAlloca);
        } else {
            initVal->setName("%var_" + node.getName());
            setVariable(node.getName(), initVal);
        }
        return;
    }

    if (initVal->getType()->isStruct() && !initVal->getType()->isClass()) {
        bool isAlloca = initVal->isInstruction() &&
            static_cast<const IRInstruction*>(initVal.get())->getOpcode() == IROpcode::Alloca;

        if (dynamic_cast<VariableExpr*>(&node.getInitializer()) || !isAlloca) {
            // Copy struct fields into a new alloca
            auto structType = std::static_pointer_cast<StructType>(initVal->getType());
            auto newAlloca = std::make_shared<AllocaInst>(structType, "%var_" + node.getName());
            emitInst(newAlloca);

            for (size_t i = 0; i < structType->getMembers().size(); ++i) {
                const auto& m = structType->getMembers()[i];
                auto srcPtr = std::make_shared<GetFieldPtrInst>(initVal, i, m.offset, m.type, nextTemp());
                emitInst(srcPtr);
                auto val = std::make_shared<LoadInst>(srcPtr, nextTemp());
                emitInst(val);

                auto dstPtr = std::make_shared<GetFieldPtrInst>(newAlloca, i, m.offset, m.type, nextTemp());
                emitInst(dstPtr);
                emitInst(std::make_shared<StoreInst>(val, dstPtr));
            }
            setVariable(node.getName(), newAlloca);
            return;
        } else {
            // Direct initialization (e.g. StructInitExpr) - reuse alloca
            initVal->setName("%var_" + node.getName());
            setVariable(node.getName(), initVal);
            return;
        }
    }

    if (initVal->getType()->isClass()) {
        initVal->setName("%var_" + node.getName());
        setVariable(node.getName(), initVal);
        return;
    }

    if (initVal->getType()->isArray()) {
        bool isAlloca = initVal->isInstruction() &&
            static_cast<const IRInstruction*>(initVal.get())->getOpcode() == IROpcode::Alloca;

        if (dynamic_cast<VariableExpr*>(&node.getInitializer()) || !isAlloca) {
            // Copy array elements into a new alloca
            auto arrType = std::static_pointer_cast<ArrayType>(initVal->getType());
            auto newAlloca = std::make_shared<AllocaInst>(arrType, "%var_" + node.getName());
            emitInst(newAlloca);
            size_t elemSize = arrType->getElementType()->getSize();

            for (size_t i = 0; i < arrType->getElementCount(); ++i) {
                auto idxVal = ConstantInt::get(static_cast<int64_t>(i), Type::getI32());
                auto srcPtr = std::make_shared<GetElementPtrInst>(initVal, idxVal, elemSize, arrType->getElementType(), nextTemp());
                emitInst(srcPtr);
                auto val = std::make_shared<LoadInst>(srcPtr, nextTemp());
                emitInst(val);

                auto dstPtr = std::make_shared<GetElementPtrInst>(newAlloca, idxVal, elemSize, arrType->getElementType(), nextTemp());
                emitInst(dstPtr);
                emitInst(std::make_shared<StoreInst>(val, dstPtr));
            }
            setVariable(node.getName(), newAlloca);
        } else {
            // Direct initialization (e.g. ArrayLiteralExpr) - reuse alloca
            initVal->setName("%var_" + node.getName());
            setVariable(node.getName(), initVal);
        }
        return;
    }

    if (initVal->getType()->isSlice()) {
        if (dynamic_cast<VariableExpr*>(&node.getInitializer())) {
            // Copy slice descriptor (16 bytes: ptr and len)
            auto sliceType = std::static_pointer_cast<SliceType>(initVal->getType());
            auto newAlloca = std::make_shared<AllocaInst>(sliceType, "%var_" + node.getName());
            emitInst(newAlloca);

            // Copy ptr (offset 0)
            auto srcPtrField = std::make_shared<GetFieldPtrInst>(initVal, 0, 0, Type::getI64(), nextTemp());
            emitInst(srcPtrField);
            auto ptrVal = std::make_shared<LoadInst>(srcPtrField, nextTemp());
            emitInst(ptrVal);
            auto dstPtrField = std::make_shared<GetFieldPtrInst>(newAlloca, 0, 0, Type::getI64(), nextTemp());
            emitInst(dstPtrField);
            emitInst(std::make_shared<StoreInst>(ptrVal, dstPtrField));

            // Copy len (offset 8)
            auto srcLenField = std::make_shared<GetFieldPtrInst>(initVal, 1, 8, Type::getI32(), nextTemp());
            emitInst(srcLenField);
            auto lenVal = std::make_shared<LoadInst>(srcLenField, nextTemp());
            emitInst(lenVal);
            auto dstLenField = std::make_shared<GetFieldPtrInst>(newAlloca, 1, 8, Type::getI32(), nextTemp());
            emitInst(dstLenField);
            emitInst(std::make_shared<StoreInst>(lenVal, dstLenField));

            setVariable(node.getName(), newAlloca);
        } else {
            // Direct initialization (e.g. SliceExpr) - reuse alloca
            initVal->setName("%var_" + node.getName());
            setVariable(node.getName(), initVal);
        }
        return;
    }

    auto varType = annotType ? annotType : (node.getInitializer().getInferredType() ? node.getInitializer().getInferredType() : initVal->getType());
    auto allocaInst = std::make_shared<AllocaInst>(varType, "%var_" + node.getName());
    emitInst(allocaInst);

    if (varType->isRc()) {
        if (dynamic_cast<VariableExpr*>(&node.getInitializer())) {
            emitInst(std::make_shared<RcRetainInst>(initVal));
        }
    }

    if (varType->isBox()) {
        if (auto* varExpr = dynamic_cast<VariableExpr*>(&node.getInitializer())) {
            for (auto it = m_activeBoxes.rbegin(); it != m_activeBoxes.rend(); ++it) {
                it->erase(varExpr->getName());
            }
        }
        if (!m_activeBoxes.empty()) {
            m_activeBoxes.back().insert(node.getName());
        }
    }

    emitInst(std::make_shared<StoreInst>(initVal, allocaInst));
    setVariable(node.getName(), allocaInst);
}

void IRGenerator::visit(StructDecl& /*node*/) {
    // Struct declarations are type metadata; no IR instructions emitted
}

void IRGenerator::visit(EnumDecl& /*node*/) {
    // Enum declarations are type metadata; no IR instructions emitted
}

void IRGenerator::visit(ImportDecl& /*node*/) {
    // Import declarations are handled during AST parsing/merging
}

void IRGenerator::visit(ReturnStmt& node) {
    LocGuard locGuard(*this, node.getSpan().start);
    std::shared_ptr<IRValue> retVal = nullptr;
    std::string returnedVarName;
    if (node.hasValue()) {
        if (auto* varExpr = dynamic_cast<VariableExpr*>(node.getValue())) {
            returnedVarName = varExpr->getName();
        }
        node.getValue()->accept(*this);
        retVal = m_lastValue;
    }

    // Execute all deferred statements across all active scopes in LIFO order
    executeDeferredAcrossAllScopes();

    // Release all Rc variables in all active scopes (except the returned variable if any)
    for (auto it = m_variableScopes.rbegin(); it != m_variableScopes.rend(); ++it) {
        for (const auto& [name, slot] : *it) {
            if (slot->getType()->isRc()) {
                if (!returnedVarName.empty() && name == returnedVarName) {
                    continue;
                }
                auto loadVal = std::make_shared<LoadInst>(slot, nextTemp());
                emitInst(loadVal);
                emitInst(std::make_shared<RcReleaseInst>(loadVal));
            }
        }
    }

    // Free all active boxes in all active scopes (except the returned variable if any)
    for (auto it = m_activeBoxes.rbegin(); it != m_activeBoxes.rend(); ++it) {
        for (const auto& name : *it) {
            if (!returnedVarName.empty() && name == returnedVarName) {
                continue;
            }
            auto slot = getVariable(name);
            if (slot) {
                auto loadVal = std::make_shared<LoadInst>(slot, nextTemp());
                emitInst(loadVal);
                emitInst(std::make_shared<CallInst>("sph_free", std::vector<std::shared_ptr<IRValue>>{loadVal}, Type::getVoid(), nextTemp()));
            }
        }
    }

    // Pop active exception frames before returning
    for (size_t i = 0; i < m_activeExceptionFrames.size(); ++i) {
        emitInst(std::make_shared<CallInst>("sph_exception_pop_frame", std::vector<std::shared_ptr<IRValue>>{}, Type::getVoid(), nextTemp()));
    }

    if (retVal) {
        if (m_currentFunction) {
            auto expectedRet = m_currentFunction->getReturnType();
            if (expectedRet && expectedRet != retVal->getType()) {
                if (retVal->getType()->isDynamic()) {
                    if (expectedRet->isI32() || expectedRet->isI64()) {
                        auto unbox = std::make_shared<CallInst>("sph_value_to_int", std::vector<std::shared_ptr<IRValue>>{retVal}, Type::getI64(), nextTemp());
                        emitInst(unbox);
                        if (expectedRet->isI32()) {
                            auto cast = std::make_shared<CastInst>(unbox, Type::getI32(), nextTemp());
                            emitInst(cast);
                            retVal = cast;
                        } else {
                            retVal = unbox;
                        }
                    } else if (expectedRet->isString()) {
                        auto unbox = std::make_shared<CallInst>("sph_value_to_string", std::vector<std::shared_ptr<IRValue>>{retVal}, Type::getString(), nextTemp());
                        emitInst(unbox);
                        retVal = unbox;
                    } else if (expectedRet->isBoolean()) {
                        auto unbox = std::make_shared<CallInst>("sph_value_to_bool", std::vector<std::shared_ptr<IRValue>>{retVal}, Type::getBool(), nextTemp());
                        emitInst(unbox);
                        retVal = unbox;
                    } else if (expectedRet->isF64()) {
                        auto unbox = std::make_shared<CallInst>("sph_value_to_float", std::vector<std::shared_ptr<IRValue>>{retVal}, Type::getF64(), nextTemp());
                        emitInst(unbox);
                        retVal = unbox;
                    }
                } else if (expectedRet->isDynamic()) {
                    retVal = boxToDynamic(retVal);
                }
            }
        }
        emitInst(std::make_shared<RetInst>(retVal));
    } else {
        emitInst(std::make_shared<RetInst>());
    }
}

void IRGenerator::visit(IfStmt& node) {
    LocGuard locGuard(*this, node.getSpan().start);
    node.getCondition().accept(*this);
    auto cond = m_lastValue;

    auto* thenBlock = m_currentFunction->createBlock("if.then");
    auto* elseBlock = node.hasElse() ? m_currentFunction->createBlock("if.else") : nullptr;
    auto* mergeBlock = m_currentFunction->createBlock("if.merge");

    emitInst(std::make_shared<CondBranchInst>(
        cond, thenBlock, elseBlock ? elseBlock : mergeBlock));

    // Then block
    m_currentBlock = thenBlock;
    node.getThenBranch().accept(*this);
    if (!m_currentBlock->hasTerminator()) {
        emitInst(std::make_shared<BranchInst>(mergeBlock));
    }

    // Else block
    if (elseBlock) {
        m_currentBlock = elseBlock;
        node.getElseBranch()->accept(*this);
        if (!m_currentBlock->hasTerminator()) {
            emitInst(std::make_shared<BranchInst>(mergeBlock));
        }
    }

    m_currentBlock = mergeBlock;
}

void IRGenerator::visit(WhileStmt& node) {
    LocGuard locGuard(*this, node.getSpan().start);
    auto* condBlock = m_currentFunction->createBlock("while.cond");
    auto* bodyBlock = m_currentFunction->createBlock("while.body");
    auto* exitBlock = m_currentFunction->createBlock("while.exit");

    emitInst(std::make_shared<BranchInst>(condBlock));
    m_loopStack.push_back({condBlock, exitBlock});

    // Cond block
    m_currentBlock = condBlock;
    node.getCondition().accept(*this);
    auto cond = m_lastValue;
    if (cond && cond->getType()->isDynamic()) {
        auto boolCall = std::make_shared<CallInst>("sph_value_to_bool",
                                                   std::vector<std::shared_ptr<IRValue>>{cond},
                                                   Type::getBool(), nextTemp());
        emitInst(boolCall);
        cond = boolCall;
    }
    emitInst(std::make_shared<CondBranchInst>(cond, bodyBlock, exitBlock));

    // Body block
    m_currentBlock = bodyBlock;
    node.getBody().accept(*this);
    if (!m_currentBlock->hasTerminator()) {
        emitInst(std::make_shared<BranchInst>(condBlock));
    }

    m_loopStack.pop_back();
    m_currentBlock = exitBlock;
}

void IRGenerator::visit(ForeachStmt& node) {
    LocGuard locGuard(*this, node.getSpan().start);
    node.getCollection().accept(*this);
    auto collVal = m_lastValue;
    if (!collVal) return;

    if (collVal->getType()->isDynamic()) {
        auto iterSlot = std::make_shared<AllocaInst>(Type::getI64(), nextTemp());
        emitInst(iterSlot);
        auto initIter = std::make_shared<CallInst>("sph_array_iter_create",
                                                   std::vector<std::shared_ptr<IRValue>>{collVal},
                                                   Type::getI64(), nextTemp());
        emitInst(initIter);
        emitInst(std::make_shared<StoreInst>(initIter, iterSlot));

        auto* condBlock = m_currentFunction->createBlock("foreach.cond");
        auto* bodyBlock = m_currentFunction->createBlock("foreach.body");
        auto* stepBlock = m_currentFunction->createBlock("foreach.step");
        auto* exitBlock = m_currentFunction->createBlock("foreach.exit");

        emitInst(std::make_shared<BranchInst>(condBlock));
        m_loopStack.push_back({stepBlock, exitBlock});

        // Condition
        m_currentBlock = condBlock;
        auto curIter = std::make_shared<LoadInst>(Type::getI64(), iterSlot, nextTemp());
        emitInst(curIter);
        auto isValid = std::make_shared<CallInst>("sph_array_iter_valid",
                                                  std::vector<std::shared_ptr<IRValue>>{curIter},
                                                  Type::getBool(), nextTemp());
        emitInst(isValid);
        emitInst(std::make_shared<CondBranchInst>(isValid, bodyBlock, exitBlock));

        // Body
        m_currentBlock = bodyBlock;
        pushScope();

        if (node.hasKeyVar()) {
            auto keyVal = std::make_shared<CallInst>("sph_array_iter_get_key",
                                                     std::vector<std::shared_ptr<IRValue>>{curIter},
                                                     Type::getDynamic(), nextTemp());
            emitInst(keyVal);
            auto keySlot = std::make_shared<AllocaInst>(Type::getDynamic(), "%var_" + node.getKeyVar());
            emitInst(keySlot);
            emitInst(std::make_shared<StoreInst>(keyVal, keySlot));
            setVariable(node.getKeyVar(), keySlot);
        }

        auto valVal = std::make_shared<CallInst>("sph_array_iter_get_val",
                                                 std::vector<std::shared_ptr<IRValue>>{curIter},
                                                 Type::getDynamic(), nextTemp());
        emitInst(valVal);
        auto valSlot = std::make_shared<AllocaInst>(Type::getDynamic(), "%var_" + node.getValVar());
        emitInst(valSlot);
        emitInst(std::make_shared<StoreInst>(valVal, valSlot));
        setVariable(node.getValVar(), valSlot);

        for (auto& stmt : node.getBody().getStatements()) {
            if (m_currentBlock && m_currentBlock->hasTerminator()) break;
            stmt->accept(*this);
        }
        popScope();

        if (m_currentBlock && !m_currentBlock->hasTerminator()) {
            emitInst(std::make_shared<BranchInst>(stepBlock));
        }

        // Step
        m_currentBlock = stepBlock;
        auto stepIter = std::make_shared<LoadInst>(Type::getI64(), iterSlot, nextTemp());
        emitInst(stepIter);
        auto nextIter = std::make_shared<CallInst>("sph_array_iter_next",
                                                   std::vector<std::shared_ptr<IRValue>>{stepIter},
                                                   Type::getI64(), nextTemp());
        emitInst(nextIter);
        emitInst(std::make_shared<StoreInst>(nextIter, iterSlot));
        emitInst(std::make_shared<BranchInst>(condBlock));

        m_loopStack.pop_back();
        m_currentBlock = exitBlock;
        return;
    }

    if (collVal->getType()->isArray()) {
        auto arrType = std::static_pointer_cast<ArrayType>(collVal->getType());
        uint64_t arrLen = arrType->getElementCount();
        auto elemType = arrType->getElementType();

        auto idxSlot = std::make_shared<AllocaInst>(Type::getI32(), nextTemp());
        emitInst(idxSlot);
        emitInst(std::make_shared<StoreInst>(ConstantInt::get(0, Type::getI32()), idxSlot));

        auto* condBlock = m_currentFunction->createBlock("foreach.cond");
        auto* bodyBlock = m_currentFunction->createBlock("foreach.body");
        auto* stepBlock = m_currentFunction->createBlock("foreach.step");
        auto* exitBlock = m_currentFunction->createBlock("foreach.exit");

        emitInst(std::make_shared<BranchInst>(condBlock));
        m_loopStack.push_back({stepBlock, exitBlock});

        // Condition
        m_currentBlock = condBlock;
        auto curIdx = std::make_shared<LoadInst>(Type::getI32(), idxSlot, nextTemp());
        emitInst(curIdx);
        auto limitVal = ConstantInt::get(arrLen, Type::getI32());
        auto cmp = std::make_shared<BinaryInst>(IROpcode::Less, curIdx, limitVal, Type::getBool(), nextTemp());
        emitInst(cmp);
        emitInst(std::make_shared<CondBranchInst>(cmp, bodyBlock, exitBlock));

        // Body
        m_currentBlock = bodyBlock;
        pushScope();

        if (node.hasKeyVar()) {
            auto keySlot = std::make_shared<AllocaInst>(Type::getI32(), "%var_" + node.getKeyVar());
            emitInst(keySlot);
            emitInst(std::make_shared<StoreInst>(curIdx, keySlot));
            setVariable(node.getKeyVar(), keySlot);
        }

        auto gep = std::make_shared<GetElementPtrInst>(collVal, curIdx, elemType->getSize(), elemType, nextTemp());
        emitInst(gep);
        auto elemVal = std::make_shared<LoadInst>(elemType, gep, nextTemp());
        emitInst(elemVal);

        auto valSlot = std::make_shared<AllocaInst>(elemType, "%var_" + node.getValVar());
        emitInst(valSlot);
        emitInst(std::make_shared<StoreInst>(elemVal, valSlot));
        setVariable(node.getValVar(), valSlot);

        for (auto& stmt : node.getBody().getStatements()) {
            if (m_currentBlock && m_currentBlock->hasTerminator()) break;
            stmt->accept(*this);
        }
        popScope();

        if (m_currentBlock && !m_currentBlock->hasTerminator()) {
            emitInst(std::make_shared<BranchInst>(stepBlock));
        }

        // Step
        m_currentBlock = stepBlock;
        auto sIdx = std::make_shared<LoadInst>(Type::getI32(), idxSlot, nextTemp());
        emitInst(sIdx);
        auto inc = std::make_shared<BinaryInst>(IROpcode::Add, sIdx, ConstantInt::get(1, Type::getI32()), Type::getI32(), nextTemp());
        emitInst(inc);
        emitInst(std::make_shared<StoreInst>(inc, idxSlot));
        emitInst(std::make_shared<BranchInst>(condBlock));

        m_loopStack.pop_back();
        m_currentBlock = exitBlock;
        return;
    }

    if (collVal->getType()->isSlice()) {
        auto sliceType = std::static_pointer_cast<SliceType>(collVal->getType());
        auto elemType = sliceType->getElementType();

        auto lenField = std::make_shared<GetFieldPtrInst>(collVal, 1, 8, Type::getI32(), nextTemp());
        emitInst(lenField);
        auto lenVal = std::make_shared<LoadInst>(Type::getI32(), lenField, nextTemp());
        emitInst(lenVal);

        auto idxSlot = std::make_shared<AllocaInst>(Type::getI32(), nextTemp());
        emitInst(idxSlot);
        emitInst(std::make_shared<StoreInst>(ConstantInt::get(0, Type::getI32()), idxSlot));

        auto* condBlock = m_currentFunction->createBlock("foreach.cond");
        auto* bodyBlock = m_currentFunction->createBlock("foreach.body");
        auto* stepBlock = m_currentFunction->createBlock("foreach.step");
        auto* exitBlock = m_currentFunction->createBlock("foreach.exit");

        emitInst(std::make_shared<BranchInst>(condBlock));
        m_loopStack.push_back({stepBlock, exitBlock});

        // Condition
        m_currentBlock = condBlock;
        auto curIdx = std::make_shared<LoadInst>(Type::getI32(), idxSlot, nextTemp());
        emitInst(curIdx);
        auto cmp = std::make_shared<BinaryInst>(IROpcode::Less, curIdx, lenVal, Type::getBool(), nextTemp());
        emitInst(cmp);
        emitInst(std::make_shared<CondBranchInst>(cmp, bodyBlock, exitBlock));

        // Body
        m_currentBlock = bodyBlock;
        pushScope();

        if (node.hasKeyVar()) {
            auto keySlot = std::make_shared<AllocaInst>(Type::getI32(), "%var_" + node.getKeyVar());
            emitInst(keySlot);
            emitInst(std::make_shared<StoreInst>(curIdx, keySlot));
            setVariable(node.getKeyVar(), keySlot);
        }

        auto basePtrField = std::make_shared<GetFieldPtrInst>(collVal, 0, 0, Type::getI64(), nextTemp());
        emitInst(basePtrField);
        auto basePtr = std::make_shared<LoadInst>(Type::getI64(), basePtrField, nextTemp());
        emitInst(basePtr);
        auto gep = std::make_shared<GetElementPtrInst>(basePtr, curIdx, elemType->getSize(), elemType, nextTemp());
        emitInst(gep);
        auto elemVal = std::make_shared<LoadInst>(elemType, gep, nextTemp());
        emitInst(elemVal);

        auto valSlot = std::make_shared<AllocaInst>(elemType, "%var_" + node.getValVar());
        emitInst(valSlot);
        emitInst(std::make_shared<StoreInst>(elemVal, valSlot));
        setVariable(node.getValVar(), valSlot);

        for (auto& stmt : node.getBody().getStatements()) {
            if (m_currentBlock && m_currentBlock->hasTerminator()) break;
            stmt->accept(*this);
        }
        popScope();

        if (m_currentBlock && !m_currentBlock->hasTerminator()) {
            emitInst(std::make_shared<BranchInst>(stepBlock));
        }

        // Step
        m_currentBlock = stepBlock;
        auto sIdx = std::make_shared<LoadInst>(Type::getI32(), idxSlot, nextTemp());
        emitInst(sIdx);
        auto inc = std::make_shared<BinaryInst>(IROpcode::Add, sIdx, ConstantInt::get(1, Type::getI32()), Type::getI32(), nextTemp());
        emitInst(inc);
        emitInst(std::make_shared<StoreInst>(inc, idxSlot));
        emitInst(std::make_shared<BranchInst>(condBlock));

        m_loopStack.pop_back();
        m_currentBlock = exitBlock;
        return;
    }
}

void IRGenerator::visit(BreakStmt& node) {
    LocGuard locGuard(*this, node.getSpan().start);
    (void)node;
    if (!m_loopStack.empty() && m_currentBlock && !m_currentBlock->hasTerminator()) {
        emitInst(std::make_shared<BranchInst>(m_loopStack.back().breakBlock));
    }
}

void IRGenerator::visit(ContinueStmt& node) {
    LocGuard locGuard(*this, node.getSpan().start);
    (void)node;
    if (!m_loopStack.empty() && m_currentBlock && !m_currentBlock->hasTerminator()) {
        emitInst(std::make_shared<BranchInst>(m_loopStack.back().continueBlock));
    }
}

void IRGenerator::visit(ExprStmt& node) {
    LocGuard locGuard(*this, node.getSpan().start);
    node.getExpr().accept(*this);
}

void IRGenerator::visit(EchoStmt& node) {
    LocGuard locGuard(*this, node.getSpan().start);
    for (auto& expr : node.getExpressions()) {
        expr->accept(*this);
        auto val = m_lastValue;
        if (!val) continue;

        if (val->getType()->isDynamic()) {
            auto call = std::make_shared<CallInst>("sph_echo_value",
                                                  std::vector<std::shared_ptr<IRValue>>{val},
                                                  Type::getVoid(), nextTemp());
            emitInst(call);
        } else if (val->getType()->isString()) {
            auto call = std::make_shared<CallInst>("sph_echo_string",
                                                  std::vector<std::shared_ptr<IRValue>>{val},
                                                  Type::getVoid(), nextTemp());
            emitInst(call);
        } else if (val->getType()->isI32()) {
            auto call = std::make_shared<CallInst>("sph_echo_i32",
                                                  std::vector<std::shared_ptr<IRValue>>{val},
                                                  Type::getVoid(), nextTemp());
            emitInst(call);
        } else if (val->getType()->isI64()) {
            auto call = std::make_shared<CallInst>("sph_echo_i64",
                                                  std::vector<std::shared_ptr<IRValue>>{val},
                                                  Type::getVoid(), nextTemp());
            emitInst(call);
        } else if (val->getType()->isF64()) {
            auto call = std::make_shared<CallInst>("sph_echo_f64",
                                                  std::vector<std::shared_ptr<IRValue>>{val},
                                                  Type::getVoid(), nextTemp());
            emitInst(call);
        } else if (val->getType()->isBoolean()) {
            auto call = std::make_shared<CallInst>("sph_echo_bool",
                                                  std::vector<std::shared_ptr<IRValue>>{val},
                                                  Type::getVoid(), nextTemp());
            emitInst(call);
        } else {
            auto dynVal = boxToDynamic(val);
            auto call = std::make_shared<CallInst>("sph_echo_value",
                                                  std::vector<std::shared_ptr<IRValue>>{dynVal},
                                                  Type::getVoid(), nextTemp());
            emitInst(call);
        }
    }
}

void IRGenerator::visit(ThrowStmt& node) {
    LocGuard locGuard(*this, node.getSpan().start);
    node.getExpr().accept(*this);
    auto thrownVal = m_lastValue;

    std::shared_ptr<IRValue> exObj = thrownVal;
    if (thrownVal && thrownVal->getType()->isClass()) {
        auto classType = std::static_pointer_cast<ClassType>(thrownVal->getType());
        auto classConst = ConstantString::get(classType->getName());
        int64_t classSize = static_cast<int64_t>(classType->getSize());
        if (classSize <= 0) classSize = 8;
        auto allocInst = std::make_shared<CallInst>("sph_alloc",
                                                    std::vector<std::shared_ptr<IRValue>>{ConstantInt::get(classSize, Type::getI64())},
                                                    classType, nextTemp());
        emitInst(allocInst);

        auto createObj = std::make_shared<CallInst>("sph_value_create_object",
                                                    std::vector<std::shared_ptr<IRValue>>{classConst, allocInst},
                                                    Type::getDynamic(), nextTemp());
        emitInst(createObj);

        // Copy all fields into the dynamic object properties and heap-allocated native instance
        for (size_t i = 0; i < classType->getMembers().size(); ++i) {
            const auto& m = classType->getMembers()[i];
            auto fieldPtr = std::make_shared<GetFieldPtrInst>(thrownVal, static_cast<int>(i), m.offset, m.type, nextTemp());
            emitInst(fieldPtr);
            auto fieldVal = std::make_shared<LoadInst>(fieldPtr, nextTemp());
            emitInst(fieldVal);

            auto heapFieldPtr = std::make_shared<GetFieldPtrInst>(allocInst, static_cast<int>(i), m.offset, m.type, nextTemp());
            emitInst(heapFieldPtr);
            emitInst(std::make_shared<StoreInst>(fieldVal, heapFieldPtr));

            std::shared_ptr<IRValue> dynVal = fieldVal;
            if (!fieldVal->getType()->isDynamic()) {
                dynVal = boxToDynamic(fieldVal);
            }
            std::string propName = m.name;
            if (!propName.empty() && propName[0] == '$') {
                propName = propName.substr(1);
            }
            auto propNameConst = ConstantString::get(propName);
            emitInst(std::make_shared<CallInst>("sph_object_set_property",
                                                                      std::vector<std::shared_ptr<IRValue>>{createObj, propNameConst, dynVal},
                                                                      Type::getVoid(), nextTemp()));
        }
        exObj = createObj;
    } else if (thrownVal && !thrownVal->getType()->isDynamic() && !thrownVal->getType()->isPointer()) {
        exObj = boxToDynamic(thrownVal);
    }

    auto callThrow = std::make_shared<CallInst>("sph_exception_throw",
                                                std::vector<std::shared_ptr<IRValue>>{exObj},
                                                Type::getVoid(), nextTemp());
    emitInst(callThrow);
}

void IRGenerator::visit(TryStmt& node) {
    LocGuard locGuard(*this, node.getSpan().start);
    // 1. Allocate frame on stack (size 320 to comfortably hold 272-byte sph_exception_frame with alignment)
    std::vector<StructMember> frameMembers;
    frameMembers.push_back(StructMember{"raw", std::make_shared<ArrayType>(Type::getI64(), 40), 0});
    auto frameType = StructType::get("sph_exception_frame", std::move(frameMembers), false, 16);
    auto frameSlot = std::make_shared<AllocaInst>(frameType, nextTemp());
    emitInst(frameSlot);

    // 2. Call SetJmpInst(frameSlot)
    auto setJmpInst = std::make_shared<SetJmpInst>(frameSlot, nextTemp());
    emitInst(setJmpInst);

    // 3. Compare setjmp result == 0
    auto zeroVal = ConstantInt::get(0, Type::getI32());
    auto cmpInst = std::make_shared<BinaryInst>(IROpcode::Equal, setJmpInst, zeroVal, Type::getBool(), nextTemp());
    emitInst(cmpInst);

    auto tryEntryBlock = m_currentFunction->createBlock("try_entry");
    auto catchDispatcherBlock = m_currentFunction->createBlock("catch_dispatch");
    auto finallyBlock = node.hasFinally() ? m_currentFunction->createBlock("finally_block") : nullptr;
    auto endBlock = m_currentFunction->createBlock("try_end");

    auto targetFinallyOrEnd = finallyBlock ? finallyBlock : endBlock;

    emitInst(std::make_shared<CondBranchInst>(cmpInst, tryEntryBlock, catchDispatcherBlock));

    // Try block
    m_currentBlock = tryEntryBlock;
    m_activeExceptionFrames.push_back(frameSlot);
    emitInst(std::make_shared<CallInst>("sph_exception_push_frame",
                                                              std::vector<std::shared_ptr<IRValue>>{frameSlot},
                                                              Type::getVoid(), nextTemp()));
    pushScope();
    node.getTryBlock().accept(*this);
    popScope();

    if (!m_currentBlock->hasTerminator()) {
        emitInst(std::make_shared<CallInst>("sph_exception_pop_frame",
                                            std::vector<std::shared_ptr<IRValue>>{},
                                            Type::getVoid(), nextTemp()));
        emitInst(std::make_shared<BranchInst>(targetFinallyOrEnd));
    }
    m_activeExceptionFrames.pop_back();

    // Catch Dispatcher block (arrived via longjmp)
    m_currentBlock = catchDispatcherBlock;
    emitInst(std::make_shared<CallInst>("sph_exception_pop_frame",
                                                              std::vector<std::shared_ptr<IRValue>>{},
                                                              Type::getVoid(), nextTemp()));
    auto exVal = std::make_shared<CallInst>("sph_exception_get_current",
                                            std::vector<std::shared_ptr<IRValue>>{},
                                            Type::getDynamic(), nextTemp());
    emitInst(exVal);

    if (node.getCatchClauses().empty()) {
        if (!m_currentBlock->hasTerminator()) {
            emitInst(std::make_shared<BranchInst>(targetFinallyOrEnd));
        }
    } else {
        for (size_t i = 0; i < node.getCatchClauses().size(); ++i) {
            const auto& clause = node.getCatchClauses()[i];
            auto catchBodyBlock = m_currentFunction->createBlock("catch_body_" + std::to_string(i));
            IRBasicBlock* nextCatchBlock = (i + 1 < node.getCatchClauses().size())
                ? m_currentFunction->createBlock("catch_next_" + std::to_string(i))
                : targetFinallyOrEnd;

            if (!clause.type.empty()) {
                auto clsCall = std::make_shared<CallInst>("sph_object_get_class",
                                                          std::vector<std::shared_ptr<IRValue>>{exVal},
                                                          Type::getString(), nextTemp());
                emitInst(clsCall);
                auto typeStrConst = ConstantString::get(clause.type);
                auto eqCall = std::make_shared<CallInst>("sph_string_eq",
                                                         std::vector<std::shared_ptr<IRValue>>{clsCall, typeStrConst},
                                                         Type::getI32(), nextTemp());
                emitInst(eqCall);
                auto oneVal = ConstantInt::get(1, Type::getI32());
                auto cmpMatch = std::make_shared<BinaryInst>(IROpcode::Equal, eqCall, oneVal, Type::getBool(), nextTemp());
                emitInst(cmpMatch);
                emitInst(std::make_shared<CondBranchInst>(cmpMatch, catchBodyBlock, nextCatchBlock));
            } else {
                emitInst(std::make_shared<BranchInst>(catchBodyBlock));
            }

            m_currentBlock = catchBodyBlock;
            pushScope();
            if (!clause.varName.empty()) {
                auto catchVarSlot = std::make_shared<AllocaInst>(Type::getDynamic(), "%var_" + clause.varName);
                emitInst(catchVarSlot);
                emitInst(std::make_shared<StoreInst>(exVal, catchVarSlot));
                setVariable(clause.varName, catchVarSlot);
            }
            clause.body->accept(*this);
            popScope();

            if (!m_currentBlock->hasTerminator()) {
                emitInst(std::make_shared<BranchInst>(targetFinallyOrEnd));
            }

            if (i + 1 < node.getCatchClauses().size()) {
                m_currentBlock = nextCatchBlock;
            }
        }
    }

    // Finally block
    if (finallyBlock) {
        m_currentBlock = finallyBlock;
        pushScope();
        node.getFinallyBlock()->accept(*this);
        popScope();
        if (!m_currentBlock->hasTerminator()) {
            emitInst(std::make_shared<BranchInst>(endBlock));
        }
    }

    m_currentBlock = endBlock;
}

void IRGenerator::visit(IntegerLiteralExpr& node) {
    m_lastValue = ConstantInt::get(node.getValue(), Type::getI32());
}

void IRGenerator::visit(FloatLiteralExpr& node) {
    m_lastValue = ConstantFloat::get(node.getValue(), Type::getF64());
}

void IRGenerator::visit(StringLiteralExpr& node) {
    m_lastValue = ConstantString::get(node.getValue());
}

void IRGenerator::visit(BoolLiteralExpr& node) {
    m_lastValue = ConstantBool::get(node.getValue());
}

void IRGenerator::visit(VariableExpr& node) {
    if (node.getName() == "CURLOPT_URL") { m_lastValue = ConstantInt::get(10002, Type::getI32()); return; }
    if (node.getName() == "CURLOPT_RETURNTRANSFER") { m_lastValue = ConstantInt::get(19913, Type::getI32()); return; }
    if (node.getName() == "CURLOPT_POST") { m_lastValue = ConstantInt::get(47, Type::getI32()); return; }
    if (node.getName() == "CURLOPT_POSTFIELDS") { m_lastValue = ConstantInt::get(10015, Type::getI32()); return; }
    if (node.getName() == "CURLOPT_HTTPHEADER") { m_lastValue = ConstantInt::get(10023, Type::getI32()); return; }
    if (node.getName() == "CURLOPT_TIMEOUT") { m_lastValue = ConstantInt::get(13, Type::getI32()); return; }
    if (node.getName() == "CURLOPT_CUSTOMREQUEST") { m_lastValue = ConstantInt::get(10036, Type::getI32()); return; }
    if (node.getName() == "CURLINFO_HTTP_CODE") { m_lastValue = ConstantInt::get(2097154, Type::getI32()); return; }

    auto slot = getVariable(node.getName());
    if (!slot) {
        if (!node.getName().empty() && node.getName()[0] == '$') {
            auto allocaInst = std::make_shared<AllocaInst>(Type::getDynamic(), "%var_" + node.getName());
            emitInst(allocaInst);
            auto nullVal = boxToDynamic(nullptr);
            emitInst(std::make_shared<StoreInst>(nullVal, allocaInst));
            setVariable(node.getName(), allocaInst);
            slot = allocaInst;
        } else {
            if (m_functionParamTypes.count(node.getName()) || (m_module && m_module->getFunction(node.getName()))) {
                m_lastValue = ConstantGlobal::get(node.getName(), PointerType::get(Type::getVoid()));
                return;
            }
            return;
        }
    }

    if (slot->getType()->isStruct() || slot->getType()->isArray() || slot->getType()->isSlice() || slot->getType()->isClass() || slot->getType()->isInterface()) {
        m_lastValue = slot;
        return;
    }

    if (m_byRefVars.count(node.getName()) && slot->getType()->isPointer()) {
        auto ptrType = std::static_pointer_cast<PointerType>(slot->getType());
        auto baseType = ptrType->getPointeeType();
        auto loadInst = std::make_shared<LoadInst>(baseType, slot, nextTemp());
        emitInst(loadInst);
        m_lastValue = loadInst;
        return;
    }

    auto loadInst = std::make_shared<LoadInst>(slot, nextTemp());
    emitInst(loadInst);
    m_lastValue = loadInst;
}

void IRGenerator::visit(AssignExpr& node) {
    LocGuard locGuard(*this, node.getSpan().start);
    if (auto* derefExpr = dynamic_cast<UnaryExpr*>(&node.getTarget())) {
        if (derefExpr->getOp() == UnaryOp::Deref) {
            derefExpr->getOperand().accept(*this);
            auto ptrVal = m_lastValue;

            node.getValue().accept(*this);
            auto val = m_lastValue;

            if (val->getType()->isStruct() || val->getType()->isClass()) {
                auto structType = std::static_pointer_cast<StructType>(val->getType());
                for (size_t i = 0; i < structType->getMembers().size(); ++i) {
                    const auto& m = structType->getMembers()[i];
                    auto srcPtr = std::make_shared<GetFieldPtrInst>(val, i, m.offset, m.type, nextTemp());
                    emitInst(srcPtr);
                    auto loadVal = std::make_shared<LoadInst>(srcPtr, nextTemp());
                    emitInst(loadVal);
                    auto dstPtr = std::make_shared<GetFieldPtrInst>(ptrVal, i, m.offset, m.type, nextTemp());
                    emitInst(dstPtr);
                    emitInst(std::make_shared<StoreInst>(loadVal, dstPtr));
                }
            } else if (val->getType()->isArray()) {
                auto arrType = std::static_pointer_cast<ArrayType>(val->getType());
                size_t elemSize = arrType->getElementType()->getSize();
                for (size_t i = 0; i < arrType->getElementCount(); ++i) {
                    auto idxVal = ConstantInt::get(static_cast<int64_t>(i), Type::getI32());
                    auto srcPtr = std::make_shared<GetElementPtrInst>(val, idxVal, elemSize, arrType->getElementType(), nextTemp());
                    emitInst(srcPtr);
                    auto loadVal = std::make_shared<LoadInst>(srcPtr, nextTemp());
                    emitInst(loadVal);
                    auto dstPtr = std::make_shared<GetElementPtrInst>(ptrVal, idxVal, elemSize, arrType->getElementType(), nextTemp());
                    emitInst(dstPtr);
                    emitInst(std::make_shared<StoreInst>(loadVal, dstPtr));
                }
            } else {
                emitInst(std::make_shared<StoreInst>(val, ptrVal));
            }

            m_lastValue = val;
            return;
        }
    }

    node.getValue().accept(*this);
    auto val = m_lastValue;

    if (auto* varExpr = dynamic_cast<VariableExpr*>(&node.getTarget())) {
        if (m_byRefVars.count(varExpr->getName())) {
            auto slot = getVariable(varExpr->getName());
            if (slot && slot->getType()->isPointer()) {
                auto ptrType = std::static_pointer_cast<PointerType>(slot->getType());
                auto baseType = ptrType->getPointeeType();
                if (baseType->isDynamic() && !val->getType()->isDynamic()) {
                    val = boxToDynamic(val);
                }
                emitInst(std::make_shared<StoreInst>(val, slot));
                m_lastValue = val;
                return;
            }
        }

        auto slot = getVariable(varExpr->getName());
        if (!slot) {
            auto allocaInst = std::make_shared<AllocaInst>(Type::getDynamic(), "%var_" + varExpr->getName());
            emitInst(allocaInst);
            setVariable(varExpr->getName(), allocaInst);
            slot = allocaInst;
        }

        if (slot->getType()->isDynamic()) {
            if (!val->getType()->isDynamic()) {
                val = boxToDynamic(val);
            }
            emitInst(std::make_shared<StoreInst>(val, slot));
            m_lastValue = val;
            return;
        }

        if (slot) {
            if (slot->getType()->isInterface()) {
                if (val->getType()->isClass()) {
                    val = coerceToInterface(val, slot->getType());
                }
                auto srcDataField = std::make_shared<GetFieldPtrInst>(val, 0, 0, Type::getI64(), nextTemp());
                emitInst(srcDataField);
                auto dataVal = std::make_shared<LoadInst>(srcDataField, nextTemp());
                emitInst(dataVal);
                auto dstDataField = std::make_shared<GetFieldPtrInst>(slot, 0, 0, Type::getI64(), nextTemp());
                emitInst(dstDataField);
                emitInst(std::make_shared<StoreInst>(dataVal, dstDataField));

                auto srcVtField = std::make_shared<GetFieldPtrInst>(val, 1, 8, Type::getI64(), nextTemp());
                emitInst(srcVtField);
                auto vtVal = std::make_shared<LoadInst>(srcVtField, nextTemp());
                emitInst(vtVal);
                auto dstVtField = std::make_shared<GetFieldPtrInst>(slot, 1, 8, Type::getI64(), nextTemp());
                emitInst(dstVtField);
                emitInst(std::make_shared<StoreInst>(vtVal, dstVtField));
            } else if (slot->getType()->isStruct() || slot->getType()->isClass()) {
                auto structType = std::static_pointer_cast<StructType>(slot->getType());
                for (size_t i = 0; i < structType->getMembers().size(); ++i) {
                    const auto& m = structType->getMembers()[i];
                    auto srcPtr = std::make_shared<GetFieldPtrInst>(val, i, m.offset, m.type, nextTemp());
                    emitInst(srcPtr);
                    auto loadVal = std::make_shared<LoadInst>(srcPtr, nextTemp());
                    emitInst(loadVal);
                    auto dstPtr = std::make_shared<GetFieldPtrInst>(slot, i, m.offset, m.type, nextTemp());
                    emitInst(dstPtr);
                    emitInst(std::make_shared<StoreInst>(loadVal, dstPtr));
                }
            } else if (slot->getType()->isArray()) {
                auto arrType = std::static_pointer_cast<ArrayType>(slot->getType());
                size_t elemSize = arrType->getElementType()->getSize();
                for (size_t i = 0; i < arrType->getElementCount(); ++i) {
                    auto idxVal = ConstantInt::get(static_cast<int64_t>(i), Type::getI32());
                    auto srcPtr = std::make_shared<GetElementPtrInst>(val, idxVal, elemSize, arrType->getElementType(), nextTemp());
                    emitInst(srcPtr);
                    auto loadVal = std::make_shared<LoadInst>(srcPtr, nextTemp());
                    emitInst(loadVal);
                    auto dstPtr = std::make_shared<GetElementPtrInst>(slot, idxVal, elemSize, arrType->getElementType(), nextTemp());
                    emitInst(dstPtr);
                    emitInst(std::make_shared<StoreInst>(loadVal, dstPtr));
                }
            } else if (slot->getType()->isRc()) {
                if (dynamic_cast<VariableExpr*>(&node.getValue())) {
                    emitInst(std::make_shared<RcRetainInst>(val));
                }
                auto oldVal = std::make_shared<LoadInst>(slot, nextTemp());
                emitInst(oldVal);
                emitInst(std::make_shared<RcReleaseInst>(oldVal));
                emitInst(std::make_shared<StoreInst>(val, slot));
            } else if (slot->getType()->isBox()) {
                auto oldVal = std::make_shared<LoadInst>(slot, nextTemp());
                emitInst(oldVal);
                emitInst(std::make_shared<CallInst>("sph_free", std::vector<std::shared_ptr<IRValue>>{oldVal}, Type::getVoid(), nextTemp()));
                if (auto* rhsVar = dynamic_cast<VariableExpr*>(&node.getValue())) {
                    for (auto it = m_activeBoxes.rbegin(); it != m_activeBoxes.rend(); ++it) {
                        it->erase(rhsVar->getName());
                    }
                }
                if (!m_activeBoxes.empty()) {
                    m_activeBoxes.back().insert(varExpr->getName());
                }
                emitInst(std::make_shared<StoreInst>(val, slot));
            } else {
                if (slot->getType()->isDynamic() && !val->getType()->isDynamic()) {
                    val = boxToDynamic(val);
                } else if (!slot->getType()->isDynamic() && val->getType()->isDynamic()) {
                    val = unboxFromDynamic(val, slot->getType());
                }
                emitInst(std::make_shared<StoreInst>(val, slot));
            }
        }
        m_lastValue = val;
        return;
    }

    if (auto* memExpr = dynamic_cast<MemberAccessExpr*>(&node.getTarget())) {
        std::shared_ptr<IRValue> basePtr;
        std::shared_ptr<StructType> structType;
        if (auto* deref = dynamic_cast<UnaryExpr*>(&memExpr->getBase()); deref && deref->getOp() == UnaryOp::Deref) {
            deref->getOperand().accept(*this);
            basePtr = m_lastValue;
            auto ptrType = std::static_pointer_cast<PointerType>(basePtr->getType());
            structType = std::static_pointer_cast<StructType>(ptrType->getPointeeType());
        } else {
            memExpr->getBase().accept(*this);
            basePtr = m_lastValue;
            if (basePtr->getType()->isDynamic()) {
                if (!val->getType()->isDynamic()) {
                    val = boxToDynamic(val);
                }
                auto propName = ConstantString::get(memExpr->getMember());
                auto setCall = std::make_shared<CallInst>("sph_object_set_property",
                                                          std::vector<std::shared_ptr<IRValue>>{basePtr, propName, val},
                                                          Type::getVoid(), nextTemp());
                emitInst(setCall);
                m_lastValue = val;
                return;
            }
            if (basePtr->getType()->isRc()) {
                auto rcType = std::static_pointer_cast<RcType>(basePtr->getType());
                structType = std::static_pointer_cast<StructType>(rcType->getInnerType());
            } else if (basePtr->getType()->isBox()) {
                auto boxType = std::static_pointer_cast<BoxType>(basePtr->getType());
                structType = std::static_pointer_cast<StructType>(boxType->getInnerType());
            } else if (basePtr->getType()->isPointer()) {
                auto ptrType = std::static_pointer_cast<PointerType>(basePtr->getType());
                structType = std::static_pointer_cast<StructType>(ptrType->getPointeeType());
            } else {
                structType = std::static_pointer_cast<StructType>(basePtr->getType());
            }
        }

        const auto* member = structType->findMember(memExpr->getMember());
        int fieldIdx = structType->getMemberIndex(memExpr->getMember());

        if (member->type->isDynamic() && !val->getType()->isDynamic()) {
            val = boxToDynamic(val);
        }

        auto fieldPtr = std::make_shared<GetFieldPtrInst>(basePtr, fieldIdx, member->offset, member->type, nextTemp());
        emitInst(fieldPtr);
        emitInst(std::make_shared<StoreInst>(val, fieldPtr));

        m_lastValue = val;
        return;
    }

    if (auto* idxExpr = dynamic_cast<IndexExpr*>(&node.getTarget())) {
        idxExpr->getBase().accept(*this);
        auto baseVal = m_lastValue;

        if (baseVal->getType()->isDynamic()) {
            if (!val->getType()->isDynamic()) {
                val = boxToDynamic(val);
            }

            if (!idxExpr->hasIndex()) {
                // $arr[] = $val (append)
                auto appCall = std::make_shared<CallInst>("sph_array_append",
                                                          std::vector<std::shared_ptr<IRValue>>{baseVal, val},
                                                          Type::getVoid(), nextTemp());
                emitInst(appCall);
            } else {
                // $arr[$key] = $val
                idxExpr->getIndex().accept(*this);
                auto indexVal = m_lastValue;
                if (!indexVal->getType()->isDynamic()) {
                    indexVal = boxToDynamic(indexVal);
                }
                auto setCall = std::make_shared<CallInst>("sph_array_set",
                                                          std::vector<std::shared_ptr<IRValue>>{baseVal, indexVal, val},
                                                          Type::getVoid(), nextTemp());
                emitInst(setCall);
            }

            m_lastValue = val;
            return;
        }

        idxExpr->getIndex().accept(*this);
        auto indexVal = m_lastValue;

        if (baseVal->getType()->isVector()) {
            auto vecType = std::static_pointer_cast<VectorType>(baseVal->getType());
            auto elemType = vecType->getElementType();
            auto getCall = std::make_shared<CallInst>("sph_vec_get", std::vector<std::shared_ptr<IRValue>>{baseVal, indexVal}, PointerType::get(elemType), nextTemp());
            emitInst(getCall);
            emitInst(std::make_shared<StoreInst>(val, getCall));
            m_lastValue = val;
            return;
        }

        if (baseVal->getType()->isSimd()) {
            std::string typeName = baseVal->getType()->toString();
            if (typeName == "f32x4" && val->getType() != Type::getF32()) {
                if (val->isConstant()) val = ConstantFloat::get(std::stof(val->getName()), Type::getF32());
                else {
                    auto c = std::make_shared<CastInst>(val, Type::getF32(), nextTemp());
                    emitInst(c);
                    val = c;
                }
            } else if (typeName == "f64x2" && val->getType() != Type::getF64()) {
                if (val->isConstant()) val = ConstantFloat::get(std::stod(val->getName()), Type::getF64());
                else {
                    auto c = std::make_shared<CastInst>(val, Type::getF64(), nextTemp());
                    emitInst(c);
                    val = c;
                }
            } else if (typeName == "i32x4" && val->getType() != Type::getI32()) {
                if (val->isConstant()) val = ConstantInt::get(std::stoi(val->getName()), Type::getI32());
                else {
                    auto c = std::make_shared<CastInst>(val, Type::getI32(), nextTemp());
                    emitInst(c);
                    val = c;
                }
            } else if (typeName == "i64x2" && val->getType() != Type::getI64()) {
                if (val->isConstant()) val = ConstantInt::get(std::stoll(val->getName()), Type::getI64());
                else {
                    auto c = std::make_shared<CastInst>(val, Type::getI64(), nextTemp());
                    emitInst(c);
                    val = c;
                }
            }
            auto callInst = std::make_shared<CallInst>("sph_simd_" + typeName + "_set",
                std::vector<std::shared_ptr<IRValue>>{baseVal, indexVal, val}, Type::getVoid(), nextTemp());
            emitInst(callInst);
            m_lastValue = val;
            return;
        }

        if (baseVal->getType()->isSlice()) {
            auto sliceType = std::static_pointer_cast<SliceType>(baseVal->getType());
            size_t elemSize = sliceType->getElementType()->getSize();

            auto ptrField = std::make_shared<GetFieldPtrInst>(baseVal, 0, 0, Type::getI64(), nextTemp());
            emitInst(ptrField);
            auto dataPtr = std::make_shared<LoadInst>(ptrField, nextTemp());
            emitInst(dataPtr);

            auto elemPtr = std::make_shared<GetElementPtrInst>(dataPtr, indexVal, elemSize, sliceType->getElementType(), nextTemp());
            emitInst(elemPtr);
            emitInst(std::make_shared<StoreInst>(val, elemPtr));

            m_lastValue = val;
            return;
        }

        auto arrType = std::static_pointer_cast<ArrayType>(baseVal->getType());
        size_t elemSize = arrType->getElementType()->getSize();

        auto elemPtr = std::make_shared<GetElementPtrInst>(baseVal, indexVal, elemSize, arrType->getElementType(), nextTemp());
        emitInst(elemPtr);
        emitInst(std::make_shared<StoreInst>(val, elemPtr));

        m_lastValue = val;
        return;
    }
}

void IRGenerator::visit(StructInitExpr& node) {
    auto structType = std::static_pointer_cast<StructType>(node.getInferredType());
    auto allocaInst = std::make_shared<AllocaInst>(structType, nextTemp());
    emitInst(allocaInst);

    for (auto& f : node.getFields()) {
        const auto* member = structType->findMember(f.first);
        int fieldIdx = structType->getMemberIndex(f.first);
        f.second->accept(*this);
        auto val = m_lastValue;

        auto fieldPtr = std::make_shared<GetFieldPtrInst>(allocaInst, fieldIdx, member->offset, member->type, nextTemp());
        emitInst(fieldPtr);
        emitInst(std::make_shared<StoreInst>(val, fieldPtr));
    }

    m_lastValue = allocaInst;
}

void IRGenerator::visit(MemberAccessExpr& node) {
    if (auto* deref = dynamic_cast<UnaryExpr*>(&node.getBase()); deref && deref->getOp() == UnaryOp::Deref) {
        deref->getOperand().accept(*this);
        auto basePtr = m_lastValue;
        auto ptrType = std::static_pointer_cast<PointerType>(basePtr->getType());
        auto structType = std::static_pointer_cast<StructType>(ptrType->getPointeeType());
        const auto* member = structType->findMember(node.getMember());
        int fieldIdx = structType->getMemberIndex(node.getMember());

        auto fieldPtr = std::make_shared<GetFieldPtrInst>(basePtr, fieldIdx, member->offset, member->type, nextTemp());
        emitInst(fieldPtr);

        auto loadInst = std::make_shared<LoadInst>(fieldPtr, nextTemp());
        emitInst(loadInst);
        m_lastValue = loadInst;
        return;
    }

    node.getBase().accept(*this);
    auto baseVal = m_lastValue;

    if (baseVal->getType()->isDynamic()) {
        auto propName = ConstantString::get(node.getMember());
        auto getCall = std::make_shared<CallInst>("sph_object_get_property",
                                                  std::vector<std::shared_ptr<IRValue>>{baseVal, propName},
                                                  Type::getDynamic(), nextTemp());
        emitInst(getCall);
        m_lastValue = getCall;
        return;
    }

    if (baseVal->getType()->isSlice()) {
        if (node.getMember() == "len" || node.getMember() == "length") {
            auto lenField = std::make_shared<GetFieldPtrInst>(baseVal, 1, 8, Type::getI32(), nextTemp());
            emitInst(lenField);
            auto loadInst = std::make_shared<LoadInst>(lenField, nextTemp());
            emitInst(loadInst);
            m_lastValue = loadInst;
            return;
        }
    }

    std::shared_ptr<StructType> structType;
    if (baseVal->getType()->isRc()) {
        auto rcType = std::static_pointer_cast<RcType>(baseVal->getType());
        structType = std::static_pointer_cast<StructType>(rcType->getInnerType());
    } else if (baseVal->getType()->isBox()) {
        auto boxType = std::static_pointer_cast<BoxType>(baseVal->getType());
        structType = std::static_pointer_cast<StructType>(boxType->getInnerType());
    } else if (baseVal->getType()->isPointer()) {
        auto ptrType = std::static_pointer_cast<PointerType>(baseVal->getType());
        structType = std::static_pointer_cast<StructType>(ptrType->getPointeeType());
    } else {
        structType = std::static_pointer_cast<StructType>(baseVal->getType());
    }

    const auto* member = structType->findMember(node.getMember());
    int fieldIdx = structType->getMemberIndex(node.getMember());

    auto fieldPtr = std::make_shared<GetFieldPtrInst>(baseVal, fieldIdx, member->offset, member->type, nextTemp());
    emitInst(fieldPtr);

    auto loadInst = std::make_shared<LoadInst>(fieldPtr, nextTemp());
    emitInst(loadInst);
    m_lastValue = loadInst;
}

void IRGenerator::visit(ArrayLiteralExpr& node) {
    if (node.getInferredType()->isDynamic()) {
        auto arrVal = std::make_shared<CallInst>("sph_value_create_array_empty",
                                                 std::vector<std::shared_ptr<IRValue>>{},
                                                 Type::getDynamic(), nextTemp());
        emitInst(arrVal);

        for (auto& elem : node.getElements()) {
            elem.value->accept(*this);
            auto val = m_lastValue;
            if (!val->getType()->isDynamic()) {
                val = boxToDynamic(val);
            }

            if (elem.key) {
                elem.key->accept(*this);
                auto key = m_lastValue;
                if (!key->getType()->isDynamic()) {
                    key = boxToDynamic(key);
                }
                auto setCall = std::make_shared<CallInst>("sph_array_set",
                                                          std::vector<std::shared_ptr<IRValue>>{arrVal, key, val},
                                                          Type::getVoid(), nextTemp());
                emitInst(setCall);
            } else {
                auto appCall = std::make_shared<CallInst>("sph_array_append",
                                                          std::vector<std::shared_ptr<IRValue>>{arrVal, val},
                                                          Type::getVoid(), nextTemp());
                emitInst(appCall);
            }
        }

        m_lastValue = arrVal;
        return;
    }

    auto arrType = std::static_pointer_cast<ArrayType>(node.getInferredType());
    auto allocaInst = std::make_shared<AllocaInst>(arrType, nextTemp());
    emitInst(allocaInst);

    size_t elemSize = arrType->getElementType()->getSize();
    for (size_t i = 0; i < node.getElements().size(); ++i) {
        node.getElements()[i].value->accept(*this);
        auto val = m_lastValue;

        auto idxVal = ConstantInt::get(static_cast<int64_t>(i), Type::getI32());
        auto elemPtr = std::make_shared<GetElementPtrInst>(allocaInst, idxVal, elemSize, arrType->getElementType(), nextTemp());
        emitInst(elemPtr);
        emitInst(std::make_shared<StoreInst>(val, elemPtr));
    }

    m_lastValue = allocaInst;
}

void IRGenerator::visit(IndexExpr& node) {
    node.getBase().accept(*this);
    auto baseVal = m_lastValue;

    if (baseVal->getType()->isDynamic()) {
        std::shared_ptr<IRValue> indexVal = nullptr;
        if (node.hasIndex()) {
            node.getIndex().accept(*this);
            indexVal = m_lastValue;
            if (!indexVal->getType()->isDynamic()) {
                indexVal = boxToDynamic(indexVal);
            }
        } else {
            indexVal = boxToDynamic(nullptr);
        }

        auto getCall = std::make_shared<CallInst>("sph_array_get",
                                                  std::vector<std::shared_ptr<IRValue>>{baseVal, indexVal},
                                                  Type::getDynamic(), nextTemp());
        emitInst(getCall);
        m_lastValue = getCall;
        return;
    }

    node.getIndex().accept(*this);
    auto indexVal = m_lastValue;

    if (baseVal->getType()->isVector()) {
        auto vecType = std::static_pointer_cast<VectorType>(baseVal->getType());
        auto elemType = vecType->getElementType();
        auto getCall = std::make_shared<CallInst>("sph_vec_get", std::vector<std::shared_ptr<IRValue>>{baseVal, indexVal}, PointerType::get(elemType), nextTemp());
        emitInst(getCall);
        auto loadInst = std::make_shared<LoadInst>(getCall, nextTemp());
        emitInst(loadInst);
        m_lastValue = loadInst;
        return;
    }

    if (baseVal->getType()->isSimd()) {
        std::string typeName = baseVal->getType()->toString();
        auto retType = (typeName == "f32x4") ? Type::getF32() :
                       (typeName == "i32x4") ? Type::getI32() :
                       (typeName == "f64x2") ? Type::getF64() : Type::getI64();
        auto getCall = std::make_shared<CallInst>("sph_simd_" + typeName + "_get",
            std::vector<std::shared_ptr<IRValue>>{baseVal, indexVal}, retType, nextTemp());
        emitInst(getCall);
        m_lastValue = getCall;
        return;
    }

    if (baseVal->getType()->isSlice()) {
        auto sliceType = std::static_pointer_cast<SliceType>(baseVal->getType());
        size_t elemSize = sliceType->getElementType()->getSize();

        auto ptrField = std::make_shared<GetFieldPtrInst>(baseVal, 0, 0, Type::getI64(), nextTemp());
        emitInst(ptrField);
        auto dataPtr = std::make_shared<LoadInst>(ptrField, nextTemp());
        emitInst(dataPtr);

        auto elemPtr = std::make_shared<GetElementPtrInst>(dataPtr, indexVal, elemSize, sliceType->getElementType(), nextTemp());
        emitInst(elemPtr);

        auto loadInst = std::make_shared<LoadInst>(elemPtr, nextTemp());
        emitInst(loadInst);
        m_lastValue = loadInst;
        return;
    }

    auto arrType = std::static_pointer_cast<ArrayType>(baseVal->getType());
    size_t elemSize = arrType->getElementType()->getSize();

    auto elemPtr = std::make_shared<GetElementPtrInst>(baseVal, indexVal, elemSize, arrType->getElementType(), nextTemp());
    emitInst(elemPtr);

    auto loadInst = std::make_shared<LoadInst>(elemPtr, nextTemp());
    emitInst(loadInst);
    m_lastValue = loadInst;
}

void IRGenerator::visit(RangeExpr& /*node*/) {
    // RangeExpr is evaluated inside visit(SliceExpr&)
}

void IRGenerator::visit(SliceExpr& node) {
    node.getBase().accept(*this);
    auto baseVal = m_lastValue;

    auto sliceType = std::static_pointer_cast<SliceType>(node.getInferredType());
    size_t elemSize = sliceType->getElementType()->getSize();

    auto sliceAlloca = std::make_shared<AllocaInst>(sliceType, nextTemp());
    emitInst(sliceAlloca);

    std::shared_ptr<IRValue> startVal;
    if (node.getRange().hasStart()) {
        const_cast<ExprAST*>(node.getRange().getStart())->accept(*this);
        startVal = m_lastValue;
    } else {
        startVal = ConstantInt::get(0, Type::getI32());
    }

    std::shared_ptr<IRValue> endVal;
    if (node.getRange().hasEnd()) {
        const_cast<ExprAST*>(node.getRange().getEnd())->accept(*this);
        endVal = m_lastValue;
    } else {
        if (baseVal->getType()->isArray()) {
            auto arrType = std::static_pointer_cast<ArrayType>(baseVal->getType());
            endVal = ConstantInt::get(static_cast<int64_t>(arrType->getElementCount()), Type::getI32());
        } else if (baseVal->getType()->isSlice()) {
            auto lenField = std::make_shared<GetFieldPtrInst>(baseVal, 1, 8, Type::getI32(), nextTemp());
            emitInst(lenField);
            auto loadLen = std::make_shared<LoadInst>(lenField, nextTemp());
            emitInst(loadLen);
            endVal = loadLen;
        } else {
            endVal = startVal;
        }
    }

    auto lenVal = std::make_shared<BinaryInst>(IROpcode::Sub, endVal, startVal, Type::getI32(), nextTemp());
    emitInst(lenVal);

    std::shared_ptr<GetElementPtrInst> dataPtr;
    if (baseVal->getType()->isSlice()) {
        auto ptrField = std::make_shared<GetFieldPtrInst>(baseVal, 0, 0, Type::getI64(), nextTemp());
        emitInst(ptrField);
        auto baseDataPtr = std::make_shared<LoadInst>(ptrField, nextTemp());
        emitInst(baseDataPtr);
        dataPtr = std::make_shared<GetElementPtrInst>(baseDataPtr, startVal, elemSize, sliceType->getElementType(), nextTemp());
        emitInst(dataPtr);
    } else {
        dataPtr = std::make_shared<GetElementPtrInst>(baseVal, startVal, elemSize, sliceType->getElementType(), nextTemp());
        emitInst(dataPtr);
    }

    auto ptrFieldSlot = std::make_shared<GetFieldPtrInst>(sliceAlloca, 0, 0, Type::getI64(), nextTemp());
    emitInst(ptrFieldSlot);
    emitInst(std::make_shared<StoreInst>(dataPtr, ptrFieldSlot));

    auto lenFieldSlot = std::make_shared<GetFieldPtrInst>(sliceAlloca, 1, 8, Type::getI32(), nextTemp());
    emitInst(lenFieldSlot);
    emitInst(std::make_shared<StoreInst>(lenVal, lenFieldSlot));

    m_lastValue = sliceAlloca;
}

void IRGenerator::visit(ScopedAccessExpr& node) {
    if (node.getScopeName() == "PDO") {
        int64_t val = 2; // SPH_PDO_FETCH_ASSOC
        if (node.getMemberName() == "FETCH_ASSOC") val = 2;
        else if (node.getMemberName() == "FETCH_NUM") val = 3;
        else if (node.getMemberName() == "FETCH_BOTH") val = 4;
        m_lastValue = ConstantInt::get(val, Type::getI32());
        return;
    }
    if (node.getScopeName() == "MemoryOrder") {
        int64_t val = 4; // SeqCst
        if (node.getMemberName() == "Relaxed") val = 0;
        else if (node.getMemberName() == "Acquire") val = 1;
        else if (node.getMemberName() == "Release") val = 2;
        else if (node.getMemberName() == "AcqRel") val = 3;
        else if (node.getMemberName() == "SeqCst") val = 4;
        m_lastValue = ConstantInt::get(val, Type::getI32());
        return;
    }

    int64_t val = 0;
    auto scopeIt = m_enumVariants.find(node.getScopeName());
    if (scopeIt != m_enumVariants.end()) {
        auto varIt = scopeIt->second.find(node.getMemberName());
        if (varIt != scopeIt->second.end()) {
            val = varIt->second;
        }
    }
    auto enumType = resolveType(node.getScopeName());
    m_lastValue = ConstantInt::get(val, enumType ? enumType : Type::getI32());
}

void IRGenerator::visit(BinaryExpr& node) {
    LocGuard locGuard(*this, node.getSpan().start);
    node.getLhs().accept(*this);
    auto lhs = m_lastValue;

    node.getRhs().accept(*this);
    auto rhs = m_lastValue;

    if (node.getOp() == BinaryOp::Concat) {
        if (lhs->getType()->isString() && rhs->getType()->isString()) {
            auto concatCall = std::make_shared<CallInst>("sph_string_concat",
                                                         std::vector<std::shared_ptr<IRValue>>{lhs, rhs},
                                                         Type::getString(), nextTemp());
            emitInst(concatCall);
            m_lastValue = concatCall;
            return;
        }
        auto dynLhs = boxToDynamic(lhs);
        auto dynRhs = boxToDynamic(rhs);
        auto concatCall = std::make_shared<CallInst>("sph_dynamic_concat",
                                                     std::vector<std::shared_ptr<IRValue>>{dynLhs, dynRhs},
                                                     Type::getDynamic(), nextTemp());
        emitInst(concatCall);
        if (node.getInferredType() && node.getInferredType()->isString()) {
            auto toStrCall = std::make_shared<CallInst>("sph_value_to_string",
                                                        std::vector<std::shared_ptr<IRValue>>{concatCall},
                                                        Type::getString(), nextTemp());
            emitInst(toStrCall);
            m_lastValue = toStrCall;
        } else {
            m_lastValue = concatCall;
        }
        return;
    }

    if (lhs->getType()->isDynamic() || rhs->getType()->isDynamic()) {
        auto dynLhs = boxToDynamic(lhs);
        auto dynRhs = boxToDynamic(rhs);

        std::string callee;
        std::shared_ptr<Type> retType = Type::getDynamic();

        switch (node.getOp()) {
            case BinaryOp::Add: callee = "sph_dynamic_add"; break;
            case BinaryOp::Sub: callee = "sph_dynamic_sub"; break;
            case BinaryOp::Mul: callee = "sph_dynamic_mul"; break;
            case BinaryOp::Div: callee = "sph_dynamic_div"; break;
            case BinaryOp::Mod: callee = "sph_dynamic_mod"; break;
            case BinaryOp::Equal: callee = "sph_dynamic_eq"; retType = Type::getBool(); break;
            case BinaryOp::NotEqual: callee = "sph_dynamic_ne"; retType = Type::getBool(); break;
            case BinaryOp::Less: callee = "sph_dynamic_lt"; retType = Type::getBool(); break;
            case BinaryOp::LessEqual: callee = "sph_dynamic_le"; retType = Type::getBool(); break;
            case BinaryOp::Greater: callee = "sph_dynamic_gt"; retType = Type::getBool(); break;
            case BinaryOp::GreaterEqual: callee = "sph_dynamic_ge"; retType = Type::getBool(); break;
            default: callee = "sph_dynamic_add"; break;
        }

        auto call = std::make_shared<CallInst>(callee,
                                              std::vector<std::shared_ptr<IRValue>>{dynLhs, dynRhs},
                                              retType, nextTemp());
        emitInst(call);
        m_lastValue = call;
        return;
    }

    if (lhs && lhs->getType()->isSimd() && rhs && rhs->getType()->isSimd()) {
        std::string typeName = lhs->getType()->toString();
        std::string opName;
        switch (node.getOp()) {
            case BinaryOp::Add: opName = "add"; break;
            case BinaryOp::Sub: opName = "sub"; break;
            case BinaryOp::Mul: opName = "mul"; break;
            case BinaryOp::Div: opName = "div"; break;
            default: break;
        }
        if (!opName.empty()) {
            auto resAlloc = std::make_shared<AllocaInst>(lhs->getType(), nextTemp());
            emitInst(resAlloc);
            auto call = std::make_shared<CallInst>("sph_simd_" + typeName + "_" + opName,
                std::vector<std::shared_ptr<IRValue>>{resAlloc, lhs, rhs}, Type::getVoid(), nextTemp());
            emitInst(call);
            m_lastValue = resAlloc;
            return;
        }
    }

    // Pointer arithmetic & string operations
    if (node.getOp() == BinaryOp::Add) {
        if (lhs->getType()->isString() && rhs->getType()->isString()) {
            auto concatCall = std::make_shared<CallInst>("sph_string_concat", std::vector<std::shared_ptr<IRValue>>{lhs, rhs}, Type::getString(), nextTemp());
            emitInst(concatCall);
            m_lastValue = concatCall;
            return;
        }
        if (lhs->getType()->isPointer() && rhs->getType()->isInteger()) {
            size_t elemSize = std::static_pointer_cast<PointerType>(lhs->getType())->getPointeeType()->getSize();
            if (elemSize == 0) elemSize = 1;
            auto gep = std::make_shared<GetElementPtrInst>(lhs, rhs, elemSize, lhs->getType(), nextTemp());
            emitInst(gep);
            m_lastValue = gep;
            return;
        }
        if (lhs->getType()->isInteger() && rhs->getType()->isPointer()) {
            size_t elemSize = std::static_pointer_cast<PointerType>(rhs->getType())->getPointeeType()->getSize();
            if (elemSize == 0) elemSize = 1;
            auto gep = std::make_shared<GetElementPtrInst>(rhs, lhs, elemSize, rhs->getType(), nextTemp());
            emitInst(gep);
            m_lastValue = gep;
            return;
        }
    } else if (node.getOp() == BinaryOp::Sub) {
        if (lhs->getType()->isPointer() && rhs->getType()->isInteger()) {
            auto negRhs = std::make_shared<UnaryInst>(IROpcode::Negate, rhs, rhs->getType(), nextTemp());
            emitInst(negRhs);
            size_t elemSize = std::static_pointer_cast<PointerType>(lhs->getType())->getPointeeType()->getSize();
            if (elemSize == 0) elemSize = 1;
            auto gep = std::make_shared<GetElementPtrInst>(lhs, negRhs, elemSize, lhs->getType(), nextTemp());
            emitInst(gep);
            m_lastValue = gep;
            return;
        }
        if (lhs->getType()->isPointer() && rhs->getType()->isPointer()) {
            auto diffBytes = std::make_shared<BinaryInst>(IROpcode::Sub, lhs, rhs, Type::getI64(), nextTemp());
            emitInst(diffBytes);
            size_t elemSize = std::static_pointer_cast<PointerType>(lhs->getType())->getPointeeType()->getSize();
            if (elemSize <= 1) {
                m_lastValue = diffBytes;
                return;
            }
            auto elemSizeConst = ConstantInt::get(static_cast<int64_t>(elemSize), Type::getI64());
            auto diffElems = std::make_shared<BinaryInst>(IROpcode::Div, diffBytes, elemSizeConst, Type::getI64(), nextTemp());
            emitInst(diffElems);
            m_lastValue = diffElems;
            return;
        }
    } else if (node.getOp() == BinaryOp::Equal || node.getOp() == BinaryOp::NotEqual) {
        if (lhs->getType()->isString() && rhs->getType()->isString()) {
            auto eqCall = std::make_shared<CallInst>("sph_string_eq", std::vector<std::shared_ptr<IRValue>>{lhs, rhs}, Type::getI32(), nextTemp());
            emitInst(eqCall);
            auto one = ConstantInt::get(1, Type::getI32());
            IROpcode compOp = (node.getOp() == BinaryOp::Equal) ? IROpcode::Equal : IROpcode::NotEqual;
            auto cmpInst = std::make_shared<BinaryInst>(compOp, eqCall, one, Type::getBool(), nextTemp());
            emitInst(cmpInst);
            m_lastValue = cmpInst;
            return;
        }
    }

    if (lhs->getType()->isInteger() && rhs->getType()->isInteger() && lhs->getType()->getSize() != rhs->getType()->getSize()) {
        if (lhs->getType()->getSize() < rhs->getType()->getSize()) {
            auto castInst = std::make_shared<CastInst>(lhs, rhs->getType(), nextTemp());
            emitInst(castInst);
            lhs = castInst;
        } else {
            auto castInst = std::make_shared<CastInst>(rhs, lhs->getType(), nextTemp());
            emitInst(castInst);
            rhs = castInst;
        }
    }

    IROpcode op;
    std::shared_ptr<Type> resultType;

    switch (node.getOp()) {
        case BinaryOp::Add:          op = IROpcode::Add;          resultType = lhs->getType(); break;
        case BinaryOp::Sub:          op = IROpcode::Sub;          resultType = lhs->getType(); break;
        case BinaryOp::Mul:          op = IROpcode::Mul;          resultType = lhs->getType(); break;
        case BinaryOp::Div:          op = IROpcode::Div;          resultType = lhs->getType(); break;
        case BinaryOp::Mod:          op = IROpcode::Mod;          resultType = lhs->getType(); break;
        case BinaryOp::BitwiseAnd:   op = IROpcode::BitwiseAnd;   resultType = lhs->getType(); break;
        case BinaryOp::BitwiseOr:    op = IROpcode::BitwiseOr;    resultType = lhs->getType(); break;
        case BinaryOp::BitwiseXor:   op = IROpcode::BitwiseXor;   resultType = lhs->getType(); break;
        case BinaryOp::ShiftLeft:    op = IROpcode::ShiftLeft;    resultType = lhs->getType(); break;
        case BinaryOp::ShiftRight:   op = IROpcode::ShiftRight;   resultType = lhs->getType(); break;
        case BinaryOp::Equal:        op = IROpcode::Equal;        resultType = Type::getBool(); break;
        case BinaryOp::NotEqual:     op = IROpcode::NotEqual;     resultType = Type::getBool(); break;
        case BinaryOp::Less:         op = IROpcode::Less;         resultType = Type::getBool(); break;
        case BinaryOp::LessEqual:    op = IROpcode::LessEqual;    resultType = Type::getBool(); break;
        case BinaryOp::Greater:      op = IROpcode::Greater;      resultType = Type::getBool(); break;
        case BinaryOp::GreaterEqual: op = IROpcode::GreaterEqual; resultType = Type::getBool(); break;
        case BinaryOp::LogicalAnd:   op = IROpcode::LogicalAnd;   resultType = Type::getBool(); break;
        case BinaryOp::LogicalOr:    op = IROpcode::LogicalOr;    resultType = Type::getBool(); break;
        case BinaryOp::Concat:       op = IROpcode::Add;          resultType = lhs->getType(); break;
    }

    auto binInst = std::make_shared<BinaryInst>(op, lhs, rhs, resultType, nextTemp());
    emitInst(binInst);
    m_lastValue = binInst;
}

void IRGenerator::visit(UnaryExpr& node) {
    LocGuard locGuard(*this, node.getSpan().start);
    if (node.getOp() == UnaryOp::AddressOf) {
        auto ptrType = node.getInferredType();
        if (!ptrType) {
            ptrType = PointerType::get(Type::getI32());
        }

        if (auto* varExpr = dynamic_cast<VariableExpr*>(&node.getOperand())) {
            auto slot = getVariable(varExpr->getName());
            if (slot) {
                auto addrInst = std::make_shared<AddressOfInst>(slot, ptrType, nextTemp());
                emitInst(addrInst);
                m_lastValue = addrInst;
                return;
            }
        } else if (auto* memExpr = dynamic_cast<MemberAccessExpr*>(&node.getOperand())) {
            std::shared_ptr<IRValue> baseVal;
            std::shared_ptr<StructType> structType;
            if (auto* deref = dynamic_cast<UnaryExpr*>(&memExpr->getBase()); deref && deref->getOp() == UnaryOp::Deref) {
                deref->getOperand().accept(*this);
                baseVal = m_lastValue;
                auto pType = std::static_pointer_cast<PointerType>(baseVal->getType());
                structType = std::static_pointer_cast<StructType>(pType->getPointeeType());
            } else {
                memExpr->getBase().accept(*this);
                baseVal = m_lastValue;
                structType = std::static_pointer_cast<StructType>(baseVal->getType());
            }
            const auto* member = structType->findMember(memExpr->getMember());
            int fieldIdx = structType->getMemberIndex(memExpr->getMember());

            auto fieldPtr = std::make_shared<GetFieldPtrInst>(baseVal, fieldIdx, member->offset, member->type, nextTemp());
            emitInst(fieldPtr);
            auto addrInst = std::make_shared<AddressOfInst>(fieldPtr, ptrType, nextTemp());
            emitInst(addrInst);
            m_lastValue = addrInst;
            return;
        } else if (auto* idxExpr = dynamic_cast<IndexExpr*>(&node.getOperand())) {
            idxExpr->getBase().accept(*this);
            auto baseVal = m_lastValue;
            idxExpr->getIndex().accept(*this);
            auto indexVal = m_lastValue;

            if (baseVal->getType()->isVector()) {
                auto vecType = std::static_pointer_cast<VectorType>(baseVal->getType());
                auto elemType = vecType->getElementType();
                auto getCall = std::make_shared<CallInst>("sph_vec_get", std::vector<std::shared_ptr<IRValue>>{baseVal, indexVal}, PointerType::get(elemType), nextTemp());
                emitInst(getCall);
                m_lastValue = getCall;
                return;
            } else if (baseVal->getType()->isSlice()) {
                auto sliceType = std::static_pointer_cast<SliceType>(baseVal->getType());
                size_t elemSize = sliceType->getElementType()->getSize();
                auto ptrField = std::make_shared<GetFieldPtrInst>(baseVal, 0, 0, Type::getI64(), nextTemp());
                emitInst(ptrField);
                auto dataPtr = std::make_shared<LoadInst>(ptrField, nextTemp());
                emitInst(dataPtr);
                auto elemPtr = std::make_shared<GetElementPtrInst>(dataPtr, indexVal, elemSize, sliceType->getElementType(), nextTemp());
                emitInst(elemPtr);
                auto addrInst = std::make_shared<AddressOfInst>(elemPtr, ptrType, nextTemp());
                emitInst(addrInst);
                m_lastValue = addrInst;
                return;
            } else if (baseVal->getType()->isArray()) {
                auto arrType = std::static_pointer_cast<ArrayType>(baseVal->getType());
                size_t elemSize = arrType->getElementType()->getSize();
                auto elemPtr = std::make_shared<GetElementPtrInst>(baseVal, indexVal, elemSize, arrType->getElementType(), nextTemp());
                emitInst(elemPtr);
                auto addrInst = std::make_shared<AddressOfInst>(elemPtr, ptrType, nextTemp());
                emitInst(addrInst);
                m_lastValue = addrInst;
                return;
            }
        } else if (auto* innerUnary = dynamic_cast<UnaryExpr*>(&node.getOperand())) {
            if (innerUnary->getOp() == UnaryOp::Deref) {
                innerUnary->getOperand().accept(*this);
                return;
            }
        }
    } else if (node.getOp() == UnaryOp::Deref) {
        node.getOperand().accept(*this);
        auto ptrVal = m_lastValue;
        auto derefType = node.getInferredType();
        if (!derefType && ptrVal->getType()->isPointer()) {
            derefType = std::static_pointer_cast<PointerType>(ptrVal->getType())->getPointeeType();
        }
        if (!derefType) derefType = Type::getI32();

        auto loadInst = std::make_shared<LoadInst>(derefType, ptrVal, nextTemp());
        emitInst(loadInst);
        m_lastValue = loadInst;
        return;
    } else {
        node.getOperand().accept(*this);
        auto operand = m_lastValue;

        IROpcode op;
        std::shared_ptr<Type> resType;
        if (node.getOp() == UnaryOp::Negate) {
            op = IROpcode::Negate;
            resType = operand->getType();
        } else if (node.getOp() == UnaryOp::BitwiseNot) {
            op = IROpcode::BitwiseNot;
            resType = operand->getType();
        } else {
            op = IROpcode::LogicalNot;
            resType = Type::getBool();
        }

        auto unInst = std::make_shared<UnaryInst>(op, operand, resType, nextTemp());
        emitInst(unInst);
        m_lastValue = unInst;
    }
}

void IRGenerator::visit(CastExpr& node) {
    LocGuard locGuard(*this, node.getSpan().start);
    node.getExpr().accept(*this);
    auto operand = m_lastValue;
    auto targetType = node.getInferredType();
    if (!targetType) {
        targetType = Type::getI32();
    }

    if (operand && operand->getType()->isDynamic() && !targetType->isDynamic()) {
        m_lastValue = unboxFromDynamic(operand, targetType);
        return;
    }
    if (operand && !operand->getType()->isDynamic() && targetType->isDynamic()) {
        m_lastValue = boxToDynamic(operand);
        return;
    }

    auto castInst = std::make_shared<CastInst>(operand, targetType, nextTemp());
    emitInst(castInst);
    m_lastValue = castInst;
}

void IRGenerator::visit(CallExpr& node) {
    LocGuard locGuard(*this, node.getSpan().start);
    std::string callee = node.getCallee();

    if (callee == "var_dump") {
        if (!node.getArgs().empty()) {
            node.getArgs()[0]->accept(*this);
            auto val = m_lastValue;
            if (!val->getType()->isDynamic()) {
                val = boxToDynamic(val);
            }
            auto callInst = std::make_shared<CallInst>("sph_var_dump",
                                                       std::vector<std::shared_ptr<IRValue>>{val},
                                                       Type::getVoid(), nextTemp());
            emitInst(callInst);
            m_lastValue = callInst;
            return;
        }
    }

    if (callee == "intval" || callee == "as_i32") {
        if (!node.getArgs().empty()) {
            node.getArgs()[0]->accept(*this);
            auto val = m_lastValue;
            if (val->getType()->isDynamic()) {
                val = unboxFromDynamic(val, Type::getI32());
            } else if (!val->getType()->isI32()) {
                auto c = std::make_shared<CastInst>(val, Type::getI32(), nextTemp());
                emitInst(c);
                val = c;
            }
            m_lastValue = val;
            return;
        }
    }

    if (callee == "as_i64") {
        if (!node.getArgs().empty()) {
            node.getArgs()[0]->accept(*this);
            auto val = m_lastValue;
            if (val->getType()->isDynamic()) {
                val = unboxFromDynamic(val, Type::getI64());
            } else if (!val->getType()->isI64()) {
                auto c = std::make_shared<CastInst>(val, Type::getI64(), nextTemp());
                emitInst(c);
                val = c;
            }
            m_lastValue = val;
            return;
        }
    }

    if (callee == "floatval" || callee == "as_f64") {
        if (!node.getArgs().empty()) {
            node.getArgs()[0]->accept(*this);
            auto val = m_lastValue;
            if (val->getType()->isDynamic()) {
                val = unboxFromDynamic(val, Type::getF64());
            } else if (!val->getType()->isF64()) {
                auto c = std::make_shared<CastInst>(val, Type::getF64(), nextTemp());
                emitInst(c);
                val = c;
            }
            m_lastValue = val;
            return;
        }
    }

    if (callee == "as_f32") {
        if (!node.getArgs().empty()) {
            node.getArgs()[0]->accept(*this);
            auto val = m_lastValue;
            if (val->getType()->isDynamic()) {
                val = unboxFromDynamic(val, Type::getF32());
            } else if (!val->getType()->isF32()) {
                auto c = std::make_shared<CastInst>(val, Type::getF32(), nextTemp());
                emitInst(c);
                val = c;
            }
            m_lastValue = val;
            return;
        }
    }

    if (callee == "strval" || callee == "as_string") {
        if (!node.getArgs().empty()) {
            node.getArgs()[0]->accept(*this);
            auto val = m_lastValue;
            if (val->getType()->isDynamic()) {
                val = unboxFromDynamic(val, Type::getString());
            }
            m_lastValue = val;
            return;
        }
    }

    if (callee == "boolval" || callee == "as_bool") {
        if (!node.getArgs().empty()) {
            node.getArgs()[0]->accept(*this);
            auto val = m_lastValue;
            if (val->getType()->isDynamic()) {
                val = unboxFromDynamic(val, Type::getBool());
            }
            m_lastValue = val;
            return;
        }
    }

    if (callee == "count") {
        if (!node.getArgs().empty()) {
            node.getArgs()[0]->accept(*this);
            auto val = m_lastValue;
            if (val->getType()->isArray()) {
                auto arrType = std::static_pointer_cast<ArrayType>(val->getType());
                m_lastValue = ConstantInt::get(static_cast<int64_t>(arrType->getSize()), Type::getI64());
                return;
            }
            if (val->getType()->isSlice()) {
                auto lenField = std::make_shared<GetFieldPtrInst>(val, 1, 8, Type::getI32(), nextTemp());
                emitInst(lenField);
                auto lenVal = std::make_shared<LoadInst>(Type::getI32(), lenField, nextTemp());
                emitInst(lenVal);
                auto castInst = std::make_shared<CastInst>(lenVal, Type::getI64(), nextTemp());
                emitInst(castInst);
                m_lastValue = castInst;
                return;
            }
            if (!val->getType()->isDynamic()) {
                val = boxToDynamic(val);
            }
            auto callInst = std::make_shared<CallInst>("sph_array_count",
                                                       std::vector<std::shared_ptr<IRValue>>{val},
                                                       Type::getI64(), nextTemp());
            emitInst(callInst);
            m_lastValue = callInst;
            return;
        }
    }

    if (callee == "is_array" || callee == "is_string" || callee == "is_int" ||
        callee == "is_float" || callee == "is_bool" || callee == "is_null" ||
        callee == "is_object") {
        if (!node.getArgs().empty()) {
            node.getArgs()[0]->accept(*this);
            auto val = m_lastValue;
            if (!val->getType()->isDynamic()) {
                val = boxToDynamic(val);
            }
            std::string c_fn = "sph_" + callee;
            auto callInst = std::make_shared<CallInst>(c_fn,
                                                       std::vector<std::shared_ptr<IRValue>>{val},
                                                       Type::getBool(), nextTemp());
            emitInst(callInst);
            m_lastValue = callInst;
            return;
        }
    }

    if (callee == "get_class") {
        if (!node.getArgs().empty()) {
            node.getArgs()[0]->accept(*this);
            auto val = m_lastValue;
            if (!val->getType()->isDynamic()) {
                val = boxToDynamic(val);
            }
            auto callInst = std::make_shared<CallInst>("sph_object_get_class",
                                                       std::vector<std::shared_ptr<IRValue>>{val},
                                                       Type::getString(), nextTemp());
            emitInst(callInst);
            m_lastValue = callInst;
            return;
        }
    }

    if (callee == "property_exists") {
        if (node.getArgs().size() == 2) {
            node.getArgs()[0]->accept(*this);
            auto objVal = m_lastValue;
            if (!objVal->getType()->isDynamic()) {
                objVal = boxToDynamic(objVal);
            }
            node.getArgs()[1]->accept(*this);
            auto propVal = m_lastValue;
            auto callInst = std::make_shared<CallInst>("sph_property_exists",
                                                       std::vector<std::shared_ptr<IRValue>>{objVal, propVal},
                                                       Type::getBool(), nextTemp());
            emitInst(callInst);
            m_lastValue = callInst;
            return;
        }
    }

    if (callee == "array_push") {
        if (node.getArgs().size() == 2) {
            node.getArgs()[0]->accept(*this);
            auto arrVal = m_lastValue;
            if (!arrVal->getType()->isDynamic()) {
                arrVal = boxToDynamic(arrVal);
            }
            node.getArgs()[1]->accept(*this);
            auto elemVal = m_lastValue;
            if (!elemVal->getType()->isDynamic()) {
                elemVal = boxToDynamic(elemVal);
            }
            auto callInst = std::make_shared<CallInst>("sph_array_push",
                                                       std::vector<std::shared_ptr<IRValue>>{arrVal, elemVal},
                                                       Type::getI64(), nextTemp());
            emitInst(callInst);
            m_lastValue = callInst;
            return;
        }
    }

    if (callee == "array_pop" || callee == "array_shift" ||
        callee == "array_keys" || callee == "array_values") {
        if (!node.getArgs().empty()) {
            node.getArgs()[0]->accept(*this);
            auto arrVal = m_lastValue;
            if (!arrVal->getType()->isDynamic()) {
                arrVal = boxToDynamic(arrVal);
            }
            std::string c_fn = "sph_" + callee;
            auto callInst = std::make_shared<CallInst>(c_fn,
                                                       std::vector<std::shared_ptr<IRValue>>{arrVal},
                                                       Type::getDynamic(), nextTemp());
            emitInst(callInst);
            m_lastValue = callInst;
            return;
        }
    }

    if (callee == "in_array") {
        if (node.getArgs().size() == 2) {
            node.getArgs()[0]->accept(*this);
            auto needleVal = m_lastValue;
            if (!needleVal->getType()->isDynamic()) {
                needleVal = boxToDynamic(needleVal);
            }
            node.getArgs()[1]->accept(*this);
            auto haystackVal = m_lastValue;
            if (!haystackVal->getType()->isDynamic()) {
                haystackVal = boxToDynamic(haystackVal);
            }
            auto callInst = std::make_shared<CallInst>("sph_in_array",
                                                       std::vector<std::shared_ptr<IRValue>>{needleVal, haystackVal},
                                                       Type::getBool(), nextTemp());
            emitInst(callInst);
            m_lastValue = callInst;
            return;
        }
    }

    if (callee == "array_key_exists") {
        if (node.getArgs().size() == 2) {
            node.getArgs()[0]->accept(*this);
            auto kVal = m_lastValue;
            if (!kVal->getType()->isDynamic()) {
                kVal = boxToDynamic(kVal);
            }
            node.getArgs()[1]->accept(*this);
            auto aVal = m_lastValue;
            if (!aVal->getType()->isDynamic()) {
                aVal = boxToDynamic(aVal);
            }
            auto callInst = std::make_shared<CallInst>("sph_array_key_exists",
                                                       std::vector<std::shared_ptr<IRValue>>{kVal, aVal},
                                                       Type::getBool(), nextTemp());
            emitInst(callInst);
            m_lastValue = callInst;
            return;
        }
    }

    if (callee == "strlen") {
        if (!node.getArgs().empty()) {
            node.getArgs()[0]->accept(*this);
            auto strVal = m_lastValue;
            if (!strVal->getType()->isDynamic()) {
                strVal = boxToDynamic(strVal);
            }
            auto callInst = std::make_shared<CallInst>("sph_php_strlen",
                                                       std::vector<std::shared_ptr<IRValue>>{strVal},
                                                       Type::getI64(), nextTemp());
            emitInst(callInst);
            m_lastValue = callInst;
            return;
        }
    }

    if (callee == "substr") {
        if (node.getArgs().size() >= 2) {
            node.getArgs()[0]->accept(*this);
            auto strVal = m_lastValue;
            if (!strVal->getType()->isDynamic()) {
                strVal = boxToDynamic(strVal);
            }
            node.getArgs()[1]->accept(*this);
            auto startVal = m_lastValue;
            if (startVal->getType()->isDynamic()) {
                startVal = unboxFromDynamic(startVal, Type::getI64());
            } else if (startVal->getType()->isI32()) {
                auto c = std::make_shared<CastInst>(startVal, Type::getI64(), nextTemp());
                emitInst(c);
                startVal = c;
            }
            std::shared_ptr<IRValue> lenVal;
            if (node.getArgs().size() >= 3) {
                node.getArgs()[2]->accept(*this);
                lenVal = m_lastValue;
                if (lenVal->getType()->isDynamic()) {
                    lenVal = unboxFromDynamic(lenVal, Type::getI64());
                } else if (lenVal->getType()->isI32()) {
                    auto c = std::make_shared<CastInst>(lenVal, Type::getI64(), nextTemp());
                    emitInst(c);
                    lenVal = c;
                }
            } else {
                lenVal = ConstantInt::get(999999999, Type::getI64());
            }
            auto callInst = std::make_shared<CallInst>("sph_php_substr",
                                                       std::vector<std::shared_ptr<IRValue>>{strVal, startVal, lenVal},
                                                       Type::getString(), nextTemp());
            emitInst(callInst);
            m_lastValue = callInst;
            return;
        }
    }

    if (callee == "strpos") {
        if (node.getArgs().size() == 2) {
            node.getArgs()[0]->accept(*this);
            auto hVal = m_lastValue;
            if (!hVal->getType()->isDynamic()) {
                hVal = boxToDynamic(hVal);
            }
            node.getArgs()[1]->accept(*this);
            auto nVal = m_lastValue;
            if (!nVal->getType()->isDynamic()) {
                nVal = boxToDynamic(nVal);
            }
            auto callInst = std::make_shared<CallInst>("sph_php_strpos",
                                                       std::vector<std::shared_ptr<IRValue>>{hVal, nVal},
                                                       Type::getI64(), nextTemp());
            emitInst(callInst);
            m_lastValue = callInst;
            return;
        }
    }

    if (callee == "explode") {
        if (node.getArgs().size() == 2) {
            node.getArgs()[0]->accept(*this);
            auto dVal = m_lastValue;
            if (!dVal->getType()->isDynamic()) {
                dVal = boxToDynamic(dVal);
            }
            node.getArgs()[1]->accept(*this);
            auto sVal = m_lastValue;
            if (!sVal->getType()->isDynamic()) {
                sVal = boxToDynamic(sVal);
            }
            auto callInst = std::make_shared<CallInst>("sph_php_explode",
                                                       std::vector<std::shared_ptr<IRValue>>{dVal, sVal},
                                                       Type::getDynamic(), nextTemp());
            emitInst(callInst);
            m_lastValue = callInst;
            return;
        }
    }

    if (callee == "implode") {
        if (node.getArgs().size() == 2) {
            node.getArgs()[0]->accept(*this);
            auto gVal = m_lastValue;
            if (!gVal->getType()->isDynamic()) {
                gVal = boxToDynamic(gVal);
            }
            node.getArgs()[1]->accept(*this);
            auto aVal = m_lastValue;
            if (!aVal->getType()->isDynamic()) {
                aVal = boxToDynamic(aVal);
            }
            auto callInst = std::make_shared<CallInst>("sph_php_implode",
                                                       std::vector<std::shared_ptr<IRValue>>{gVal, aVal},
                                                       Type::getString(), nextTemp());
            emitInst(callInst);
            m_lastValue = callInst;
            return;
        }
    }

    if (callee == "curl_init") {
        std::shared_ptr<IRValue> urlVal = ConstantInt::get(0, Type::getI64());
        if (!node.getArgs().empty()) {
            node.getArgs()[0]->accept(*this);
            urlVal = m_lastValue;
        }
        auto callInst = std::make_shared<CallInst>("sph_curl_init",
                                                   std::vector<std::shared_ptr<IRValue>>{urlVal},
                                                   Type::getDynamic(), nextTemp());
        emitInst(callInst);
        m_lastValue = callInst;
        return;
    }

    if (callee == "curl_setopt") {
        if (node.getArgs().size() == 3) {
            node.getArgs()[0]->accept(*this);
            auto chVal = m_lastValue;
            node.getArgs()[1]->accept(*this);
            auto optVal = m_lastValue;
            node.getArgs()[2]->accept(*this);
            auto valVal = m_lastValue;
            if (!valVal->getType()->isDynamic()) {
                valVal = boxToDynamic(valVal);
            }
            auto callInst = std::make_shared<CallInst>("sph_curl_setopt",
                                                       std::vector<std::shared_ptr<IRValue>>{chVal, optVal, valVal},
                                                       Type::getBool(), nextTemp());
            emitInst(callInst);
            m_lastValue = callInst;
            return;
        }
    }

    if (callee == "curl_exec") {
        if (!node.getArgs().empty()) {
            node.getArgs()[0]->accept(*this);
            auto chVal = m_lastValue;
            auto callInst = std::make_shared<CallInst>("sph_curl_exec",
                                                       std::vector<std::shared_ptr<IRValue>>{chVal},
                                                       Type::getDynamic(), nextTemp());
            emitInst(callInst);
            m_lastValue = callInst;
            return;
        }
    }

    if (callee == "curl_getinfo") {
        if (!node.getArgs().empty()) {
            node.getArgs()[0]->accept(*this);
            auto chVal = m_lastValue;
            std::shared_ptr<IRValue> optVal = ConstantInt::get(0, Type::getI32());
            if (node.getArgs().size() >= 2) {
                node.getArgs()[1]->accept(*this);
                optVal = m_lastValue;
            }
            auto callInst = std::make_shared<CallInst>("sph_curl_getinfo",
                                                       std::vector<std::shared_ptr<IRValue>>{chVal, optVal},
                                                       Type::getDynamic(), nextTemp());
            emitInst(callInst);
            m_lastValue = callInst;
            return;
        }
    }

    if (callee == "curl_close") {
        if (!node.getArgs().empty()) {
            node.getArgs()[0]->accept(*this);
            auto chVal = m_lastValue;
            auto callInst = std::make_shared<CallInst>("sph_curl_close",
                                                       std::vector<std::shared_ptr<IRValue>>{chVal},
                                                       Type::getVoid(), nextTemp());
            emitInst(callInst);
            m_lastValue = callInst;
            return;
        }
    }

    if (callee == "file_get_contents") {
        if (!node.getArgs().empty()) {
            node.getArgs()[0]->accept(*this);
            auto pathVal = m_lastValue;
            auto callInst = std::make_shared<CallInst>("sph_file_read",
                                                       std::vector<std::shared_ptr<IRValue>>{pathVal},
                                                       Type::getString(), nextTemp());
            emitInst(callInst);
            m_lastValue = callInst;
            return;
        }
    }

    if (callee == "Ok" || callee == "Result::ok") {
        if (!node.getArgs().empty()) {
            node.getArgs()[0]->accept(*this);
            auto val = m_lastValue;
            auto tempSlot = std::make_shared<AllocaInst>(val->getType(), nextTemp());
            emitInst(tempSlot);
            emitInst(std::make_shared<StoreInst>(val, tempSlot));
            int64_t valSize = static_cast<int64_t>(val->getType()->getSize());
            if (valSize <= 0) valSize = 8;
            auto sizeVal = ConstantInt::get(valSize, Type::getI64());
            auto callInst = std::make_shared<CallInst>("sph_result_create_ok", std::vector<std::shared_ptr<IRValue>>{sizeVal, tempSlot}, node.getInferredType(), nextTemp());
            emitInst(callInst);
            m_lastValue = callInst;
            return;
        }
    }
    if (callee == "Err" || callee == "Result::err") {
        if (!node.getArgs().empty()) {
            node.getArgs()[0]->accept(*this);
            auto val = m_lastValue;
            auto tempSlot = std::make_shared<AllocaInst>(val->getType(), nextTemp());
            emitInst(tempSlot);
            emitInst(std::make_shared<StoreInst>(val, tempSlot));
            int64_t valSize = static_cast<int64_t>(val->getType()->getSize());
            if (valSize <= 0) valSize = 8;
            auto sizeVal = ConstantInt::get(valSize, Type::getI64());
            auto callInst = std::make_shared<CallInst>("sph_result_create_err", std::vector<std::shared_ptr<IRValue>>{sizeVal, tempSlot}, node.getInferredType(), nextTemp());
            emitInst(callInst);
            m_lastValue = callInst;
            return;
        }
    }

    auto it = m_functionParamTypes.find(node.getCallee());
    auto byRefIt = m_functionParamByRef.find(node.getCallee());
    std::vector<std::shared_ptr<IRValue>> args;
    for (size_t i = 0; i < node.getArgs().size(); ++i) {
        bool isByRef = (byRefIt != m_functionParamByRef.end() && i < byRefIt->second.size() && byRefIt->second[i]);
        if (isByRef) {
            if (auto* varExpr = dynamic_cast<VariableExpr*>(node.getArgs()[i].get())) {
                auto slot = getVariable(varExpr->getName());
                if (!slot) {
                    auto allocaInst = std::make_shared<AllocaInst>(Type::getDynamic(), "%var_" + varExpr->getName());
                    emitInst(allocaInst);
                    setVariable(varExpr->getName(), allocaInst);
                    slot = allocaInst;
                }
                if (slot->isInstruction() && static_cast<const IRInstruction*>(slot.get())->getOpcode() == IROpcode::Alloca) {
                    auto addrInst = std::make_shared<AddressOfInst>(slot, PointerType::get(slot->getType()), nextTemp());
                    emitInst(addrInst);
                    args.push_back(addrInst);
                } else {
                    args.push_back(slot);
                }
                continue;
            }
        }
        node.getArgs()[i]->accept(*this);
        auto argVal = m_lastValue;
        if (it != m_functionParamTypes.end() && i < it->second.size()) {
            const auto& paramType = it->second[i];
            if (paramType && paramType->isInterface() && argVal && argVal->getType()->isClass()) {
                argVal = coerceToInterface(argVal, paramType);
            } else if (paramType && paramType->isDynamic() && argVal && !argVal->getType()->isDynamic()) {
                argVal = boxToDynamic(argVal);
            } else if (paramType && !paramType->isDynamic() && argVal && argVal->getType()->isDynamic()) {
                argVal = unboxFromDynamic(argVal, paramType);
            } else if (paramType && paramType->isI64() && argVal && argVal->getType()->isI32()) {
                auto c = std::make_shared<CastInst>(argVal, Type::getI64(), nextTemp());
                emitInst(c);
                argVal = c;
            } else if (paramType && paramType->isF64() && argVal && (argVal->getType()->isI32() || argVal->getType()->isI64())) {
                auto c = std::make_shared<CastInst>(argVal, Type::getF64(), nextTemp());
                emitInst(c);
                argVal = c;
            }
        }
        if (auto* varExpr = dynamic_cast<VariableExpr*>(node.getArgs()[i].get())) {
            if (argVal && argVal->getType()->isBox()) {
                for (auto sIt = m_activeBoxes.rbegin(); sIt != m_activeBoxes.rend(); ++sIt) {
                    sIt->erase(varExpr->getName());
                }
            }
        }
        args.push_back(argVal);
    }

    std::shared_ptr<Type> retType = node.getInferredType();
    if (!retType) retType = Type::getVoid();

    if (callee == "f32x4" || callee == "i32x4" || callee == "f64x2" || callee == "i64x2") {
        auto vecType = (callee == "f32x4") ? Type::getF32x4() :
                       (callee == "i32x4") ? Type::getI32x4() :
                       (callee == "f64x2") ? Type::getF64x2() : Type::getI64x2();
        auto allocaInst = std::make_shared<AllocaInst>(vecType, nextTemp());
        emitInst(allocaInst);
        size_t count = vecType->getMembers().size();
        for (size_t i = 0; i < count; ++i) {
            const auto& m = vecType->getMembers()[i];
            auto fieldPtr = std::make_shared<GetFieldPtrInst>(allocaInst, static_cast<int>(i), m.offset, m.type, nextTemp());
            emitInst(fieldPtr);
            std::shared_ptr<IRValue> val = nullptr;
            if (args.size() == count) val = args[i];
            else if (args.size() == 1) val = args[0];
            else {
                if (m.type->isFloatingPoint()) val = ConstantFloat::get(0.0, m.type);
                else val = ConstantInt::get(0, m.type);
            }
            if (val) {
                if (m.type->isF32() && val->getType() != Type::getF32()) {
                    if (val->isConstant()) {
                        val = ConstantFloat::get(std::stof(val->getName()), Type::getF32());
                    } else {
                        auto c = std::make_shared<CastInst>(val, Type::getF32(), nextTemp());
                        emitInst(c);
                        val = c;
                    }
                } else if (m.type->isF64() && val->getType() != Type::getF64()) {
                    if (val->isConstant()) {
                        val = ConstantFloat::get(std::stod(val->getName()), Type::getF64());
                    } else {
                        auto c = std::make_shared<CastInst>(val, Type::getF64(), nextTemp());
                        emitInst(c);
                        val = c;
                    }
                } else if (m.type->isI32() && val->getType() != Type::getI32()) {
                    if (val->isConstant()) {
                        val = ConstantInt::get(std::stoi(val->getName()), Type::getI32());
                    } else {
                        auto c = std::make_shared<CastInst>(val, Type::getI32(), nextTemp());
                        emitInst(c);
                        val = c;
                    }
                } else if (m.type->isI64() && val->getType() != Type::getI64()) {
                    if (val->isConstant()) {
                        val = ConstantInt::get(std::stoll(val->getName()), Type::getI64());
                    } else {
                        auto c = std::make_shared<CastInst>(val, Type::getI64(), nextTemp());
                        emitInst(c);
                        val = c;
                    }
                }
            }
            emitInst(std::make_shared<StoreInst>(val, fieldPtr));
        }
        m_lastValue = allocaInst;
        return;
    }

    if (callee == "f32x4::zero" || callee == "i32x4::zero" || callee == "f64x2::zero" || callee == "i64x2::zero") {
        std::string typeName = callee.substr(0, 5);
        auto vecType = (typeName == "f32x4") ? Type::getF32x4() :
                       (typeName == "i32x4") ? Type::getI32x4() :
                       (typeName == "f64x2") ? Type::getF64x2() : Type::getI64x2();
        auto allocaInst = std::make_shared<AllocaInst>(vecType, nextTemp());
        emitInst(allocaInst);
        auto call = std::make_shared<CallInst>("sph_simd_" + typeName + "_zero",
            std::vector<std::shared_ptr<IRValue>>{allocaInst}, Type::getVoid(), nextTemp());
        emitInst(call);
        m_lastValue = allocaInst;
        return;
    }

    if (callee == "f32x4::splat" || callee == "i32x4::splat" || callee == "f64x2::splat" || callee == "i64x2::splat") {
        std::string typeName = callee.substr(0, 5);
        auto vecType = (typeName == "f32x4") ? Type::getF32x4() :
                       (typeName == "i32x4") ? Type::getI32x4() :
                       (typeName == "f64x2") ? Type::getF64x2() : Type::getI64x2();
        auto splatVal = args[0];
        if (typeName == "f32x4" && splatVal->getType() != Type::getF32()) {
            if (splatVal->isConstant()) splatVal = ConstantFloat::get(std::stof(splatVal->getName()), Type::getF32());
            else {
                auto c = std::make_shared<CastInst>(splatVal, Type::getF32(), nextTemp());
                emitInst(c);
                splatVal = c;
            }
        } else if (typeName == "f64x2" && splatVal->getType() != Type::getF64()) {
            if (splatVal->isConstant()) splatVal = ConstantFloat::get(std::stod(splatVal->getName()), Type::getF64());
            else {
                auto c = std::make_shared<CastInst>(splatVal, Type::getF64(), nextTemp());
                emitInst(c);
                splatVal = c;
            }
        } else if (typeName == "i32x4" && splatVal->getType() != Type::getI32()) {
            if (splatVal->isConstant()) splatVal = ConstantInt::get(std::stoi(splatVal->getName()), Type::getI32());
            else {
                auto c = std::make_shared<CastInst>(splatVal, Type::getI32(), nextTemp());
                emitInst(c);
                splatVal = c;
            }
        } else if (typeName == "i64x2" && splatVal->getType() != Type::getI64()) {
            if (splatVal->isConstant()) splatVal = ConstantInt::get(std::stoll(splatVal->getName()), Type::getI64());
            else {
                auto c = std::make_shared<CastInst>(splatVal, Type::getI64(), nextTemp());
                emitInst(c);
                splatVal = c;
            }
        }
        auto allocaInst = std::make_shared<AllocaInst>(vecType, nextTemp());
        emitInst(allocaInst);
        auto call = std::make_shared<CallInst>("sph_simd_" + typeName + "_splat",
            std::vector<std::shared_ptr<IRValue>>{allocaInst, splatVal}, Type::getVoid(), nextTemp());
        emitInst(call);
        m_lastValue = allocaInst;
        return;
    }

    if (callee == "f32x4::load" || callee == "i32x4::load" || callee == "f64x2::load" || callee == "i64x2::load") {
        std::string typeName = callee.substr(0, 5);
        auto vecType = (typeName == "f32x4") ? Type::getF32x4() :
                       (typeName == "i32x4") ? Type::getI32x4() :
                       (typeName == "f64x2") ? Type::getF64x2() : Type::getI64x2();
        auto allocaInst = std::make_shared<AllocaInst>(vecType, nextTemp());
        emitInst(allocaInst);
        auto call = std::make_shared<CallInst>("sph_simd_" + typeName + "_load",
            std::vector<std::shared_ptr<IRValue>>{allocaInst, args[0]}, Type::getVoid(), nextTemp());
        emitInst(call);
        m_lastValue = allocaInst;
        return;
    }

    if (callee == "f32x4::store" || callee == "i32x4::store" || callee == "f64x2::store" || callee == "i64x2::store") {
        std::string typeName = callee.substr(0, 5);
        auto call = std::make_shared<CallInst>("sph_simd_" + typeName + "_store",
            std::vector<std::shared_ptr<IRValue>>{args[0], args[1]}, Type::getVoid(), nextTemp());
        emitInst(call);
        m_lastValue = nullptr;
        return;
    }

    if (callee == "File::read") callee = "sph_file_read";
    else if (callee == "File::write") callee = "sph_file_write";
    else if (callee == "File::append") callee = "sph_file_append";
    else if (callee == "File::exists") callee = "sph_file_exists";
    else if (callee == "File::delete") callee = "sph_file_delete";
    else if (callee == "Process::exit") callee = "sph_process_exit";
    else if (callee == "Env::get") callee = "sph_env_get";
    else if (callee == "Env::set") callee = "sph_env_set";
    else if (callee == "panic") callee = "sph_panic";
    else if (callee == "Thread::spawn") callee = "sph_thread_spawn";
    else if (callee == "Thread::join") callee = "sph_thread_join";
    else if (callee == "Thread::sleep_ms") callee = "sph_thread_sleep_ms";
    else if (callee == "EventLoop::run") callee = "sph_event_loop_run";
    else if (callee == "EventLoop::yield_now") callee = "sph_event_loop_yield";
    else if (callee == "EventLoop::spawn") callee = "sph_event_loop_spawn";
    else if (callee == "Simd::has_sse") callee = "sph_simd_has_sse";
    else if (callee == "Simd::has_sse2") callee = "sph_simd_has_sse2";
    else if (callee == "Simd::has_sse3") callee = "sph_simd_has_sse3";
    else if (callee == "Simd::has_sse41") callee = "sph_simd_has_sse41";
    else if (callee == "Simd::has_avx") callee = "sph_simd_has_avx";
    else if (callee == "Simd::has_avx2") callee = "sph_simd_has_avx2";
    else if (callee == "Simd::has_neon") callee = "sph_simd_has_neon";

    if (m_asyncFunctions.count(callee)) {
        std::shared_ptr<IRValue> argVal = nullptr;
        if (!args.empty()) {
            argVal = args[0];
            if (argVal && !argVal->getType()->isI64()) {
                auto c = std::make_shared<CastInst>(argVal, Type::getI64(), nextTemp());
                emitInst(c);
                argVal = c;
            }
        } else {
            argVal = ConstantInt::get(0, Type::getI64());
        }
        auto fnPtr = ConstantGlobal::get(callee, PointerType::get(Type::getVoid()));
        auto spawnCall = std::make_shared<CallInst>("sph_event_loop_spawn", std::vector<std::shared_ptr<IRValue>>{fnPtr, argVal}, retType, nextTemp());
        emitInst(spawnCall);
        m_lastValue = spawnCall;
        return;
    }

    auto callInst = std::make_shared<CallInst>(callee, std::move(args), retType, nextTemp());
    emitInst(callInst);
    m_lastValue = callInst;
}

void IRGenerator::visit(ClassDecl& node) {
    auto classType = std::static_pointer_cast<ClassType>(m_types[node.getName()]);
    for (auto& method : node.getMethods()) {
        m_tempCounter = 0;
        auto returnType = resolveType(method->getReturnType());
        std::vector<std::shared_ptr<IRParameter>> params;

        // First parameter is 'this'
        params.push_back(std::make_shared<IRParameter>(classType, "%this", 0));

        // Method parameters
        for (size_t i = 0; i < method->getParams().size(); ++i) {
            const auto& p = method->getParams()[i];
            auto pType = resolveType(p.type);
            params.push_back(std::make_shared<IRParameter>(pType, "%arg_" + p.name, i + 1));
        }

        std::string mangledName = node.getName() + "_" + method->getName();
        auto irFn = std::make_unique<IRFunction>(mangledName, returnType, params);
        irFn->setLocation(method->getSpan().start);
        m_currentFunction = irFn.get();
        m_module->addFunction(std::move(irFn));

        m_currentBlock = m_currentFunction->createBlock("entry");
        pushScope();

        // 'this' and '$this' are stored as the parameter pointer
        setVariable("this", params[0]);
        setVariable("$this", params[0]);

        for (size_t i = 0; i < method->getParams().size(); ++i) {
            const auto& param = params[i + 1];
            const std::string& origName = method->getParams()[i].name;

            if (param->getType()->isRc()) {
            auto allocaInst = std::make_shared<AllocaInst>(param->getType(), "%var_" + origName);
            emitInst(allocaInst);
            emitInst(std::make_shared<RcRetainInst>(param));
            emitInst(std::make_shared<StoreInst>(param, allocaInst));
            setVariable(origName, allocaInst);
        } else if (param->getType()->isBox()) {
            auto allocaInst = std::make_shared<AllocaInst>(param->getType(), "%var_" + origName);
            emitInst(allocaInst);
            emitInst(std::make_shared<StoreInst>(param, allocaInst));
            setVariable(origName, allocaInst);
            if (!m_activeBoxes.empty()) {
                m_activeBoxes.back().insert(origName);
            }
        } else if (param->getType()->isStruct() || param->getType()->isArray() || param->getType()->isSlice() || param->getType()->isClass() || param->getType()->isInterface()) {
                setVariable(origName, param);
            } else {
                auto allocaInst = std::make_shared<AllocaInst>(param->getType(), "%var_" + origName);
                emitInst(allocaInst);
                emitInst(std::make_shared<StoreInst>(param, allocaInst));
                setVariable(origName, allocaInst);
            }
        }

        method->getBody().accept(*this);

        if (m_currentBlock && !m_currentBlock->hasTerminator()) {
            if (returnType->isVoid()) {
                emitInst(std::make_shared<RetInst>());
            } else {
                emitInst(std::make_shared<RetInst>(ConstantInt::get(0, returnType)));
            }
        }

        popScope();
        m_currentFunction = nullptr;
        m_currentBlock = nullptr;
    }
}

void IRGenerator::visit(ThisExpr& /*node*/) {
    m_lastValue = getVariable("this");
}

void IRGenerator::visit(MethodCallExpr& node) {
    LocGuard locGuard(*this, node.getSpan().start);
    node.getReceiver().accept(*this);
    auto recvVal = m_lastValue;

    if (recvVal->getType()->isSimd()) {
        std::string typeName = recvVal->getType()->toString();
        const std::string& mName = node.getMethodName();
        if (mName == "get") {
            node.getArgs()[0]->accept(*this);
            auto idxVal = m_lastValue;
            auto retType = (typeName == "f32x4") ? Type::getF32() :
                           (typeName == "i32x4") ? Type::getI32() :
                           (typeName == "f64x2") ? Type::getF64() : Type::getI64();
            auto callInst = std::make_shared<CallInst>("sph_simd_" + typeName + "_get",
                std::vector<std::shared_ptr<IRValue>>{recvVal, idxVal}, retType, nextTemp());
            emitInst(callInst);
            m_lastValue = callInst;
            return;
        }
        if (mName == "set") {
            node.getArgs()[0]->accept(*this);
            auto idxVal = m_lastValue;
            node.getArgs()[1]->accept(*this);
            auto setVal = m_lastValue;
            if (typeName == "f32x4" && setVal->getType() != Type::getF32()) {
                if (setVal->isConstant()) setVal = ConstantFloat::get(std::stof(setVal->getName()), Type::getF32());
                else {
                    auto c = std::make_shared<CastInst>(setVal, Type::getF32(), nextTemp());
                    emitInst(c);
                    setVal = c;
                }
            } else if (typeName == "f64x2" && setVal->getType() != Type::getF64()) {
                if (setVal->isConstant()) setVal = ConstantFloat::get(std::stod(setVal->getName()), Type::getF64());
                else {
                    auto c = std::make_shared<CastInst>(setVal, Type::getF64(), nextTemp());
                    emitInst(c);
                    setVal = c;
                }
            } else if (typeName == "i32x4" && setVal->getType() != Type::getI32()) {
                if (setVal->isConstant()) setVal = ConstantInt::get(std::stoi(setVal->getName()), Type::getI32());
                else {
                    auto c = std::make_shared<CastInst>(setVal, Type::getI32(), nextTemp());
                    emitInst(c);
                    setVal = c;
                }
            } else if (typeName == "i64x2" && setVal->getType() != Type::getI64()) {
                if (setVal->isConstant()) setVal = ConstantInt::get(std::stoll(setVal->getName()), Type::getI64());
                else {
                    auto c = std::make_shared<CastInst>(setVal, Type::getI64(), nextTemp());
                    emitInst(c);
                    setVal = c;
                }
            }
            auto callInst = std::make_shared<CallInst>("sph_simd_" + typeName + "_set",
                std::vector<std::shared_ptr<IRValue>>{recvVal, idxVal, setVal}, Type::getVoid(), nextTemp());
            emitInst(callInst);
            m_lastValue = callInst;
            return;
        }
        if (mName == "add" || mName == "sub" || mName == "mul" || mName == "div" || mName == "min" || mName == "max") {
            node.getArgs()[0]->accept(*this);
            auto otherVal = m_lastValue;
            auto resAlloc = std::make_shared<AllocaInst>(recvVal->getType(), nextTemp());
            emitInst(resAlloc);
            auto callInst = std::make_shared<CallInst>("sph_simd_" + typeName + "_" + mName,
                std::vector<std::shared_ptr<IRValue>>{resAlloc, recvVal, otherVal}, Type::getVoid(), nextTemp());
            emitInst(callInst);
            m_lastValue = resAlloc;
            return;
        }
        if (mName == "sqrt" || mName == "abs") {
            auto resAlloc = std::make_shared<AllocaInst>(recvVal->getType(), nextTemp());
            emitInst(resAlloc);
            auto callInst = std::make_shared<CallInst>("sph_simd_" + typeName + "_" + mName,
                std::vector<std::shared_ptr<IRValue>>{resAlloc, recvVal}, Type::getVoid(), nextTemp());
            emitInst(callInst);
            m_lastValue = resAlloc;
            return;
        }
        if (mName == "sum") {
            auto retType = (typeName == "f32x4") ? Type::getF32() :
                           (typeName == "i32x4") ? Type::getI32() :
                           (typeName == "f64x2") ? Type::getF64() : Type::getI64();
            auto callInst = std::make_shared<CallInst>("sph_simd_" + typeName + "_sum",
                std::vector<std::shared_ptr<IRValue>>{recvVal}, retType, nextTemp());
            emitInst(callInst);
            m_lastValue = callInst;
            return;
        }
        if (mName == "dot") {
            node.getArgs()[0]->accept(*this);
            auto otherVal = m_lastValue;
            auto retType = (typeName == "f32x4") ? Type::getF32() :
                           (typeName == "i32x4") ? Type::getI32() :
                           (typeName == "f64x2") ? Type::getF64() : Type::getI64();
            auto callInst = std::make_shared<CallInst>("sph_simd_" + typeName + "_dot",
                std::vector<std::shared_ptr<IRValue>>{recvVal, otherVal}, retType, nextTemp());
            emitInst(callInst);
            m_lastValue = callInst;
            return;
        }
        if (mName == "store") {
            node.getArgs()[0]->accept(*this);
            auto ptrVal = m_lastValue;
            auto callInst = std::make_shared<CallInst>("sph_simd_" + typeName + "_store",
                std::vector<std::shared_ptr<IRValue>>{ptrVal, recvVal}, Type::getVoid(), nextTemp());
            emitInst(callInst);
            m_lastValue = callInst;
            return;
        }
        if (mName == "to_string") {
            auto callInst = std::make_shared<CallInst>("sph_simd_" + typeName + "_to_string",
                std::vector<std::shared_ptr<IRValue>>{recvVal}, Type::getString(), nextTemp());
            emitInst(callInst);
            m_lastValue = callInst;
            return;
        }
    }

    if (recvVal->getType()->isArena()) {
        if (node.getMethodName() == "reset") {
            auto callInst = std::make_shared<CallInst>("sph_arena_reset", std::vector<std::shared_ptr<IRValue>>{recvVal}, Type::getVoid(), nextTemp());
            emitInst(callInst);
            m_lastValue = callInst;
            return;
        }
        if (node.getMethodName() == "destroy") {
            auto callInst = std::make_shared<CallInst>("sph_arena_destroy", std::vector<std::shared_ptr<IRValue>>{recvVal}, Type::getVoid(), nextTemp());
            emitInst(callInst);
            m_lastValue = callInst;
            return;
        }
        if (node.getMethodName() == "alloc") {
            if (!node.getArgs().empty()) {
                m_lastValue = emitArenaAlloc(recvVal, *node.getArgs()[0]);
                return;
            }
        }
    }

    if (recvVal->getType()->isString()) {
        if (node.getMethodName() == "len") {
            auto callInst = std::make_shared<CallInst>("sph_string_len", std::vector<std::shared_ptr<IRValue>>{recvVal}, Type::getI64(), nextTemp());
            emitInst(callInst);
            m_lastValue = callInst;
            return;
        }
    }

    if (recvVal->getType()->isVector()) {
        auto vecType = std::static_pointer_cast<VectorType>(recvVal->getType());
        auto elemType = vecType->getElementType();

        if (node.getMethodName() == "len") {
            auto callInst = std::make_shared<CallInst>("sph_vec_len", std::vector<std::shared_ptr<IRValue>>{recvVal}, Type::getI64(), nextTemp());
            emitInst(callInst);
            m_lastValue = callInst;
            return;
        }
        if (node.getMethodName() == "capacity") {
            auto callInst = std::make_shared<CallInst>("sph_vec_cap", std::vector<std::shared_ptr<IRValue>>{recvVal}, Type::getI64(), nextTemp());
            emitInst(callInst);
            m_lastValue = callInst;
            return;
        }
        if (node.getMethodName() == "destroy") {
            auto callInst = std::make_shared<CallInst>("sph_vec_destroy", std::vector<std::shared_ptr<IRValue>>{recvVal}, Type::getVoid(), nextTemp());
            emitInst(callInst);
            m_lastValue = callInst;
            return;
        }
        if (node.getMethodName() == "pop") {
            auto outSlot = std::make_shared<AllocaInst>(elemType, nextTemp());
            emitInst(outSlot);
            auto callInst = std::make_shared<CallInst>("sph_vec_pop", std::vector<std::shared_ptr<IRValue>>{recvVal, outSlot}, Type::getVoid(), nextTemp());
            emitInst(callInst);
            auto loadInst = std::make_shared<LoadInst>(outSlot, nextTemp());
            emitInst(loadInst);
            m_lastValue = loadInst;
            return;
        }
        if (node.getMethodName() == "push") {
            if (!node.getArgs().empty()) {
                node.getArgs()[0]->accept(*this);
                auto argVal = m_lastValue;
                auto tempSlot = std::make_shared<AllocaInst>(argVal->getType(), nextTemp());
                emitInst(tempSlot);
                emitInst(std::make_shared<StoreInst>(argVal, tempSlot));
                auto callInst = std::make_shared<CallInst>("sph_vec_push", std::vector<std::shared_ptr<IRValue>>{recvVal, tempSlot}, Type::getVoid(), nextTemp());
                emitInst(callInst);
                m_lastValue = callInst;
                return;
            }
        }
    }

    if (recvVal->getType()->isResult()) {
        auto resType = std::static_pointer_cast<ResultType>(recvVal->getType());
        auto okType = resType->getOkType();

        if (node.getMethodName() == "is_ok") {
            auto callInst = std::make_shared<CallInst>("sph_result_is_ok", std::vector<std::shared_ptr<IRValue>>{recvVal}, Type::getBool(), nextTemp());
            emitInst(callInst);
            m_lastValue = callInst;
            return;
        }
        if (node.getMethodName() == "is_err") {
            auto callInst = std::make_shared<CallInst>("sph_result_is_err", std::vector<std::shared_ptr<IRValue>>{recvVal}, Type::getBool(), nextTemp());
            emitInst(callInst);
            m_lastValue = callInst;
            return;
        }
        if (node.getMethodName() == "unwrap") {
            auto unwrapCall = std::make_shared<CallInst>("sph_result_unwrap", std::vector<std::shared_ptr<IRValue>>{recvVal}, PointerType::get(okType), nextTemp());
            emitInst(unwrapCall);
            if (okType->isStruct() || okType->isClass()) {
                m_lastValue = unwrapCall;
            } else {
                auto loadInst = std::make_shared<LoadInst>(unwrapCall, nextTemp());
                emitInst(loadInst);
                m_lastValue = loadInst;
            }
            return;
        }
        if (node.getMethodName() == "destroy") {
            auto callInst = std::make_shared<CallInst>("sph_result_destroy", std::vector<std::shared_ptr<IRValue>>{recvVal}, Type::getVoid(), nextTemp());
            emitInst(callInst);
            m_lastValue = callInst;
            return;
        }
    }

    bool isUserMethod = false;
    std::string atomicClassName = "";
    bool isPromise = false;
    if (recvVal->getType()->isClass()) {
        auto ct = std::static_pointer_cast<ClassType>(recvVal->getType());
        if (ct->getName() == "AtomicI32" || ct->getName() == "AtomicI64" || ct->getName() == "AtomicBool") {
            atomicClassName = ct->getName();
        } else if (ct->getName() == "Promise") {
            isPromise = true;
        } else if (ct->getName() != "PDO" && ct->getName() != "PDOStatement" && ct->findMethod(node.getMethodName())) {
            isUserMethod = true;
        }
    } else if (recvVal->getType()->isPointer()) {
        auto pt = std::static_pointer_cast<PointerType>(recvVal->getType());
        if (pt->getPointeeType() && pt->getPointeeType()->isClass()) {
            auto ct = std::static_pointer_cast<ClassType>(pt->getPointeeType());
            if (ct->getName() == "AtomicI32" || ct->getName() == "AtomicI64" || ct->getName() == "AtomicBool") {
                atomicClassName = ct->getName();
            } else if (ct->getName() == "Promise") {
                isPromise = true;
            } else if (ct->getName() != "PDO" && ct->getName() != "PDOStatement" && ct->findMethod(node.getMethodName())) {
                isUserMethod = true;
            }
        }
    }

    if (!atomicClassName.empty()) {
        std::shared_ptr<IRValue> ptrVal = recvVal;
        if (!recvVal->getType()->isPointer()) {
            auto addrInst = std::make_shared<AddressOfInst>(recvVal, PointerType::get(Type::getVoid()), nextTemp());
            emitInst(addrInst);
            ptrVal = addrInst;
        }

        const std::string& mName = node.getMethodName();
        if (mName == "load") {
            std::shared_ptr<IRValue> orderVal = ConstantInt::get(4, Type::getI32());
            if (!node.getArgs().empty()) {
                node.getArgs()[0]->accept(*this);
                orderVal = m_lastValue;
            }
            std::string cFn = (atomicClassName == "AtomicI32") ? "sph_atomic_load_i32" :
                              (atomicClassName == "AtomicI64") ? "sph_atomic_load_i64" : "sph_atomic_load_bool";
            auto retType = (atomicClassName == "AtomicI32") ? Type::getI32() :
                           (atomicClassName == "AtomicI64") ? Type::getI64() : Type::getBool();
            auto callInst = std::make_shared<CallInst>(cFn, std::vector<std::shared_ptr<IRValue>>{ptrVal, orderVal}, retType, nextTemp());
            emitInst(callInst);
            m_lastValue = callInst;
            return;
        }
        if (mName == "store") {
            node.getArgs()[0]->accept(*this);
            auto val = m_lastValue;
            if (atomicClassName == "AtomicI64" && val && val->getType()->isI32()) {
                auto c = std::make_shared<CastInst>(val, Type::getI64(), nextTemp());
                emitInst(c);
                val = c;
            }
            std::shared_ptr<IRValue> orderVal = ConstantInt::get(4, Type::getI32());
            if (node.getArgs().size() > 1) {
                node.getArgs()[1]->accept(*this);
                orderVal = m_lastValue;
            }
            std::string cFn = (atomicClassName == "AtomicI32") ? "sph_atomic_store_i32" :
                              (atomicClassName == "AtomicI64") ? "sph_atomic_store_i64" : "sph_atomic_store_bool";
            auto callInst = std::make_shared<CallInst>(cFn, std::vector<std::shared_ptr<IRValue>>{ptrVal, val, orderVal}, Type::getVoid(), nextTemp());
            emitInst(callInst);
            m_lastValue = callInst;
            return;
        }
        if (mName == "fetch_add" || mName == "fetch_sub") {
            node.getArgs()[0]->accept(*this);
            auto val = m_lastValue;
            if (atomicClassName == "AtomicI64" && val && val->getType()->isI32()) {
                auto c = std::make_shared<CastInst>(val, Type::getI64(), nextTemp());
                emitInst(c);
                val = c;
            }
            std::shared_ptr<IRValue> orderVal = ConstantInt::get(4, Type::getI32());
            if (node.getArgs().size() > 1) {
                node.getArgs()[1]->accept(*this);
                orderVal = m_lastValue;
            }
            std::string cFn = (mName == "fetch_add") ?
                              ((atomicClassName == "AtomicI32") ? "sph_atomic_fetch_add_i32" : "sph_atomic_fetch_add_i64") :
                              ((atomicClassName == "AtomicI32") ? "sph_atomic_fetch_sub_i32" : "sph_atomic_fetch_sub_i64");
            auto retType = (atomicClassName == "AtomicI32") ? Type::getI32() : Type::getI64();
            auto callInst = std::make_shared<CallInst>(cFn, std::vector<std::shared_ptr<IRValue>>{ptrVal, val, orderVal}, retType, nextTemp());
            emitInst(callInst);
            m_lastValue = callInst;
            return;
        }
        if (mName == "exchange") {
            node.getArgs()[0]->accept(*this);
            auto val = m_lastValue;
            if (atomicClassName == "AtomicI64" && val && val->getType()->isI32()) {
                auto c = std::make_shared<CastInst>(val, Type::getI64(), nextTemp());
                emitInst(c);
                val = c;
            }
            std::shared_ptr<IRValue> orderVal = ConstantInt::get(4, Type::getI32());
            if (node.getArgs().size() > 1) {
                node.getArgs()[1]->accept(*this);
                orderVal = m_lastValue;
            }
            std::string cFn = (atomicClassName == "AtomicI32") ? "sph_atomic_exchange_i32" :
                              (atomicClassName == "AtomicI64") ? "sph_atomic_exchange_i64" : "sph_atomic_exchange_bool";
            auto retType = (atomicClassName == "AtomicI32") ? Type::getI32() :
                           (atomicClassName == "AtomicI64") ? Type::getI64() : Type::getBool();
            auto callInst = std::make_shared<CallInst>(cFn, std::vector<std::shared_ptr<IRValue>>{ptrVal, val, orderVal}, retType, nextTemp());
            emitInst(callInst);
            m_lastValue = callInst;
            return;
        }
        if (mName == "compare_exchange") {
            node.getArgs()[0]->accept(*this);
            auto expVal = m_lastValue;
            if (atomicClassName == "AtomicI64" && expVal && expVal->getType()->isI32()) {
                auto c = std::make_shared<CastInst>(expVal, Type::getI64(), nextTemp());
                emitInst(c);
                expVal = c;
            }
            node.getArgs()[1]->accept(*this);
            auto desVal = m_lastValue;
            if (atomicClassName == "AtomicI64" && desVal && desVal->getType()->isI32()) {
                auto c = std::make_shared<CastInst>(desVal, Type::getI64(), nextTemp());
                emitInst(c);
                desVal = c;
            }
            std::shared_ptr<IRValue> succOrder = ConstantInt::get(4, Type::getI32());
            std::shared_ptr<IRValue> failOrder = ConstantInt::get(4, Type::getI32());
            if (node.getArgs().size() > 2) {
                node.getArgs()[2]->accept(*this);
                succOrder = m_lastValue;
            }
            if (node.getArgs().size() > 3) {
                node.getArgs()[3]->accept(*this);
                failOrder = m_lastValue;
            } else {
                failOrder = succOrder;
            }
            std::string cFn = (atomicClassName == "AtomicI32") ? "sph_atomic_compare_exchange_i32" :
                              (atomicClassName == "AtomicI64") ? "sph_atomic_compare_exchange_i64" : "sph_atomic_compare_exchange_bool";
            auto callInst = std::make_shared<CallInst>(cFn, std::vector<std::shared_ptr<IRValue>>{ptrVal, expVal, desVal, succOrder, failOrder}, Type::getBool(), nextTemp());
            emitInst(callInst);
            m_lastValue = callInst;
            return;
        }
    }

    if (isPromise) {
        std::shared_ptr<IRValue> promiseVal = recvVal;
        const std::string& mName = node.getMethodName();
        if (mName == "resolve" || mName == "reject") {
            node.getArgs()[0]->accept(*this);
            auto val = m_lastValue;
            if (val && !val->getType()->isI64()) {
                auto c = std::make_shared<CastInst>(val, Type::getI64(), nextTemp());
                emitInst(c);
                val = c;
            }
            std::string cFn = (mName == "resolve") ? "sph_promise_resolve" : "sph_promise_reject";
            auto callInst = std::make_shared<CallInst>(cFn, std::vector<std::shared_ptr<IRValue>>{promiseVal, val}, Type::getVoid(), nextTemp());
            emitInst(callInst);
            m_lastValue = callInst;
            return;
        }
        if (mName == "is_ready") {
            auto callInst = std::make_shared<CallInst>("sph_promise_is_ready", std::vector<std::shared_ptr<IRValue>>{promiseVal}, Type::getBool(), nextTemp());
            emitInst(callInst);
            m_lastValue = callInst;
            return;
        }
        if (mName == "get_value") {
            auto retType = node.getInferredType() ? node.getInferredType() : Type::getI64();
            auto callInst = std::make_shared<CallInst>("sph_promise_get_value", std::vector<std::shared_ptr<IRValue>>{promiseVal}, Type::getI64(), nextTemp());
            emitInst(callInst);
            if (retType->isI32()) {
                auto c = std::make_shared<CastInst>(callInst, Type::getI32(), nextTemp());
                emitInst(c);
                m_lastValue = c;
            } else {
                m_lastValue = callInst;
            }
            return;
        }
        if (mName == "await") {
            auto retType = node.getInferredType() ? node.getInferredType() : Type::getI64();
            auto callInst = std::make_shared<CallInst>("sph_promise_await", std::vector<std::shared_ptr<IRValue>>{promiseVal}, Type::getI64(), nextTemp());
            emitInst(callInst);
            if (retType->isI32()) {
                auto c = std::make_shared<CastInst>(callInst, Type::getI32(), nextTemp());
                emitInst(c);
                m_lastValue = c;
            } else {
                m_lastValue = callInst;
            }
            return;
        }
    }

    if (!isUserMethod) {
        if (node.getMethodName() == "exec") {
            std::vector<std::shared_ptr<IRValue>> args;
            args.push_back(recvVal);
            if (!node.getArgs().empty()) {
                node.getArgs()[0]->accept(*this);
                args.push_back(m_lastValue);
            } else {
                args.push_back(ConstantString::get(""));
            }
            auto callInst = std::make_shared<CallInst>("sph_pdo_exec", std::move(args), Type::getI64(), nextTemp());
            emitInst(callInst);
            m_lastValue = boxToDynamic(callInst);
            return;
        }
        if (node.getMethodName() == "query") {
            std::vector<std::shared_ptr<IRValue>> args;
            args.push_back(recvVal);
            if (!node.getArgs().empty()) {
                node.getArgs()[0]->accept(*this);
                args.push_back(m_lastValue);
            } else {
                args.push_back(ConstantString::get(""));
            }
            auto callInst = std::make_shared<CallInst>("sph_pdo_query", std::move(args), Type::getDynamic(), nextTemp());
            emitInst(callInst);
            m_lastValue = callInst;
            return;
        }
        if (node.getMethodName() == "prepare") {
            std::vector<std::shared_ptr<IRValue>> args;
            args.push_back(recvVal);
            if (!node.getArgs().empty()) {
                node.getArgs()[0]->accept(*this);
                args.push_back(m_lastValue);
            } else {
                args.push_back(ConstantString::get(""));
            }
            auto callInst = std::make_shared<CallInst>("sph_pdo_prepare", std::move(args), Type::getDynamic(), nextTemp());
            emitInst(callInst);
            m_lastValue = callInst;
            return;
        }
        if (node.getMethodName() == "lastInsertId") {
            auto callInst = std::make_shared<CallInst>("sph_pdo_last_insert_id",
                                                       std::vector<std::shared_ptr<IRValue>>{recvVal},
                                                       Type::getI64(), nextTemp());
            emitInst(callInst);
            m_lastValue = boxToDynamic(callInst);
            return;
        }
        if (node.getMethodName() == "execute") {
            std::vector<std::shared_ptr<IRValue>> args;
            args.push_back(recvVal);
            if (!node.getArgs().empty()) {
                node.getArgs()[0]->accept(*this);
                auto pVal = m_lastValue;
                if (!pVal->getType()->isDynamic()) {
                    pVal = boxToDynamic(pVal);
                }
                args.push_back(pVal);
            } else {
                args.push_back(ConstantInt::get(0, Type::getI64()));
            }
            auto callInst = std::make_shared<CallInst>("sph_pdostatement_execute", std::move(args), Type::getBool(), nextTemp());
            emitInst(callInst);
            m_lastValue = boxToDynamic(callInst);
            return;
        }
        if (node.getMethodName() == "fetch") {
            std::vector<std::shared_ptr<IRValue>> args;
            args.push_back(recvVal);
            if (!node.getArgs().empty()) {
                node.getArgs()[0]->accept(*this);
                args.push_back(m_lastValue);
            } else {
                args.push_back(ConstantInt::get(2, Type::getI32())); // PDO::FETCH_ASSOC
            }
            auto callInst = std::make_shared<CallInst>("sph_pdostatement_fetch", std::move(args), Type::getDynamic(), nextTemp());
            emitInst(callInst);
            m_lastValue = callInst;
            return;
        }
        if (node.getMethodName() == "fetchAll") {
            std::vector<std::shared_ptr<IRValue>> args;
            args.push_back(recvVal);
            if (!node.getArgs().empty()) {
                node.getArgs()[0]->accept(*this);
                args.push_back(m_lastValue);
            } else {
                args.push_back(ConstantInt::get(2, Type::getI32())); // PDO::FETCH_ASSOC
            }
            auto callInst = std::make_shared<CallInst>("sph_pdostatement_fetch_all", std::move(args), Type::getDynamic(), nextTemp());
            emitInst(callInst);
            m_lastValue = callInst;
            return;
        }
        if (node.getMethodName() == "rowCount") {
            auto callInst = std::make_shared<CallInst>("sph_pdostatement_row_count",
                                                       std::vector<std::shared_ptr<IRValue>>{recvVal},
                                                       Type::getI64(), nextTemp());
            emitInst(callInst);
            m_lastValue = boxToDynamic(callInst);
            return;
        }
        if (node.getMethodName() == "columnCount") {
            auto callInst = std::make_shared<CallInst>("sph_pdostatement_column_count",
                                                       std::vector<std::shared_ptr<IRValue>>{recvVal},
                                                       Type::getI32(), nextTemp());
            emitInst(callInst);
            m_lastValue = boxToDynamic(callInst);
            return;
        }
    }

    if (recvVal->getType()->isInterface()) {
        auto ifaceType = std::static_pointer_cast<InterfaceType>(recvVal->getType());
        int methodIdx = ifaceType->getMethodIndex(node.getMethodName());
        const auto* methodInfo = ifaceType->findMethod(node.getMethodName());
        auto returnType = methodInfo ? methodInfo->returnType : Type::getVoid();

        // 1. Load data pointer from offset 0
        auto dataField = std::make_shared<GetFieldPtrInst>(recvVal, 0, 0, Type::getI64(), nextTemp());
        emitInst(dataField);
        auto dataPtr = std::make_shared<LoadInst>(dataField, nextTemp());
        emitInst(dataPtr);

        // 2. Load vtable pointer from offset 8
        auto vtField = std::make_shared<GetFieldPtrInst>(recvVal, 1, 8, Type::getI64(), nextTemp());
        emitInst(vtField);
        auto vtPtr = std::make_shared<LoadInst>(vtField, nextTemp());
        emitInst(vtPtr);

        // 3. Index into vtable at methodIdx
        auto fnSlot = std::make_shared<GetElementPtrInst>(vtPtr, ConstantInt::get(methodIdx, Type::getI64()), 8, Type::getI64(), nextTemp());
        emitInst(fnSlot);
        auto fnPtr = std::make_shared<LoadInst>(fnSlot, nextTemp());
        emitInst(fnPtr);

        // 4. Arguments: receiver (%this) is dataPtr
        std::vector<std::shared_ptr<IRValue>> args;
        args.push_back(dataPtr);

        for (size_t i = 0; i < node.getArgs().size(); ++i) {
            node.getArgs()[i]->accept(*this);
            auto argVal = m_lastValue;
            if (methodInfo && i < methodInfo->paramTypes.size()) {
                if (methodInfo->paramTypes[i]->isInterface() && argVal->getType()->isClass()) {
                    argVal = coerceToInterface(argVal, methodInfo->paramTypes[i]);
                }
            }
            args.push_back(argVal);
        }

        // 5. Indirect call
        auto callInst = std::make_shared<CallInst>(fnPtr, std::move(args), returnType, nextTemp());
        emitInst(callInst);
        m_lastValue = callInst;
        return;
    }

    if (recvVal->getType()->isDynamic()) {
        // Find which class implements this method
        std::shared_ptr<ClassType> foundClass = nullptr;
        for (const auto& [typeName, typePtr] : m_types) {
            if (typePtr && typePtr->isClass()) {
                auto ct = std::static_pointer_cast<ClassType>(typePtr);
                if (ct->findMethod(node.getMethodName())) {
                    foundClass = ct;
                    break;
                }
            }
        }

        std::shared_ptr<Type> nativeType = foundClass ? std::static_pointer_cast<Type>(foundClass) : Type::getI64();
        auto getNative = std::make_shared<CallInst>("sph_object_get_native",
                                                     std::vector<std::shared_ptr<IRValue>>{recvVal},
                                                     nativeType, nextTemp());
        emitInst(getNative);

        std::string mangledName;
        std::shared_ptr<Type> returnType = Type::getVoid();
        const ClassMethodInfo* methodInfo = nullptr;

        if (foundClass) {
            mangledName = foundClass->getName() + "_" + node.getMethodName();
            methodInfo = foundClass->findMethod(node.getMethodName());
            if (methodInfo) returnType = methodInfo->returnType;
        } else {
            mangledName = node.getMethodName();
        }

        std::vector<std::shared_ptr<IRValue>> args;
        args.push_back(getNative);

        for (size_t i = 0; i < node.getArgs().size(); ++i) {
            node.getArgs()[i]->accept(*this);
            auto argVal = m_lastValue;
            if (methodInfo && i < methodInfo->paramTypes.size()) {
                if (methodInfo->paramTypes[i]->isInterface() && argVal->getType()->isClass()) {
                    argVal = coerceToInterface(argVal, methodInfo->paramTypes[i]);
                } else if (methodInfo->paramTypes[i]->isDynamic() && !argVal->getType()->isDynamic()) {
                    argVal = boxToDynamic(argVal);
                }
            }
            args.push_back(argVal);
        }

        auto callInst = std::make_shared<CallInst>(mangledName, std::move(args), returnType, nextTemp());
        emitInst(callInst);

        if (!returnType->isVoid() && !returnType->isDynamic()) {
            m_lastValue = boxToDynamic(callInst);
        } else {
            m_lastValue = callInst;
        }
        return;
    }

    std::shared_ptr<ClassType> classType;
    if (recvVal->getType()->isPointer()) {
        auto ptrType = std::static_pointer_cast<PointerType>(recvVal->getType());
        classType = std::static_pointer_cast<ClassType>(ptrType->getPointeeType());
    } else {
        classType = std::static_pointer_cast<ClassType>(recvVal->getType());
    }
    std::string mangledName = classType->getName() + "_" + node.getMethodName();

    std::vector<std::shared_ptr<IRValue>> args;
    args.push_back(recvVal);

    auto methodInfo = classType->findMethod(node.getMethodName());
    auto returnType = methodInfo ? methodInfo->returnType : Type::getVoid();

    for (size_t i = 0; i < node.getArgs().size(); ++i) {
        node.getArgs()[i]->accept(*this);
        auto argVal = m_lastValue;
        if (methodInfo && i < methodInfo->paramTypes.size()) {
            if (methodInfo->paramTypes[i]->isInterface() && argVal->getType()->isClass()) {
                argVal = coerceToInterface(argVal, methodInfo->paramTypes[i]);
            } else if (methodInfo->paramTypes[i]->isDynamic() && !argVal->getType()->isDynamic()) {
                argVal = boxToDynamic(argVal);
            } else if (methodInfo->paramTypes[i]->isI64() && argVal->getType()->isI32()) {
                auto c = std::make_shared<CastInst>(argVal, Type::getI64(), nextTemp());
                emitInst(c);
                argVal = c;
            } else if (methodInfo->paramTypes[i]->isF64() && (argVal->getType()->isI32() || argVal->getType()->isI64())) {
                auto c = std::make_shared<CastInst>(argVal, Type::getF64(), nextTemp());
                emitInst(c);
                argVal = c;
            }
        }
        args.push_back(argVal);
    }

    auto callInst = std::make_shared<CallInst>(mangledName, std::move(args), returnType, nextTemp());
    emitInst(callInst);
    m_lastValue = callInst;
}

void IRGenerator::visit(InterfaceDecl& /*node*/) {
    // Processed during pre-registration in generate()
}

void IRGenerator::visit(RcNewExpr& node) {
    auto innerType = node.getValue().getInferredType();
    if (!innerType) {
        innerType = Type::getVoid();
    }
    size_t allocSize = innerType->getSize();
    if (allocSize == 0) allocSize = 1;

    auto rcPtr = std::make_shared<RcAllocInst>(innerType, allocSize, nextTemp());
    emitInst(rcPtr);

    if (auto* structInit = dynamic_cast<StructInitExpr*>(&node.getValue())) {
        auto structType = std::static_pointer_cast<StructType>(innerType);
        for (auto& f : structInit->getFields()) {
            const auto* member = structType->findMember(f.first);
            int fieldIdx = structType->getMemberIndex(f.first);
            f.second->accept(*this);
            auto val = m_lastValue;

            auto fieldPtr = std::make_shared<GetFieldPtrInst>(rcPtr, fieldIdx, member->offset, member->type, nextTemp());
            emitInst(fieldPtr);
            emitInst(std::make_shared<StoreInst>(val, fieldPtr));
        }
    } else {
        node.getValue().accept(*this);
        auto val = m_lastValue;
        if (innerType->isStruct() || innerType->isClass()) {
            auto structType = std::static_pointer_cast<StructType>(innerType);
            for (size_t i = 0; i < structType->getMembers().size(); ++i) {
                const auto& m = structType->getMembers()[i];
                auto srcPtr = std::make_shared<GetFieldPtrInst>(val, i, m.offset, m.type, nextTemp());
                emitInst(srcPtr);
                auto fVal = std::make_shared<LoadInst>(srcPtr, nextTemp());
                emitInst(fVal);
                auto dstPtr = std::make_shared<GetFieldPtrInst>(rcPtr, i, m.offset, m.type, nextTemp());
                emitInst(dstPtr);
                emitInst(std::make_shared<StoreInst>(fVal, dstPtr));
            }
        } else {
            emitInst(std::make_shared<StoreInst>(val, rcPtr));
        }
    }

    m_lastValue = rcPtr;
}

void IRGenerator::visit(RcCountExpr& node) {
    node.getPointer().accept(*this);
    auto ptrVal = m_lastValue;
    auto callInst = std::make_shared<CallInst>("sph_rc_count", std::vector<std::shared_ptr<IRValue>>{ptrVal}, Type::getI64(), nextTemp());
    emitInst(callInst);
    m_lastValue = callInst;
}

void IRGenerator::visit(BoxNewExpr& node) {
    auto innerType = node.getValue().getInferredType();
    if (!innerType) {
        innerType = Type::getVoid();
    }
    size_t allocSize = innerType->getSize();
    if (allocSize == 0) allocSize = 1;

    auto allocSizeVal = ConstantInt::get(static_cast<int64_t>(allocSize), Type::getI64());
    auto boxPtr = std::make_shared<CallInst>("sph_alloc", std::vector<std::shared_ptr<IRValue>>{allocSizeVal}, BoxType::get(innerType), nextTemp());
    emitInst(boxPtr);

    if (auto* structInit = dynamic_cast<StructInitExpr*>(&node.getValue())) {
        auto structType = std::static_pointer_cast<StructType>(innerType);
        for (auto& f : structInit->getFields()) {
            const auto* member = structType->findMember(f.first);
            int fieldIdx = structType->getMemberIndex(f.first);
            f.second->accept(*this);
            auto val = m_lastValue;

            auto fieldPtr = std::make_shared<GetFieldPtrInst>(boxPtr, fieldIdx, member->offset, member->type, nextTemp());
            emitInst(fieldPtr);
            emitInst(std::make_shared<StoreInst>(val, fieldPtr));
        }
    } else {
        node.getValue().accept(*this);
        auto val = m_lastValue;
        if (innerType->isStruct() || innerType->isClass()) {
            auto structType = std::static_pointer_cast<StructType>(innerType);
            for (size_t i = 0; i < structType->getMembers().size(); ++i) {
                const auto& m = structType->getMembers()[i];
                auto srcPtr = std::make_shared<GetFieldPtrInst>(val, i, m.offset, m.type, nextTemp());
                emitInst(srcPtr);
                auto fVal = std::make_shared<LoadInst>(srcPtr, nextTemp());
                emitInst(fVal);
                auto dstPtr = std::make_shared<GetFieldPtrInst>(boxPtr, i, m.offset, m.type, nextTemp());
                emitInst(dstPtr);
                emitInst(std::make_shared<StoreInst>(fVal, dstPtr));
            }
        } else {
            emitInst(std::make_shared<StoreInst>(val, boxPtr));
        }
    }

    m_lastValue = boxPtr;
}

std::shared_ptr<IRValue> IRGenerator::emitArenaAlloc(std::shared_ptr<IRValue> arenaVal, ExprAST& valueExpr) {
    auto innerType = valueExpr.getInferredType();
    if (!innerType) {
        innerType = Type::getVoid();
    }
    size_t allocSize = innerType->getSize();
    if (allocSize == 0) allocSize = 1;
    size_t align = innerType->getAlignment();
    if (align == 0) align = 8;

    auto allocSizeVal = ConstantInt::get(static_cast<int64_t>(allocSize), Type::getI64());
    auto alignVal = ConstantInt::get(static_cast<int64_t>(align), Type::getI64());

    auto arenaAlloc = std::make_shared<CallInst>(
        "sph_arena_alloc",
        std::vector<std::shared_ptr<IRValue>>{arenaVal, allocSizeVal, alignVal},
        PointerType::get(innerType),
        nextTemp()
    );
    emitInst(arenaAlloc);

    if (auto* structInit = dynamic_cast<StructInitExpr*>(&valueExpr)) {
        auto structType = std::static_pointer_cast<StructType>(innerType);
        for (auto& f : structInit->getFields()) {
            const auto* member = structType->findMember(f.first);
            int fieldIdx = structType->getMemberIndex(f.first);
            f.second->accept(*this);
            auto val = m_lastValue;

            auto fieldPtr = std::make_shared<GetFieldPtrInst>(arenaAlloc, fieldIdx, member->offset, member->type, nextTemp());
            emitInst(fieldPtr);
            emitInst(std::make_shared<StoreInst>(val, fieldPtr));
        }
    } else {
        valueExpr.accept(*this);
        auto val = m_lastValue;
        if (innerType->isStruct() || innerType->isClass()) {
            auto structType = std::static_pointer_cast<StructType>(innerType);
            for (size_t i = 0; i < structType->getMembers().size(); ++i) {
                const auto& m = structType->getMembers()[i];
                auto srcPtr = std::make_shared<GetFieldPtrInst>(val, i, m.offset, m.type, nextTemp());
                emitInst(srcPtr);
                auto fVal = std::make_shared<LoadInst>(srcPtr, nextTemp());
                emitInst(fVal);
                auto dstPtr = std::make_shared<GetFieldPtrInst>(arenaAlloc, i, m.offset, m.type, nextTemp());
                emitInst(dstPtr);
                emitInst(std::make_shared<StoreInst>(fVal, dstPtr));
            }
        } else {
            emitInst(std::make_shared<StoreInst>(val, arenaAlloc));
        }
    }

    return arenaAlloc;
}

void IRGenerator::visit(ArenaNewExpr& node) {
    std::shared_ptr<IRValue> capVal;
    if (node.hasCapacity()) {
        node.getCapacity().accept(*this);
        capVal = m_lastValue;
        if (auto* cInt = dynamic_cast<ConstantInt*>(capVal.get())) {
            capVal = ConstantInt::get(cInt->getValue(), Type::getI64());
        } else if (capVal->getType()->getSize() < 8) {
            auto castInst = std::make_shared<CastInst>(capVal, Type::getI64(), nextTemp());
            emitInst(castInst);
            capVal = castInst;
        }
    } else {
        capVal = ConstantInt::get(0, Type::getI64());
    }
    auto callInst = std::make_shared<CallInst>("sph_arena_create", std::vector<std::shared_ptr<IRValue>>{capVal}, ArenaType::get(), nextTemp());
    emitInst(callInst);
    m_lastValue = callInst;
}

void IRGenerator::visit(ArenaAllocExpr& node) {
    node.getArena().accept(*this);
    auto arenaVal = m_lastValue;
    m_lastValue = emitArenaAlloc(arenaVal, node.getValue());
}

void IRGenerator::visit(ArenaResetExpr& node) {
    node.getArena().accept(*this);
    auto arenaVal = m_lastValue;
    auto callInst = std::make_shared<CallInst>("sph_arena_reset", std::vector<std::shared_ptr<IRValue>>{arenaVal}, Type::getVoid(), nextTemp());
    emitInst(callInst);
    m_lastValue = callInst;
}

void IRGenerator::visit(ArenaDestroyExpr& node) {
    node.getArena().accept(*this);
    auto arenaVal = m_lastValue;
    auto callInst = std::make_shared<CallInst>("sph_arena_destroy", std::vector<std::shared_ptr<IRValue>>{arenaVal}, Type::getVoid(), nextTemp());
    emitInst(callInst);
    m_lastValue = callInst;
}

void IRGenerator::visit(VecNewExpr& node) {
    auto vecType = std::static_pointer_cast<VectorType>(node.getInferredType());
    auto elemType = vecType ? vecType->getElementType() : Type::getI32();
    size_t elemSize = elemType ? elemType->getSize() : 8;
    if (elemSize == 0) elemSize = 1;

    std::shared_ptr<IRValue> capVal;
    if (node.hasCapacity()) {
        node.getCapacity().accept(*this);
        capVal = m_lastValue;
        if (auto* cInt = dynamic_cast<ConstantInt*>(capVal.get())) {
            capVal = ConstantInt::get(cInt->getValue(), Type::getI64());
        } else if (capVal->getType()->getSize() < 8) {
            auto castInst = std::make_shared<CastInst>(capVal, Type::getI64(), nextTemp());
            emitInst(castInst);
            capVal = castInst;
        }
    } else {
        capVal = ConstantInt::get(4, Type::getI64());
    }

    auto elemSizeVal = ConstantInt::get(static_cast<int64_t>(elemSize), Type::getI64());
    auto callInst = std::make_shared<CallInst>("sph_vec_create", std::vector<std::shared_ptr<IRValue>>{elemSizeVal, capVal}, node.getInferredType(), nextTemp());
    emitInst(callInst);
    m_lastValue = callInst;
}

void IRGenerator::visit(NewExpr& node) {
    LocGuard locGuard(*this, node.getSpan().start);
    std::string className = node.getClassName();
    if (className == "Atomic<i32>") className = "AtomicI32";
    else if (className == "Atomic<i64>") className = "AtomicI64";
    else if (className == "Atomic<bool>") className = "AtomicBool";

    if (className == "Promise" || (className.rfind("Promise<", 0) == 0 && className.back() == '>')) {
        auto callInst = std::make_shared<CallInst>("sph_promise_create", std::vector<std::shared_ptr<IRValue>>{}, node.getInferredType() ? node.getInferredType() : Type::getI64(), nextTemp());
        emitInst(callInst);
        m_lastValue = callInst;
        return;
    }

    if (className == "AtomicI32" || className == "AtomicI64" || className == "AtomicBool") {
        auto it = m_types.find(className);
        auto classType = (it != m_types.end()) ? std::static_pointer_cast<ClassType>(it->second) : nullptr;
        if (!classType) return;

        auto elemType = classType->getMembers()[0].type;
        auto allocaInst = std::make_shared<AllocaInst>(classType, nextTemp());
        emitInst(allocaInst);

        auto fieldPtr = std::make_shared<GetFieldPtrInst>(allocaInst, 0, 0, elemType, nextTemp());
        emitInst(fieldPtr);

        std::shared_ptr<IRValue> initVal = nullptr;
        if (!node.getArgs().empty()) {
            node.getArgs()[0]->accept(*this);
            initVal = m_lastValue;
            if (elemType->isI64() && initVal && initVal->getType()->isI32()) {
                auto c = std::make_shared<CastInst>(initVal, Type::getI64(), nextTemp());
                emitInst(c);
                initVal = c;
            }
        } else {
            if (elemType->isBoolean()) initVal = ConstantBool::get(false);
            else initVal = ConstantInt::get(0, elemType);
        }
        emitInst(std::make_shared<StoreInst>(initVal, fieldPtr));
        m_lastValue = allocaInst;
        return;
    }

    if (node.getClassName() == "PDO") {
        std::vector<std::shared_ptr<IRValue>> args;
        for (auto& arg : node.getArgs()) {
            arg->accept(*this);
            args.push_back(m_lastValue);
        }
        while (args.size() < 3) {
            args.push_back(ConstantInt::get(0, Type::getI64()));
        }
        auto callInst = std::make_shared<CallInst>("sph_pdo_open", std::move(args), Type::getDynamic(), nextTemp());
        emitInst(callInst);
        m_lastValue = callInst;
        return;
    }

    if (className == "f32x4" || className == "i32x4" || className == "f64x2" || className == "i64x2") {
        auto vecType = (className == "f32x4") ? Type::getF32x4() :
                       (className == "i32x4") ? Type::getI32x4() :
                       (className == "f64x2") ? Type::getF64x2() : Type::getI64x2();
        auto allocaInst = std::make_shared<AllocaInst>(vecType, nextTemp());
        emitInst(allocaInst);

        std::vector<std::shared_ptr<IRValue>> argVals;
        for (auto& arg : node.getArgs()) {
            arg->accept(*this);
            argVals.push_back(m_lastValue);
        }

        size_t count = vecType->getMembers().size();
        for (size_t i = 0; i < count; ++i) {
            const auto& m = vecType->getMembers()[i];
            auto fieldPtr = std::make_shared<GetFieldPtrInst>(allocaInst, static_cast<int>(i), m.offset, m.type, nextTemp());
            emitInst(fieldPtr);
            std::shared_ptr<IRValue> val = nullptr;
            if (argVals.size() == count) val = argVals[i];
            else if (argVals.size() == 1) val = argVals[0];
            else {
                if (m.type->isFloatingPoint()) val = ConstantFloat::get(0.0, m.type);
                else val = ConstantInt::get(0, m.type);
            }
            if (val) {
                if (m.type->isF32() && val->getType() != Type::getF32()) {
                    if (val->isConstant()) {
                        val = ConstantFloat::get(std::stof(val->getName()), Type::getF32());
                    } else {
                        auto c = std::make_shared<CastInst>(val, Type::getF32(), nextTemp());
                        emitInst(c);
                        val = c;
                    }
                } else if (m.type->isF64() && val->getType() != Type::getF64()) {
                    if (val->isConstant()) {
                        val = ConstantFloat::get(std::stod(val->getName()), Type::getF64());
                    } else {
                        auto c = std::make_shared<CastInst>(val, Type::getF64(), nextTemp());
                        emitInst(c);
                        val = c;
                    }
                } else if (m.type->isI32() && val->getType() != Type::getI32()) {
                    if (val->isConstant()) {
                        val = ConstantInt::get(std::stoi(val->getName()), Type::getI32());
                    } else {
                        auto c = std::make_shared<CastInst>(val, Type::getI32(), nextTemp());
                        emitInst(c);
                        val = c;
                    }
                } else if (m.type->isI64() && val->getType() != Type::getI64()) {
                    if (val->isConstant()) {
                        val = ConstantInt::get(std::stoll(val->getName()), Type::getI64());
                    } else {
                        auto c = std::make_shared<CastInst>(val, Type::getI64(), nextTemp());
                        emitInst(c);
                        val = c;
                    }
                }
            }
            emitInst(std::make_shared<StoreInst>(val, fieldPtr));
        }
        m_lastValue = allocaInst;
        return;
    }

    auto classType = std::static_pointer_cast<ClassType>(node.getInferredType());
    if (!classType) {
        auto it = m_types.find(node.getClassName());
        if (it != m_types.end() && it->second && it->second->isClass()) {
            classType = std::static_pointer_cast<ClassType>(it->second);
        }
    }
    if (!classType) return;

    auto allocaInst = std::make_shared<AllocaInst>(classType, nextTemp());
    emitInst(allocaInst);

    // Initialize all fields to default / null / zero
    for (size_t i = 0; i < classType->getMembers().size(); ++i) {
        const auto& m = classType->getMembers()[i];
        auto fieldPtr = std::make_shared<GetFieldPtrInst>(allocaInst, static_cast<int>(i), m.offset, m.type, nextTemp());
        emitInst(fieldPtr);

        std::shared_ptr<IRValue> defaultVal = nullptr;
        if (m.type->isDynamic()) {
            defaultVal = boxToDynamic(nullptr);
        } else if (m.type->isNumeric()) {
            defaultVal = ConstantInt::get(0, m.type);
        } else if (m.type->isBoolean()) {
            defaultVal = ConstantBool::get(false);
        } else if (m.type->isString()) {
            defaultVal = ConstantString::get("");
        }
        if (defaultVal) {
            emitInst(std::make_shared<StoreInst>(defaultVal, fieldPtr));
        }
    }

    // Check for constructor (__construct or ClassName)
    const ClassMethodInfo* ctor = classType->findMethod("__construct");
    std::string ctorName = classType->getName() + "___construct";
    if (!ctor) {
        ctor = classType->findMethod(node.getClassName());
        ctorName = classType->getName() + "_" + node.getClassName();
    }

    if (ctor) {
        std::vector<std::shared_ptr<IRValue>> args;
        args.push_back(allocaInst);

        for (size_t i = 0; i < node.getArgs().size(); ++i) {
            node.getArgs()[i]->accept(*this);
            auto argVal = m_lastValue;
            if (i < ctor->paramTypes.size()) {
                if (ctor->paramTypes[i]->isInterface() && argVal->getType()->isClass()) {
                    argVal = coerceToInterface(argVal, ctor->paramTypes[i]);
                } else if (ctor->paramTypes[i]->isDynamic() && !argVal->getType()->isDynamic()) {
                    argVal = boxToDynamic(argVal);
                }
            }
            args.push_back(argVal);
        }

        auto ctorCall = std::make_shared<CallInst>(ctorName, std::move(args), ctor->returnType, nextTemp());
        emitInst(ctorCall);
    }

    m_lastValue = allocaInst;
}

void IRGenerator::visit(AwaitExpr& node) {
    LocGuard locGuard(*this, node.getSpan().start);
    node.getExpr().accept(*this);
    auto promiseVal = m_lastValue;
    if (!promiseVal) return;

    auto retType = node.getInferredType() ? node.getInferredType() : Type::getI64();
    auto callInst = std::make_shared<CallInst>("sph_promise_await", std::vector<std::shared_ptr<IRValue>>{promiseVal}, Type::getI64(), nextTemp());
    emitInst(callInst);

    if (retType->isI32()) {
        auto c = std::make_shared<CastInst>(callInst, Type::getI32(), nextTemp());
        emitInst(c);
        m_lastValue = c;
    } else {
        m_lastValue = callInst;
    }
}

} // namespace sph
