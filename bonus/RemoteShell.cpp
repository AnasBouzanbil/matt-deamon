
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
    
    // Add timeout to prevent hanging commands
    std::string timeoutCommand = "timeout 10s " + command + " 2>&1";
    
    // Use a lambda wrapper to avoid the ignored attributes warning
    auto pclose_wrapper = [](FILE* f) -> int {
        return pclose(f);
    };
    
    std::unique_ptr<FILE, decltype(pclose_wrapper)> pipe(popen(timeoutCommand.c_str(), "r"), pclose_wrapper);
    
    if (!pipe) {
        result = "Error: Failed to execute command\n\n";
        return result;
    }
    
    char buffer[1024];
    result = "Command output:\n";
    result += "$ " + command + "\n";
    result += "<---------------------------------------->\n";
    
    size_t output_size = 0;
    const size_t MAX_OUTPUT = 10000; // Limit output to 10KB
    
    while (fgets(buffer, sizeof(buffer), pipe.get()) != nullptr && output_size < MAX_OUTPUT) {
        result += buffer;
        output_size += strlen(buffer);
    }
    
    if (output_size >= MAX_OUTPUT) {
        result += "\n[Output truncated - too large]\n";
    }
    
    result += "<---------------------------------------->\n";
    
    // Check if command was killed by timeout
    int exit_status = pclose(pipe.release());
    if (WEXITSTATUS(exit_status) == 124) {
        result += "\n[Command timed out after 10 seconds]\n";
    }
    
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

