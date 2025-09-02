#include "Tintin_reporter.hpp"

// Default Constructor
Tintin_reporter::Tintin_reporter() 
    : log_file_path("/tmp/log/matt_daemon/matt_daemon.log"), log_to_console(false) {
    test_log_file_access();
}

// Parameterized Constructor
Tintin_reporter::Tintin_reporter(const std::string& file_path, bool console)
    : log_file_path(file_path), log_to_console(console) {
    test_log_file_access();
}

// Copy Constructor (Coplien Form)
Tintin_reporter::Tintin_reporter(const Tintin_reporter& other)
    : log_file_path(other.log_file_path), log_to_console(other.log_to_console), clientAuthStatus(other.clientAuthStatus) {
    // No dynamic allocation, so simple member copy is sufficient
}

// Assignment Operator (Coplien Form)
Tintin_reporter& Tintin_reporter::operator=(const Tintin_reporter& other) {
    if (this != &other) {
        log_file_path = other.log_file_path;
        log_to_console = other.log_to_console;
        clientAuthStatus = other.clientAuthStatus;
    }
    return *this;
}

Tintin_reporter::~Tintin_reporter() {
    // Destructor - nothing specific to clean up
}

void Tintin_reporter::info(const std::string& message) {
    write_log("INFO", message);
}

void Tintin_reporter::warning(const std::string& message) {
    write_log("WARNING", message);
}

void Tintin_reporter::error(const std::string& message) {
    write_log("ERROR", message);
}

void Tintin_reporter::debug(const std::string& message) {
    write_log("DEBUG", message);
}

void Tintin_reporter::log(const std::string& message) {
    write_log("LOG", message);
}

void Tintin_reporter::set_log_file(const std::string& file_path) {
    log_file_path = file_path;
}

void Tintin_reporter::enable_console_output(bool enable) {
    log_to_console = enable;
}

void Tintin_reporter::clear_log() {
    std::ofstream file(log_file_path, std::ios::trunc);
    if (file.is_open()) {
        file.close();
    }
}

bool Tintin_reporter::is_log_file_accessible() const {
    std::ofstream test_file(log_file_path, std::ios::app);
    return test_file.is_open();
}

void Tintin_reporter::write_log(const std::string& level, const std::string& message) {
    std::string log_entry = "[" + get_timestamp() + "] [ " + level + " ] - " + message;
    
    // Write to file
    std::ofstream log_file(log_file_path, std::ios::app);
    if (log_file.is_open()) {
        log_file << log_entry << std::endl;
        log_file.close();
    }
    
    // Write to console if enabled
    if (log_to_console) {
        std::cout << log_entry << std::endl;
    }
}

std::string Tintin_reporter::get_timestamp() const {
    std::time_t now = std::time(nullptr);
    char timestamp[100];
    // Format: [DD/MM/YYYY-HH:MM:SS] as required by subject
    std::strftime(timestamp, sizeof(timestamp), "%d/%m/%Y-%H:%M:%S", std::localtime(&now));
    return std::string(timestamp);
}

void Tintin_reporter::test_log_file_access() {
    // Test if we can write to the log file
    std::ofstream test_file(log_file_path, std::ios::app);
    if (!test_file.is_open() && log_to_console) {
        std::cerr << "[Tintin_reporter] Warning: Cannot open log file: " << log_file_path << std::endl;
    }
}