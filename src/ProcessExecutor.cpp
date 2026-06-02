#include "ProcessExecutor.hpp"
#include "CatUpdateCore.hpp"
#include <iostream>
#include <sstream>
#include <array>

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#else
#include <sys/wait.h>
#include <unistd.h>
#include <stdio.h>
#endif

namespace CatUpdate {

#if defined(_WIN32)

std::optional<ProcessExecutionResult> ProcessExecutor::ExecuteCommand(
    const std::vector<std::string>& commandAndArguments
) {
    if (commandAndArguments.empty()) {
        return std::nullopt;
    }

    // Build the command line string
    std::wstring commandLine;
    for (size_t i = 0; i < commandAndArguments.size(); ++i) {
        std::string arg = commandAndArguments[i];
        
        // Simple quoting for arguments containing spaces
        if (arg.find(' ') != std::string::npos) {
            arg = "\"" + arg + "\"";
        }
        
        // Convert to wide string
        int sizeNeeded = MultiByteToWideChar(CP_UTF8, 0, arg.c_str(), -1, NULL, 0);
        std::wstring wArg(sizeNeeded - 1, 0);
        MultiByteToWideChar(CP_UTF8, 0, arg.c_str(), -1, wArg.data(), sizeNeeded);

        if (i > 0) {
            commandLine += L" ";
        }
        commandLine += wArg;
    }

    // Create pipes for stdout/stderr redirection
    HANDLE stdoutReadPipe = NULL;
    HANDLE stdoutWritePipe = NULL;
    SECURITY_ATTRIBUTES securityAttributes = { sizeof(SECURITY_ATTRIBUTES) };
    securityAttributes.bInheritHandle = TRUE;
    securityAttributes.lpSecurityDescriptor = NULL;

    if (!CreatePipe(&stdoutReadPipe, &stdoutWritePipe, &securityAttributes, 0)) {
        SystemLogger::LogError("Failed to create execution pipes.");
        return std::nullopt;
    }

    // Ensure the read handle to the pipe is not inherited
    if (!SetHandleInformation(stdoutReadPipe, HANDLE_FLAG_INHERIT, 0)) {
        CloseHandle(stdoutReadPipe);
        CloseHandle(stdoutWritePipe);
        return std::nullopt;
    }

    STARTUPINFOW startupInfo = { sizeof(startupInfo) };
    startupInfo.dwFlags = STARTF_USESTDHANDLES;
    startupInfo.hStdOutput = stdoutWritePipe;
    startupInfo.hStdError = stdoutWritePipe; // Redirect stderr to stdout for simplicity
    startupInfo.hStdInput = NULL;

    PROCESS_INFORMATION processInfo = {};

    // Launch process with CREATE_NO_WINDOW flag
    BOOL success = CreateProcessW(
        NULL,
        const_cast<LPWSTR>(commandLine.c_str()),
        NULL,
        NULL,
        TRUE,
        CREATE_NO_WINDOW,
        NULL,
        NULL,
        &startupInfo,
        &processInfo
    );

    // Close the write end of the pipe as we no longer need it
    CloseHandle(stdoutWritePipe);

    if (!success) {
        CloseHandle(stdoutReadPipe);
        SystemLogger::LogError("CreateProcessW failed to launch: " + commandAndArguments[0]);
        return std::nullopt;
    }

    // Read output from pipe
    std::string standardOutput;
    std::array<char, 1024> buffer;
    DWORD bytesRead = 0;
    while (ReadFile(stdoutReadPipe, buffer.data(), static_cast<DWORD>(buffer.size() - 1), &bytesRead, NULL) && bytesRead > 0) {
        buffer[bytesRead] = '\0';
        standardOutput.append(buffer.data(), bytesRead);
    }

    // Wait for process to exit and get code
    WaitForSingleObject(processInfo.hProcess, INFINITE);
    DWORD exitCode = 0;
    GetExitCodeProcess(processInfo.hProcess, &exitCode);

    // Clean up handles
    CloseHandle(processInfo.hProcess);
    CloseHandle(processInfo.hThread);
    CloseHandle(stdoutReadPipe);

    ProcessExecutionResult result;
    result.exitCode = static_cast<int>(exitCode);
    result.standardOutput = standardOutput;
    result.standardError = "";
    return result;
}

std::optional<ProcessExecutionResult> ProcessExecutor::ExecuteShellCommand(
    const std::string& shellCommandLine
) {
    // Convert command to Wide String
    int sizeNeeded = MultiByteToWideChar(CP_UTF8, 0, shellCommandLine.c_str(), -1, NULL, 0);
    std::wstring wCommandLine(sizeNeeded - 1, 0);
    MultiByteToWideChar(CP_UTF8, 0, shellCommandLine.c_str(), -1, wCommandLine.data(), sizeNeeded);

    // Execute via cmd.exe
    std::wstring fullCommand = L"cmd.exe /c " + wCommandLine;

    HANDLE stdoutReadPipe = NULL;
    HANDLE stdoutWritePipe = NULL;
    SECURITY_ATTRIBUTES securityAttributes = { sizeof(SECURITY_ATTRIBUTES) };
    securityAttributes.bInheritHandle = TRUE;

    if (!CreatePipe(&stdoutReadPipe, &stdoutWritePipe, &securityAttributes, 0)) {
        return std::nullopt;
    }

    SetHandleInformation(stdoutReadPipe, HANDLE_FLAG_INHERIT, 0);

    STARTUPINFOW startupInfo = { sizeof(startupInfo) };
    startupInfo.dwFlags = STARTF_USESTDHANDLES;
    startupInfo.hStdOutput = stdoutWritePipe;
    startupInfo.hStdError = stdoutWritePipe;
    startupInfo.hStdInput = NULL;

    PROCESS_INFORMATION processInfo = {};

    BOOL success = CreateProcessW(
        NULL,
        const_cast<LPWSTR>(fullCommand.c_str()),
        NULL,
        NULL,
        TRUE,
        CREATE_NO_WINDOW,
        NULL,
        NULL,
        &startupInfo,
        &processInfo
    );

    CloseHandle(stdoutWritePipe);

    if (!success) {
        CloseHandle(stdoutReadPipe);
        return std::nullopt;
    }

    std::string standardOutput;
    std::array<char, 1024> buffer;
    DWORD bytesRead = 0;
    while (ReadFile(stdoutReadPipe, buffer.data(), static_cast<DWORD>(buffer.size() - 1), &bytesRead, NULL) && bytesRead > 0) {
        buffer[bytesRead] = '\0';
        standardOutput.append(buffer.data(), bytesRead);
    }

    WaitForSingleObject(processInfo.hProcess, INFINITE);
    DWORD exitCode = 0;
    GetExitCodeProcess(processInfo.hProcess, &exitCode);

    CloseHandle(processInfo.hProcess);
    CloseHandle(processInfo.hThread);
    CloseHandle(stdoutReadPipe);

    ProcessExecutionResult result;
    result.exitCode = static_cast<int>(exitCode);
    result.standardOutput = standardOutput;
    return result;
}

#else // POSIX implementation (Linux / macOS)

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

#endif

} // namespace CatUpdate
