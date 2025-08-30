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
#include "Tintin_reporter.hpp"
#include "RemoteShell.hpp"
#include "Auth.hpp"

const int MAX_CONNECTIONS = 3;
const std::string LOCK_FILE = "/tmp/matt_daemon.lock";
const std::string LOG_FILE = "/tmp/matt_daemon.log";
bool daemon_running = true;
Tintin_reporter* global_logger = nullptr;

bool check_lock_file() {
    std::ifstream file(LOCK_FILE);
    if (file.is_open()) {
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
    return true;
}

void create_lock_file() {
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
    // Check if another daemon is already running
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
        // Parent process - just exit without removing lock file
        std::cout << "Socket daemon started with PID: " << pid << std::endl;
        delete global_logger;
        exit(0);
    }
    
    // Child process becomes daemon - now create lock file with daemon PID
    
    // Setup exit handler for daemon process only
    atexit(remove_lock_file);
    create_lock_file();
    
    // Child process becomes daemon
    setsid();
    close(0); 
    close(1);
    close(2); 
    
    // Recreate logger for daemon process
    delete global_logger;
    global_logger = new Tintin_reporter(LOG_FILE);
    global_logger->info("Matt_daemon: Creating server.");
    
    // Setup signal handlers
    signal(SIGTERM, signal_handler);
    signal(SIGINT, signal_handler);  
    signal(SIGQUIT, signal_handler);
    signal(SIGHUP, signal_handler);
    
    // Create socket
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
    
    // Bind socket
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
        client_fds[i] = -1; // -1 means empty slot
    }
        
    fd_set read_fds, master_fds;
    FD_ZERO(&master_fds);
    FD_SET(server_fd, &master_fds);
    int max_fd = server_fd;
    
    // Main daemon loop
    while (daemon_running) {
        read_fds = master_fds;
        
        if (select(max_fd + 1, &read_fds, NULL, NULL, NULL) < 0) {
            if (errno == EINTR) {
                // Interrupted by signal, continue
                continue;
            }
            if (daemon_running) {
                global_logger->error("Matt_daemon: Select failed");
            }
            continue;
        }
        
        // Check for new connections
        if (FD_ISSET(server_fd, &read_fds)) {
            struct sockaddr_in client_address;
            socklen_t client_len = sizeof(client_address);
            int new_client = accept(server_fd, (struct sockaddr *)&client_address, &client_len);
            std::string loginPrompt = "Matt_daemon Authentication Required\n";
            loginPrompt += "Please login to continue.\n";
            loginPrompt += "Usage: LOGIN <username> <password>\n";
            loginPrompt += "Type HELP for more information\n\n";

            if(send(new_client, loginPrompt.c_str(), loginPrompt.length(), 0) < 0) {
                perror("send");
            }
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
                    // Accept the connection
                    client_fds[slot] = new_client;
                    FD_SET(new_client, &master_fds);
                    max_fd = std::max(max_fd, new_client);
                    global_logger->info("Matt_daemon: Client connected, slot " + std::to_string(slot));
                } else {
                    // No available slots, reject connection
                    global_logger->warning("Matt_daemon: Maximum connections reached, rejecting client");
                    close(new_client);
                }
            }
        }
        
        // Check existing client connections for data
        for (int i = 0; i < MAX_CONNECTIONS; i++) {
            if (client_fds[i] != -1 && FD_ISSET(client_fds[i], &read_fds)) {
                char buffer[1024];
                memset(buffer, 0, sizeof(buffer));
                int bytes_read = recv(client_fds[i], buffer, sizeof(buffer) - 1, 0);
                
                if (bytes_read <= 0) {
                    // Client disconnected
                    global_logger->info("Matt_daemon: Client disconnected from slot " + std::to_string(i));
                    FD_CLR(client_fds[i], &master_fds);
                    close(client_fds[i]);
                    client_fds[i] = -1;
                } else {
                    // Process received data
                    std::string message(buffer, bytes_read);
                    
                    // Remove trailing newline if present
                    if (!message.empty() && message.back() == '\n') {
                        message.pop_back();
                    }
                    auth.start(message, global_logger->clientAuthStatus, client_fds[i]);
                    if (message == "quit") {
                        global_logger->info("Matt_daemon: Request quit.");
                        global_logger->info("Matt_daemon: Quitting.");
                        
                        // Notify all connected clients before shutting down
                        for (int j = 0; j < MAX_CONNECTIONS; j++) {
                            if (client_fds[j] != -1) {
                                send(client_fds[j], "Daemon shutting down\n", 21, 0);
                                close(client_fds[j]);
                            }
                        }
                        
                        daemon_running = false;
                        break; // Exit the client loop
                    } else {
                        // Log user input using LOG level
                        global_logger->log("Matt_daemon: User input: " + message);
                    }
                }
            }
        }
    }
    
    // Cleanup when shutting down
    global_logger->info("Matt_daemon: Quitting.");
    
    // Close all client connections
    for (int i = 0; i < MAX_CONNECTIONS; i++) {
        if (client_fds[i] != -1) {
            close(client_fds[i]);
        }
    }
    
    close(server_fd);
    
    // Remove lock file
    remove_lock_file();
    
    // Cleanup logger
    delete global_logger;
    return 0;
}