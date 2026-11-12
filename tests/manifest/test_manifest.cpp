#include "common/test_framework.hpp"
#include "compiler/manifest/Manifest.hpp"

TEST_CASE("Manifest parses standard sph.toml configuration") {
    std::string toml =
        "[project]\n"
        "name = \"my_app\"\n"
        "version = \"0.2.1\"\n"
        "entry = \"src/app.sph\"\n\n"
        "[target]\n"
        "triple = \"x86_64-windows\"\n\n"
        "[build]\n"
        "mode = \"release\"\n"
        "optimization = \"3\"\n\n"
        "[compatibility]\n"
        "php = true\n"
        "mode = \"dynamic\"\n";

    auto manifestOpt = sph::Manifest::fromString(toml);
    CHECK(manifestOpt.has_value());

    const auto& m = *manifestOpt;
    CHECK_EQ(m.projectName, "my_app");
    CHECK_EQ(m.version, "0.2.1");
    CHECK_EQ(m.entry, "src/app.sph");
    CHECK_EQ(m.targetTriple, "x86_64-windows");
    CHECK_EQ(m.buildMode, "release");
    CHECK_EQ(m.optimization, "3");
    CHECK(m.compatPhp);
    CHECK(m.compatMode == sph::CompatibilityMode::Dynamic);
    CHECK(m.isDynamicMode());
}

TEST_CASE("Manifest defaults to strict mode when not specified") {
    std::string toml =
        "[project]\n"
        "name = \"strict_app\"\n";

    auto manifestOpt = sph::Manifest::fromString(toml);
    CHECK(manifestOpt.has_value());

    const auto& m = *manifestOpt;
    CHECK_EQ(m.projectName, "strict_app");
    CHECK(!m.compatPhp);
    CHECK(m.compatMode == sph::CompatibilityMode::Strict);
    CHECK(!m.isDynamicMode());
}

