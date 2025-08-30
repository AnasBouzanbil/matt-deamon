#ifndef TINTIN_REPORTER_HPP
#define TINTIN_REPORTER_HPP

#include <string>
#include <fstream>
#include <iostream>
#include <ctime>
#include <map>
#include "Auth.hpp"

class Tintin_reporter {
private:
    std::string log_file_path;
    bool log_to_console;
    

public:
    // Constructor
    Tintin_reporter(const std::string& file_path = "/tmp/matt_daemon.log", bool console = false);
    std::map<int, bool> clientAuthStatus;
    // Destructor
    ~Tintin_reporter();
    
    // Main logging methods
    void info(const std::string& message);
    void warning(const std::string& message);
    void error(const std::string& message);
    void debug(const std::string& message);
    void log(const std::string& message);  // Added LOG level for user input
    
    // Configuration methods
    void set_log_file(const std::string& file_path);
    void enable_console_output(bool enable);
    
    // Utility methods
    void clear_log();
    bool is_log_file_accessible() const;

private:
    // Helper methods
    void write_log(const std::string& level, const std::string& message);
    std::string get_timestamp() const;
};

#endif // TINTIN_REPORTER_HPP