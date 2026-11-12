#pragma once

#include <concepts>
#include <functional>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

namespace sph::testing {

template <typename T>
concept Streamable = requires(std::ostream& os, const T& t) {
    os << t;
};

template <typename T>
std::string toStringRepr(const T& val) {
    if constexpr (Streamable<T>) {
        std::ostringstream ss;
        ss << val;
        return ss.str();
    } else {
        return "<unprintable>";
    }
}

struct TestCase {
    std::string name;
    std::function<void()> func;
};

class TestRegistry {
public:
    static TestRegistry& instance() {
        static TestRegistry reg;
        return reg;
    }

    void registerTest(std::string name, std::function<void()> func) {
        m_tests.push_back(TestCase{std::move(name), std::move(func)});
    }

    [[nodiscard]] const std::vector<TestCase>& getTests() const {
        return m_tests;
    }

private:
    std::vector<TestCase> m_tests;
};

struct TestContext {
    int assertionsPassed = 0;
    int assertionsFailed = 0;
    std::vector<std::string> failureMessages;

    static TestContext& current() {
        static TestContext ctx;
        return ctx;
    }

    void reset() {
        assertionsPassed = 0;
        assertionsFailed = 0;
        failureMessages.clear();
    }
};

struct AutoTestRegisterer {
    AutoTestRegisterer(std::string name, std::function<void()> func) {
        TestRegistry::instance().registerTest(std::move(name), std::move(func));
    }
};

inline int runAllTests() {
    auto& tests = TestRegistry::instance().getTests();
    int testsPassed = 0;
    int testsFailed = 0;

    std::cout << "========================================\n";
    std::cout << " Running " << tests.size() << " test case(s)...\n";
    std::cout << "========================================\n\n";

    for (const auto& test : tests) {
        auto& ctx = TestContext::current();
        ctx.reset();

        std::cout << "[ RUN      ] " << test.name << "\n";
        try {
            test.func();
        } catch (const std::exception& ex) {
            ctx.assertionsFailed++;
            ctx.failureMessages.push_back(std::string("Exception caught: ") + ex.what());
        } catch (...) {
            ctx.assertionsFailed++;
            ctx.failureMessages.push_back("Unknown exception caught");
        }

        if (ctx.assertionsFailed == 0) {
            std::cout << "[       OK ] " << test.name << " (" << ctx.assertionsPassed << " assertions passed)\n";
            testsPassed++;
        } else {
            std::cout << "[  FAILED  ] " << test.name << " (" << ctx.assertionsFailed << " assertion failure(s))\n";
            for (const auto& msg : ctx.failureMessages) {
                std::cout << "    " << msg << "\n";
            }
            testsFailed++;
        }
    }

    std::cout << "\n========================================\n";
    std::cout << " Test Summary: " << testsPassed << " passed, " << testsFailed << " failed.\n";
    std::cout << "========================================\n";

    return (testsFailed == 0) ? 0 : 1;
}

} // namespace sph::testing

#define SPH_CONCAT_INTERNAL(a, b) a##b
#define SPH_CONCAT(a, b) SPH_CONCAT_INTERNAL(a, b)

#define TEST_CASE(name)                                                         \
    static void SPH_CONCAT(sph_test_func_, __LINE__)();                         \
    static ::sph::testing::AutoTestRegisterer SPH_CONCAT(sph_test_reg_, __LINE__)( \
        name, SPH_CONCAT(sph_test_func_, __LINE__));                           \
    static void SPH_CONCAT(sph_test_func_, __LINE__)()

#define CHECK(expr)                                                             \
    do {                                                                        \
        if (expr) {                                                             \
            ::sph::testing::TestContext::current().assertionsPassed++;          \
        } else {                                                                \
            ::sph::testing::TestContext::current().assertionsFailed++;          \
            std::ostringstream ss;                                              \
            ss << __FILE__ << ":" << __LINE__ << ": CHECK(" #expr ") failed";   \
            ::sph::testing::TestContext::current().failureMessages.push_back(   \
                ss.str());                                                      \
        }                                                                       \
    } while (0)

#define CHECK_EQ(a, b)                                                          \
    do {                                                                        \
        auto valA = (a);                                                        \
        auto valB = (b);                                                        \
        if (valA == valB) {                                                     \
            ::sph::testing::TestContext::current().assertionsPassed++;          \
        } else {                                                                \
            ::sph::testing::TestContext::current().assertionsFailed++;          \
            std::ostringstream ss;                                              \
            ss << __FILE__ << ":" << __LINE__ << ": CHECK_EQ(" #a ", " #b       \
               << ") failed: [" << ::sph::testing::toStringRepr(valA) << "] != [" \
               << ::sph::testing::toStringRepr(valB) << "]";                    \
            ::sph::testing::TestContext::current().failureMessages.push_back(   \
                ss.str());                                                      \
        }                                                                       \
    } while (0)

#define CHECK_NE(a, b)                                                          \
    do {                                                                        \
        auto valA = (a);                                                        \
        auto valB = (b);                                                        \
        if (valA != valB) {                                                     \
            ::sph::testing::TestContext::current().assertionsPassed++;          \
        } else {                                                                \
            ::sph::testing::TestContext::current().assertionsFailed++;          \
            std::ostringstream ss;                                              \
            ss << __FILE__ << ":" << __LINE__ << ": CHECK_NE(" #a ", " #b       \
               << ") failed: both are [" << ::sph::testing::toStringRepr(valA) \
               << "]";                                                          \
            ::sph::testing::TestContext::current().failureMessages.push_back(   \
                ss.str());                                                      \
        }                                                                       \
    } while (0)
