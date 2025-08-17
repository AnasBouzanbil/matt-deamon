#include <iostream>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <cstdlib>
#include <fstream>
#include <cstring>
#include <sys/select.h>
#include <algorithm>
#include <signal.h>
#include <sstream>
#include <vector>
#include <ctime>
#include "Tintin_reporter.hpp"

const int MAX_CONNECTIONS = 3;
const std::string LOCK_FILE = "/tmp/matt_daemon.lock";
const std::string LOG_FILE = "/tmp/matt_daemon.log";
const std::string AUTH_FILE = "./matt_daemon_auth.txt";  // Later change to "/root/matt_daemon_auth.txt"
bool daemon_running = true;
Tintin_reporter* global_logger = nullptr;

// Simple session management
std::vector<std::string> active_sessions;

// Generate a simple session token
std::string generate_session_token() {
    std::string token;
    const char charset[] = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";
    srand(time(nullptr));
    for (int i = 0; i < 32; ++i) {
        token += charset[rand() % (sizeof(charset) - 1)];
    }
    return token;
}

// Check if session token is valid
bool is_valid_session(const std::string& token) {
    return std::find(active_sessions.begin(), active_sessions.end(), token) != active_sessions.end();
}

// Create or read the authentication file
void create_auth_file() {
    std::ifstream check_file(AUTH_FILE);
    if (!check_file.is_open()) {
        // Create default password file
        std::ofstream auth_file(AUTH_FILE);
        if (auth_file.is_open()) {
            auth_file << "admin123" << std::endl;  // Default password
            auth_file.close();
            if (global_logger) {
                global_logger->info("Matt_daemon: Created authentication file with default password");
            }
        }
    } else {
        check_file.close();
    }
}

// Verify password from auth file
bool verify_password(const std::string& password) {
    std::ifstream auth_file(AUTH_FILE);
    if (!auth_file.is_open()) {
        return false;
    }
    
    std::string stored_password;
    std::getline(auth_file, stored_password);
    auth_file.close();
    
    return password == stored_password;
}

// Check if user has root-like privileges (for now, just check if auth file exists)
bool has_root_privileges() {
    // For now, we'll simulate this by checking if we can read the auth file
    // Later, you can add: return getuid() == 0;
    std::ifstream auth_file(AUTH_FILE);
    bool can_read = auth_file.is_open();
    if (can_read) auth_file.close();
    return can_read;
}

std::string get_login_page() {
    std::string html = "<!DOCTYPE html>\n"
        "<html>\n"
        "<head>\n"
        "    <title>Matt Daemon - Authentication Required</title>\n"
        "    <style>\n"
        "        body { font-family: Arial, sans-serif; margin: 0; padding: 0; background: linear-gradient(135deg, #667eea 0%, #764ba2 100%); height: 100vh; display: flex; justify-content: center; align-items: center; }\n"
        "        .login-container { background: white; padding: 40px; border-radius: 15px; box-shadow: 0 10px 30px rgba(0,0,0,0.3); width: 400px; text-align: center; }\n"
        "        .logo { font-size: 2.5em; margin-bottom: 20px; color: #333; }\n"
        "        h1 { color: #333; margin-bottom: 10px; }\n"
        "        .subtitle { color: #666; margin-bottom: 30px; font-size: 14px; }\n"
        "        .privilege-warning { background-color: #fff3cd; color: #856404; padding: 15px; border-radius: 5px; margin-bottom: 20px; border: 1px solid #ffeaa7; }\n"
        "        .form-group { margin-bottom: 20px; text-align: left; }\n"
        "        label { display: block; margin-bottom: 5px; color: #333; font-weight: bold; }\n"
        "        input[type=password] { width: 100%; padding: 12px; border: 2px solid #ddd; border-radius: 5px; font-size: 16px; box-sizing: border-box; }\n"
        "        input[type=password]:focus { border-color: #667eea; outline: none; }\n"
        "        .login-btn { background: linear-gradient(135deg, #667eea 0%, #764ba2 100%); color: white; padding: 12px 30px; border: none; border-radius: 5px; font-size: 16px; cursor: pointer; width: 100%; }\n"
        "        .login-btn:hover { opacity: 0.9; }\n"
        "        .error { background-color: #f8d7da; color: #721c24; padding: 10px; border-radius: 5px; margin-top: 15px; display: none; }\n"
        "        .info { background-color: #d1ecf1; color: #0c5460; padding: 15px; border-radius: 5px; margin-bottom: 20px; font-size: 12px; }\n"
        "    </style>\n"
        "</head>\n"
        "<body>\n"
        "    <div class=\"login-container\">\n"
        "        <div class=\"logo\">🔒</div>\n"
        "        <h1>Matt Daemon</h1>\n"
        "        <div class=\"subtitle\">Authentication Required</div>\n"
        "        \n"
        "        <div class=\"privilege-warning\">\n"
        "            ⚠️ <strong>Root Access Required</strong><br>\n"
        "            This daemon requires administrator privileges to access.\n"
        "        </div>\n"
        "        \n"
        "        <div class=\"info\">\n"
        "            <strong>Authentication File:</strong> " + AUTH_FILE + "<br>\n"
        "            <strong>Status:</strong> Password protection active\n"
        "        </div>\n"
        "        \n"
        "        <form id=\"loginForm\">\n"
        "            <div class=\"form-group\">\n"
        "                <label for=\"password\">Administrator Password:</label>\n"
        "                <input type=\"password\" id=\"password\" name=\"password\" placeholder=\"Enter admin password\" required>\n"
        "            </div>\n"
        "            <button type=\"submit\" class=\"login-btn\">🔓 Access Daemon</button>\n"
        "        </form>\n"
        "        \n"
        "        <div id=\"error\" class=\"error\"></div>\n"
        "        \n"
        "        <script>\n"
        "            document.getElementById('loginForm').addEventListener('submit', function(e) {\n"
        "                e.preventDefault();\n"
        "                \n"
        "                const password = document.getElementById('password').value;\n"
        "                const errorDiv = document.getElementById('error');\n"
        "                \n"
        "                fetch('/auth', {\n"
        "                    method: 'POST',\n"
        "                    headers: { 'Content-Type': 'application/json' },\n"
        "                    body: JSON.stringify({ password: password })\n"
        "                })\n"
        "                .then(response => response.json())\n"
        "                .then(data => {\n"
        "                    if (data.success) {\n"
        "                        // Store session token\n"
        "                        localStorage.setItem('matt_daemon_session', data.token);\n"
        "                        // Redirect to main interface\n"
        "                        window.location.href = '/dashboard';\n"
        "                    } else {\n"
        "                        errorDiv.textContent = 'Invalid password. Access denied.';\n"
        "                        errorDiv.style.display = 'block';\n"
        "                        document.getElementById('password').value = '';\n"
        "                    }\n"
        "                })\n"
        "                .catch(error => {\n"
        "                    errorDiv.textContent = 'Authentication error: ' + error;\n"
        "                    errorDiv.style.display = 'block';\n"
        "                });\n"
        "            });\n"
        "        </script>\n"
        "    </div>\n"
        "</body>\n"
        "</html>";
    
    return html;
}

