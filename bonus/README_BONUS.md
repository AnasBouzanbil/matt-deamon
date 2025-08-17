# Matt Daemon - Bonus Version with Web Interface

A comprehensive C++ daemon implementation with advanced web interface, session management, authentication, and remote shell capabilities.

## Table of Contents
- [Overview](#overview)
- [Features](#features)
- [Architecture](#architecture)
- [Function Documentation](#function-documentation)
  - [Session Management](#session-management)
  - [Authentication Functions](#authentication-functions)
  - [Web Interface Functions](#web-interface-functions)
  - [File Management Functions](#file-management-functions)
  - [Core Daemon Functions](#core-daemon-functions)
  - [Utility Functions](#utility-functions)
- [Web Interface](#web-interface)
- [Security Features](#security-features)
- [Compilation and Usage](#compilation-and-usage)

## Overview

The Matt Daemon bonus version extends the basic daemon functionality with a sophisticated web interface that provides:
- User authentication system
- Remote shell access
- Log management and archiving
- Real-time log viewing
- Session-based security
- HTTP protocol support

## Features

### Core Daemon Features
- **Proper Daemonization**: Fork, setsid, file descriptor management
- **Connection Management**: Maximum 3 concurrent connections
- **Lock File Management**: Prevents multiple instances
- **Signal Handling**: Graceful shutdown on system signals
- **Comprehensive Logging**: Multi-level logging with Tintin_reporter

### Web Interface Features
- **Authentication System**: Password-based access control
- **Session Management**: Token-based session tracking
- **Remote Shell Interface**: Execute system commands remotely
- **Log Viewer**: Real-time log viewing and downloading
- **Log Archiving**: Create and manage log archives
- **Log Statistics**: Detailed log analysis
- **Responsive UI**: Modern web interface with tabs

### Security Features
- **Authentication Required**: Password protection for all sensitive operations
- **Session Tokens**: Secure session management
- **Command Filtering**: Blocks dangerous shell commands
- **Root Privilege Checking**: Validates administrator access
- **HTTPS-ready**: Designed for secure deployment

## Architecture

```
┌─────────────────┐    ┌─────────────────┐    ┌─────────────────┐
│   Web Browser   │    │   Matt Daemon   │    │  Tintin_reporter│
│                 │    │                 │    │                 │
│ ┌─────────────┐ │    │ ┌─────────────┐ │    │ ┌─────────────┐ │
│ │Login Page   │ │◄──►│ │Auth System  │ │    │ │Log Manager  │ │
│ └─────────────┘ │    │ └─────────────┘ │    │ └─────────────┘ │
│ ┌─────────────┐ │    │ ┌─────────────┐ │    │ ┌─────────────┐ │
│ │Dashboard    │ │◄──►│ │HTTP Handler │ │◄──►│ │File I/O     │ │
│ └─────────────┘ │    │ └─────────────┘ │    │ └─────────────┘ │
│ ┌─────────────┐ │    │ ┌─────────────┐ │    │                 │
│ │Shell Interface│◄──►│ │Shell Executor│ │    │                 │
│ └─────────────┘ │    │ └─────────────┘ │    │                 │
└─────────────────┘    └─────────────────┘    └─────────────────┘
```

## Function Documentation

### Session Management

#### `std::string generate_session_token()`
**Purpose**: Creates cryptographically random session tokens for user authentication.

**Description**: 
- Generates a 32-character alphanumeric token
- Uses time-seeded random number generation
- Provides unique session identification

**Returns**: String containing the generated session token

**Usage**:
```cpp
std::string token = generate_session_token();
active_sessions.push_back(token);
```

#### `bool is_valid_session(const std::string& token)`
**Purpose**: Validates if a session token is currently active.

**Parameters**:
- `token`: Session token to validate

**Returns**: `true` if session is valid, `false` otherwise

**Description**:
- Searches active sessions vector for the provided token
- Used for protecting authenticated routes
- Essential for maintaining session security

### Authentication Functions

#### `void create_auth_file()`
**Purpose**: Creates the authentication file with default credentials if it doesn't exist.

**Description**:
- Checks for existing authentication file
- Creates default password file with "admin123"
- Logs authentication file creation
- Ensures authentication system is always available

**File Location**: `./matt_daemon_auth.txt` (configurable)

#### `bool verify_password(const std::string& password)`
**Purpose**: Verifies user-provided password against stored credentials.

**Parameters**:
- `password`: User-provided password to verify

**Returns**: `true` if password matches, `false` otherwise

**Description**:
- Reads stored password from authentication file
- Performs direct string comparison
- Logs authentication attempts
- Critical for system security

#### `bool has_root_privileges()`
**Purpose**: Checks if the daemon has appropriate privileges to run.

**Returns**: `true` if has required privileges, `false` otherwise

**Description**:
- Currently checks authentication file accessibility
- Can be extended to check actual user ID (getuid() == 0)
- Ensures daemon runs with appropriate permissions

### Web Interface Functions

#### `std::string get_login_page()`
**Purpose**: Generates the HTML login page for web authentication.

**Returns**: Complete HTML string for the login interface

**Features**:
- Modern responsive design
- CSS styling embedded
- JavaScript authentication handling
- Error message display
- Root privilege warnings

#### `std::string get_web_page()`
**Purpose**: Generates the main dashboard HTML interface.

**Returns**: Complete HTML string for the main dashboard

**Features**:
- Multi-tab interface (Logs, Shell, Archive)
- Real-time log viewing
- Remote shell interface
- Log management tools
- Auto-refresh functionality

#### `void handle_http_request(int client_fd, const std::string& request)`
**Purpose**: Processes HTTP requests and generates appropriate responses.

**Parameters**:
- `client_fd`: Client socket file descriptor
- `request`: Raw HTTP request string

**Description**:
- Routes requests to appropriate handlers
- Manages session authentication
- Handles all web interface endpoints:
  - `/` - Login page
  - `/auth` - Authentication endpoint
  - `/dashboard` - Main interface
  - `/logs` - Log retrieval
  - `/shell` - Command execution
  - `/stats` - Log statistics
  - `/archive` - Log archiving
  - `/clean` - Log cleaning
  - `/stop` - Daemon shutdown

### File Management Functions

#### `std::string read_log_file()`
**Purpose**: Reads and returns the last 50 lines from the log file.

**Returns**: String containing recent log entries

**Description**:
- Reads entire log file into memory
- Extracts last 50 lines for display
- Handles file access errors gracefully
- Optimized for web interface display

#### `std::string get_log_statistics()`
**Purpose**: Analyzes log file and returns detailed statistics.

**Returns**: Formatted string with log statistics

**Statistics Provided**:
- Total number of log lines
- Count of INFO messages
- Count of ERROR messages
- Count of WARNING messages
- Count of LOG messages
- File path information

#### `std::string archive_logs()`
**Purpose**: Creates a timestamped archive copy of the current log file.

**Returns**: Status message about archive operation

**Description**:
- Creates timestamp-based archive filename
- Copies current log file to archive location
- Preserves original log file
- Provides archive confirmation details

#### `std::string clean_old_logs()`
**Purpose**: Removes old log entries, keeping only the most recent 100 lines.

**Returns**: Status message about cleaning operation

**Description**:
- Reads all log entries into memory
- Keeps only the last 100 lines
- Rewrites log file with cleaned data
- Reports number of lines removed

### Core Daemon Functions

#### `bool check_lock_file()`
**Purpose**: Checks if another daemon instance is already running.

**Returns**: `true` if safe to start, `false` if another instance exists

**Description**:
- Reads PID from existing lock file
- Uses `kill(pid, 0)` to check if process is alive
- Removes stale lock files from dead processes
- Prevents multiple daemon instances

#### `void create_lock_file()`
**Purpose**: Creates a lock file with the current process PID.

**Description**:
- Writes current PID to lock file
- Prevents other instances from starting
- Essential for daemon singleton behavior
- Located at `/tmp/matt_daemon.lock`

#### `void remove_lock_file()`
**Purpose**: Removes the lock file during daemon shutdown.

**Description**:
- Deletes lock file to allow future daemon starts
- Called during normal shutdown and signal handling
- Registered with `atexit()` for automatic cleanup

#### `void signal_handler(int signal)`
**Purpose**: Handles system signals for graceful daemon shutdown.

**Parameters**:
- `signal`: Signal number received

**Handled Signals**:
- `SIGTERM`: Termination request
- `SIGINT`: Interrupt (Ctrl+C)
- `SIGQUIT`: Quit signal
- `SIGHUP`: Hang up signal

**Description**:
- Logs signal reception
- Sets `daemon_running = false`
- Removes lock file
- Forces clean exit

### Utility Functions

#### `std::string execute_shell_command(const std::string& command)`
**Purpose**: Safely executes shell commands and returns output.

**Parameters**:
- `command`: Shell command to execute

**Returns**: Command output or error message

**Security Features**:
- Blocks dangerous commands (`rm -rf`, `sudo`, `su`)
- Captures both stdout and stderr
- Includes exit status in output
- Logs all command executions

#### `bool is_http_request(const std::string& data)`
**Purpose**: Determines if incoming data is an HTTP request.

**Parameters**:
- `data`: Incoming data string

**Returns**: `true` if data starts with HTTP methods

**Description**:
- Checks for "GET " or "POST " at string beginning
- Distinguishes HTTP requests from raw socket data
- Enables proper request routing

#### `std::string extract_json_command(const std::string& request)`
**Purpose**: Extracts shell command from JSON POST request body.

**Parameters**:
- `request`: Complete HTTP request string

**Returns**: Extracted command string

**Description**:
- Finds HTTP body after headers
- Parses JSON to extract "command" field
- Handles malformed requests gracefully

#### `std::string extract_json_password(const std::string& request)`
**Purpose**: Extracts password from JSON authentication request.

**Parameters**:
- `request`: Complete HTTP request string

**Returns**: Extracted password string

**Description**:
- Locates request body
- Parses JSON to find "password" field
- Used for authentication processing

#### `std::string extract_session_token(const std::string& request)`
**Purpose**: Extracts session token from HTTP cookies or headers.

**Parameters**:
- `request`: Complete HTTP request string

**Returns**: Session token string if found

**Description**:
- Searches for "matt_daemon_session=" in headers
- Extracts token value from cookie
- Handles missing tokens gracefully

## Web Interface

### Authentication Flow
1. User accesses daemon via web browser
2. Login page presented with password field
3. Password validated against auth file
4. Session token generated and stored
5. User redirected to main dashboard

### Dashboard Features

#### Logs Tab
- **Real-time Viewing**: Auto-refreshing log display
- **Download Logs**: Save logs to local file
- **Last 50 Lines**: Displays recent activity

#### Remote Shell Tab
- **Command Execution**: Run system commands remotely
- **Security Filtering**: Blocks dangerous operations
- **Command History**: Persistent shell output
- **Enter Key Support**: Quick command execution

#### Archive Tab
- **Create Archives**: Timestamped log backups
- **Log Statistics**: Detailed analysis of log content
- **Clean Old Logs**: Remove old entries to save space

### API Endpoints

| Endpoint | Method | Purpose | Authentication |
|----------|--------|---------|----------------|
| `/` | GET | Login page | No |
| `/auth` | POST | User authentication | No |
| `/dashboard` | GET | Main interface | Yes |
| `/logs` | GET | Retrieve log content | Yes |
| `/shell` | POST | Execute shell command | Yes |
| `/stats` | GET | Log statistics | Yes |
| `/archive` | POST | Create log archive | Yes |
| `/clean` | POST | Clean old logs | Yes |
| `/stop` | POST | Shutdown daemon | Yes |

## Security Features

### Authentication System
- **Password Protection**: All sensitive operations require authentication
- **Session Management**: Token-based session tracking
- **Session Timeout**: Sessions can be invalidated
- **Privilege Checking**: Validates administrator access

### Command Filtering
The remote shell interface includes security measures:
- Blocks `rm -rf` commands
- Prevents `sudo` and `su` usage
- Captures command output and errors
- Logs all command executions

### Session Security
- **Unique Tokens**: Cryptographically random session IDs
- **HTTP-Only Cookies**: Prevents JavaScript access to tokens
- **Session Validation**: All protected routes check session validity

## Compilation and Usage

### Prerequisites
- C++11 compatible compiler
- POSIX-compliant system (Linux/Unix)
- Network access for web interface

### Build Instructions
```bash
cd bonus/
make
```

### Manual Compilation
```bash
g++ -Wall -Wextra -std=c++11 -o daemon main.cpp Tintin_reporter.cpp
```

### Running the Daemon
```bash
# Start the daemon
./daemon

# The daemon will output:
# Socket daemon started with PID: [PID]
# Web interface available at: http://localhost:8080
```

### Accessing the Web Interface
1. Open web browser
2. Navigate to `http://localhost:8080`
3. Enter password (default: "admin123")
4. Access dashboard features

### Changing Default Password
```bash
# Edit the authentication file
echo "your_new_password" > ./matt_daemon_auth.txt
```

### Monitoring the Daemon
```bash
# Check if running
ps aux | grep daemon

# View logs in real-time
tail -f /tmp/matt_daemon.log

# Check lock file
cat /tmp/matt_daemon.lock
```

### Stopping the Daemon
Multiple methods available:
1. **Web Interface**: Use "Stop Daemon" button in dashboard
2. **Command**: Send "quit" via telnet/netcat
3. **Signal**: `kill [PID]` or `killall daemon`

### Configuration Files
- **Lock File**: `/tmp/matt_daemon.lock` (PID storage)
- **Log File**: `/tmp/matt_daemon.log` (Activity logs)
- **Auth File**: `./matt_daemon_auth.txt` (Password storage)

## Troubleshooting

### Common Issues

#### "Can't open: /tmp/matt_daemon.lock"
- Another daemon instance is running
- Kill existing process or wait for it to exit

#### "Authentication error"
- Check password in `matt_daemon_auth.txt`
- Ensure file is readable by daemon

#### "Connection refused"
- Daemon may not be running
- Check if port 8080 is already in use
- Verify daemon started successfully

#### "Permission denied"
- Daemon requires appropriate file permissions
- Check write access to `/tmp/` directory

### Debug Information
- All activities logged to `/tmp/matt_daemon.log`
- Web interface provides real-time log viewing
- Error messages displayed in browser console

## Code Quality

The bonus implementation demonstrates:
- **Modular Design**: Separated concerns for web, authentication, and daemon logic
- **Error Handling**: Comprehensive error checking and recovery
- **Security Best Practices**: Authentication, session management, command filtering
- **Modern C++**: C++11 features and standard library usage
- **Cross-Platform**: POSIX-compliant system calls
- **Maintainable Code**: Clear function separation and documentation

## Future Enhancements

Potential improvements:
- HTTPS support with SSL/TLS
- Multi-user authentication system
- Advanced command history and completion
- Real-time process monitoring
- Configuration file support
- Advanced logging filters and search
- WebSocket support for real-time updates
