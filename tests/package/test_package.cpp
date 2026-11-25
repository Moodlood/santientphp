#include "common/test_framework.hpp"
#include "compiler/manifest/Manifest.hpp"
#include "compiler/package/PackageManager.hpp"
#include <chrono>
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

    for (const auto& entry : std::filesystem::recursive_directory_iterator(testDir, ec)) {
        std::filesystem::permissions(entry.path(), std::filesystem::perms::owner_write, std::filesystem::perm_options::add, ec);
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

    for (const auto& entry : std::filesystem::recursive_directory_iterator(testDir, ec)) {
        std::filesystem::permissions(entry.path(), std::filesystem::perms::owner_write, std::filesystem::perm_options::add, ec);
    }
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

    for (const auto& entry : std::filesystem::recursive_directory_iterator(testDir, ec)) {
        std::filesystem::permissions(entry.path(), std::filesystem::perms::owner_write, std::filesystem::perm_options::add, ec);
    }
    std::filesystem::remove_all(testDir, ec);
}

TEST_CASE("Package: SemVer parsing and comparison") {
    auto v1 = SemVer::parse("1.2.3");
    CHECK(v1.has_value());
    CHECK_EQ(v1->major, 1u);
    CHECK_EQ(v1->minor, 2u);
    CHECK_EQ(v1->patch, 3u);
    CHECK_EQ(v1->toString(), "1.2.3");

    auto v2 = SemVer::parse("v2.0.0-alpha.1+build.42");
    CHECK(v2.has_value());
    CHECK_EQ(v2->major, 2u);
    CHECK_EQ(v2->minor, 0u);
    CHECK_EQ(v2->patch, 0u);
    CHECK_EQ(v2->prerelease, "alpha.1");
    CHECK_EQ(v2->build, "build.42");

    auto v3 = SemVer::parse("1.2.4");
    CHECK(*v1 < *v3);
    CHECK(!(*v3 < *v1));
    CHECK(*v1 <= *v3);
    CHECK(*v3 > *v1);

    auto vPre = SemVer::parse("1.0.0-beta");
    auto vRelease = SemVer::parse("1.0.0");
    CHECK(*vPre < *vRelease);
}

TEST_CASE("Package: VersionReq semantic matching") {
    // Caret matching
    auto reqCaret = VersionReq::parse("^1.2.3");
    CHECK(reqCaret.has_value());
    CHECK(reqCaret->matches(*SemVer::parse("1.2.3")));
    CHECK(reqCaret->matches(*SemVer::parse("1.2.9")));
    CHECK(reqCaret->matches(*SemVer::parse("1.9.0")));
    CHECK(!reqCaret->matches(*SemVer::parse("1.2.2")));
    CHECK(!reqCaret->matches(*SemVer::parse("2.0.0")));

    // Tilde matching
    auto reqTilde = VersionReq::parse("~1.2.3");
    CHECK(reqTilde.has_value());
    CHECK(reqTilde->matches(*SemVer::parse("1.2.3")));
    CHECK(reqTilde->matches(*SemVer::parse("1.2.8")));
    CHECK(!reqTilde->matches(*SemVer::parse("1.3.0")));

    // Range matching
    auto reqRange = VersionReq::parse(">=1.0.0, <2.0.0");
    CHECK(reqRange.has_value());
    CHECK(reqRange->matches(*SemVer::parse("1.0.0")));
    CHECK(reqRange->matches(*SemVer::parse("1.5.0")));
    CHECK(!reqRange->matches(*SemVer::parse("0.9.9")));
    CHECK(!reqRange->matches(*SemVer::parse("2.0.0")));
}

TEST_CASE("Package: Lockfile serialization and integrity verification") {
    std::filesystem::path testDir = "test_lockfile_dir";
    std::error_code ec;
    std::filesystem::remove_all(testDir, ec);
    std::filesystem::create_directories(testDir, ec);

    std::string errorMsg;
    CHECK(PackageManager::initProject(testDir, errorMsg));

    // Create a mock dependency directory
    std::filesystem::path depDir = testDir / "local_dep";
    std::filesystem::create_directories(depDir, ec);
    {
        std::ofstream f(depDir / "module.sph");
        f << "<?php\nfn hello_dep(): void {}\n";
    }

    CHECK(PackageManager::addDependency(testDir, "local_dep", "local_dep", false, errorMsg));

    // Generate lockfile
    CHECK(PackageManager::generateLockfile(testDir, errorMsg));
    CHECK(std::filesystem::exists(testDir / "sph.lock"));

    // Parse and verify lockfile contents
    auto lf = Lockfile::fromFile(testDir / "sph.lock");
    CHECK(lf.has_value());
    if (lf) {
        CHECK_EQ(lf->version, 1u);
        const auto* pkg = lf->findPackage("local_dep");
        CHECK(pkg != nullptr);
        if (pkg) {
            CHECK_EQ(pkg->name, "local_dep");
            CHECK(!pkg->checksum.empty());
        }
    }

    // Verify lockfile integrity
    CHECK(PackageManager::verifyLockfile(testDir, errorMsg));

    for (const auto& entry : std::filesystem::recursive_directory_iterator(testDir, ec)) {
        std::filesystem::permissions(entry.path(), std::filesystem::perms::owner_write, std::filesystem::perm_options::add, ec);
    }
    std::filesystem::remove_all(testDir, ec);
}