std::string get_web_page() {
    std::string html = "<!DOCTYPE html>\n"
        "<html>\n"
        "<head>\n"
        "    <title>Matt Daemon Controller</title>\n"
        "    <style>\n"
        "        body { font-family: Arial, sans-serif; margin: 40px; background-color: #f0f0f0; }\n"
        "        .container { max-width: 800px; margin: 0 auto; background: white; padding: 30px; border-radius: 10px; box-shadow: 0 4px 6px rgba(0,0,0,0.1); }\n"
        "        h1 { color: #333; text-align: center; }\n"
        "        .status { padding: 15px; margin: 20px 0; border-radius: 5px; text-align: center; font-weight: bold; }\n"
        "        .running { background-color: #d4edda; color: #155724; border: 1px solid #c3e6cb; }\n"
        "        .stopped { background-color: #f8d7da; color: #721c24; border: 1px solid #f5c6cb; }\n"
        "        button { background-color: #dc3545; color: white; border: none; padding: 15px 30px; font-size: 16px; border-radius: 5px; cursor: pointer; display: block; margin: 20px auto; }\n"
        "        button:hover { background-color: #c82333; }\n"
        "        button:disabled { background-color: #6c757d; cursor: not-allowed; }\n"
        "        .info { background-color: #d1ecf1; color: #0c5460; padding: 15px; border-radius: 5px; margin: 20px 0; }\n"
        "        .logs { background-color: #f8f9fa; padding: 15px; border-radius: 5px; margin: 20px 0; max-height: 300px; overflow-y: auto; font-family: monospace; white-space: pre-wrap; }\n"
        "        .shell-section { margin: 30px 0; padding: 20px; background-color: #f8f9fa; border-radius: 5px; border: 1px solid #dee2e6; }\n"
        "        .shell-input { width: 100%; padding: 10px; margin: 10px 0; border: 1px solid #ccc; border-radius: 3px; font-family: monospace; }\n"
        "        .shell-output { background-color: #000; color: #0f0; padding: 15px; border-radius: 5px; max-height: 300px; overflow-y: auto; font-family: monospace; white-space: pre-wrap; }\n"
        "        .btn-primary { background-color: #007bff; color: white; border: none; padding: 10px 20px; border-radius: 3px; cursor: pointer; margin: 5px; }\n"
        "        .btn-primary:hover { background-color: #0056b3; }\n"
        "        .btn-success { background-color: #28a745; }\n"
        "        .btn-success:hover { background-color: #1e7e34; }\n"
        "        .tabs { display: flex; margin-bottom: 20px; }\n"
        "        .tab { padding: 10px 20px; background-color: #e9ecef; border: 1px solid #dee2e6; cursor: pointer; margin-right: 5px; border-radius: 5px 5px 0 0; }\n"
        "        .tab.active { background-color: #fff; border-bottom: 1px solid #fff; }\n"
        "        .tab-content { display: none; }\n"
        "        .tab-content.active { display: block; }\n"
        "    </style>\n"
        "</head>\n"
        "<body>\n"
        "    <div class=\"container\">\n"
        "        <h1>Matt Daemon Controller</h1>\n"
        "        <div class=\"status running\">\n"
        "            🟢 Daemon is Running\n"
        "        </div>\n"
        "        <div class=\"info\">\n"
        "            <strong>Daemon Information:</strong><br>\n"
        "            • Server running on port 8080<br>\n"
        "            • Web interface on port 8080<br>\n"
        "            • Lock file: /tmp/matt_daemon.lock<br>\n"
        "            • Log file: /tmp/matt_daemon.log\n"
        "        </div>\n"
        "        <button onclick=\"stopDaemon()\">Stop Daemon</button>\n"
        "        <div id=\"message\"></div>\n"
        "        \n"
        "        <!-- Enhanced Tabs Interface -->\n"
        "        <div class=\"tabs\">\n"
        "            <div class=\"tab active\" onclick=\"switchTab('logs')\">📋 Logs</div>\n"
        "            <div class=\"tab\" onclick=\"switchTab('shell')\">💻 Remote Shell</div>\n"
        "            <div class=\"tab\" onclick=\"switchTab('archive')\">📦 Log Archive</div>\n"
        "        </div>\n"
        "        \n"
        "        <!-- Logs Tab -->\n"
        "        <div id=\"logs-tab\" class=\"tab-content active\">\n"
        "            <div class=\"info\">\n"
        "                <strong>Daemon Logs (Last 50 lines):</strong>\n"
        "                <button class=\"btn-primary\" onclick=\"loadLogs()\">🔄 Refresh</button>\n"
        "                <button class=\"btn-success\" onclick=\"downloadLogs()\">💾 Download</button>\n"
        "            </div>\n"
        "            <div id=\"logs\" class=\"logs\">Click 'Refresh' to load logs...</div>\n"
        "        </div>\n"
        "        \n"
        "        <!-- Remote Shell Tab -->\n"
        "        <div id=\"shell-tab\" class=\"tab-content\">\n"
        "            <div class=\"shell-section\">\n"
        "                <h3>🔧 Remote Shell Interface</h3>\n"
        "                <p>Execute commands on the server (be careful!):</p>\n"
        "                <input type=\"text\" id=\"shellInput\" class=\"shell-input\" placeholder=\"Enter command (e.g., 'ls -la', 'ps aux', 'uptime')\" onkeypress=\"handleShellEnter(event)\">\n"
        "                <button class=\"btn-primary\" onclick=\"executeShellCommand()\">Execute</button>\n"
        "                <button class=\"btn-primary\" onclick=\"clearShellOutput()\">Clear</button>\n"
        "                <div id=\"shellOutput\" class=\"shell-output\">Welcome to Matt Daemon Remote Shell\\nType commands and press Execute or Enter\\n\\n</div>\n"
        "            </div>\n"
        "        </div>\n"
        "        \n"
        "        <!-- Archive Tab -->\n"
        "        <div id=\"archive-tab\" class=\"tab-content\">\n"
        "            <div class=\"info\">\n"
        "                <h3>📦 Advanced Log Archival</h3>\n"
        "                <p>Manage and archive daemon logs:</p>\n"
        "                <button class=\"btn-primary\" onclick=\"archiveLogs()\">📦 Create Archive</button>\n"
        "                <button class=\"btn-primary\" onclick=\"getLogStats()\">📊 Get Statistics</button>\n"
        "                <button class=\"btn-success\" onclick=\"cleanOldLogs()\">🧹 Clean Old Logs</button>\n"
        "            </div>\n"
        "            <div id=\"archiveOutput\" class=\"logs\">Archive operations will appear here...</div>\n"
        "        </div>\n"
        "        \n"
        "        <script>\n"
        "            function switchTab(tabName) {\n"
        "                // Hide all tabs\n"
        "                document.querySelectorAll('.tab-content').forEach(tab => tab.classList.remove('active'));\n"
        "                document.querySelectorAll('.tab').forEach(tab => tab.classList.remove('active'));\n"
        "                \n"
        "                // Show selected tab\n"
        "                document.getElementById(tabName + '-tab').classList.add('active');\n"
        "                event.target.classList.add('active');\n"
        "            }\n"
        "            \n"
        "            function stopDaemon() {\n"
        "                const button = document.querySelector('button');\n"
        "                button.disabled = true;\n"
        "                button.textContent = 'Stopping...';\n"
        "                \n"
        "                fetch('/stop', { method: 'POST' })\n"
        "                .then(response => response.text())\n"
        "                .then(data => {\n"
        "                    document.getElementById('message').innerHTML = '<div class=\"info\">Daemon stop signal sent. The daemon is shutting down...</div>';\n"
        "                    setTimeout(() => {\n"
        "                        document.querySelector('.status').className = 'status stopped';\n"
        "                        document.querySelector('.status').innerHTML = '🔴 Daemon Stopped';\n"
        "                        button.textContent = 'Daemon Stopped';\n"
        "                    }, 1000);\n"
        "                })\n"
        "                .catch(error => {\n"
        "                    document.getElementById('message').innerHTML = '<div class=\"info\">Error: ' + error + '</div>';\n"
        "                    button.disabled = false;\n"
        "                    button.textContent = 'Stop Daemon';\n"
        "                });\n"
        "            }\n"
        "            \n"
        "            function loadLogs() {\n"
        "                fetch('/logs', { method: 'GET' })\n"
        "                .then(response => response.text())\n"
        "                .then(data => {\n"
        "                    document.getElementById('logs').textContent = data;\n"
        "                    document.getElementById('logs').scrollTop = document.getElementById('logs').scrollHeight;\n"
        "                })\n"
        "                .catch(error => {\n"
        "                    document.getElementById('logs').textContent = 'Error loading logs: ' + error;\n"
        "                });\n"
        "            }\n"
        "            \n"
        "            function downloadLogs() {\n"
        "                fetch('/logs')\n"
        "                .then(response => response.text())\n"
        "                .then(data => {\n"
        "                    const blob = new Blob([data], { type: 'text/plain' });\n"
        "                    const url = window.URL.createObjectURL(blob);\n"
        "                    const a = document.createElement('a');\n"
        "                    a.href = url;\n"
        "                    a.download = 'matt_daemon_logs_' + new Date().toISOString().slice(0,19).replace(/:/g, '-') + '.txt';\n"
        "                    a.click();\n"
        "                    window.URL.revokeObjectURL(url);\n"
        "                });\n"
        "            }\n"
        "            \n"
        "            function executeShellCommand() {\n"
        "                const command = document.getElementById('shellInput').value;\n"
        "                if (!command.trim()) return;\n"
        "                \n"
        "                const output = document.getElementById('shellOutput');\n"
        "                output.textContent += '$ ' + command + '\\n';\n"
        "                \n"
        "                fetch('/shell', {\n"
        "                    method: 'POST',\n"
        "                    headers: { 'Content-Type': 'application/json' },\n"
        "                    body: JSON.stringify({ command: command })\n"
        "                })\n"
        "                .then(response => response.text())\n"
        "                .then(data => {\n"
        "                    output.textContent += data + '\\n\\n';\n"
        "                    output.scrollTop = output.scrollHeight;\n"
        "                    document.getElementById('shellInput').value = '';\n"
        "                })\n"
        "                .catch(error => {\n"
        "                    output.textContent += 'Error: ' + error + '\\n\\n';\n"
        "                });\n"
        "            }\n"
        "            \n"
        "            function handleShellEnter(event) {\n"
        "                if (event.key === 'Enter') {\n"
        "                    executeShellCommand();\n"
        "                }\n"
        "            }\n"
        "            \n"
        "            function clearShellOutput() {\n"
        "                document.getElementById('shellOutput').textContent = 'Shell cleared.\\n\\n';\n"
        "            }\n"
        "            \n"
        "            function archiveLogs() {\n"
        "                fetch('/archive', { method: 'POST' })\n"
        "                .then(response => response.text())\n"
        "                .then(data => {\n"
        "                    document.getElementById('archiveOutput').textContent = data;\n"
        "                })\n"
        "                .catch(error => {\n"
        "                    document.getElementById('archiveOutput').textContent = 'Error: ' + error;\n"
        "                });\n"
        "            }\n"
        "            \n"
        "            function getLogStats() {\n"
        "                fetch('/stats', { method: 'GET' })\n"
        "                .then(response => response.text())\n"
        "                .then(data => {\n"
        "                    document.getElementById('archiveOutput').textContent = data;\n"
        "                })\n"
        "                .catch(error => {\n"
        "                    document.getElementById('archiveOutput').textContent = 'Error: ' + error;\n"
        "                });\n"
        "            }\n"
        "            \n"
        "            function cleanOldLogs() {\n"
        "                if (confirm('Are you sure you want to clean old log entries? This action cannot be undone.')) {\n"
        "                    fetch('/clean', { method: 'POST' })\n"
        "                    .then(response => response.text())\n"
        "                    .then(data => {\n"
        "                        document.getElementById('archiveOutput').textContent = data;\n"
        "                    })\n"
        "                    .catch(error => {\n"
        "                        document.getElementById('archiveOutput').textContent = 'Error: ' + error;\n"
        "                    });\n"
        "                }\n"
        "            }\n"
        "            \n"
        "            // Auto-load logs on page load\n"
        "            window.onload = function() {\n"
        "                loadLogs();\n"
        "                // Auto-refresh logs every 10 seconds\n"
        "                setInterval(loadLogs, 10000);\n"
        "            }\n"
        "        </script>\n"
        "    </div>\n"
        "</body>\n"
        "</html>";
    
    return html;
}

