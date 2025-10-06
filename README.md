# Autonomous Vehicle Telemetry System

A concurrent client-server system for real-time vehicle telemetry transmission and control using Berkeley Sockets API and a custom application protocol.

## Authors

- Alejandro Garces Ramírez
- Cristóbal Gutiérrez Castro
- Nicolás Ospina Torres
  
---

## Overview

This project implements an **Autonomous Vehicle Telemetry Protocol (AVT)** that enables real-time monitoring and control of a simulated autonomous vehicle. The system consists of:

- A **multithreaded server** written in C that manages multiple concurrent client connections
- An **admin client** with GUI (Python/CustomTkinter) with full control capabilities
- An **observer client** with GUI (Java/JavaFX) for read-only monitoring

The server broadcasts telemetry data (speed, battery, temperature, direction) every 10 seconds to all connected clients and processes control commands from authenticated administrators.

---

### Key Components

- **Server:** Handles client connections using POSIX threads (pthread), maintains global client list with mutex protection, and broadcasts telemetry periodically
- **Admin Client:** Full access with authentication, control commands, and real-time dashboard
- **Observer Client:** Read-only access for monitoring telemetry data

---

## Protocol Specification

### User Types

| Type | Capabilities |
|------|-------------|
| **Administrator** | Authenticate, send control commands (SPEED, TURN), list users |
| **Observer** | Receive telemetry, query role (read-only) |

### Client-to-Server Commands

| Command | Description |
|---------|-------------|
| `HELLO [name=<text>]` | Identify client with optional name |
| `AUTH <user> <password>` | Authenticate (admin/admin123) |
| `ROLE?` | Request assigned role |
| `LIST USERS` | Show connected users (admin only) |
| `SPEED UP` | Increase vehicle speed |
| `SLOW DOWN` | Decrease vehicle speed |
| `TURN LEFT` | Turn vehicle left |
| `TURN RIGHT` | Turn vehicle right |
| `QUIT` | Close connection |

### Server-to-Client Responses

| Response | Description |
|----------|-------------|
| `OK <msg>` | Successful operation |
| `ERR <reason>` | Error or invalid command |
| `BYE` | Session closed |
| `TLM speed=<int>;battery=<int>;temp=<float>;dir=<char>;ts=<string>` | Telemetry data broadcast |

### Protocol Rules

1. Server sends welcome message on connection
2. Client identifies with `HELLO` and authenticates with `AUTH`
3. Valid authentication grants ADMIN role
4. Telemetry broadcast every 10 seconds to all clients
5. `SPEED` and `TURN` commands require admin role
6. Speed commands denied when battery < 15%
7. Invalid commands receive `ERR unknown`
8. Disconnection removes client from global list

---

## Installation & Execution

### Server (C)

#### Requirements
- GCC compiler
- POSIX threads support (pthread)

#### Compilation & Execution

```bash
gcc server.c -o server -lpthread
./server <port> <logfile>
```

**Example:**
```bash
gcc server.c -o server -lpthread
./server 9000 logs.txt
```

The server will:
- Listen on the specified port (e.g., 9000)
- Log all requests/responses to the specified file
- Format: `[YYYY-MM-DD HH:MM:SS] IP REQ/RES <Response>`

---

### Admin Client (Python)

#### Requirements
- Python 3.8+
- CustomTkinter library

#### Installation & Execution

```bash
pip install -r requirements.txt
python admin_client.py
```

#### Interface Features

<img width="921" height="541" alt="image" src="https://github.com/user-attachments/assets/8d34f4a2-4120-415e-9647-21c674d82b28" />
The admin interface displays:

- **Real-time telemetry:**
  - Current speed (km/h)
  - Battery level (%) with progress bar
  - Internal temperature (°C)
  - Current direction (N, E, S, W)
  - Connection status

- **Control panel:**
  - SPEED UP / SLOW DOWN buttons
  - TURN LEFT / TURN RIGHT buttons
  - CONNECT / DISCONNECT controls

The client uses asynchronous threading to maintain responsive UI while receiving continuous telemetry updates.

---

### Observer Client (Java)

#### Requirements
- Java JDK 11+
- Maven (dependency management)

#### Maven Installation

**Ubuntu/Debian:**
```bash
sudo apt install maven
```
Other systems: Follow this [video tutorial](https://www.youtube.com/watch?v=cIneTHgkrQw) or visit Maven's official site

#### Execution

```bash
bash run-observer.sh
```

The script can be executed on:
- Linux/macOS: directly in terminal
- Windows: using Git Bash

#### Interface Features
<img width="921" height="585" alt="image" src="https://github.com/user-attachments/assets/8786f2c8-25b7-4a44-9520-daace3d55963" />
The observer interface shows:

- Real-time telemetry dashboard
- Connection status indicator
- Event log panel with timestamp
- Automatic reconnection with exponential backoff
- Thread-safe UI updates using `Platform.runLater()`

**Note:** This client is read-only and cannot send control commands.

---

## Demo Video

Watch the full demonstration and explanation of the project:

[Video link](https://youtu.be/H2jMV2aOWvI)

---

## Features

### Server Capabilities
Concurrent multi-client handling with pthread  
TCP-based reliable communication  
Periodic telemetry broadcasting (10s intervals)  
Role-based access control (Admin/Observer)  
Request/response logging with timestamps  
Thread-safe client list management with mutex  
Battery-aware command validation  

### Client Capabilities
Real-time telemetry visualization  
GUI-based control interfaces  
Asynchronous message handling  
Auto-reconnection support (Java client)  
Cross-platform compatibility  
Authentication system  

---

## Technical Requirements

| Component | Technology | Version |
|-----------|-----------|---------|
| Server | C (GCC) | C99+ |
| Threading | POSIX pthread | - |
| Protocol | TCP/IP | IPv4 |
| Admin Client | Python + CustomTkinter | 3.8+ |
| Observer Client | Java + JavaFX + Maven | 11+ |

---

## Project Structure

```
.
├── server.c                 # C server implementation
├── admin_client.py          # Python admin client
├── requirements.txt         # Python dependencies
├── run-observer.sh          # Java client launcher
├── logs.txt                 # Server log file (generated)
└── README.md               # This file
```