TEST_CASE("Package: createProject supports GUI, Web, and Lib templates") {
    std::filesystem::path testDir = "test_templates_dir";
    std::error_code ec;
    std::filesystem::remove_all(testDir, ec);

    std::string errorMsg;

    // GUI Template
    CHECK(PackageManager::createProject("gui_app", testDir, ProjectTemplate::GUI, errorMsg));
    auto guiManifest = Manifest::fromFile((testDir / "gui_app" / "sph.toml").string());
    CHECK(guiManifest.has_value());
    CHECK(guiManifest->isGuiSubsystem());
    CHECK(std::filesystem::exists(testDir / "gui_app" / "src" / "main.sph"));

    // Web Template
    CHECK(PackageManager::createProject("web_srv", testDir, ProjectTemplate::Web, errorMsg));
    auto webManifest = Manifest::fromFile((testDir / "web_srv" / "sph.toml").string());
    CHECK(webManifest.has_value());
    CHECK(std::filesystem::exists(testDir / "web_srv" / "src" / "main.sph"));

    // Lib Template
    CHECK(PackageManager::createProject("math_lib", testDir, ProjectTemplate::Lib, errorMsg));
    auto libManifest = Manifest::fromFile((testDir / "math_lib" / "sph.toml").string());
    CHECK(libManifest.has_value());
    CHECK_EQ(libManifest->entry, "src/lib.sph");
    CHECK(std::filesystem::exists(testDir / "math_lib" / "src" / "lib.sph"));

    for (const auto& entry : std::filesystem::recursive_directory_iterator(testDir, ec)) {
        std::filesystem::permissions(entry.path(), std::filesystem::perms::owner_write, std::filesystem::perm_options::add, ec);
    }
    std::filesystem::remove_all(testDir, ec);
}

TEST_CASE("Package: Lockfile pins and restores exact git commit SHA") {
    auto nonce = std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());
    std::filesystem::path testDir = std::filesystem::temp_directory_path() / ("sph_git_pin_" + nonce);
    std::error_code ec;
    std::filesystem::remove_all(testDir, ec);
    std::filesystem::create_directories(testDir, ec);

    std::filesystem::path repoDir = testDir / "upstream_pkg";
    std::filesystem::path appDir = testDir / "consumer_app";
    std::filesystem::create_directories(repoDir, ec);
    std::filesystem::create_directories(appDir, ec);

    // Initialize local git repository with two commits to verify pinning to exact SHA
    std::string initCmd =
        "git -C \"" + repoDir.string() + "\" init -q && "
        "git -C \"" + repoDir.string() + "\" config user.email test@example.com && "
        "git -C \"" + repoDir.string() + "\" config user.name Tester";
    CHECK_EQ(std::system(initCmd.c_str()), 0);
    {
        std::ofstream f(repoDir / "lib.sph");
        f << "<?php\nfn v1(): i32 { return 1; }\n";
    }
    std::string c1Cmd =
        "git -C \"" + repoDir.string() + "\" add lib.sph && "
        "git -C \"" + repoDir.string() + "\" commit -q -m \"v1\"";
    CHECK_EQ(std::system(c1Cmd.c_str()), 0);

    std::string sha1 = PackageManager::resolveGitHeadCommit(repoDir.string());
    CHECK(!sha1.empty());
    CHECK(sha1.size() >= 40);

    std::string err;
    CHECK(PackageManager::initProject(appDir, err));
    CHECK(PackageManager::addDependency(appDir, "upstream_pkg", repoDir.string(), true, err));
    CHECK(PackageManager::generateLockfile(appDir, err));

    auto lf = Lockfile::fromFile(appDir / "sph.lock");
    CHECK(lf.has_value());
    if (lf) {
        const auto* pkg = lf->findPackage("upstream_pkg");
        CHECK(pkg != nullptr);
        if (pkg) {
            CHECK_EQ(pkg->commit, sha1);
        }
    }

    // Advance upstream repo with a second commit; restoreDependencies must pin back to sha1!
    {
        std::ofstream f(repoDir / "lib.sph");
        f << "<?php\nfn v2(): i32 { return 2; }\n";
    }
    std::string c2Cmd =
        "git -C \"" + repoDir.string() + "\" add lib.sph && "
        "git -C \"" + repoDir.string() + "\" commit -q -m \"v2\"";
    CHECK_EQ(std::system(c2Cmd.c_str()), 0);
    std::string sha2 = PackageManager::resolveGitHeadCommit(repoDir.string());
    CHECK(sha2 != sha1);

    CHECK(PackageManager::restoreDependencies(appDir, err));
    std::string restoredHead = PackageManager::resolveGitHeadCommit((appDir / ".sph_deps" / "upstream_pkg").string());
    CHECK_EQ(restoredHead, sha1);

    for (const auto& entry : std::filesystem::recursive_directory_iterator(testDir, ec)) {
        std::filesystem::permissions(entry.path(), std::filesystem::perms::owner_write, std::filesystem::perm_options::add, ec);
    }
    std::filesystem::remove_all(testDir, ec);
}

