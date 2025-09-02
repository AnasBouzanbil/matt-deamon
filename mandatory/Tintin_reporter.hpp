#ifndef TINTIN_REPORTER_HPP
#define TINTIN_REPORTER_HPP

#include <string>
#include <fstream>
#include <iostream>
#include <ctime>

class Tintin_reporter {
private:
    std::string log_file_path;
    bool log_to_console;

public:
    // Default Constructor
    Tintin_reporter();
    
    // Parameterized Constructor
    Tintin_reporter(const std::string& file_path, bool console = false);
    
    // Copy Constructor (Coplien Form)
    Tintin_reporter(const Tintin_reporter& other);
    
    // Assignment Operator (Coplien Form)
    Tintin_reporter& operator=(const Tintin_reporter& other);
    
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
    void test_log_file_access();
};

#endif // TINTIN_REPORTER_HPP