bool is_http_request(const std::string& data) {
    return data.find("GET ") == 0 || data.find("POST ") == 0;
}

std::string read_log_file() {
    std::ifstream log_file(LOG_FILE);
    std::string logs;
    std::string line;
    
    if (!log_file.is_open()) {
        return "Error: Could not open log file";
    }
    
    // Read last 50 lines (or all if less than 50)
    std::vector<std::string> lines;
    while (std::getline(log_file, line)) {
        lines.push_back(line);
    }
    
    // Keep only last 50 lines
    size_t start = lines.size() > 50 ? lines.size() - 50 : 0;
    for (size_t i = start; i < lines.size(); ++i) {
        logs += lines[i] + "\n";
    }
    
    log_file.close();
    return logs;
}

std::string execute_shell_command(const std::string& command) {
    // Basic security check - only allow safe commands
    if (command.find("rm") != std::string::npos && command.find("-rf") != std::string::npos) {
        return "Error: Dangerous command blocked for security";
    }
    if (command.find("sudo") != std::string::npos || command.find("su ") != std::string::npos) {
        return "Error: Privilege escalation commands blocked";
    }
    
    std::string safe_command = command + " 2>&1"; // Capture both stdout and stderr
    FILE* pipe = popen(safe_command.c_str(), "r");
    if (!pipe) {
        return "Error: Could not execute command";
    }
    
    std::string result;
    char buffer[128];
    while (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
        result += buffer;
    }
    
    int status = pclose(pipe);
    if (status != 0) {
        result += "\nCommand exited with status: " + std::to_string(status);
    }
    
    return result.empty() ? "Command executed successfully (no output)" : result;
}

