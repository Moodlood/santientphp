#include "LLVMEmitter.hpp"
#include "compiler/ir/IRBasicBlock.hpp"
#include "compiler/ir/IRFunction.hpp"
#include "compiler/ir/IRInstruction.hpp"
#include <iostream>

namespace sph {

std::string LLVMEmitter::mapType(const Type& type) {
    switch (type.getKind()) {
        case TypeKind::Void:     return "void";
        case TypeKind::I32:      return "i32";
        case TypeKind::I64:      return "i64";
        case TypeKind::F32:      return "float";
        case TypeKind::F64:      return "double";
        case TypeKind::Bool:     return "i1";
        case TypeKind::String:   return "ptr";
        case TypeKind::Function: return "ptr";
        case TypeKind::Struct:   return "%struct." + static_cast<const StructType&>(type).getName();
        case TypeKind::Class:     return "%class." + static_cast<const StructType&>(type).getName();
        case TypeKind::Slice:     return "{ ptr, i32 }";
        case TypeKind::Interface: return "{ ptr, ptr }";
        case TypeKind::Enum:      return "i32";
        case TypeKind::Array: {
            const auto& arr = static_cast<const ArrayType&>(type);
            return "[" + std::to_string(arr.getElementCount()) + " x " + mapType(*arr.getElementType()) + "]";
        }
        case TypeKind::Pointer:
        case TypeKind::Rc:
        case TypeKind::Box:
        case TypeKind::Arena:
        case TypeKind::Vector:
        case TypeKind::Result:
        case TypeKind::Dynamic:   return "ptr";
        case TypeKind::Error:
        case TypeKind::Unknown:  return "void";
    }
    return "void";
}

std::string LLVMEmitter::formatValue(const IRValue& val) const {
    if (val.isConstant()) {
        if (const auto* global = dynamic_cast<const ConstantGlobal*>(&val)) {
            return "@" + global->getGlobalName();
        }
        if (val.getType()->isBoolean()) {
            return (val.getName() == "true") ? "1" : "0";
        }
        return val.getName();
    }

    if (!val.getName().empty() && val.getName().front() != '%') {
        return "%" + val.getName();
    }
    return val.getName();
}

void LLVMEmitter::emit(const IRModule& module) {
    m_out << "; ModuleID = '" << module.getName() << "'\n";
    m_out << "target datalayout = \"" << m_target.getDataLayout() << "\"\n";
    m_out << "target triple = \"" << m_target.getLLVMTriple() << "\"\n\n";

    for (const auto& vt : module.getVTables()) {
        m_out << "@" << vt.name << " = constant [" << vt.functionNames.size() << " x ptr] [";
        for (size_t i = 0; i < vt.functionNames.size(); ++i) {
            if (i > 0) m_out << ", ";
            m_out << "ptr @" << vt.functionNames[i];
        }
        m_out << "]\n";
    }
    if (!module.getVTables().empty()) {
        m_out << "\n";
    }

    // Runtime declarations
    m_out << "declare void @sph_println_i32(i32)\n";
    m_out << "declare void @sph_println_i64(i64)\n";
    m_out << "declare void @sph_println_f32(float)\n";
    m_out << "declare void @sph_println_f64(double)\n";
    m_out << "declare void @sph_println_bool(i32)\n";
    m_out << "declare void @sph_println_string(ptr)\n";
    m_out << "declare void @sph_println_f32x4(ptr)\n";
    m_out << "declare void @sph_println_i32x4(ptr)\n";
    m_out << "declare void @sph_println_f64x2(ptr)\n";
    m_out << "declare void @sph_println_i64x2(ptr)\n";
    m_out << "declare ptr @sph_rc_alloc(i64)\n";
    m_out << "declare void @sph_rc_retain(ptr)\n";
    m_out << "declare void @sph_rc_release(ptr)\n";
    m_out << "declare i64 @sph_rc_count(ptr)\n";
    m_out << "declare ptr @sph_file_read(ptr)\n";
    m_out << "declare i32 @sph_file_write(ptr, ptr)\n";
    m_out << "declare i32 @sph_file_append(ptr, ptr)\n";
    m_out << "declare i32 @sph_file_exists(ptr)\n";
    m_out << "declare i32 @sph_file_delete(ptr)\n";
    m_out << "declare void @sph_process_exit(i32)\n";
    m_out << "declare ptr @sph_env_get(ptr)\n";
    m_out << "declare i32 @sph_env_set(ptr, ptr)\n";
    m_out << "declare i64 @sph_string_len(ptr)\n";
    m_out << "declare ptr @sph_string_concat(ptr, ptr)\n";
    m_out << "declare i32 @sph_string_eq(ptr, ptr)\n";
    m_out << "declare ptr @sph_string_slice(ptr, i64, i64)\n";
    m_out << "declare ptr @sph_vec_create(i64, i64)\n";
    m_out << "declare void @sph_vec_push(ptr, ptr)\n";
    m_out << "declare void @sph_vec_pop(ptr, ptr)\n";
    m_out << "declare ptr @sph_vec_get(ptr, i64)\n";
    m_out << "declare void @sph_vec_set(ptr, i64, ptr)\n";
    m_out << "declare i64 @sph_vec_len(ptr)\n";
    m_out << "declare i64 @sph_vec_cap(ptr)\n";
    m_out << "declare void @sph_vec_destroy(ptr)\n";
    m_out << "declare void @sph_panic(ptr)\n";
    m_out << "declare ptr @sph_result_create_ok(i64, ptr)\n";
    m_out << "declare ptr @sph_result_create_err(i64, ptr)\n";
    m_out << "declare i32 @sph_result_is_ok(ptr)\n";
    m_out << "declare i32 @sph_result_is_err(ptr)\n";
    m_out << "declare ptr @sph_result_unwrap(ptr)\n";
    m_out << "declare void @sph_result_destroy(ptr)\n";
    m_out << "declare ptr @sph_value_create_null()\n";
    m_out << "declare ptr @sph_value_create_bool(i32)\n";
    m_out << "declare ptr @sph_value_create_int(i64)\n";
    m_out << "declare ptr @sph_value_create_float(double)\n";
    m_out << "declare ptr @sph_value_create_string(ptr)\n";
    m_out << "declare ptr @sph_value_create_array(ptr)\n";
    m_out << "declare void @sph_value_destroy(ptr)\n";
    m_out << "declare i32 @sph_value_get_type(ptr)\n";
    m_out << "declare i32 @sph_value_to_bool(ptr)\n";
    m_out << "declare i64 @sph_value_to_int(ptr)\n";
    m_out << "declare double @sph_value_to_float(ptr)\n";
    m_out << "declare ptr @sph_value_to_string(ptr)\n";
    m_out << "declare ptr @sph_dynamic_add(ptr, ptr)\n";
    m_out << "declare ptr @sph_dynamic_sub(ptr, ptr)\n";
    m_out << "declare ptr @sph_dynamic_mul(ptr, ptr)\n";
    m_out << "declare ptr @sph_dynamic_div(ptr, ptr)\n";
    m_out << "declare ptr @sph_dynamic_mod(ptr, ptr)\n";
    m_out << "declare ptr @sph_dynamic_concat(ptr, ptr)\n";
    m_out << "declare i32 @sph_dynamic_eq(ptr, ptr)\n";
    m_out << "declare i32 @sph_dynamic_ne(ptr, ptr)\n";
    m_out << "declare i32 @sph_dynamic_lt(ptr, ptr)\n";
    m_out << "declare i32 @sph_dynamic_le(ptr, ptr)\n";
    m_out << "declare i32 @sph_dynamic_gt(ptr, ptr)\n";
    m_out << "declare i32 @sph_dynamic_ge(ptr, ptr)\n";
    m_out << "declare void @sph_echo_value(ptr)\n";
    m_out << "declare void @sph_echo_string(ptr)\n";
    m_out << "declare void @sph_echo_i32(i32)\n";
    m_out << "declare void @sph_echo_i64(i64)\n";
    m_out << "declare void @sph_echo_f64(double)\n";
    m_out << "declare void @sph_echo_bool(i32)\n";
    m_out << "declare void @sph_var_dump(ptr)\n";
    m_out << "declare ptr @sph_value_create_array_empty()\n";
    m_out << "declare ptr @sph_array_create()\n";
    m_out << "declare ptr @sph_array_get(ptr, ptr)\n";
    m_out << "declare void @sph_array_set(ptr, ptr, ptr)\n";
    m_out << "declare void @sph_array_append(ptr, ptr)\n";
    m_out << "declare i64 @sph_array_count(ptr)\n";
    m_out << "declare void @sph_array_destroy(ptr)\n";
    m_out << "declare void @sph_exception_push_frame(ptr)\n";
    m_out << "declare void @sph_exception_pop_frame()\n";
    m_out << "declare void @sph_exception_throw(ptr)\n";
    m_out << "declare ptr @sph_exception_get_current()\n";
    m_out << "declare i32 @sph_setjmp(ptr)\n";
    m_out << "declare ptr @sph_pdo_open(ptr, ptr, ptr)\n";
    m_out << "declare void @sph_pdo_close(ptr)\n";
    m_out << "declare i64 @sph_pdo_exec(ptr, ptr)\n";
    m_out << "declare ptr @sph_pdo_query(ptr, ptr)\n";
    m_out << "declare ptr @sph_pdo_prepare(ptr, ptr)\n";
    m_out << "declare i64 @sph_pdo_last_insert_id(ptr)\n";
    m_out << "declare i32 @sph_pdostatement_execute(ptr, ptr)\n";
    m_out << "declare ptr @sph_pdostatement_fetch(ptr, i32)\n";
    m_out << "declare ptr @sph_pdostatement_fetch_all(ptr, i32)\n";
    m_out << "declare i64 @sph_pdostatement_row_count(ptr)\n";
    m_out << "declare i32 @sph_pdostatement_column_count(ptr)\n";
    m_out << "declare void @sph_pdostatement_close(ptr)\n";
    m_out << "declare ptr @sph_curl_init(ptr)\n";
    m_out << "declare i32 @sph_curl_setopt(ptr, i32, ptr)\n";
    m_out << "declare ptr @sph_curl_exec(ptr)\n";
    m_out << "declare ptr @sph_curl_getinfo(ptr, i32)\n";
    m_out << "declare void @sph_curl_close(ptr)\n";
    m_out << "declare ptr @sph_http_get(ptr)\n";
    m_out << "declare i32 @sph_atomic_load_i32(ptr, i32)\n";
    m_out << "declare void @sph_atomic_store_i32(ptr, i32, i32)\n";
    m_out << "declare i32 @sph_atomic_fetch_add_i32(ptr, i32, i32)\n";
    m_out << "declare i32 @sph_atomic_fetch_sub_i32(ptr, i32, i32)\n";
    m_out << "declare i32 @sph_atomic_exchange_i32(ptr, i32, i32)\n";
    m_out << "declare i32 @sph_atomic_compare_exchange_i32(ptr, i32, i32, i32, i32)\n";
    m_out << "declare i64 @sph_atomic_load_i64(ptr, i32)\n";
    m_out << "declare void @sph_atomic_store_i64(ptr, i64, i32)\n";
    m_out << "declare i64 @sph_atomic_fetch_add_i64(ptr, i64, i32)\n";
    m_out << "declare i64 @sph_atomic_fetch_sub_i64(ptr, i64, i32)\n";
    m_out << "declare i64 @sph_atomic_exchange_i64(ptr, i64, i32)\n";
    m_out << "declare i32 @sph_atomic_compare_exchange_i64(ptr, i64, i64, i32, i32)\n";
    m_out << "declare i32 @sph_atomic_load_bool(ptr, i32)\n";
    m_out << "declare void @sph_atomic_store_bool(ptr, i32, i32)\n";
    m_out << "declare i32 @sph_atomic_exchange_bool(ptr, i32, i32)\n";
    m_out << "declare i32 @sph_atomic_compare_exchange_bool(ptr, i32, i32, i32, i32)\n";
    m_out << "declare ptr @sph_thread_spawn(ptr, ptr)\n";
    m_out << "declare void @sph_thread_join(ptr)\n";
    m_out << "declare void @sph_thread_sleep_ms(i32)\n";
    m_out << "declare ptr @sph_promise_create()\n";
    m_out << "declare void @sph_promise_resolve(ptr, ptr)\n";
    m_out << "declare void @sph_promise_reject(ptr, ptr)\n";
    m_out << "declare i32 @sph_promise_is_ready(ptr)\n";
    m_out << "declare ptr @sph_promise_get_value(ptr)\n";
    m_out << "declare ptr @sph_promise_await(ptr)\n";
    m_out << "declare void @sph_event_loop_init()\n";
    m_out << "declare ptr @sph_event_loop_spawn(ptr, ptr)\n";
    m_out << "declare void @sph_event_loop_yield()\n";
    m_out << "declare void @sph_event_loop_run()\n\n";

    for (const auto& fn : module.getFunctions()) {
        emitFunction(*fn);
        m_out << "\n";
    }

    if (m_debugInfo) {
        m_out << "; Debug metadata\n";
        m_out << "!llvm.dbg.cu = !{!0}\n";
        m_out << "!llvm.module.flags = !{!2, !3}\n";
        m_out << "!llvm.ident = !{!4}\n\n";

        std::string mainFile = "main.sph";
        if (!module.getSourceFiles().empty()) {
            mainFile = module.getSourceFiles().begin()->second;
        }
        for (char& c : mainFile) {
            if (c == '\\') c = '/';
        }

        m_out << "!0 = distinct !DICompileUnit(language: DW_LANG_C99, file: !1, producer: \"sphc 0.1\", isOptimized: false, runtimeVersion: 0, emissionKind: FullDebug)\n";
        m_out << "!1 = !DIFile(filename: \"" << mainFile << "\", directory: \".\")\n";
        m_out << "!2 = !{i32 7, !\"Dwarf Version\", i32 4}\n";
        m_out << "!3 = !{i32 2, !\"Debug Info Version\", i32 3}\n";
        m_out << "!4 = !{!\"sphc 0.1\"}\n";
    }
}

void LLVMEmitter::emitFunction(const IRFunction& fn) {
    if (fn.isDeclaration()) {
        m_out << "declare " << mapType(*fn.getReturnType()) << " @" << fn.getName() << "(";
        const auto& params = fn.getParameters();
        for (size_t i = 0; i < params.size(); ++i) {
            if (i > 0) m_out << ", ";
            m_out << mapType(*params[i]->getType());
        }
        m_out << ")\n";
        return;
    }

    m_out << "define " << mapType(*fn.getReturnType()) << " @" << fn.getName() << "(";
    const auto& params = fn.getParameters();
    for (size_t i = 0; i < params.size(); ++i) {
        if (i > 0) m_out << ", ";
        m_out << mapType(*params[i]->getType()) << " " << formatValue(*params[i]);
    }
    m_out << ") {\n";

    for (const auto& bb : fn.getBlocks()) {
        emitBasicBlock(*bb);
    }

    m_out << "}\n";
}

void LLVMEmitter::emitBasicBlock(const IRBasicBlock& bb) {
    m_out << bb.getName() << ":\n";
    for (const auto& inst : bb.getInstructions()) {
        m_out << "  ";
        emitInstruction(*inst);
        if (m_debugInfo && inst->getLocation().isValid()) {
            m_out << " ; !dbg " << inst->getLocation().line << ":" << inst->getLocation().column;
        }
        m_out << "\n";
    }
}

void LLVMEmitter::emitInstruction(const IRInstruction& inst) {
    switch (inst.getOpcode()) {
        case IROpcode::Alloca: {
            const auto& allocaInst = static_cast<const AllocaInst&>(inst);
            m_out << formatValue(allocaInst) << " = alloca " << mapType(*allocaInst.getAllocatedType());
            break;
        }
        case IROpcode::Load: {
            const auto& loadInst = static_cast<const LoadInst&>(inst);
            m_out << formatValue(loadInst) << " = load " << mapType(*loadInst.getType())
                  << ", ptr " << formatValue(*loadInst.getPointer());
            break;
        }
        case IROpcode::Store: {
            const auto& storeInst = static_cast<const StoreInst&>(inst);
            m_out << "store " << mapType(*storeInst.getValue()->getType()) << " "
                  << formatValue(*storeInst.getValue()) << ", ptr "
                  << formatValue(*storeInst.getPointer());
            break;
        }
        case IROpcode::GetFieldPtr: {
            const auto& gfp = static_cast<const GetFieldPtrInst&>(inst);
            std::string structName = "struct";
            if (gfp.getBasePtr()->getType()->isStruct() || gfp.getBasePtr()->getType()->isClass()) {
                structName = mapType(*gfp.getBasePtr()->getType());
            }
            m_out << formatValue(gfp) << " = getelementptr inbounds " << structName
                  << ", ptr " << formatValue(*gfp.getBasePtr()) << ", i32 0, i32 " << gfp.getFieldIndex();
            break;
        }
        case IROpcode::GetElementPtr: {
            const auto& gep = static_cast<const GetElementPtrInst&>(inst);
            std::string elemType = mapType(*gep.getType());
            m_out << formatValue(gep) << " = getelementptr inbounds " << elemType
                  << ", ptr " << formatValue(*gep.getBasePtr()) << ", "
                  << mapType(*gep.getIndex()->getType()) << " " << formatValue(*gep.getIndex());
            break;
        }
        case IROpcode::AddressOf: {
            const auto& addrInst = static_cast<const AddressOfInst&>(inst);
            m_out << formatValue(addrInst) << " = getelementptr i8, ptr "
                  << formatValue(*addrInst.getTarget()) << ", i32 0";
            break;
        }
        case IROpcode::Add:
        case IROpcode::Sub:
        case IROpcode::Mul:
        case IROpcode::Div:
        case IROpcode::Mod: {
            const auto& binInst = static_cast<const BinaryInst&>(inst);
            std::string opStr;
            if (binInst.getType()->isFloatingPoint()) {
                switch (binInst.getOpcode()) {
                    case IROpcode::Add: opStr = "fadd"; break;
                    case IROpcode::Sub: opStr = "fsub"; break;
                    case IROpcode::Mul: opStr = "fmul"; break;
                    case IROpcode::Div: opStr = "fdiv"; break;
                    case IROpcode::Mod: opStr = "frem"; break;
                    default:            opStr = "fadd"; break;
                }
            } else {
                switch (binInst.getOpcode()) {
                    case IROpcode::Add: opStr = "add"; break;
                    case IROpcode::Sub: opStr = "sub"; break;
                    case IROpcode::Mul: opStr = "mul"; break;
                    case IROpcode::Div: opStr = "sdiv"; break;
                    case IROpcode::Mod: opStr = "srem"; break;
                    default:            opStr = "add"; break;
                }
            }
            m_out << formatValue(binInst) << " = " << opStr << " "
                  << mapType(*binInst.getLhs()->getType()) << " "
                  << formatValue(*binInst.getLhs()) << ", " << formatValue(*binInst.getRhs());
            break;
        }
        case IROpcode::Equal:
        case IROpcode::NotEqual:
        case IROpcode::Less:
        case IROpcode::LessEqual:
        case IROpcode::Greater:
        case IROpcode::GreaterEqual: {
            const auto& binInst = static_cast<const BinaryInst&>(inst);
            std::string cmpPred;
            switch (binInst.getOpcode()) {
                case IROpcode::Equal:        cmpPred = "eq"; break;
                case IROpcode::NotEqual:     cmpPred = "ne"; break;
                case IROpcode::Less:         cmpPred = "slt"; break;
                case IROpcode::LessEqual:    cmpPred = "sle"; break;
                case IROpcode::Greater:      cmpPred = "sgt"; break;
                case IROpcode::GreaterEqual: cmpPred = "sge"; break;
                default:                     cmpPred = "eq"; break;
            }
            m_out << formatValue(binInst) << " = icmp " << cmpPred << " "
                  << mapType(*binInst.getLhs()->getType()) << " "
                  << formatValue(*binInst.getLhs()) << ", " << formatValue(*binInst.getRhs());
            break;
        }
        case IROpcode::LogicalAnd:
        case IROpcode::LogicalOr: {
            const auto& binInst = static_cast<const BinaryInst&>(inst);
            std::string logOp = (binInst.getOpcode() == IROpcode::LogicalAnd) ? "and" : "or";
            m_out << formatValue(binInst) << " = " << logOp << " i1 "
                  << formatValue(*binInst.getLhs()) << ", " << formatValue(*binInst.getRhs());
            break;
        }
        case IROpcode::BitwiseAnd:
        case IROpcode::BitwiseOr:
        case IROpcode::BitwiseXor:
        case IROpcode::ShiftLeft:
        case IROpcode::ShiftRight: {
            const auto& binInst = static_cast<const BinaryInst&>(inst);
            std::string opStr;
            switch (binInst.getOpcode()) {
                case IROpcode::BitwiseAnd: opStr = "and"; break;
                case IROpcode::BitwiseOr:  opStr = "or"; break;
                case IROpcode::BitwiseXor: opStr = "xor"; break;
                case IROpcode::ShiftLeft:  opStr = "shl"; break;
                case IROpcode::ShiftRight: opStr = "ashr"; break;
                default: break;
            }
            m_out << formatValue(binInst) << " = " << opStr << " "
                  << mapType(*binInst.getLhs()->getType()) << " "
                  << formatValue(*binInst.getLhs()) << ", " << formatValue(*binInst.getRhs());
            break;
        }
        case IROpcode::Negate: {
            const auto& unInst = static_cast<const UnaryInst&>(inst);
            if (unInst.getType()->isFloatingPoint()) {
                m_out << formatValue(unInst) << " = fneg " << mapType(*unInst.getType()) << " "
                      << formatValue(*unInst.getOperand());
            } else {
                m_out << formatValue(unInst) << " = sub " << mapType(*unInst.getType()) << " 0, "
                      << formatValue(*unInst.getOperand());
            }
            break;
        }
        case IROpcode::LogicalNot: {
            const auto& unInst = static_cast<const UnaryInst&>(inst);
            m_out << formatValue(unInst) << " = xor i1 " << formatValue(*unInst.getOperand()) << ", 1";
            break;
        }
        case IROpcode::BitwiseNot: {
            const auto& unInst = static_cast<const UnaryInst&>(inst);
            m_out << formatValue(unInst) << " = xor " << mapType(*unInst.getType()) << " "
                  << formatValue(*unInst.getOperand()) << ", -1";
            break;
        }
        case IROpcode::Cast: {
            const auto& castInst = static_cast<const CastInst&>(inst);
            auto srcType = castInst.getOperand()->getType();
            auto dstType = castInst.getType();
            std::string castOp = "bitcast";

            if (srcType->isPointer() && dstType->isInteger()) {
                castOp = "ptrtoint";
            } else if (srcType->isInteger() && dstType->isPointer()) {
                castOp = "inttoptr";
            } else if (srcType->isInteger() && dstType->isInteger()) {
                if (srcType->getSize() < dstType->getSize()) castOp = "sext";
                else if (srcType->getSize() > dstType->getSize()) castOp = "trunc";
                else castOp = "bitcast";
            } else if (srcType->isFloatingPoint() && dstType->isInteger()) {
                castOp = "fptosi";
            } else if (srcType->isInteger() && dstType->isFloatingPoint()) {
                castOp = "sitofp";
            }

            m_out << formatValue(castInst) << " = " << castOp << " "
                  << mapType(*srcType) << " " << formatValue(*castInst.getOperand())
                  << " to " << mapType(*dstType);
            break;
        }
        case IROpcode::Ret: {
            const auto& retInst = static_cast<const RetInst&>(inst);
            if (retInst.hasValue()) {
                m_out << "ret " << mapType(*retInst.getValue()->getType()) << " "
                      << formatValue(*retInst.getValue());
            } else {
                m_out << "ret void";
            }
            break;
        }
        case IROpcode::Br: {
            const auto& brInst = static_cast<const BranchInst&>(inst);
            m_out << "br label %" << brInst.getTarget()->getName();
            break;
        }
        case IROpcode::CondBr: {
            const auto& condBr = static_cast<const CondBranchInst&>(inst);
            m_out << "br i1 " << formatValue(*condBr.getCondition())
                  << ", label %" << condBr.getTrueTarget()->getName()
                  << ", label %" << condBr.getFalseTarget()->getName();
            break;
        }
        case IROpcode::Call: {
            const auto& callInst = static_cast<const CallInst&>(inst);

            if (!callInst.getType()->isVoid()) {
                m_out << formatValue(callInst) << " = ";
            }

            if (callInst.isIndirect()) {
                m_out << "call " << mapType(*callInst.getType()) << " " << formatValue(*callInst.getCalleeValue()) << "(";
            } else {
                std::string calleeName = callInst.getCallee();

                // Map builtin var_dump to runtime function
                if (calleeName == "var_dump") {
                    calleeName = "sph_var_dump";
                }

                // Map builtin println to runtime function
                if (calleeName == "println" && !callInst.getArgs().empty()) {
                    auto argType = callInst.getArgs()[0]->getType();
                    if (argType->isF32x4()) calleeName = "sph_println_f32x4";
                    else if (argType->isI32x4()) calleeName = "sph_println_i32x4";
                    else if (argType->isF64x2()) calleeName = "sph_println_f64x2";
                    else if (argType->isI64x2()) calleeName = "sph_println_i64x2";
                    else if (argType->isI64()) calleeName = "sph_println_i64";
                    else if (argType->isF32()) calleeName = "sph_println_f32";
                    else if (argType->isFloatingPoint()) calleeName = "sph_println_f64";
                    else if (argType->isBoolean()) calleeName = "sph_println_bool";
                    else if (argType->isString()) calleeName = "sph_println_string";
                    else calleeName = "sph_println_i32";
                }

                m_out << "call " << mapType(*callInst.getType()) << " @" << calleeName << "(";
            }

            const auto& args = callInst.getArgs();
            for (size_t i = 0; i < args.size(); ++i) {
                if (i > 0) m_out << ", ";
                m_out << mapType(*args[i]->getType()) << " " << formatValue(*args[i]);
            }
            m_out << ")";
            break;
        }
        case IROpcode::RcAlloc: {
            const auto& rcAlloc = static_cast<const RcAllocInst&>(inst);
            m_out << formatValue(rcAlloc) << " = call ptr @sph_rc_alloc(i64 " << rcAlloc.getAllocSize() << ")";
            break;
        }
        case IROpcode::RcRetain: {
            const auto& rcRetain = static_cast<const RcRetainInst&>(inst);
            m_out << "call void @sph_rc_retain(ptr " << formatValue(*rcRetain.getPtr()) << ")";
            break;
        }
        case IROpcode::RcRelease: {
            const auto& rcRelease = static_cast<const RcReleaseInst&>(inst);
            m_out << "call void @sph_rc_release(ptr " << formatValue(*rcRelease.getPtr()) << ")";
            break;
        }
        case IROpcode::SetJmp: {
            const auto& sj = static_cast<const SetJmpInst&>(inst);
            m_out << formatValue(sj) << " = call i32 @sph_setjmp(ptr " << formatValue(*sj.getFramePtr()) << ")";
            break;
        }
    }
}

} // namespace sph

