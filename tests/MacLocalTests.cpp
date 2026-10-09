#include "TestHarness.h"
#include "source/platform/MacLocal.h"
#include <fstream>
#include <iterator>

namespace {
    namespace fs = std::filesystem;

    struct Paths {
        fs::path previous = fs::current_path();
        fs::path root = fs::temp_directory_path() /
                ("duel6r-mac-path-test-" + std::to_string(
                        std::chrono::steady_clock::now().time_since_epoch().count()));
        fs::path resources = root / "Duel 6 Reloaded.app/Contents/Resources";
        fs::path support = root / "Library/Application Support/Duel 6 Reloaded";
        Paths() {
            for (const char *name : {"data", "levels", "profiles", "shaders", "sound", "textures"})
                fs::create_directories(resources / name);
        }
        ~Paths() {
            fs::current_path(previous);
            fs::permissions(resources, fs::perms::owner_all, fs::perm_options::add);
            fs::remove_all(root);
        }
    };
}

D6R_TEST_CASE("macOS paths create only per-user saves and resolve resources independently of cwd") {
    Paths paths;
    const auto save = paths.support / "data/persons.json";
    fs::permissions(paths.resources, fs::perms::owner_read | fs::perms::owner_exec);
    Duel6::MacLocal::preparePaths(paths.resources, paths.support);
    // macOS may report /private/var for a requested /var directory alias.
    D6R_REQUIRE(fs::equivalent(paths.resources, fs::current_path()));
    D6R_REQUIRE_EQ(save.string(), Duel6::MacLocal::personDataPath());
    D6R_REQUIRE_EQ(fs::canonical(paths.resources).string(), Duel6::MacLocal::resourceDirectory());
    D6R_REQUIRE(fs::is_directory(save.parent_path()));
    D6R_REQUIRE(!fs::exists(save)); // Missing-file behavior remains with Menu.
    D6R_REQUIRE(!fs::exists(paths.resources / "data/persons.json"));
    const std::string saved = R"({"persons":[{"name":"Player One","elo":1200}],"playing":["Player One"],"rounds":7})";
    { std::ofstream output(save); output << saved; }
    // A relaunch/replacement selects the same user data and never initializes it
    // from the bundle. The unchanged Menu/JSON code owns its schema and timing.
    fs::current_path(paths.previous);
    { std::ofstream shipped(paths.resources / "data/persons.json"); shipped << "not user data"; }
    Duel6::MacLocal::preparePaths(paths.resources, paths.support);
    std::ifstream input(save);
    D6R_REQUIRE_EQ(saved, std::string(std::istreambuf_iterator<char>(input), {}));
}

#ifndef _WIN32
D6R_TEST_CASE("macOS resource directory aliases select the same directory and relative assets") {
    Paths paths;
    const auto alias = paths.root / "Resources alias";
    fs::create_directory_symlink(fs::canonical(paths.resources), alias);
    { std::ofstream asset(paths.resources / "data/alias-test.txt"); asset << "bundled asset"; }
    Duel6::MacLocal::preparePaths(alias, paths.support);
    D6R_REQUIRE(alias != fs::current_path()); // Reproduce the lexical-alias distinction.
    D6R_REQUIRE(fs::equivalent(alias, fs::current_path()));
    D6R_REQUIRE(fs::equivalent(paths.resources, fs::current_path()));
    D6R_REQUIRE(!fs::equivalent(paths.support, fs::current_path()));
    std::ifstream asset("data/alias-test.txt");
    D6R_REQUIRE_EQ(std::string("bundled asset"), std::string(std::istreambuf_iterator<char>(asset), {}));
    D6R_REQUIRE_EQ((paths.support / "data/persons.json").string(), Duel6::MacLocal::personDataPath());
}
#endif

D6R_TEST_CASE("macOS paths reject relative or incomplete bundles and unusable save directories") {
    Paths paths;
    D6R_REQUIRE_THROW(Duel6::MacLocal::preparePaths("resources", paths.support), std::runtime_error);
    D6R_REQUIRE_THROW(Duel6::MacLocal::preparePaths(paths.resources, "support"), std::runtime_error);
    fs::remove(paths.resources / "levels");
    D6R_REQUIRE_THROW(Duel6::MacLocal::preparePaths(paths.resources, paths.support), std::runtime_error);
    D6R_REQUIRE_EQ(paths.previous, fs::current_path());
    fs::create_directory(paths.resources / "levels");
    fs::create_directories(paths.support);
    { std::ofstream file(paths.support / "data"); file << "not a directory"; }
    D6R_REQUIRE_THROW(Duel6::MacLocal::preparePaths(paths.resources, paths.support), fs::filesystem_error);
    D6R_REQUIRE_EQ(paths.previous, fs::current_path());
}

D6R_TEST_CASE("macOS separate-instance support roots keep local person records isolated") {
    Paths paths;
    const auto other = paths.root / "Other user/Application Support/Duel 6 Reloaded";
    Duel6::MacLocal::preparePaths(paths.resources, paths.support);
    const auto first = Duel6::MacLocal::personDataPath();
    { std::ofstream file(first); file << "first instance local records"; }
    Duel6::MacLocal::preparePaths(paths.resources, other);
    D6R_REQUIRE(first != Duel6::MacLocal::personDataPath());
    D6R_REQUIRE(!fs::exists(Duel6::MacLocal::personDataPath()));
    { std::ofstream file(Duel6::MacLocal::personDataPath()); file << "second instance local records"; }
    Duel6::MacLocal::preparePaths(paths.resources, paths.support);
    std::ifstream file(first);
    D6R_REQUIRE_EQ(std::string("first instance local records"), std::string(std::istreambuf_iterator<char>(file), {}));
}
