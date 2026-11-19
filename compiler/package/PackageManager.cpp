#include "compiler/package/PackageManager.hpp"
#include <cctype>
#include <fstream>
#include <sstream>

namespace sph {

namespace {

bool isValidProjectName(std::string_view name) {
    if (name.empty()) return false;
    for (char c : name) {
        if (!std::isalnum(static_cast<unsigned char>(c)) && c != '_' && c != '-') {
            return false;
        }
    }
    return true;
}

} // namespace

bool PackageManager::createProject(const std::string& projectName,
                                  const std::filesystem::path& parentDir,
                                  ProjectTemplate tmpl,
                                  std::string& errorMsg) {
    if (!isValidProjectName(projectName)) {
        errorMsg = "Invalid project name '" + projectName + "'. Use alphanumeric characters, dashes, and underscores.";
        return false;
    }

    std::filesystem::path projectDir = parentDir / projectName;
    if (std::filesystem::exists(projectDir)) {
        errorMsg = "Directory already exists: " + projectDir.string();
        return false;
    }

    std::error_code ec;
    std::filesystem::path srcDir = projectDir / "src";
    if (!std::filesystem::create_directories(srcDir, ec)) {
        errorMsg = "Failed to create directory structure: " + ec.message();
        return false;
    }

    std::string entryFile = (tmpl == ProjectTemplate::Lib) ? "src/lib.sph" : "src/main.sph";
    std::string subsystem = (tmpl == ProjectTemplate::GUI) ? "windows" : "console";

    // 1. sph.toml
    {
        std::ofstream toml(projectDir / "sph.toml");
        if (!toml.is_open()) {
            errorMsg = "Failed to create sph.toml";
            return false;
        }
        toml << "[project]\n"
             << "name = \"" << projectName << "\"\n"
             << "version = \"0.1.0\"\n"
             << "entry = \"" << entryFile << "\"\n\n"
             << "[compiler]\n"
             << "language_version = \"0.1\"\n\n"
             << "[target]\n"
             << "triple = \"x86_64-windows\"\n\n"
             << "[build]\n"
             << "mode = \"debug\"\n"
             << "optimization = \"0\"\n"
             << "subsystem = \"" << subsystem << "\"\n\n"
             << "[dependencies]\n";
    }

    // 2. Entry source file
    {
        std::filesystem::path sourcePath = projectDir / entryFile;
        std::ofstream file(sourcePath);
        if (!file.is_open()) {
            errorMsg = "Failed to create " + entryFile;
            return false;
        }

        switch (tmpl) {
            case ProjectTemplate::CLI:
                file << "<?php\n\n"
                     << "fn main(): i32 {\n"
                     << "    println(\"Hello from " << projectName << "!\");\n"
                     << "    return 0;\n"
                     << "}\n";
                break;
            case ProjectTemplate::GUI:
                file << "<?php\n"
                     << "import \"std/gui.sph\";\n\n"
                     << "fn main(): i32 {\n"
                     << "    let win = new Window(\"" << projectName << "\", 800, 600);\n"
                     << "    win.show();\n"
                     << "    return 0;\n"
                     << "}\n";
                break;
            case ProjectTemplate::Web:
                file << "<?php\n"
                     << "import \"std/net.sph\";\n"
                     << "import \"std/json.sph\";\n\n"
                     << "fn main(): i32 {\n"
                     << "    println(\"SPH web microservice listening on port 8080...\");\n"
                     << "    return 0;\n"
                     << "}\n";
                break;
            case ProjectTemplate::Lib:
                file << "<?php\n\n"
                     << "fn add(a: i32, b: i32): i32 {\n"
                     << "    return a + b;\n"
                     << "}\n";
                break;
        }
    }

    // 3. .gitignore
    {
        std::ofstream gitignore(projectDir / ".gitignore");
        if (gitignore.is_open()) {
            gitignore << "build/\nbin/\n*.exe\n*.o\n*.a\n*.s\n*.ll\n";
        }
    }

    // 4. README.md
    {
        std::ofstream readme(projectDir / "README.md");
        if (readme.is_open()) {
            readme << "# " << projectName << "\n\n"
                   << "A SantientPHP native project.\n\n"
                   << "## Build & Run\n\n"
                   << "```powershell\n"
                   << "sphc build\n"
                   << "sphc run\n"
                   << "```\n";
        }
    }

    return true;
}

bool PackageManager::initProject(const std::filesystem::path& targetDir,
                                 std::string& errorMsg) {
    std::filesystem::path manifestPath = targetDir / "sph.toml";
    if (std::filesystem::exists(manifestPath)) {
        errorMsg = "sph.toml already exists in " + targetDir.string();
        return false;
    }

    std::string projName = targetDir.filename().string();
    if (projName.empty() || projName == ".") {
        projName = "app";
    }

    std::ofstream toml(manifestPath);
    if (!toml.is_open()) {
        errorMsg = "Failed to create sph.toml in " + targetDir.string();
        return false;
    }

    toml << "[project]\n"
         << "name = \"" << projName << "\"\n"
         << "version = \"0.1.0\"\n"
         << "entry = \"main.sph\"\n\n"
         << "[compiler]\n"
         << "language_version = \"0.1\"\n\n"
         << "[target]\n"
         << "triple = \"x86_64-windows\"\n\n"
         << "[build]\n"
         << "mode = \"debug\"\n"
         << "optimization = \"0\"\n\n"
         << "[dependencies]\n";

    return true;
}

bool PackageManager::addDependency(const std::filesystem::path& pathOrDir,
                                  const std::string& depName,
                                  const std::string& location,
                                  bool isGit,
                                  std::string& errorMsg) {
    std::filesystem::path manifestPath = pathOrDir;
    if (std::filesystem::is_directory(pathOrDir)) {
        manifestPath = pathOrDir / "sph.toml";
    }

    if (!std::filesystem::exists(manifestPath)) {
        errorMsg = "Could not find sph.toml at " + manifestPath.string();
        return false;
    }

    std::ifstream in(manifestPath);
    if (!in.is_open()) {
        errorMsg = "Could not open " + manifestPath.string();
        return false;
    }

    std::string content((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    in.close();

    std::string depEntry;
    if (isGit) {
        depEntry = depName + " = { git = \"" + location + "\" }";
    } else {
        depEntry = depName + " = { path = \"" + location + "\" }";
    }

    // Check if [dependencies] exists
    size_t depSectionPos = content.find("[dependencies]");
    if (depSectionPos != std::string::npos) {
        size_t lineEnd = content.find('\n', depSectionPos);
        if (lineEnd == std::string::npos) {
            content += "\n" + depEntry + "\n";
        } else {
            content.insert(lineEnd + 1, depEntry + "\n");
        }
    } else {
        content += "\n[dependencies]\n" + depEntry + "\n";
    }

    std::ofstream out(manifestPath, std::ios::trunc);
    if (!out.is_open()) {
        errorMsg = "Could not write to " + manifestPath.string();
        return false;
    }

    out << content;
    return true;
}

std::vector<Dependency> PackageManager::listDependencies(const std::filesystem::path& path,
                                                        std::string& errorMsg) {
    std::filesystem::path manifestPath = path;
    if (std::filesystem::is_directory(path)) {
        manifestPath = path / "sph.toml";
    }

    auto manifest = Manifest::fromFile(manifestPath.string());
    if (!manifest) {
        errorMsg = "Could not read manifest at " + manifestPath.string();
        return {};
    }

    return manifest->dependencies;
}

bool PackageManager::generateLockfile(const std::filesystem::path& projectDir,
                                     std::string& errorMsg) {
    std::filesystem::path manifestPath = projectDir / "sph.toml";
    auto manifest = Manifest::fromFile(manifestPath.string());
    if (!manifest) {
        errorMsg = "Cannot generate lockfile: missing or invalid " + manifestPath.string();
        return false;
    }

    Lockfile lockfile;
    lockfile.version = 1;

    for (const auto& dep : manifest->dependencies) {
        LockedPackage pkg;
        pkg.name = dep.name;
        pkg.version = dep.version.empty() ? "0.1.0" : dep.version;

        if (!dep.path.empty()) {
            pkg.source = "path:" + dep.path;
            std::filesystem::path depDir = projectDir / dep.path;
            if (std::filesystem::exists(depDir)) {
                pkg.checksum = Lockfile::computeDirSha256(depDir);
            } else {
                pkg.checksum = Lockfile::computeSha256(dep.name + "@" + dep.path);
            }
        } else if (!dep.gitUrl.empty()) {
            pkg.source = "git:" + dep.gitUrl;
            pkg.checksum = Lockfile::computeSha256(dep.name + "@" + dep.gitUrl);
        } else {
            pkg.source = "registry:" + dep.name;
            pkg.checksum = Lockfile::computeSha256(dep.name + "@" + pkg.version);
        }

        lockfile.addOrUpdatePackage(std::move(pkg));
    }

    std::filesystem::path lockPath = projectDir / "sph.lock";
    if (!lockfile.toFile(lockPath)) {
        errorMsg = "Failed to write lockfile to " + lockPath.string();
        return false;
    }

    return true;
}

bool PackageManager::verifyLockfile(const std::filesystem::path& projectDir,
                                   std::string& errorMsg) {
    std::filesystem::path lockPath = projectDir / "sph.lock";
    auto lockfile = Lockfile::fromFile(lockPath);
    if (!lockfile) {
        errorMsg = "Lockfile not found at " + lockPath.string();
        return false;
    }

    for (const auto& pkg : lockfile->packages) {
        if (pkg.source.rfind("path:", 0) == 0) {
            std::string relPath = pkg.source.substr(5);
            std::filesystem::path depDir = projectDir / relPath;
            if (!std::filesystem::exists(depDir)) {
                errorMsg = "Package '" + pkg.name + "' not found on disk at " + depDir.string();
                return false;
            }
            std::string currentChecksum = Lockfile::computeDirSha256(depDir);
            if (!pkg.checksum.empty() && currentChecksum != pkg.checksum) {
                errorMsg = "Integrity check failed for package '" + pkg.name + "': checksum mismatch";
                return false;
            }
        }
    }

    return true;
}

} // namespace sph
