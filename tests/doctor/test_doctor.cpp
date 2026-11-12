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

    for (const auto& c : checks) {
        if (c.name.find("Operating System") != std::string::npos) {
            foundHost = true;
            CHECK(c.isHealthy);
        }
        if (c.name.find("Compiler") != std::string::npos) {
            foundCompiler = true;
            CHECK(c.isHealthy);
            CHECK(!c.details.empty());
        }
        if (c.name.find("Core Runtime") != std::string::npos) {
            foundRuntime = true;
            CHECK(c.isHealthy);
        }
    }

    CHECK(foundHost);
    CHECK(foundCompiler);
    CHECK(foundRuntime);
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

