#include <chrono>
#include <filesystem>
#include <fstream>

#include "source/console/Console.h"
#include "tests/TestHarness.h"

namespace {
    struct ConfigurationFile {
        std::filesystem::path directory;
        ConfigurationFile() {
            const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
            for (unsigned attempt = 0; attempt < 100; ++attempt) {
                auto candidate = std::filesystem::temp_directory_path() /
                    ("duel6r config " + std::to_string(stamp) + "-" + std::to_string(attempt));
                if (std::filesystem::create_directory(candidate)) {
                    directory = candidate;
                    return;
                }
            }
            throw std::runtime_error("Unable to create isolated configuration fixture");
        }
        ~ConfigurationFile() {
            std::error_code error;
            std::filesystem::remove_all(directory, error);
        }
        std::string command(const std::string &verb) const {
            return verb + " \"" + (directory / "config.script").generic_string() + "\"";
        }
    };

    struct Settings {
        Duel6::Int32 saved = 3;
        bool enabled = false;
        Duel6::Int32 sessionOnly = 5;
        Duel6::Console console{Duel6::Console::NonFlag};
        Settings() {
            using Console = Duel6::Console;
            Console::registerBasicCommands(console);
            console.registerVariable("saved", Console::Variable::ArchiveFlag,
                                     std::make_unique<Console::IntVariable>(saved));
            console.registerVariable("enabled", Console::Variable::ArchiveFlag,
                                     std::make_unique<Console::BoolVariable>(enabled));
            console.registerVariable("session_only", Console::Variable::NoneFlag,
                                     std::make_unique<Console::IntVariable>(sessionOnly));
        }
    };
}

D6R_TEST_CASE("console archive and exec restore saved values in a fresh console without session-only state") {
    ConfigurationFile file;
    {
        Settings original;
        original.console.exec("saved 17\nenabled true\nsession_only 99");
        original.console.exec(file.command("archive"));
        D6R_REQUIRE(std::filesystem::is_regular_file(file.directory / "config.script"));
    }
    Settings restarted;
    restarted.console.exec(file.command("exec"));
    D6R_REQUIRE_EQ(17, restarted.saved);
    D6R_REQUIRE(restarted.enabled);
    D6R_REQUIRE_EQ(5, restarted.sessionOnly);

    // A load is repeatable and replaces intervening changes, not just defaults.
    restarted.console.exec("saved 91\nenabled false");
    restarted.console.exec(file.command("exec"));
    D6R_REQUIRE_EQ(17, restarted.saved);
    D6R_REQUIRE(restarted.enabled);
}

D6R_TEST_CASE("console config executes in order and later commands override loaded settings") {
    ConfigurationFile file;
    {
        std::ofstream output(file.directory / "config.script");
        output << "// existing script format, not INI\nsaved 8\nsaved 12\nenabled true\n";
        D6R_REQUIRE(output.good());
    }
    Settings settings;
    settings.console.exec(file.command("exec"));
    D6R_REQUIRE_EQ(12, settings.saved);
    D6R_REQUIRE(settings.enabled);
    settings.console.exec("saved 21");
    settings.console.exec("saved 34");
    D6R_REQUIRE_EQ(34, settings.saved);
}

D6R_TEST_CASE("missing console config preserves defaults and allows subsequent commands") {
    ConfigurationFile file;
    Settings settings;
    settings.console.exec(file.command("exec"));
    D6R_REQUIRE_EQ(3, settings.saved);
    D6R_REQUIRE(!settings.enabled);
    settings.console.exec("saved 9");
    D6R_REQUIRE_EQ(9, settings.saved);
}
