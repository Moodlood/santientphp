#include "Linker.hpp"
#include <chrono>
#include <cstdlib>
#include <sstream>
#include <thread>
#include <vector>

namespace sph {

std::optional<std::string> Linker::findCrossToolchain(const Target& target) const {
    if (target.isHost()) {
        return "gcc";
    }

    std::vector<std::string> candidates = {
        target.getDefaultLinker(),
        target.getCrossCompilerPrefix() + "gcc",
        "clang"
    };

    for (const auto& cand : candidates) {
        if (cand.empty()) continue;
#if defined(_WIN32)
        std::string checkCmd = "where.exe " + cand + " >nul 2>&1";
#else
        std::string checkCmd = "which " + cand + " >/dev/null 2>&1";
#endif
        if (std::system(checkCmd.c_str()) == 0) {
            return cand;
        }
    }

    return std::nullopt;
}

bool Linker::linkExecutable(const std::filesystem::path& asmPath,
                           const std::filesystem::path& outputPath,
                           const std::filesystem::path& runtimeLibPath,
                           std::string& errorMsg,
                           const std::string& optLevel,
                           bool debugInfo,
                           const Target& target) const {
    std::string tool;
    if (target.isHost()) {
        tool = "gcc";
    } else {
        auto crossToolOpt = findCrossToolchain(target);
        if (!crossToolOpt) {
            errorMsg = "cross-compilation to target '" + target.getCanonicalTriple() +
                       "' requires a cross-compiler (such as " + target.getDefaultLinker() +
                       " or clang) which was not found in PATH. Target assembly was generated at: " +
                       asmPath.string();
            return false;
        }
        tool = *crossToolOpt;
    }

    std::ostringstream cmd;
    cmd << tool << " ";
    if (!target.isHost() && tool == "clang") {
        cmd << "--target=" << target.getLLVMTriple() << " ";
    }
    if (debugInfo) {
        cmd << "-g ";
    }
    if (!optLevel.empty()) {
        if (optLevel.front() == 'O') {
            cmd << "-" << optLevel << " ";
        } else {
            cmd << "-O" << optLevel << " ";
        }
        if (!debugInfo && (optLevel == "s" || optLevel == "Os")) {
            cmd << "-s ";
        }
    }
    cmd << "\"" << asmPath.string() << "\" ";

    if (std::filesystem::exists(runtimeLibPath)) {
        cmd << "\"" << runtimeLibPath.string() << "\" ";
    }
#if defined(_WIN32)
    cmd << "-lws2_32 ";
#else
    cmd << "-lpthread -ldl -lm ";
#endif

    cmd << "-o \"" << outputPath.string() << "\"";

    std::error_code ec;
    for (int r = 0; r < 5; ++r) {
        std::filesystem::remove(outputPath, ec);
        if (!std::filesystem::exists(outputPath)) {
            break;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(40));
    }

    int result = std::system(cmd.str().c_str());
    if (result != 0) {
        // Retry once after a brief delay in case antivirus or indexing locked the executable
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        std::filesystem::remove(outputPath, ec);
        result = std::system(cmd.str().c_str());
    }

    if (result != 0) {
        errorMsg = "linker command failed with exit code " + std::to_string(result) + ": " + cmd.str();
        return false;
    }

    if (!std::filesystem::exists(outputPath)) {
        errorMsg = "linker reported success but output executable not found: " + outputPath.string();
        return false;
    }

    return true;
}

} // namespace sph

