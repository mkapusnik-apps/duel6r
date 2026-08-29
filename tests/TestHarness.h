#ifndef DUEL6_TESTS_TESTHARNESS_H
#define DUEL6_TESTS_TESTHARNESS_H

#include <array>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <iomanip>
#include <sstream>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

namespace Duel6::Test {
    template<typename T, typename = void>
    struct IsStreamWritable : std::false_type {};

    template<typename T>
    struct IsStreamWritable<T, std::void_t<decltype(std::declval<std::ostringstream &>()
                                                    << std::declval<const T &>())>> : std::true_type {};

    class Failure : public std::runtime_error {
    public:
        explicit Failure(const std::string &message)
                : std::runtime_error(message) {}
    };

    struct TestCase {
        const char *name;
        void (*function)();
    };

    inline std::vector<TestCase> &registry() {
        static std::vector<TestCase> testCases;
        return testCases;
    }

    class Registrar {
    public:
        Registrar(const char *name, void (*function)()) {
            registry().push_back({name, function});
        }
    };

    template<typename T, typename std::enable_if<!std::is_enum<T>::value, int>::type = 0>
    std::string toString(const T &value) {
        if constexpr (IsStreamWritable<T>::value) {
            std::ostringstream stream;
            stream << value;
            return stream.str();
        }
        return "<unprintable>";
    }

    template<typename T, typename std::enable_if<std::is_enum<T>::value, int>::type = 0>
    std::string toString(const T &value) {
        using Underlying = typename std::underlying_type<T>::type;
        if constexpr (std::is_signed<Underlying>::value)
            return std::to_string(static_cast<std::int64_t>(static_cast<Underlying>(value)));
        return std::to_string(static_cast<std::uint64_t>(static_cast<Underlying>(value)));
    }

    template<typename T, std::size_t Size>
    std::string toString(const std::array<T, Size> &value) {
        std::ostringstream stream;
        stream << '[';
        for (std::size_t index = 0; index < Size; ++index) {
            if (index) stream << ", ";
            if constexpr (std::is_same<T, std::uint8_t>::value || std::is_same<T, unsigned char>::value) {
                stream << "0x" << std::hex << std::setw(2) << std::setfill('0')
                       << static_cast<unsigned>(value[index]) << std::dec;
            } else {
                stream << toString(value[index]);
            }
        }
        stream << ']';
        return stream.str();
    }

    template<typename Clock, typename Duration>
    std::string toString(const std::chrono::time_point<Clock, Duration> &value) {
        return std::to_string(std::chrono::duration_cast<std::chrono::nanoseconds>(
                value.time_since_epoch()).count()) + "ns";
    }

    inline std::string toString(const std::string &value) {
        return '"' + value + '"';
    }

    inline std::string toString(const char *value) {
        return value == nullptr ? "<null>" : '"' + std::string(value) + '"';
    }

    [[noreturn]] inline void fail(const char *expression, const char *file, int line, const std::string &details = "") {
        std::ostringstream stream;
        stream << file << ':' << line << ": assertion failed: " << expression;
        if (!details.empty()) {
            stream << " (" << details << ')';
        }
        throw Failure(stream.str());
    }

    template<typename TExpected, typename TActual>
    void requireEqual(const TExpected &expected, const TActual &actual, const char *expectedExpr, const char *actualExpr,
                      const char *file, int line) {
        if (!(expected == actual)) {
            fail(actualExpr, file, line,
                 std::string("expected ") + expectedExpr + " == " + actualExpr + ", got " + toString(expected) +
                 " and " + toString(actual));
        }
    }

    template<typename TExpected, typename TActual, typename TEpsilon>
    void requireNear(const TExpected &expected, const TActual &actual, const TEpsilon &epsilon, const char *expectedExpr,
                     const char *actualExpr, const char *epsilonExpr, const char *file, int line) {
        auto diff = std::fabs(static_cast<double>(expected - actual));
        if (diff > static_cast<double>(epsilon)) {
            fail(actualExpr, file, line,
                 std::string("expected ") + expectedExpr + " ~= " + actualExpr + " within " + epsilonExpr +
                 ", diff is " + toString(diff));
        }
    }
}

#define D6R_TEST_CONCAT_IMPL(left, right) left##right
#define D6R_TEST_CONCAT(left, right) D6R_TEST_CONCAT_IMPL(left, right)

#define D6R_TEST_CASE(name) \
    static void D6R_TEST_CONCAT(testFunction_, __LINE__)(); \
    static ::Duel6::Test::Registrar D6R_TEST_CONCAT(testRegistrar_, __LINE__)(name, D6R_TEST_CONCAT(testFunction_, __LINE__)); \
    static void D6R_TEST_CONCAT(testFunction_, __LINE__)()

#define D6R_REQUIRE(condition) \
    do { \
        if (!(condition)) { \
            ::Duel6::Test::fail(#condition, __FILE__, __LINE__); \
        } \
    } while (false)

#define D6R_REQUIRE_EQ(expected, actual) \
    do { \
        ::Duel6::Test::requireEqual((expected), (actual), #expected, #actual, __FILE__, __LINE__); \
    } while (false)

#define D6R_REQUIRE_NEAR(expected, actual, epsilon) \
    do { \
        ::Duel6::Test::requireNear((expected), (actual), (epsilon), #expected, #actual, #epsilon, __FILE__, __LINE__); \
    } while (false)

#define D6R_REQUIRE_THROW(expression, exception_type) \
    do { \
        bool d6rCaughtExpectedException = false; \
        try { \
            expression; \
        } catch (const exception_type &) { \
            d6rCaughtExpectedException = true; \
        } catch (...) { \
            ::Duel6::Test::fail(#expression, __FILE__, __LINE__, "threw an unexpected exception type"); \
        } \
        if (!d6rCaughtExpectedException) { \
            ::Duel6::Test::fail(#expression, __FILE__, __LINE__, "did not throw " #exception_type); \
        } \
    } while (false)

#endif
