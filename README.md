# Matt Daemon - Socket Server Daemon

A C++ daemon that creates a socket server listening on port 8080, handles up to 3 concurrent connections, and logs all activities using a custom logging class.

## Table of Contents
- [Overview](#overview)
- [Features](#features)
- [Project Structure](#project-structure)
- [Code Explanation](#code-explanation)
- [Compilation and Usage](#compilation-and-usage)
- [Testing](#testing)
- [Signal Handling](#signal-handling)

## Overview

This project implements a daemon process that:
- Runs as a background service (daemon)
- Listens for incoming socket connections on port 8080
- Handles up to 3 simultaneous connections
- Logs all activities to `/tmp/matt_daemon.log`
- Responds to "quit" command to gracefully shutdown
- Handles system signals properly

## Features

- **Daemonization**: Proper daemon creation with `fork()`, `setsid()`, and file descriptor closure
- **Connection Management**: Accepts maximum 3 concurrent connections using `select()`
- **HTTP Protocol Support**: Handles basic HTTP requests from clients like Postman
- **Graceful Shutdown**: Responds to "quit" command and system signals
- **Comprehensive Logging**: Custom `Tintin_reporter` class for structured logging
- **Signal Handling**: Intercepts and logs SIGTERM, SIGINT, SIGQUIT, SIGHUP

## Project Structure

```
mattdeamon/
├── main.cpp              # Main daemon implementation
├── Tintin_reporter.hpp   # Logger class header
├── Tintin_reporter.cpp   # Logger class implementation
├── Makefile             # Build configuration
└── README.md            # This file
```

## Code Explanation

### 1. Daemon Creation Process

```cpp
pid_t pid = fork();
if (pid > 0) exit(0);  // Parent exits
setsid();              // Create new session
close(0); close(1); close(2);  // Close standard file descriptors
```

**Why we do this:**
- **`fork()`**: Creates a child process that becomes the daemon
- **Parent exits**: Ensures the daemon runs independently 
- **`setsid()`**: Makes the process a session leader, detaching it from the controlling terminal
- **Close file descriptors**: Prevents the daemon from accidentally reading/writing to terminal

### 2. Signal Handling

```cpp
void signal_handler(int signal) {
    global_logger->info("Received signal. Shutting down gracefully.");
    daemon_running = false;
}
```

**Why we need this:**
- Allows graceful shutdown when system sends termination signals
- Logs the signal reception for debugging
- Sets `daemon_running = false` to exit the main loop cleanly

### 3. Socket Creation and Binding

```cpp
int server_fd = socket(AF_INET, SOCK_STREAM, 0);
setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
```

**Why we do this:**
- **`socket()`**: Creates a TCP socket for communication
- **`SO_REUSEADDR`**: Allows reusing the port immediately after restart (prevents "Address already in use" error)

### 4. Connection Management with `select()`

```cpp
fd_set read_fds, master_fds;
FD_SET(server_fd, &master_fds);
while (daemon_running) {
    select(max_fd + 1, &read_fds, NULL, NULL, NULL);
    // Handle connections...
}
```

**Why we chose `select()` over `fork()` for each connection:**
- **Memory Efficient**: Single process handles all connections
- **Precise Control**: Easy to limit to exactly 3 connections
- **No Race Conditions**: All connection management in one process
- **Simpler Resource Management**: No need to track child processes

### 5. HTTP Request Parsing

```cpp
std::string received_data(buffer, bytes_read);
size_t body_start = received_data.find("\r\n\r\n");
if (body_start != std::string::npos) {
    received_message = received_data.substr(body_start + 4);
}
```

**Why we parse HTTP:**
- Postman and web browsers send HTTP requests with headers
- We need to extract the actual message body (like "quit") from the HTTP request
- `\r\n\r\n` separates HTTP headers from the body

### 6. Connection Limiting Logic

```cpp
if (slot != -1) {
    // Accept connection
    client_fds[slot] = new_client;
} else {
    // Reject with 503 Service Unavailable
    send(new_client, reject_response, strlen(reject_response), 0);
    close(new_client);
}
```

**Why we limit connections:**
- Requirement: Handle maximum 3 simultaneous connections
- Graceful rejection: Send proper HTTP 503 response to 4th client
- Resource protection: Prevents server overload

### 7. Quit Command Handling

```cpp
if (received_message == "quit") {
    daemon_running = false;
    // Send acknowledgment and break
}
```

**Why we handle quit specially:**
- Requirement: Daemon must quit when receiving "quit" command
- Graceful shutdown: Sends acknowledgment before closing
- Breaks the main loop to trigger cleanup

### 8. Tintin_reporter Logger Class

```cpp
class Tintin_reporter {
    void info(const std::string& message);
    void warning(const std::string& message);
    void error(const std::string& message);
};
```

**Why we created a custom logger:**
- **Requirement**: Must create reusable logging class
- **Timestamped entries**: Each log has timestamp
- **Multiple log levels**: INFO, WARNING, ERROR for different message types
- **File-based logging**: Daemons can't use stdout/stderr (they're closed)

## Compilation and Usage

### Compile the project:
```bash
make
# or manually:
g++ -Wall -Wextra -std=c++11 -o matt_daemon main.cpp Tintin_reporter.cpp
```

### Run the daemon:
```bash
./matt_daemon
```

### Check if it's running:
```bash
ps aux | grep matt_daemon
```

### View logs:
```bash
tail -f /tmp/matt_daemon.log
```

## Testing

### Using Postman:
1. **URL**: `http://localhost:8080`
2. **Method**: POST
3. **Body**: 
   - Type: raw/text
   - Content: `quit` (to shutdown) or any other text

### Using curl:
```bash
# Send quit command
curl -X POST -d "quit" http://localhost:8080

# Send test message
curl -X POST -d "hello" http://localhost:8080

# Test connection limit (run 4 times simultaneously)
for i in {1..4}; do curl -X POST -d "test$i" http://localhost:8080 & done
```

### Using telnet:
```bash
telnet localhost 8080
# Then type any message and press Enter
```

## Signal Handling

The daemon handles these signals gracefully:

- **SIGTERM**: Termination request (kill command)
- **SIGINT**: Interrupt signal (Ctrl+C)
- **SIGQUIT**: Quit signal
- **SIGHUP**: Hangup signal

**Test signal handling:**
```bash
# Find the daemon PID
ps aux | grep matt_daemon

# Send signals
kill -TERM [PID]    # or kill [PID]
kill -INT [PID]
kill -HUP [PID]
```

## Key Design Decisions

1. **Single Process + select()**: More efficient than forking for each connection
2. **HTTP Support**: Allows testing with standard web tools
3. **Graceful Connection Rejection**: Proper HTTP 503 response for excess connections
4. **Comprehensive Logging**: Every action is logged with timestamps
5. **Signal Safety**: Logger is checked before use in signal handler
6. **Resource Cleanup**: All file descriptors and memory are properly cleaned up

This daemon demonstrates proper UNIX daemon programming practices while handling modern HTTP-based communication.
Daily Comments ---->          at      2025-08-17 By Anas Bouzanbil
Daily Comments ---->          at      2025-08-17 By Anas Bouzanbil