std::string get_log_statistics() {
    std::ifstream log_file(LOG_FILE);
    if (!log_file.is_open()) {
        return "Error: Could not open log file for statistics";
    }
    
    std::string line;
    int total_lines = 0;
    int info_count = 0;
    int error_count = 0;
    int warning_count = 0;
    int log_count = 0;
    
    while (std::getline(log_file, line)) {
        total_lines++;
        if (line.find("[ INFO ]") != std::string::npos) info_count++;
        else if (line.find("[ ERROR ]") != std::string::npos) error_count++;
        else if (line.find("[ WARNING ]") != std::string::npos) warning_count++;
        else if (line.find("[ LOG ]") != std::string::npos) log_count++;
    }
    
    log_file.close();
    
    std::string stats = "📊 Log File Statistics:\n";
    stats += "================================\n";
    stats += "Total lines: " + std::to_string(total_lines) + "\n";
    stats += "INFO messages: " + std::to_string(info_count) + "\n";
    stats += "ERROR messages: " + std::to_string(error_count) + "\n";
    stats += "WARNING messages: " + std::to_string(warning_count) + "\n";
    stats += "LOG messages: " + std::to_string(log_count) + "\n";
    stats += "================================\n";
    stats += "File: " + LOG_FILE + "\n";
    
    return stats;
}

