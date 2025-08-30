
#pragma once
#include <string>
#include <sstream>
#include <map>
#include <vector>
#include <iostream>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <cstring>
#include <cerrno>
#include <memory>

class Remote_shell
{public:
    Remote_shell();
    ~Remote_shell();
    std::string start(const std::string& message);
private:
    std::string executeShellCommand(const std::string& command);
    std::string processCommand(const std::string& message);
};

