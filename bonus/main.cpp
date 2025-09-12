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
#include <errno.h>
#include <sys/stat.h>
#include "Tintin_reporter.hpp"
#include "RemoteShell.hpp"
#include "Auth.hpp"
// For parsing HTTP GET query
#include <sstream>
#include <map>
#include "Pages.hpp"

std::string get_last_n_lines(const std::string& filename, int n) {
    std::ifstream file(filename);
    if (!file.is_open()) return "Could not open log file.";
    std::vector<std::string> lines;
    std::string line;
    while (std::getline(file, line)) {
        lines.push_back(line);
    }
    std::string result;
    int start = std::max(0, (int)lines.size() - n);
    for (int i = start; i < (int)lines.size(); ++i) {
        result += lines[i] + "\n";
    }
    return result;
}
bool extract_credentials(const std::string& message, std::string& username, std::string& password) {
    // Look for /(username)&(password) for example localhost:8080/anas&1234 or localhost:8080/ahmed&ahmed1234
    
    // Find the start of the path after GET 
    std::size_t get_pos = message.find("GET /");
    if (get_pos == std::string::npos)
        return false;
    
    std::size_t path_start = get_pos + 5; // after 'GET /'
    std::size_t path_end = message.find(" ", path_start);
    if (path_end == std::string::npos)
        return false;
    
    std::string path = message.substr(path_start, path_end - path_start);
    
    // Look for the pattern username&password
    std::size_t amp_pos = path.find('&');
    if (amp_pos == std::string::npos || amp_pos == 0 || amp_pos == path.length() - 1)
        return false;
    
    username = path.substr(0, amp_pos);
    password = path.substr(amp_pos + 1);
    
    // Check if both username and password are not empty
    if (username.empty() || password.empty())
        return false;
    
    // Validate credentials
    if (username == "1337" && password == "ad123")
        return true;
    
    return false;
}

const int MAX_CONNECTIONS = 3;
const std::string LOCK_FILE = "/var/lock/matt_daemon.lock";
const std::string LOG_FILE = "/var/log/matt_daemon/matt_daemon.log";
bool daemon_running = true;
Tintin_reporter* global_logger = nullptr;

void create_directories() {
    // Create lock directory
    mkdir("/var/lock/matt_daemon", 0755);
    
    // Create log directory structure
    mkdir("/var/log", 0755);
    mkdir("/var/log/matt_daemon", 0755);
}

bool check_lock_file() {
    std::ifstream file(LOCK_FILE);
    if (file.is_open()) {
        int pid;
        file >> pid;
        file.close();
        

        if (kill(pid, 0) == 0) {
            std::cerr << "Can't open :" << LOCK_FILE << std::endl;
            return false;
        } else {
            unlink(LOCK_FILE.c_str());
            return true;
        }
    }
    return true;
}

void create_lock_file() {
    create_directories(); // Ensure directories exist
    std::ofstream file(LOCK_FILE);
    if (file.is_open()) {
        file << getpid() << std::endl;
        file.close();
    } else {
        std::cerr << "Can't create lock file: " << LOCK_FILE << std::endl;
        exit(1);
    }
}

void remove_lock_file() {
    unlink(LOCK_FILE.c_str());
}

void signal_handler(int signal) {
    if (global_logger) {
        global_logger->info("Matt_daemon: Signal handler.");
        global_logger->info("Matt_daemon: Quitting.");
    }
    daemon_running = false;
    
    // Remove lock file when signal is received
    remove_lock_file();
    
    (void)signal;
}
int main() {
    if (getuid() != 0) {
        std::cerr << "Matt_daemon must be run as root" << std::endl;
        return 1;
    }

    create_directories();
    
    Auth auth;
    if (!check_lock_file()) {
        return 1;
    }
    
    global_logger = new Tintin_reporter(LOG_FILE, true);
    global_logger->info("Matt_daemon: Started.");
    
    pid_t pid = fork();
    
    if (pid < 0) {
        global_logger->error("Matt_daemon: Fork failed");
        delete global_logger;
        return 1;
    }
    
    if (pid > 0) {
        std::cout << "Socket daemon started with PID: " << pid << std::endl;
        delete global_logger;
        exit(0);
    }
    
    atexit(remove_lock_file);
    create_lock_file();
    
    setsid();
    close(0); 
    close(1);
    close(2); 
    
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
    
    int client_fds[MAX_CONNECTIONS];
    for (int i = 0; i < MAX_CONNECTIONS; i++) {
        client_fds[i] = -1;
    }
        
    fd_set read_fds, master_fds;
    FD_ZERO(&master_fds);
    FD_SET(server_fd, &master_fds);
    int max_fd = server_fd;
    
    while (daemon_running) {
        read_fds = master_fds;
        
        if (select(max_fd + 1, &read_fds, NULL, NULL, NULL) < 0) {
            if (errno == EINTR) {
                continue;
            }
            if (daemon_running) {
                global_logger->error("Matt_daemon: Select failed");
            }
            continue;
        }
        
        
        if (FD_ISSET(server_fd, &read_fds)) {
            struct sockaddr_in client_address;
            socklen_t client_len = sizeof(client_address);
            int new_client = accept(server_fd, (struct sockaddr *)&client_address, &client_len);
           
            global_logger->clientAuthStatus[new_client] = false;
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
                    
                    
                    if (!message.empty() && message.back() == '\n') {
                        message.pop_back();
                    }

         
                    if (message.find("HTTP/") != std::string::npos) {
    std::string username, password;
    
    if (extract_credentials(message, username, password)) {
        global_logger->info("Matt_daemon: Browser login attempt from " + std::to_string(i));
        
        std::string response = render_success_page(username, i, get_last_n_lines(LOG_FILE, 20));;
        
        send(client_fds[i], response.c_str(), response.length(), 0);
    }
    else {
        global_logger->info("Matt_daemon: Browser failed login attempt from " + std::to_string(i));
        std::string response = render_failure_page();

        send(client_fds[i], response.c_str(), response.length(), 0);
    }
    
    FD_CLR(client_fds[i], &master_fds);
    close(client_fds[i]);
    client_fds[i] = -1;
} else {

                        auth.start(message, global_logger->clientAuthStatus, client_fds[i]);
                        if (message == "quit") {
                            global_logger->info("Matt_daemon: Client requested quit from slot " + std::to_string(i));
                            FD_CLR(client_fds[i], &master_fds);
                            close(client_fds[i]);
                            client_fds[i] = -1;

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