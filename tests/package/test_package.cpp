#include "common/test_framework.hpp"
#include "compiler/manifest/Manifest.hpp"
#include "compiler/package/PackageManager.hpp"
#include <filesystem>
#include <fstream>
#include <string>

using namespace sph;

TEST_CASE("Package: createProject scaffolds valid project structure") {
    std::filesystem::path testDir = "test_scaffold_output";
    std::error_code ec;
    std::filesystem::remove_all(testDir, ec);

    std::string errorMsg;
    bool ok = PackageManager::createProject("my_awesome_app", testDir, errorMsg);
    CHECK(ok);

    std::filesystem::path projPath = testDir / "my_awesome_app";
    CHECK(std::filesystem::exists(projPath));
    CHECK(std::filesystem::exists(projPath / "sph.toml"));
    CHECK(std::filesystem::exists(projPath / "src" / "main.sph"));
    CHECK(std::filesystem::exists(projPath / ".gitignore"));
    CHECK(std::filesystem::exists(projPath / "README.md"));

    // Verify manifest contents
    auto manifest = Manifest::fromFile((projPath / "sph.toml").string());
    CHECK(manifest.has_value());
    if (manifest) {
        CHECK_EQ(manifest->projectName, "my_awesome_app");
        CHECK_EQ(manifest->entry, "src/main.sph");
    }

    std::filesystem::remove_all(testDir, ec);
}

TEST_CASE("Package: createProject rejects invalid names") {
    std::string errorMsg;
    bool ok = PackageManager::createProject("bad/name?with*symbols", ".", errorMsg);
    CHECK(!ok);
    CHECK(!errorMsg.empty());
}

TEST_CASE("Package: initProject creates sph.toml and rejects duplicates") {
    std::filesystem::path testDir = "test_init_dir";
    std::error_code ec;
    std::filesystem::remove_all(testDir, ec);
    std::filesystem::create_directories(testDir, ec);

    std::string errorMsg;
    CHECK(PackageManager::initProject(testDir, errorMsg));
    CHECK(std::filesystem::exists(testDir / "sph.toml"));

    // Second init should fail because sph.toml exists
    CHECK(!PackageManager::initProject(testDir, errorMsg));

    std::filesystem::remove_all(testDir, ec);
}

TEST_CASE("Package: addDependency and listDependencies manage dependencies") {
    std::filesystem::path testDir = "test_dep_dir";
    std::error_code ec;
    std::filesystem::remove_all(testDir, ec);
    std::filesystem::create_directories(testDir, ec);

    std::string errorMsg;
    CHECK(PackageManager::initProject(testDir, errorMsg));

    // Add path dependency
    CHECK(PackageManager::addDependency(testDir, "core_math", "../core_math", false, errorMsg));

    // Add git dependency
    CHECK(PackageManager::addDependency(testDir, "network_lib", "https://github.com/example/net.git", true, errorMsg));

    // List dependencies
    auto deps = PackageManager::listDependencies(testDir, errorMsg);
    CHECK_EQ(deps.size(), size_t(2));

    bool foundMath = false;
    bool foundNet = false;
    for (const auto& d : deps) {
        if (d.name == "core_math") {
            foundMath = true;
            CHECK_EQ(d.path, "../core_math");
        }
        if (d.name == "network_lib") {
            foundNet = true;
            CHECK_EQ(d.gitUrl, "https://github.com/example/net.git");
        }
    }
    CHECK(foundMath);
    CHECK(foundNet);

    std::filesystem::remove_all(testDir, ec);
}

