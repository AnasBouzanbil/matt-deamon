#ifndef AUTH_HPP
#define AUTH_HPP

#include <string>
#include <map>
#include <sstream>
#include "Tintin_reporter.hpp"
#include "RemoteShell.hpp"


class Auth {
private:
    std::map<std::string, std::string> users; // username -> password mapping

public:
    Auth();
    ~Auth();
    Remote_shell remoteShell;
    void help(std::map<int, bool>& authStatusMap, int clientSocket) const;
    std::string processCommand(const std::string& message, std::map<int, bool>& authStatusMap, int clientSocket);
    void start(std::string& message, std::map<int, bool>& authStatusMap , int clientSocket);
    std::string handleCreateUser(const std::string& message);
    bool authenticateUser(const std::string& username, const std::string& password);

};

#endif // AUTH_HPP