std::string archive_logs() {
    std::string timestamp = std::to_string(time(nullptr));
    std::string archive_name = "/tmp/matt_daemon_archive_" + timestamp + ".log";
    
    std::ifstream src(LOG_FILE);
    std::ofstream dst(archive_name);
    
    if (!src.is_open() || !dst.is_open()) {
        return "Error: Could not create archive";
    }
    
    dst << src.rdbuf();
    src.close();
    dst.close();
    
    std::string result = "📦 Archive created successfully!\n";
    result += "Archive file: " + archive_name + "\n";
    result += "Original file: " + LOG_FILE + "\n";
    result += "Timestamp: " + timestamp + "\n";
    
    return result;
}

std::string clean_old_logs() {
    std::ifstream log_file(LOG_FILE);
    if (!log_file.is_open()) {
        return "Error: Could not open log file for cleaning";
    }
    
    std::vector<std::string> lines;
    std::string line;
    while (std::getline(log_file, line)) {
        lines.push_back(line);
    }
    log_file.close();
    
    // Keep only last 100 lines
    size_t original_count = lines.size();
    if (lines.size() > 100) {
        std::ofstream out_file(LOG_FILE);
        size_t start = lines.size() - 100;
        for (size_t i = start; i < lines.size(); ++i) {
            out_file << lines[i] << "\n";
        }
        out_file.close();
    }
    
    std::string result = "🧹 Log cleaning completed!\n";
    result += "Original lines: " + std::to_string(original_count) + "\n";
    result += "Remaining lines: " + std::to_string(std::min(original_count, (size_t)100)) + "\n";
    result += "Removed lines: " + std::to_string(original_count > 100 ? original_count - 100 : 0) + "\n";
    
    return result;
}

std::string extract_json_command(const std::string& request) {
    size_t body_start = request.find("\r\n\r\n");
    if (body_start == std::string::npos) {
        return "";
    }
    
    std::string body = request.substr(body_start + 4);
    size_t cmd_start = body.find("\"command\":\"");
    if (cmd_start == std::string::npos) {
        return "";
    }
    
    cmd_start += 11; // Length of "command":""
    size_t cmd_end = body.find("\"", cmd_start);
    if (cmd_end == std::string::npos) {
        return "";
    }
    
    return body.substr(cmd_start, cmd_end - cmd_start);
}

