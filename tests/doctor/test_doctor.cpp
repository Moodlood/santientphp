#include "common/test_framework.hpp"
#include "compiler/doctor/Doctor.hpp"
#include <sstream>

TEST_CASE("Doctor: runChecks verifies environment and toolchain sanity") {
    sph::Doctor doctor;
    auto checks = doctor.runChecks();

    CHECK(!checks.empty());

    bool foundHost = false;
    bool foundCompiler = false;
    bool foundRuntime = false;
    bool foundStdlib = false;

    for (const auto& c : checks) {
        if (c.name.find("Operating System") != std::string::npos) {
            foundHost = true;
            CHECK(c.isHealthy);
        }
        if (c.name.find("Host C/C++ Compiler") != std::string::npos) {
            foundCompiler = true;
            CHECK(c.isHealthy);
            CHECK(!c.details.empty());
        }
        if (c.name.find("Core Runtime") != std::string::npos) {
            foundRuntime = true;
            CHECK(c.isHealthy);
        }
        if (c.name.find("Standard Library") != std::string::npos) {
            foundStdlib = true;
            CHECK(c.isHealthy);
        }
    }

    CHECK(foundHost);
    CHECK(foundCompiler);
    CHECK(foundRuntime);
    CHECK(foundStdlib);
}

TEST_CASE("Doctor: report generates clean health report") {
    sph::Doctor doctor;
    std::ostringstream out;

    int code = doctor.report(out);
    std::string text = out.str();

    CHECK_EQ(code, 0);
    CHECK(text.find("SantientPHP Doctor") != std::string::npos);
    CHECK(text.find("[OK]") != std::string::npos);
    CHECK(text.find("Status: All required toolchains and system dependencies are healthy.") != std::string::npos);
}

TEST_CASE("Doctor: reports optional LLVM check and runtime library detection") {
    sph::Doctor doctor;
    auto checks = doctor.runChecks();

    bool foundLlc = false;
    bool runtimeSatisfied = false;

    for (const auto& c : checks) {
        if (c.name.find("LLVM Tools") != std::string::npos) {
            foundLlc = true;
            CHECK(c.isOptional);
        }
        if (c.name.find("Core Runtime") != std::string::npos) {
            runtimeSatisfied = c.isHealthy;
            CHECK(c.details.find("libsph_runtime.a") != std::string::npos ||
                  c.details.find("Modular runtime sources present") != std::string::npos ||
                  c.details.find("runtime/core/runtime.c") != std::string::npos);
        }
    }

    CHECK(foundLlc);
    CHECK(runtimeSatisfied);
}


