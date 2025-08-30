
#include "RemoteShell.hpp"
#include <iostream>
#include <string>
#include <vector>
#include <map>
#include <fstream>
#include <sstream>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <cstring>
#include <cerrno>
#include <memory>


Remote_shell::Remote_shell() {
}

Remote_shell::~Remote_shell() {
    
}

std::string Remote_shell::start(const std::string& message) {
    return processCommand(message);
}

std::string Remote_shell::executeShellCommand(const std::string& command) {

    std::string result;
    
    std::string sanitizedCommand = command + " 2>&1"; // Redirect stderr to stdout
    std::unique_ptr<FILE, decltype(&pclose)> pipe(popen(sanitizedCommand.c_str(), "r"), pclose);

    if (!pipe) {
        result = "Error: Failed to execute command\n\n";
    }
    
    char buffer[1024];
    result = "Command output:\n";
    result += "$ " + command + "\n";
    result += "<---------------------------------------->\n";
    
    while (fgets(buffer, sizeof(buffer), pipe.get()) != nullptr) {
        result += buffer;
    }

    result += "<---------------------------------------->\n";
    return result;
}

std::string Remote_shell::processCommand(const std::string& message) {
    std::string response;
    if (message.substr(0, 5) == "SHELL" || message.substr(0, 5) == "shell") {
        if (message.length() <= 6) {
            response = "Error: SHELL command requires an argument\n";
            response += "Usage: SHELL <command>\n";
            response += "Example: SHELL ls -la\n\n";
            return response;
        }
        
        std::string shellCommand = message.substr(6); // Skip "SHELL "
        response = executeShellCommand(shellCommand);
    }
    return response;
}

