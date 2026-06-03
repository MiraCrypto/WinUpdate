#pragma once

#include <functional>
#include <stdexcept>
#include <string>

namespace CatUpdate {

struct TestCase {
  std::string suiteName;
  std::string testName;
  std::function<void()> testFunction;
};

class TestRunner {
public:
  static TestRunner& Instance() {
    static TestRunner runner;
    return runner;
  }

  void RegisterTestCase(const std::string& suiteName, const std::string& testName,
                        std::function<void()> testFunc) {
    m_testCases.push_back({suiteName, testName, testFunc});
  }

  std::vector<TestCase> GetTestCases() const { return m_testCases; }

private:
  std::vector<TestCase> m_testCases;
};

} // namespace CatUpdate

// Macro helper to define tests
#define TEST(SuiteName, TestName)                                                                  \
  void SuiteName##_##TestName##_Func();                                                            \
  static const struct SuiteName##_##TestName##_Register {                                          \
    SuiteName##_##TestName##_Register() noexcept {                                                 \
      try {                                                                                        \
        ::CatUpdate::TestRunner::Instance().RegisterTestCase(#SuiteName, #TestName,                \
                                                             SuiteName##_##TestName##_Func);       \
      } catch (...) {                                                                              \
      }                                                                                            \
    }                                                                                              \
  } SuiteName##_##TestName##_RegInstance;                                                          \
  void SuiteName##_##TestName##_Func()

// Custom assert helper
#define ASSERT_TRUE(condition)                                                                     \
  if (!(condition)) {                                                                              \
    throw std::runtime_error("Assertion failed: " #condition " is false on line " +                \
                             std::to_string(__LINE__));                                            \
  }

#define ASSERT_FALSE(condition)                                                                    \
  if ((condition)) {                                                                               \
    throw std::runtime_error("Assertion failed: " #condition " is true on line " +                 \
                             std::to_string(__LINE__));                                            \
  }
