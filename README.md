# Multi-Client TCP Chat Server that supports both IPv4 and IPv6.
This is a multithreaded TCP chat server implemented in C that accepts simultaneous client connections both over IPv4 and IPv6. The server receives messages from the clients, and the server responds with an acknowledgment for each message received.

## Key Features
- Handles up to 20 concurrent client connections. (`MAXUSERS` can be modified manually.)
- Dual-stack support: Accepts not just IPv4 but IPv6 connections.
- Threaded client handling via `pthread`.
- Thread-safe client management via `pthread_mutex_t`.
- Eloquent separation of server and client responsibilities.

## Compliation

```bash
gcc server.c -o server -pthread
gcc client.c -o client
```

## Usage
First start the server.
```bash
./server
```

Then start a client in a separate terminal.
```bash
./client
```

---

## Example
Client Terminal:
```bash
Connection established
Enter the message: Hello there
From server: Server has received your message
```

Server Terminal:
```bash
Server running... Waiting for clients.
New client: 5
IP address: ::1
Port      : 37136
Client [5]: "Hello there"
Client[5] disconnected.
```

---
