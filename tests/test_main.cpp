#include "test_helper.hpp"
#include <iostream>

// ANSI Terminal Colors for test suite
#define TEST_COLOR_RESET   "\033[0m"
#define TEST_COLOR_GREEN   "\033[1;32m"
#define TEST_COLOR_RED     "\033[1;31m"
#define TEST_COLOR_CYAN    "\033[1;36m"
#define TEST_COLOR_BOLD    "\033[1m"

int main() {
    auto testCases = ::CatUpdate::TestRunner::Instance().GetTestCases();
    int passedCount = 0;
    int failedCount = 0;

    std::cout << TEST_COLOR_CYAN << "====================================================" << TEST_COLOR_RESET << std::endl;
    std::cout << TEST_COLOR_BOLD << "              CatUpdate Unit Test Suite" << TEST_COLOR_RESET << std::endl;
    std::cout << TEST_COLOR_CYAN << "====================================================" << TEST_COLOR_RESET << std::endl << std::endl;

    for (const auto& testCase : testCases) {
        std::cout << "[ RUN      ] " << testCase.suiteName << "." << testCase.testName << std::endl;
        
        try {
            testCase.testFunction();
            std::cout << TEST_COLOR_GREEN << "[       OK ] " << TEST_COLOR_RESET << testCase.suiteName << "." << testCase.testName << std::endl;
            passedCount++;
        } catch (const std::exception& ex) {
            std::cout << TEST_COLOR_RED << "[  FAILED  ] " << TEST_COLOR_RESET << testCase.suiteName << "." << testCase.testName 
                      << " (Reason: " << ex.what() << ")" << std::endl;
            failedCount++;
        } catch (...) {
            std::cout << TEST_COLOR_RED << "[  FAILED  ] " << TEST_COLOR_RESET << testCase.suiteName << "." << testCase.testName 
                      << " (Reason: Unknown exception occurred)" << std::endl;
            failedCount++;
        }
    }

    std::cout << std::endl;
    std::cout << TEST_COLOR_CYAN << "====================================================" << TEST_COLOR_RESET << std::endl;
    std::cout << TEST_COLOR_BOLD << "Test Summary:" << TEST_COLOR_RESET << std::endl;
    std::cout << "  Total:  " << testCases.size() << std::endl;
    std::cout << "  " << TEST_COLOR_GREEN << "Passed: " << TEST_COLOR_RESET << passedCount << std::endl;
    std::cout << "  " << TEST_COLOR_RED << "Failed: " << TEST_COLOR_RESET << failedCount << std::endl;
    std::cout << TEST_COLOR_CYAN << "====================================================" << TEST_COLOR_RESET << std::endl;

    return (failedCount == 0) ? 0 : 1;
}
