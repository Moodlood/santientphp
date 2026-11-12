#pragma once

#include "IRFunction.hpp"
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace sph {

struct IRVTable {
    std::string name;
    std::vector<std::string> functionNames;
};

class IRModule {
public:
    explicit IRModule(std::string name) : m_name(std::move(name)) {}

    [[nodiscard]] const std::string& getName() const noexcept { return m_name; }

    void addFunction(std::unique_ptr<IRFunction> fn) {
        if (fn) {
            m_functions.push_back(std::move(fn));
        }
    }

    [[nodiscard]] const std::vector<std::unique_ptr<IRFunction>>& getFunctions() const noexcept {
        return m_functions;
    }
    [[nodiscard]] std::vector<std::unique_ptr<IRFunction>>& getFunctions() noexcept {
        return m_functions;
    }

    [[nodiscard]] const IRFunction* getFunction(const std::string& name) const noexcept;
    [[nodiscard]] IRFunction* getFunction(const std::string& name) noexcept;

    void addVTable(IRVTable vt) { m_vtables.push_back(std::move(vt)); }
    [[nodiscard]] const std::vector<IRVTable>& getVTables() const noexcept { return m_vtables; }

    void addSourceFile(uint32_t fileId, std::string path) {
        m_sourceFiles[fileId] = std::move(path);
    }
    [[nodiscard]] const std::unordered_map<uint32_t, std::string>& getSourceFiles() const noexcept {
        return m_sourceFiles;
    }
    [[nodiscard]] std::string getSourceFile(uint32_t fileId) const {
        auto it = m_sourceFiles.find(fileId);
        return (it != m_sourceFiles.end()) ? it->second : "";
    }

private:
    std::string m_name;
    std::vector<std::unique_ptr<IRFunction>> m_functions;
    std::vector<IRVTable> m_vtables;
    std::unordered_map<uint32_t, std::string> m_sourceFiles;
};

} // namespace sph

