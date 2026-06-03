#include "CatUpdateCore.hpp"
#include "ProcessExecutor.hpp"
#include <array>
#include <cstdio>
#include <sys/wait.h>
#include <unistd.h>
#include <vector>

namespace CatUpdate {

std::optional<ProcessExecutionResult>
ProcessExecutor::ExecuteCommand(const std::vector<std::string>& commandAndArguments) {
  if (commandAndArguments.empty()) {
    return std::nullopt;
  }

  // Use pipe to capture output
  std::array<int, 2> stdoutPipe{};
  if (pipe(stdoutPipe.data()) == -1) {
    SystemLogger::LogError("Failed to create POSIX pipes.");
    return std::nullopt;
  }

  const pid_t pid = fork();
  if (pid == -1) {
    close(stdoutPipe[0]);
    close(stdoutPipe[1]);
    SystemLogger::LogError("POSIX fork failed.");
    return std::nullopt;
  }

  if (pid == 0) {
    // Child process
    // Redirect stdout and stderr to pipe
    dup2(stdoutPipe[1], STDOUT_FILENO);
    dup2(stdoutPipe[1], STDERR_FILENO);
    close(stdoutPipe[0]);
    close(stdoutPipe[1]);

    // Convert command vector to C-style array of strings (avoid const_cast)
    std::vector<std::vector<char>> argBuffers;
    argBuffers.reserve(commandAndArguments.size());
    std::vector<char*> cArgs;
    cArgs.reserve(commandAndArguments.size() + 1);
    for (const auto& arg : commandAndArguments) {
      std::vector<char> buffer(arg.begin(), arg.end());
      buffer.push_back('\0');
      argBuffers.push_back(std::move(buffer));
      cArgs.push_back(argBuffers.back().data());
    }
    cArgs.push_back(nullptr);

    execvp(cArgs[0], cArgs.data());

    // If exec fails, terminate immediately
    constexpr int EXEC_FAILURE_EXIT_CODE = 127;
    exit(EXEC_FAILURE_EXIT_CODE);
  }

  // Parent process
  close(stdoutPipe[1]);

  std::string standardOutput;
  constexpr size_t BUFFER_SIZE = 1024;
  while (true) {
    std::array<char, BUFFER_SIZE> buffer{};
    const ssize_t bytesRead = read(stdoutPipe[0], buffer.data(), buffer.size() - 1);
    if (bytesRead <= 0) {
      break;
    }
    standardOutput.append(buffer.data(), static_cast<size_t>(bytesRead));
  }

  close(stdoutPipe[0]);

  int status = 0;
  waitpid(pid, &status, 0);

  int exitCode = 0;
  if (WIFEXITED(status)) {
    exitCode = WEXITSTATUS(status);
  } else {
    exitCode = -1;
  }

  ProcessExecutionResult result;
  result.exitCode = exitCode;
  result.standardOutput = standardOutput;
  result.standardError = "";
  return result;
}

std::optional<ProcessExecutionResult>
ProcessExecutor::ExecuteShellCommand(const std::string& shellCommandLine) {
  // Append 2>&1 to shell command to merge stderr into stdout
  std::string commandWithRedirect = shellCommandLine + " 2>&1";
  FILE* fileStream = popen(commandWithRedirect.c_str(), "r"); // NOLINT(bugprone-command-processor)
  if (fileStream == nullptr) {
    return std::nullopt;
  }

  std::string standardOutput;
  constexpr size_t SHELL_BUFFER_SIZE = 1024;
  std::array<char, SHELL_BUFFER_SIZE> buffer{};
  while (fgets(buffer.data(), static_cast<int>(buffer.size()), fileStream) != nullptr) {
    standardOutput.append(buffer.data());
  }

  int status = pclose(fileStream);
  int exitCode = 0;
  if (WIFEXITED(status)) {
    exitCode = WEXITSTATUS(status);
  } else {
    exitCode = -1;
  }

  ProcessExecutionResult result;
  result.exitCode = exitCode;
  result.standardOutput = standardOutput;
  result.standardError = "";
  return result;
}

} // namespace CatUpdate
