
#include "Auth.hpp"
#include <iostream>
#include <string>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <cstring>
#include <sstream>
#include "RemoteShell.hpp"


Auth::Auth() {}
Auth::~Auth() {}

void Auth::start(std::string& message, std::map<int, bool>& authStatusMap , int clientSocket) {
 
    std::string response = processCommand(message, authStatusMap, clientSocket);
    if (!response.empty()) {
        if (send(clientSocket, response.c_str(), response.length(), 0) == -1) {
            perror("send");
        }
    }
}

void Auth::help(std::map<int, bool>& authStatusMap, int clientSocket) const {
    std::string helpMessage;
    if (!authStatusMap[clientSocket]) {
        helpMessage += "Available commands:\n";
        helpMessage += "LOGIN <username> <password> - Log in with your credentials\n";
        helpMessage += "HELP - Show this help message\n\n";
    } else {
        helpMessage = "Available commands:\n";
        helpMessage += "LOGOUT - Log out of the session\n";
        helpMessage += "SHELL <command> - Execute a shell command\n";
        helpMessage += "QUIT - Shut down Matt_daemon\n";
        helpMessage += "HELP - Show this help message\n\n";
    }
    if (send(clientSocket, helpMessage.c_str(), helpMessage.length(), 0) == -1) {
        perror("send");
    }
}

std::string handleLogin(const std::string& message, std::map<int, bool>& authStatusMap, int clientSocket) {
    std::string response;
    std::istringstream iss(message);
    std::string command, username, password;
    iss >> command >> username >> password;
    
    if (username == "admin" && password == "1234") {
        authStatusMap[clientSocket] = true;
        response += "#############################\n";
        response += "#   WELCOME TO MATT DAEMON  #\n";
        response += "#############################\n\n";
        response += "Login successful. You are now authenticated.\n\n";
        response += "Available commands:\n";
        response += "LOGOUT - Log out of the session\n";
        response += "SHELL <command> - Execute a shell command\n";
        response += "QUIT - Shut down Matt_daemon\n";
        response += "HELP - Show this help message\n\n";
    } else {
        response = "Login failed. Invalid username or password.\n\n";
    }
    return response;
}

std::string Auth::processCommand(const std::string& message, std::map<int, bool>& authStatusMap, int clientSocket) {
    std::string response;
    
    if (message.substr(0, 5) == "LOGIN" || message.substr(0, 5) == "login") {
        // check if already logged in
        if (authStatusMap[clientSocket]) {
            return "You are already logged in.\n\n";
        }
        return handleLogin(message, authStatusMap, clientSocket);
    }
    
    if (message == "HELP" || message == "help") {
        help(authStatusMap, clientSocket);
        return "";
    }
    
    // Check if user is authenticated before allowing SHELL commands
    if (message.substr(0, 5) == "SHELL" || message.substr(0, 5) == "shell") {
        if (!authStatusMap[clientSocket]) {
            return "Access denied. Please login first.\n\n";
        }
        std::string response = remoteShell.start(message);
        return response;
    }
    
    // Handle LOGOUT command
    if (message == "LOGOUT" || message == "logout") {
        if (authStatusMap[clientSocket]) {
            authStatusMap[clientSocket] = false;
            return "Logged out successfully.\n\n";
        } else {
            return "You are not logged in.\n\n";
        }
    }
    
    return "";
}