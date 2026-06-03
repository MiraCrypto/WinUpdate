#include "test_helper.hpp"
#include <iostream>

// ANSI Terminal Colors for test suite
constexpr const char* const TEST_COLOR_RESET = "\033[0m";
constexpr const char* const TEST_COLOR_GREEN = "\033[1;32m";
constexpr const char* const TEST_COLOR_RED = "\033[1;31m";
constexpr const char* const TEST_COLOR_CYAN = "\033[1;36m";
constexpr const char* const TEST_COLOR_BOLD = "\033[1m";

#if defined(_WIN32) && defined(UNICODE)
int wmain() noexcept {
#else
int main() noexcept {
#endif
  try {
    auto testCases = ::CatUpdate::TestRunner::Instance().GetTestCases();
    int passedCount = 0;
    int failedCount = 0;

    std::cout << TEST_COLOR_CYAN
              << "====================================================" << TEST_COLOR_RESET << '\n';
    std::cout << TEST_COLOR_BOLD << "              CatUpdate Unit Test Suite" << TEST_COLOR_RESET
              << '\n';
    std::cout << TEST_COLOR_CYAN
              << "====================================================" << TEST_COLOR_RESET << '\n'
              << '\n';

    for (const auto& testCase : testCases) {
      std::cout << "[ RUN      ] " << testCase.suiteName << "." << testCase.testName << '\n';

      try {
        testCase.testFunction();
        std::cout << TEST_COLOR_GREEN << "[       OK ] " << TEST_COLOR_RESET << testCase.suiteName
                  << "." << testCase.testName << '\n';
        passedCount++;
      } catch (const std::exception& ex) {
        std::cout << TEST_COLOR_RED << "[  FAILED  ] " << TEST_COLOR_RESET << testCase.suiteName
                  << "." << testCase.testName << " (Reason: " << ex.what() << ")" << '\n';
        failedCount++;
      } catch (...) {
        std::cout << TEST_COLOR_RED << "[  FAILED  ] " << TEST_COLOR_RESET << testCase.suiteName
                  << "." << testCase.testName << " (Reason: Unknown exception occurred)" << '\n';
        failedCount++;
      }
    }

    std::cout << '\n';
    std::cout << TEST_COLOR_CYAN
              << "====================================================" << TEST_COLOR_RESET << '\n';
    std::cout << TEST_COLOR_BOLD << "Test Summary:" << TEST_COLOR_RESET << '\n';
    std::cout << "  Total:  " << testCases.size() << '\n';
    std::cout << "  " << TEST_COLOR_GREEN << "Passed: " << TEST_COLOR_RESET << passedCount << '\n';
    std::cout << "  " << TEST_COLOR_RED << "Failed: " << TEST_COLOR_RESET << failedCount << '\n';
    std::cout << TEST_COLOR_CYAN
              << "====================================================" << TEST_COLOR_RESET << '\n';

    return (failedCount == 0) ? 0 : 1;
  } catch (const std::exception& ex) {
    std::cerr << "Unhandled exception: " << ex.what() << '\n';
    return 1;
  } catch (...) {
    std::cerr << "Unknown unhandled exception occurred\n";
    return 1;
  }
}
