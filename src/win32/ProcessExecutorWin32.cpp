#include "CatUpdateCore.hpp"
#include "ProcessExecutor.hpp"
#include <array>
#include <vector>

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

namespace CatUpdate {

std::optional<ProcessExecutionResult>
ProcessExecutor::ExecuteCommand(const std::vector<std::string>& commandAndArguments) {
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

    std::wstring wArg = Utils::ToWString(arg);

    if (i > 0) {
      commandLine += L" ";
    }
    commandLine += wArg;
  }

  // Create pipes for stdout/stderr redirection
  HANDLE stdoutReadPipe = NULL;
  HANDLE stdoutWritePipe = NULL;
  SECURITY_ATTRIBUTES securityAttributes = {sizeof(SECURITY_ATTRIBUTES)};
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

  STARTUPINFOW startupInfo = {sizeof(startupInfo)};
  startupInfo.dwFlags = STARTF_USESTDHANDLES;
  startupInfo.hStdOutput = stdoutWritePipe;
  startupInfo.hStdError = stdoutWritePipe; // Redirect stderr to stdout for simplicity
  startupInfo.hStdInput = NULL;

  PROCESS_INFORMATION processInfo = {};

  // Launch process with CREATE_NO_WINDOW flag
  BOOL success = CreateProcessW(NULL, const_cast<LPWSTR>(commandLine.c_str()), NULL, NULL, TRUE,
                                CREATE_NO_WINDOW, NULL, NULL, &startupInfo, &processInfo);

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
  while (ReadFile(stdoutReadPipe, buffer.data(), static_cast<DWORD>(buffer.size() - 1), &bytesRead,
                  NULL) &&
         bytesRead > 0) {
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

std::optional<ProcessExecutionResult>
ProcessExecutor::ExecuteShellCommand(const std::string& shellCommandLine) {
  std::wstring wCommandLine = Utils::ToWString(shellCommandLine);
  std::wstring fullCommand = L"cmd.exe /c " + wCommandLine;

  HANDLE stdoutReadPipe = NULL;
  HANDLE stdoutWritePipe = NULL;
  SECURITY_ATTRIBUTES securityAttributes = {sizeof(SECURITY_ATTRIBUTES)};
  securityAttributes.bInheritHandle = TRUE;

  if (!CreatePipe(&stdoutReadPipe, &stdoutWritePipe, &securityAttributes, 0)) {
    return std::nullopt;
  }

  SetHandleInformation(stdoutReadPipe, HANDLE_FLAG_INHERIT, 0);

  STARTUPINFOW startupInfo = {sizeof(startupInfo)};
  startupInfo.dwFlags = STARTF_USESTDHANDLES;
  startupInfo.hStdOutput = stdoutWritePipe;
  startupInfo.hStdError = stdoutWritePipe;
  startupInfo.hStdInput = NULL;

  PROCESS_INFORMATION processInfo = {};

  BOOL success = CreateProcessW(NULL, const_cast<LPWSTR>(fullCommand.c_str()), NULL, NULL, TRUE,
                                CREATE_NO_WINDOW, NULL, NULL, &startupInfo, &processInfo);

  CloseHandle(stdoutWritePipe);

  if (!success) {
    CloseHandle(stdoutReadPipe);
    return std::nullopt;
  }

  std::string standardOutput;
  std::array<char, 1024> buffer;
  DWORD bytesRead = 0;
  while (ReadFile(stdoutReadPipe, buffer.data(), static_cast<DWORD>(buffer.size() - 1), &bytesRead,
                  NULL) &&
         bytesRead > 0) {
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

} // namespace CatUpdate
