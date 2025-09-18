# Autonomous Vehicle Telemetry Project

**Course:** Internet Architecture and Protocols  
**Semester:** 2025-2  
**Weight:** 20% of final grade  

## Introduction
This project evaluates skills in **network programming** with concurrency and in designing/implementing **application layer protocols**.  

A terrestrial autonomous vehicle transmits telemetry (speed, battery, temperature, direction) to multiple users and receives control commands.  
The server is implemented in **C using the Berkeley Sockets API**, while clients must be implemented in **at least two different languages** (e.g., Python and Java), each with a simple GUI that shows telemetry in real time.

---

## Requirements
1. The vehicle must send **telemetry information to all connected users every 10 seconds**, maintaining an updated list of clients (adding/removing users as they connect/disconnect).  
2. Supported **commands**:  
   - `SPEED UP`  
   - `SLOW DOWN`  
   - `TURN LEFT`  
   - `TURN RIGHT`  
   The vehicle may refuse commands (e.g., due to low battery or speed limits).  
3. **Two types of users**:  
   - **Administrator**: can send commands and query the list of users.  
   - **Observer**: only receives telemetry.  
   Administrator authentication must be preserved even if the IP changes.  
4. The **application layer protocol** must be text-based and clearly specified, including:  
   - Service specification  
   - Message vocabulary  
   - Message format (headers/fields)  
   - Operational rules  
   - Examples and usage scenarios (RFC-inspired style).  
5. Integration with **TCP/IP via Berkeley sockets**; justify transport choice (TCP for reliable delivery, optional UDP for fast telemetry).  
6. Support for **multiple concurrent clients** using **threads**.  
7. Implement **exception handling** for failed connections, invalid messages, etc.

---

## Implementation Details
- **Server:** C (mandatory), Berkeley Sockets API.  
- **Clients:** At least two different languages (e.g., Python and Java).  
- **Client GUI:** Must display speed, battery, and direction in real time.  
- **Logging:** Server must log requests and responses to **console and file** (`<LogsFile>`).