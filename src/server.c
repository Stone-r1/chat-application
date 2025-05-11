#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <arpa/inet.h>
#include <signal.h>
#include <sys/select.h>

#define MYPORT 7270
#define MAXUSERS 5
#define BUFFERSIZE 1024

/* TODO
 * handle partial reads/writes
*/

volatile sig_atomic_t running = 1;

void handleShutdown(int signal) {
    running = 0;
}

struct SocketWrapper {
    int sock;
    struct sockaddr_in6 addr;
    int family;
};

int createSocket(int family) {
    int sock = socket(family, SOCK_STREAM, 0);
    if (sock < 0) {
        perror("socket");
        return -1;
    }
    return sock;
}

void setupServerDetails(SocketWrapper* socketWrapper) {

    memset(&socketWrapper -> addr, 0, sizeof(socketWrapper -> addr));
    socketWrapper -> addr.sin6_family = socketWrapper -> family; // specifies family

    // bind to any local address
    if (socketWrapper -> family == AF_INET6) {
        socketWrapper -> addr.sin6_addr = in6addr_any; // IPv6 case
    } else {
        // IMPORTANT: in6addr_any is for IPv6, cannot cast directly to IPv4
        struct in_addr ipv4_any = {INADDR_ANY};
        memcpy(&socketWrapper -> addr.sin6_addr, &ipv4_any, sizeof(ipv4_any));
        // socketWrapper -> addr.sin6_addr = *((struct in_addr*) &in6addr_any); // IPv4 case
    }
    socketWrapper -> addr.sin6_port = htons(MYPORT);
}

int bindSocket(SocketWrapper* socketWrapper) {
    if (bind(socketWrapper -> sock, (struct sockaddr*) &socketWrapper -> addr, sizeof(socketWrapper -> addr)) < 0) {
        perror("bind");
        return -1;
    }
    return 0;
}

// handle partial write (ssize_t -> signed size_t)
ssize_t sendAll(int socket, const char* buffer, size_t length) {
    size_t totalSent = 0;
    while (totalSent < length) {
        ssize_t sent = write(socket, buffer + totalSent, length - totalSent);
        if (sent <= 0) {
            return -1; // ERROR or Connection closed.
        }
        totalSent += sent;
    }

    return totalSent;
}

void handleClient(int clientSocket) {
    char buffer[BUFFERSIZE + 1] = {0};
    int message;
    while ((message = read(clientSocket, buffer, BUFFERSIZE)) > 0) {
        buffer[message] = '\0';
        printf("Client: \"%s\"\n", buffer);
        const char* replyText = "Server has received your message\n";
        sendAll(clientSocket, replyText, strlen(replyText));
    }
}

// Prepares the set of sockets to be monitored
void selectSockets(fd_set* readfds, SocketWrapper* ipv4Socket, SocketWrapper* ipv6Socket) {
    FD_ZERO(readfds); // clean
    FD_SET(ipv4Socket -> sock, readfds); // monitor for events
    FD_SET(ipv6Socket -> sock, readfds); // monitor for events
}

int acceptConnection(SocketWrapper* socketWrapper, fd_set* readfds) {
    struct sockaddr_in6 clientAddr;
    socklen_t clientAddrLen = sizeof(clientAddr);

    if (FD_ISSET(socketWrapper -> sock, readfds)) {
        int clientSocket = accept(socketWrapper -> sock, (struct sockaddr*) &clientAddr, &clientAddrLen);
        if (clientSocket < 0) {
            perror("accept");
            return 1;
        }

        char clientIP[INET6_ADDRSTRLEN];
        inet_ntop(socketWrapper -> family, &clientAddr.sin6_addr, clientIP, sizeof(clientIP));
        // ntohs -> converts the port number from network byte order to host byte order 
        printf("IP address: %s\n", clientIP);
        printf("Port      : %d\n", ntohs(clientAddr.sin6_port));

        handleClient(clientSocket);
        close(clientSocket);
    }

    return 0;
}

int main(int args, char* argv[]) {
    signal(SIGINT, handleShutdown); // signal handler for proper shutdown
   
    SocketWrapper ipv6Socket = {createSocket(AF_INET6), {}, AF_INET6};
    SocketWrapper ipv4Socket = {createSocket(AF_INET), {}, AF_INET};

    if (ipv6Socket.sock < 0 || ipv4Socket.sock < 0) {
        return 1;
    }
    
    int opt = 1;
    if (setsockopt(ipv6Socket.sock, IPPROTO_IPV6, IPV6_V6ONLY, &opt, sizeof(opt)) < 0) {
        perror("setsockopt IPV6_V6ONLY");
        return 1;
    }

    // after creating ipv6, ipv4 is getting blocked, because it's address is already in use by ipv6
    // setsockopt(ipv6Socket.sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    // setsockopt(ipv4Socket.sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    setupServerDetails(&ipv6Socket);
    setupServerDetails(&ipv4Socket);

    
    // binds local name to the socket
    if (bindSocket(&ipv6Socket) < 0 || bindSocket(&ipv4Socket) < 0) {
        return 1;
    }    

    if (listen(ipv6Socket.sock, MAXUSERS) < 0) {
        perror("listen (IPv6)");
        return 1;
    } 

    if (listen(ipv4Socket.sock, MAXUSERS) < 0) {
        perror("listen (IPv4)");
        return 1;
    }

    printf("Server running... Waiting for clients.\n");

    // file descriptor for reading.
    // --- future update add writing ---
    fd_set readfds; 
    while (running) {
        selectSockets(&readfds, &ipv4Socket, &ipv6Socket);

        // monitor both sockets for incoming connections
        int maxfd, activity;
        if (ipv6Socket.sock > ipv4Socket.sock) {
            maxfd = ipv6Socket.sock;
        } else {
            maxfd = ipv4Socket.sock;
        }

        // nfds, readfds, writefds, exceptfds, timeout
        activity = select(maxfd + 1, &readfds, NULL, NULL, NULL); 
        if (activity < 0) {
            perror("select");
            return 1;
        }

        acceptConnection(&ipv6Socket, &readfds);
        acceptConnection(&ipv4Socket, &readfds);
    }
}
