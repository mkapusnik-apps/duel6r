#include <cstdlib>
#include <exception>
#include <iostream>
#include <string>

#include "source/Exception.h"
#include "tests/TestHarness.h"

int main() {
    const auto &tests = Duel6::Test::registry();
    const char *filterValue = std::getenv("D6R_TEST_FILTER");
    const std::string filter = filterValue ? filterValue : "";
    std::size_t failed = 0;
    std::size_t executed = 0;

    for (const auto &test: tests) {
        if (!filter.empty() && std::string(test.name).find(filter) == std::string::npos) continue;
        ++executed;
        try {
            test.function();
            std::cout << "[PASS] " << test.name << '\n';
        } catch (const Duel6::Test::Failure &failure) {
            ++failed;
            std::cerr << "[FAIL] " << test.name << '\n' << "  " << failure.what() << '\n';
        } catch (const Duel6::Exception &exception) {
            ++failed;
            std::cerr << "[FAIL] " << test.name << '\n'
                      << "  Unexpected Duel6 exception at " << exception.getFile() << ':' << exception.getLine()
                      << " - " << exception.getMessage() << '\n';
        } catch (const std::exception &exception) {
            ++failed;
            std::cerr << "[FAIL] " << test.name << '\n' << "  Unexpected std::exception: " << exception.what()
                      << '\n';
        } catch (...) {
            ++failed;
            std::cerr << "[FAIL] " << test.name << '\n' << "  Unknown exception\n";
        }
    }

    std::cout << "Executed " << executed << " test(s), failures: " << failed << '\n';
    return failed == 0 ? 0 : 1;
}
