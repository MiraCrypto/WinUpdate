#pragma once

#include <string>
#include <vector>
#include <optional>

namespace CatUpdate {

struct ProcessExecutionResult {
    int exitCode;
    std::string standardOutput;
    std::string standardError;
};

class ProcessExecutor {
public:
    /**
     * Executes a system command synchronously and returns the exit code and output streams.
     * 
     * @param commandAndArguments A vector where the first element is the executable name/path
     *                            and subsequent elements are the arguments.
     * @return ProcessExecutionResult containing the exit code and console outputs.
     */
    static std::optional<ProcessExecutionResult> ExecuteCommand(
        const std::vector<std::string>& commandAndArguments
    );

    /**
     * A helper that runs a single command-line string directly using system shell conventions.
     * On Windows: Run via cmd.exe or directly.
     * On POSIX: Run via /bin/sh.
     */
    static std::optional<ProcessExecutionResult> ExecuteShellCommand(
        const std::string& shellCommandLine
    );
};

} // namespace CatUpdate