std::string extract_json_password(const std::string& request) {
    size_t body_start = request.find("\r\n\r\n");
    if (body_start == std::string::npos) {
        return "";
    }
    
    std::string body = request.substr(body_start + 4);
    size_t pwd_start = body.find("\"password\":\"");
    if (pwd_start == std::string::npos) {
        return "";
    }
    
    pwd_start += 12; // Length of "password":""
    size_t pwd_end = body.find("\"", pwd_start);
    if (pwd_end == std::string::npos) {
        return "";
    }
    
    return body.substr(pwd_start, pwd_end - pwd_start);
}

std::string extract_session_token(const std::string& request) {
    // Look for session token in cookie or header
    size_t auth_pos = request.find("matt_daemon_session=");
    if (auth_pos == std::string::npos) {
        return "";
    }
    
    auth_pos += 20; // Length of "matt_daemon_session="
    size_t end_pos = request.find_first_of(" ;\r\n", auth_pos);
    if (end_pos == std::string::npos) {
        end_pos = request.length();
    }
    
    return request.substr(auth_pos, end_pos - auth_pos);
}

void handle_http_request(int client_fd, const std::string& request) {
    std::string response;
    
    // Check authentication for protected routes
    bool is_protected_route = (request.find("GET /dashboard") == 0 || 
                              request.find("GET /logs") == 0 || 
                              request.find("POST /shell") == 0 ||
                              request.find("GET /stats") == 0 ||
                              request.find("POST /archive") == 0 ||
                              request.find("POST /clean") == 0 ||
                              request.find("POST /stop") == 0);
                              
    if (is_protected_route) {
        std::string session_token = extract_session_token(request);
        if (!is_valid_session(session_token)) {
            // Redirect to login page
            response = "HTTP/1.1 302 Found\r\n";
            response += "Location: /\r\n";
            response += "Connection: close\r\n\r\n";
            send(client_fd, response.c_str(), response.length(), 0);
            return;
        }
    }
    
    if (request.find("GET / ") == 0 || request.find("GET /login") == 0) {
        // Show login page
        std::string page = get_login_page();
        response = "HTTP/1.1 200 OK\r\n";
        response += "Content-Type: text/html\r\n";
        response += "Content-Length: " + std::to_string(page.length()) + "\r\n";
        response += "Connection: close\r\n\r\n";
        response += page;
    }
    else if (request.find("POST /auth") == 0) {
        // Handle authentication
        std::string password = extract_json_password(request);
        
        if (!has_root_privileges()) {
            std::string json_response = "{\"success\": false, \"error\": \"Root privileges required\"}";
            response = "HTTP/1.1 403 Forbidden\r\n";
            response += "Content-Type: application/json\r\n";
            response += "Content-Length: " + std::to_string(json_response.length()) + "\r\n";
            response += "Connection: close\r\n\r\n";
            response += json_response;
        }
        else if (verify_password(password)) {
            // Generate session token
            std::string token = generate_session_token();
            active_sessions.push_back(token);
            
            std::string json_response = "{\"success\": true, \"token\": \"" + token + "\"}";
            response = "HTTP/1.1 200 OK\r\n";
            response += "Content-Type: application/json\r\n";
            response += "Set-Cookie: matt_daemon_session=" + token + "; Path=/; HttpOnly\r\n";
            response += "Content-Length: " + std::to_string(json_response.length()) + "\r\n";
            response += "Connection: close\r\n\r\n";
            response += json_response;
            
            if (global_logger) {
                global_logger->info("Matt_daemon: User authenticated successfully");
            }
        } else {
            std::string json_response = "{\"success\": false, \"error\": \"Invalid password\"}";
            response = "HTTP/1.1 401 Unauthorized\r\n";
            response += "Content-Type: application/json\r\n";
            response += "Content-Length: " + std::to_string(json_response.length()) + "\r\n";
            response += "Connection: close\r\n\r\n";
            response += json_response;
            
            if (global_logger) {
                global_logger->warning("Matt_daemon: Failed authentication attempt");
            }
        }
    }
    else if (request.find("GET /dashboard") == 0) {
        // Show main dashboard (protected)
        std::string page = get_web_page();
        response = "HTTP/1.1 200 OK\r\n";
        response += "Content-Type: text/html\r\n";
        response += "Content-Length: " + std::to_string(page.length()) + "\r\n";
        response += "Connection: close\r\n\r\n";
        response += page;
    }
    else if (request.find("GET /logs") == 0) {
        std::string logs = read_log_file();
        response = "HTTP/1.1 200 OK\r\n";
        response += "Content-Type: text/plain\r\n";
        response += "Content-Length: " + std::to_string(logs.length()) + "\r\n";
        response += "Connection: close\r\n\r\n";
        response += logs;
    }
    else if (request.find("POST /shell") == 0) {
        std::string command = extract_json_command(request);
        std::string result = execute_shell_command(command);
        
        response = "HTTP/1.1 200 OK\r\n";
        response += "Content-Type: text/plain\r\n";
        response += "Content-Length: " + std::to_string(result.length()) + "\r\n";
        response += "Connection: close\r\n\r\n";
        response += result;
        
        if (global_logger) {
            global_logger->log("Matt_daemon: Shell command executed: " + command);
        }
    }
    else if (request.find("GET /stats") == 0) {
        std::string stats = get_log_statistics();
        response = "HTTP/1.1 200 OK\r\n";
        response += "Content-Type: text/plain\r\n";
        response += "Content-Length: " + std::to_string(stats.length()) + "\r\n";
        response += "Connection: close\r\n\r\n";
        response += stats;
    }
    else if (request.find("POST /archive") == 0) {
        std::string archive_result = archive_logs();
        response = "HTTP/1.1 200 OK\r\n";
        response += "Content-Type: text/plain\r\n";
        response += "Content-Length: " + std::to_string(archive_result.length()) + "\r\n";
        response += "Connection: close\r\n\r\n";
        response += archive_result;
        
        if (global_logger) {
            global_logger->info("Matt_daemon: Log archive created via web interface.");
        }
    }
    else if (request.find("POST /clean") == 0) {
        std::string clean_result = clean_old_logs();
        response = "HTTP/1.1 200 OK\r\n";
        response += "Content-Type: text/plain\r\n";
        response += "Content-Length: " + std::to_string(clean_result.length()) + "\r\n";
        response += "Connection: close\r\n\r\n";
        response += clean_result;
        
        if (global_logger) {
            global_logger->info("Matt_daemon: Log cleaning performed via web interface.");
        }
    }
    else if (request.find("POST /stop") == 0) {
        response = "HTTP/1.1 200 OK\r\n";
        response += "Content-Type: text/plain\r\n";
        response += "Content-Length: 20\r\n";
        response += "Connection: close\r\n\r\n";
        response += "Daemon stopping...";
        
        if (global_logger) {
            global_logger->info("Matt_daemon: Web interface requested shutdown.");
        }
        daemon_running = false;
    }
    else {
        response = "HTTP/1.1 404 Not Found\r\n";
        response += "Content-Type: text/plain\r\n";
        response += "Content-Length: 9\r\n";
        response += "Connection: close\r\n\r\n";
        response += "Not Found";
    }
    
    send(client_fd, response.c_str(), response.length(), 0);
}

