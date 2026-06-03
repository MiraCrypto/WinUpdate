#include "ProcessExecutor.hpp"
#include "CatUpdateCore.hpp"
#include <sys/wait.h>
#include <unistd.h>
#include <stdio.h>
#include <array>
#include <vector>

namespace CatUpdate {

std::optional<ProcessExecutionResult> ProcessExecutor::ExecuteCommand(
    const std::vector<std::string>& commandAndArguments
) {
    if (commandAndArguments.empty()) {
        return std::nullopt;
    }

    // Use pipe to capture output
    int stdoutPipe[2];
    if (pipe(stdoutPipe) == -1) {
        SystemLogger::LogError("Failed to create POSIX pipes.");
        return std::nullopt;
    }

    pid_t pid = fork();
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

        // Convert command vector to C-style array of strings
        std::vector<char*> cArgs;
        cArgs.reserve(commandAndArguments.size() + 1);
        for (const auto& arg : commandAndArguments) {
            cArgs.push_back(const_cast<char*>(arg.c_str()));
        }
        cArgs.push_back(nullptr);

        execvp(cArgs[0], cArgs.data());
        
        // If exec fails, terminate immediately
        exit(127);
    }

    // Parent process
    close(stdoutPipe[1]);

    std::string standardOutput;
    std::array<char, 1024> buffer;
    ssize_t bytesRead = 0;
    while ((bytesRead = read(stdoutPipe[0], buffer.data(), buffer.size() - 1)) > 0) {
        buffer[bytesRead] = '\0';
        standardOutput.append(buffer.data(), bytesRead);
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

std::optional<ProcessExecutionResult> ProcessExecutor::ExecuteShellCommand(
    const std::string& shellCommandLine
) {
    // Append 2>&1 to shell command to merge stderr into stdout
    std::string commandWithRedirect = shellCommandLine + " 2>&1";
    FILE* fileStream = popen(commandWithRedirect.c_str(), "r");
    if (!fileStream) {
        return std::nullopt;
    }

    std::string standardOutput;
    std::array<char, 1024> buffer;
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
