#pragma once

#include <atomic>
#include <concepts>
#include <functional>
#include <iostream>
#include <mutex>
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
    std::atomic<int> assertionsPassed{0};
    std::atomic<int> assertionsFailed{0};
    std::mutex mtx;
    std::vector<std::string> failureMessages;

    static TestContext& current() {
        static TestContext ctx;
        return ctx;
    }

    void reset() {
        assertionsPassed.store(0, std::memory_order_relaxed);
        assertionsFailed.store(0, std::memory_order_relaxed);
        std::lock_guard<std::mutex> lock(mtx);
        failureMessages.clear();
    }

    void recordPass() {
        assertionsPassed.fetch_add(1, std::memory_order_relaxed);
    }

    void recordFailure(std::string msg) {
        assertionsFailed.fetch_add(1, std::memory_order_relaxed);
        std::lock_guard<std::mutex> lock(mtx);
        failureMessages.push_back(std::move(msg));
    }
};

struct AutoTestRegisterer {
    AutoTestRegisterer(std::string name, std::function<void()> func) {
        TestRegistry::instance().registerTest(std::move(name), std::move(func));
    }
};

inline int runAllTests(const std::string& filter = "") {
    auto& tests = TestRegistry::instance().getTests();
    int testsPassed = 0;
    int testsFailed = 0;
    std::vector<std::pair<std::string, std::vector<std::string>>> failedTests;

    size_t activeCount = 0;
    for (const auto& test : tests) {
        if (filter.empty() || test.name.find(filter) != std::string::npos) {
            activeCount++;
        }
    }

    std::cout << "========================================\n";
    std::cout << " Running " << activeCount << " test case(s)...\n";
    std::cout << "========================================\n\n";

    for (const auto& test : tests) {
        if (!filter.empty() && test.name.find(filter) == std::string::npos) {
            continue;
        }
        auto& ctx = TestContext::current();
        ctx.reset();

        std::cout << "[ RUN      ] " << test.name << std::endl;
        try {
            test.func();
        } catch (const std::exception& ex) {
            ctx.recordFailure(std::string("Exception caught: ") + ex.what());
        } catch (...) {
            ctx.recordFailure("Unknown exception caught");
        }

        const int failedCount = ctx.assertionsFailed.load(std::memory_order_relaxed);
        const int passedCount = ctx.assertionsPassed.load(std::memory_order_relaxed);
        if (failedCount == 0) {
            std::cout << "[       OK ] " << test.name << " (" << passedCount << " assertions passed)" << std::endl;
            testsPassed++;
        } else {
            std::cout << "[  FAILED  ] " << test.name << " (" << failedCount << " assertion failure(s))" << std::endl;
            std::lock_guard<std::mutex> lock(ctx.mtx);
            for (const auto& msg : ctx.failureMessages) {
                std::cout << "    " << msg << "\n";
                std::cout << "::error title=" << test.name << "::" << msg << "\n";
            }
            failedTests.push_back({test.name, ctx.failureMessages});
            testsFailed++;
        }
    }

    if (!failedTests.empty()) {
        std::cout << "\n========================================\n";
        std::cout << " SUMMARY OF FAILED TESTS (" << failedTests.size() << "):\n";
        for (const auto& [name, msgs] : failedTests) {
            std::cout << " [FAIL] " << name << "\n";
            for (const auto& m : msgs) {
                std::cout << "        " << m << "\n";
            }
        }
        std::cout << "========================================\n";
    }

    std::cout << "\n========================================\n";
    std::cout << " Test Summary: " << testsPassed << " passed, " << testsFailed << " failed.\n";
    std::cout << "========================================\n";

    return (testsFailed == 0) ? 0 : 1;
}

} // namespace sph::testing

#if defined(_WIN32)
#define SPH_RUN_PREFIX ".\\"
#else
#define SPH_RUN_PREFIX "./"
#endif

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
            ::sph::testing::TestContext::current().recordPass();                \
        } else {                                                                \
            std::ostringstream ss;                                              \
            ss << __FILE__ << ":" << __LINE__ << ": CHECK(" #expr ") failed";   \
            ::sph::testing::TestContext::current().recordFailure(ss.str());     \
        }                                                                       \
    } while (0)

#define CHECK_EQ(a, b)                                                          \
    do {                                                                        \
        auto valA = (a);                                                        \
        auto valB = (b);                                                        \
        if (valA == valB) {                                                     \
            ::sph::testing::TestContext::current().recordPass();                \
        } else {                                                                \
            std::ostringstream ss;                                              \
            ss << __FILE__ << ":" << __LINE__ << ": CHECK_EQ(" #a ", " #b       \
               << ") failed: [" << ::sph::testing::toStringRepr(valA) << "] != [" \
               << ::sph::testing::toStringRepr(valB) << "]";                    \
            ::sph::testing::TestContext::current().recordFailure(ss.str());     \
        }                                                                       \
    } while (0)

#define CHECK_NE(a, b)                                                          \
    do {                                                                        \
        auto valA = (a);                                                        \
        auto valB = (b);                                                        \
        if (valA != valB) {                                                     \
            ::sph::testing::TestContext::current().recordPass();                \
        } else {                                                                \
            std::ostringstream ss;                                              \
            ss << __FILE__ << ":" << __LINE__ << ": CHECK_NE(" #a ", " #b       \
               << ") failed: both are [" << ::sph::testing::toStringRepr(valA) \
               << "]";                                                          \
            ::sph::testing::TestContext::current().recordFailure(ss.str());     \
        }                                                                       \
    } while (0)
