#include "Tintin_reporter.hpp"

Tintin_reporter::Tintin_reporter(const std::string& file_path, bool console)
    : log_file_path(file_path), log_to_console(console) {
    // Test if we can write to the log file
    std::ofstream test_file(log_file_path, std::ios::app);
    if (!test_file.is_open() && log_to_console) {
        std::cerr << "[Tintin_reporter] Warning: Cannot open log file: " << log_file_path << std::endl;
    }
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