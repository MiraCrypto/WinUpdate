#include "CatUpdateCore.hpp"
#include "ProcessExecutor.hpp"
#include <array>
#include <cstddef>
#include <fileapi.h>
#include <handleapi.h>
#include <minwinbase.h>
#include <minwindef.h>
#include <namedpipeapi.h>
#include <optional>
#include <processthreadsapi.h>
#include <string>
#include <synchapi.h>
#include <vector>
#include <winnt.h>

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

namespace CatUpdate {

namespace {
std::optional<ProcessExecutionResult> ExecuteCommandLineInternal(const std::wstring& commandLine, bool logErrors) {
  HANDLE stdoutReadPipe = nullptr;
  HANDLE stdoutWritePipe = nullptr;
  SECURITY_ATTRIBUTES securityAttributes = {.nLength = sizeof(SECURITY_ATTRIBUTES)};
  securityAttributes.bInheritHandle = TRUE;
  securityAttributes.lpSecurityDescriptor = nullptr;

  if (CreatePipe(&stdoutReadPipe, &stdoutWritePipe, &securityAttributes, 0) == 0) {
    if (logErrors) {
      SystemLogger::LogError("Failed to create execution pipes.");
    }
    return std::nullopt;
  }

  if (SetHandleInformation(stdoutReadPipe, HANDLE_FLAG_INHERIT, 0) == 0) {
    CloseHandle(stdoutReadPipe);
    CloseHandle(stdoutWritePipe);
    return std::nullopt;
  }

  STARTUPINFOW startupInfo = {.cb = sizeof(startupInfo)};
  startupInfo.dwFlags = STARTF_USESTDHANDLES;
  startupInfo.hStdOutput = stdoutWritePipe;
  startupInfo.hStdError = stdoutWritePipe;
  startupInfo.hStdInput = nullptr;

  PROCESS_INFORMATION processInfo = {};

  BOOL const success = CreateProcessW(nullptr, const_cast<LPWSTR>(commandLine.c_str()), nullptr, nullptr, TRUE,
                                      CREATE_NO_WINDOW, nullptr, nullptr, &startupInfo, &processInfo);

  // Close the write end of the pipe as we no longer need it
  CloseHandle(stdoutWritePipe);

  if (success == 0) {
    CloseHandle(stdoutReadPipe);
    if (logErrors) {
      SystemLogger::LogError("CreateProcessW failed to launch command.");
    }
    return std::nullopt;
  }

  std::string standardOutput;
  std::array<char, 1024> buffer{};
  DWORD bytesRead = 0;
  while ((ReadFile(stdoutReadPipe, buffer.data(), static_cast<DWORD>(buffer.size() - 1), &bytesRead, nullptr) != 0) &&
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
  result.standardError = "";
  return result;
}
} // namespace

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

    std::wstring const wArg = Utils::ToWString(arg);

    if (i > 0) {
      commandLine += L" ";
    }
    commandLine += wArg;
  }

  return ExecuteCommandLineInternal(commandLine, true);
}

std::optional<ProcessExecutionResult> ProcessExecutor::ExecuteShellCommand(const std::string& shellCommandLine) {
  std::wstring const wCommandLine = Utils::ToWString(shellCommandLine);
  std::wstring const fullCommand = L"cmd.exe /c " + wCommandLine;
  return ExecuteCommandLineInternal(fullCommand, false);
}

} // namespace CatUpdate
