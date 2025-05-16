# Multi-Client TCP Chat Server With IPv4 and IPv6 Support.
It's a multithreaded TCP chat server written in C that supports concurrent connections from several clients on both IPv4 and IPv6. Client can connect, register with a unique username, and broadcast messages to all other clients. The server broadcasts messages with usernames prepended to offer a basic group chat functionality.

## Essential Elements
- Manages up to 20 client connections at once. (You can manually change `MAXUSERS`).
- Dual-stack compatibility: Supports both IPv4 and IPv6 connections.
- Broadcasting of messages: All clients can see messages sent by one client.
- Username system: client must provide a unique username upon connection.
- Preservation of Input During Incoming Messages: Clients are able to type uninterrupted. Other users' messages are displayed without interfering with or replacing the active input line. (done via `<termios.h>`).
- Using `pthread` to handle threaded clients.
- The use of `pthread_mutex_t` allows for thread-safe client handling.
- The server and client duties are eloquently separated.
- Inbuilt chat functions like `/list` and `/private`.

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
Client-1 Terminal:
```bash
Connection established
Enter your username: stoney
[stoney]: hello everyone
[dark]: haiii
[dark]: clients are not interrupting each other.
[stoney]: heh :D
[PM from dark]: private
[stoney]: /list
=== Users online (2) ===
 - stoney
 - dark
Client dark has disconnected.
[stoney]: /list
=== Users online (1) ===
 - stoney
[stoney]: /exit
Client Exit...
```

Client-2 Terminal:
```bash
Connection established
Enter your username: dark
[dark]: haiii
[dark]: clients are not interrupting each other.  
[stoney]: heh :D
[dark]: /private -u stoney -m private
[dark]: /exit
Client Exit...
```

Server Terminal:
```bash
Server running... Waiting for clients.
New client: 5
IP address: ::1
Port      : 42292
stoney: hello everyone
New client: 6
IP address: ::1
Port      : 60660
dark: haiii
dark: clients are not interrupting each other.
stoney: heh :D
dark: private -u stoney -m private
stoney: /list
Client: dark has disconnected.
stoney: /list
Client: stoney has disconnected.
```

---

## NOTE
- Due to lingering socket bindings, the server may become temporarily unavailable for restart if it is shut down while clients are still connected. Make sure every client disconnects before shutting down the server to prevent this.
