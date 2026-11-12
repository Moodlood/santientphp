#pragma once

#include "compiler/docgen/DocComment.hpp"
#include <cstdint>
#include <string>
#include <vector>

namespace sph {

struct DocParamInfo {
    std::string name;
    std::string type;
    std::string description;
    bool isByRef = false;
};

struct DocFunction {
    std::string name;
    std::string returnType;
    std::vector<DocParamInfo> params;
    std::vector<std::string> typeParams;
    bool isAsync = false;
    bool isExtern = false;
    std::string abi;
    DocComment doc;
    uint32_t line = 0;
};

struct DocFieldInfo {
    std::string name;
    std::string type;
    DocComment doc;
};

struct DocStruct {
    std::string name;
    std::vector<std::string> typeParams;
    std::vector<DocFieldInfo> fields;
    DocComment doc;
    uint32_t line = 0;
};

struct DocClass {
    std::string name;
    std::vector<std::string> implementsInterfaces;
    std::vector<DocFieldInfo> fields;
    std::vector<DocFunction> methods;
    DocComment doc;
    uint32_t line = 0;
};

struct DocInterface {
    std::string name;
    std::vector<DocFunction> methods;
    DocComment doc;
    uint32_t line = 0;
};

struct DocEnumVariantInfo {
    std::string name;
    std::string explicitValue;
    DocComment doc;
};

struct DocEnum {
    std::string name;
    std::vector<DocEnumVariantInfo> variants;
    DocComment doc;
    uint32_t line = 0;
};

struct DocModule {
    std::string name;
    std::string sourceFile;
    DocComment moduleDoc;
    std::vector<DocFunction> functions;
    std::vector<DocStruct> structs;
    std::vector<DocClass> classes;
    std::vector<DocInterface> interfaces;
    std::vector<DocEnum> enums;
};

struct DocProject {
    std::string title = "SantientPHP Documentation";
    std::string version = "0.1.0";
    std::string description;
    std::vector<DocModule> modules;
};

} // namespace sph