bool check_lock_file() {
    std::ifstream file(LOCK_FILE);
    std::cout << " I am checking if " << LOCK_FILE << " exists" << std::endl;
    if (file.is_open()) {
        std::cout << "Lock file exists: " << LOCK_FILE << std::endl;
        int pid;
        file >> pid;
        file.close();
        if (kill(pid, 0) == 0) {
            std::cerr << "Can't open: " << LOCK_FILE << std::endl;
            return false;
        } else {
            unlink(LOCK_FILE.c_str());
            return true;
        }
    }
    std::cout << "Lock file does not exist: " << LOCK_FILE << std::endl;
    return true;
}

void create_lock_file() {
    std::ofstream file(LOCK_FILE);
    std::cout << " I am creating " << LOCK_FILE << std::endl;
    if (file.is_open()) {
        std::cout << "Creating lock file: " << LOCK_FILE << " with PID: " << getpid() << " is created" << std::endl;
        file << getpid() << std::endl;
        file.close();
    } else {
        std::cerr << "Can't open: " << LOCK_FILE << std::endl;
    }
}

void remove_lock_file() {
    std::cout << "Removing lock file: " << LOCK_FILE << std::endl;
    unlink(LOCK_FILE.c_str());
}

void signal_handler(int signal) {
    if (global_logger) {
        global_logger->info("Matt_daemon: Signal handler.");
        global_logger->info("Matt_daemon: Quitting.");
    }
    daemon_running = false;
    
    remove_lock_file();
    
    (void)signal;
    exit(0);  // Force exit to ensure cleanup
}

