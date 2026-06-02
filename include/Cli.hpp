#pragma once

#include <vector>
#include <string>

namespace CatUpdate {

class CommandLineInterface {
public:
    /**
     * Entry point to parse CLI arguments and dispatch to the correct action.
     */
    static int Run(const std::vector<std::string>& arguments);

private:
    static void PrintUsage();
    static int ExecuteListCommand();
    static int ExecuteInfoCommand(const std::string& packageId);
    static int ExecuteInstallCommand(const std::string& packageId, const std::string& versionOverride);
    static int ExecuteUninstallCommand(const std::string& packageId);
    static int ExecutePathCommand(const std::vector<std::string>& pathArguments);
};

} // namespace CatUpdate