int main() {
    std::cout << "Step 1: Checking if we can start..." << std::endl;
    if (!check_lock_file()) {
        std::cout << "Cannot start - another instance is running" << std::endl;
        return 1;
    }
    
    std::cout << "Step 2: Creating lock file..." << std::endl;
    create_lock_file();
    
    std::cout << "Step 3: Verifying lock file was created..." << std::endl;
    std::ifstream verify_file(LOCK_FILE);
    if (verify_file.is_open()) {
        std::cout << "Lock file successfully created!" << std::endl;
        verify_file.close();
    } else {
        std::cout << "ERROR: Lock file was not created!" << std::endl;
        return 1;
    }
    
    global_logger = new Tintin_reporter(LOG_FILE, true);
    global_logger->info("Matt_daemon: Started.");
    
    pid_t pid = fork();
    
    if (pid < 0) {
        global_logger->error("Matt_daemon: Fork failed");
        remove_lock_file();
        delete global_logger;
        return 1;
    }
    
    if (pid > 0) {
        std::cout << "Socket daemon started with PID: " << pid << std::endl;
        std::cout << "Web interface available at: http://localhost:8080" << std::endl;
        std::cout << "Open your browser and navigate to the above URL to control the daemon." << std::endl;
        delete global_logger;
        exit(0);
    }
    
    atexit(remove_lock_file);
    
    setsid();
    close(0); 
    close(1);
    close(2); 
    
    std::ofstream update_lock(LOCK_FILE);
    if (update_lock.is_open()) {
        update_lock << getpid() << std::endl;
        update_lock.close();
    }
    
    delete global_logger;
    global_logger = new Tintin_reporter(LOG_FILE);
    global_logger->info("Matt_daemon: Creating server.");
    
    signal(SIGTERM, signal_handler);
    signal(SIGINT, signal_handler);  
    signal(SIGQUIT, signal_handler);
    signal(SIGHUP, signal_handler);
    
    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) {
        global_logger->error("Matt_daemon: Socket creation failed");
        remove_lock_file();
        delete global_logger;
        return 1;
    }
    
    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)); 
    
    struct sockaddr_in address;
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(8080); 
    
    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
        global_logger->error("Matt_daemon: Bind failed");
        close(server_fd);
        remove_lock_file();
        delete global_logger;
        return 1;
    }
    
    if (listen(server_fd, 10) < 0) {
        global_logger->error("Matt_daemon: Listen failed");
        close(server_fd);
        remove_lock_file();
        delete global_logger;
        return 1;
    }
    
    global_logger->info("Matt_daemon: Server created.");
    global_logger->info("Matt_daemon: Entering Daemon mode.");
    global_logger->info("Matt_daemon: started. PID: " + std::to_string(getpid()) + ".");
    global_logger->info("Matt_daemon: Web interface available at http://localhost:8080");
    
    int client_fds[MAX_CONNECTIONS];
    for (int i = 0; i < MAX_CONNECTIONS; i++) {
        client_fds[i] = -1; // -1 means empty slot
    }
        
    fd_set read_fds, master_fds;
    FD_ZERO(&master_fds);
    FD_SET(server_fd, &master_fds);
    int max_fd = server_fd;
    
    while (daemon_running) {
        read_fds = master_fds;
        
        if (select(max_fd + 1, &read_fds, NULL, NULL, NULL) < 0) {
            if (daemon_running) {
                global_logger->error("Matt_daemon: Select failed");
            }
            continue;
        }
        
        if (FD_ISSET(server_fd, &read_fds)) {
            struct sockaddr_in client_address;
            socklen_t client_len = sizeof(client_address);
            int new_client = accept(server_fd, (struct sockaddr *)&client_address, &client_len);
            
            if (new_client < 0) {
                global_logger->error("Matt_daemon: Accept failed");
            } else {
                int slot = -1;
                for (int i = 0; i < MAX_CONNECTIONS; i++) {
                    if (client_fds[i] == -1) {
                        slot = i;
                        break;
                    }
                }
                
                if (slot != -1) {
                    client_fds[slot] = new_client;
                    FD_SET(new_client, &master_fds);
                    max_fd = std::max(max_fd, new_client);
                    global_logger->info("Matt_daemon: Client connected, slot " + std::to_string(slot));
                } else {
                    global_logger->warning("Matt_daemon: Maximum connections reached, rejecting client");
                    close(new_client);
                }
            }
        }
        
        for (int i = 0; i < MAX_CONNECTIONS; i++) {
            if (client_fds[i] != -1 && FD_ISSET(client_fds[i], &read_fds)) {
                char buffer[1024];
                memset(buffer, 0, sizeof(buffer));
                int bytes_read = recv(client_fds[i], buffer, sizeof(buffer) - 1, 0);
                
                if (bytes_read <= 0) {
                    global_logger->info("Matt_daemon: Client disconnected from slot " + std::to_string(i));
                    FD_CLR(client_fds[i], &master_fds);
                    close(client_fds[i]);
                    client_fds[i] = -1;
                } else {
                    std::string message(buffer, bytes_read);
                    
                    if (is_http_request(message)) {
                        global_logger->info("Matt_daemon: HTTP request received");
                        handle_http_request(client_fds[i], message);
                        FD_CLR(client_fds[i], &master_fds);
                        close(client_fds[i]);
                        client_fds[i] = -1;
                    } else {
                        if (!message.empty() && message.back() == '\n') {
                            message.pop_back();
                        }
                        
                        if (message == "quit") {
                            global_logger->info("Matt_daemon: Request quit.");
                            global_logger->info("Matt_daemon: Quitting.");
                            
                            for (int j = 0; j < MAX_CONNECTIONS; j++) {
                                if (client_fds[j] != -1) {
                                    send(client_fds[j], "Daemon shutting down\n", 21, 0);
                                    close(client_fds[j]);
                                }
                            }
                            
                            daemon_running = false;
                            break;
                        } else {
                            global_logger->log("Matt_daemon: User input: " + message);
                        }
                    }
                }
            }
        }
    }
    
    global_logger->info("Matt_daemon: Quitting.");
    
    for (int i = 0; i < MAX_CONNECTIONS; i++) {
        if (client_fds[i] != -1) {
            close(client_fds[i]);
        }
    }
    
    close(server_fd);
    remove_lock_file();
    delete global_logger;
    return 0;
}