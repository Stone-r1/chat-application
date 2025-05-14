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
#include <pthread.h>

#define MYPORT 7270
#define MAXUSERS 20
#define BUFFERSIZE 1024
#define MAXUSERNAMELEN 65

volatile sig_atomic_t running = 1;

void handleShutdown(int signal) {
    running = 0;
}

// = neccessary tools =
typedef struct {
    char username[MAXUSERNAMELEN];  
    int sockfd;
} client_t;

typedef struct {
    int sock;
    struct sockaddr_in6 addr;
    int family;
} SocketWrapper;

static pthread_mutex_t clientMutex = PTHREAD_MUTEX_INITIALIZER; // unlocked mutex
static client_t *clientList[MAXUSERS]; // mutex used to ensure that only on thread can modify the array at a time.
// ====================

void addClient(client_t* client) {
    pthread_mutex_lock(&clientMutex);
    for (int i = 0; i < MAXUSERS; i++) {
        if (!clientList[i]) {
            clientList[i] = client;
            break;
        }
    }
    pthread_mutex_unlock(&clientMutex);
}

void removeClient(int fd) {
    pthread_mutex_lock(&clientMutex);
    for (int i = 0; i < MAXUSERS; i++) {
        if (clientList[i] && clientList[i] -> sockfd == fd) {
            free(clientList[i]);
            clientList[i] = NULL;
            break;
        }
    }
    pthread_mutex_unlock(&clientMutex);
}

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
ssize_t sendAll(const char* buffer, size_t length, const char* username) {
    for (int i = 0; i < MAXUSERS; i++) {
        if (clientList[i] == NULL || clientList[i] -> username == username) {
            continue;
        }

        // each client gets it's own counter of sent bytes.
        int socket = clientList[i] -> sockfd;
        size_t bytesLeft = length;
        const char* bufferPtr = buffer;

        while (bytesLeft > 0) {
            ssize_t sent = write(socket, bufferPtr, bytesLeft);
            if (sent <= 0) {
                perror("write");
                break; // ERROR or Connection closed.
            }
            bytesLeft -= sent;
            bufferPtr += sent;
        } 
    } 
    return (ssize_t)length;
}

int getUsername(int clientSocket, client_t* client) { 
    char username[MAXUSERNAMELEN];
    ssize_t messageLen;
    if ((messageLen = read(clientSocket, username, sizeof(username) - 1)) <= 0) {
        perror("read");
        return 0;
    }

    username[messageLen] = '\0';
    strncpy(client -> username, username, sizeof(client -> username));
    return 1;
}

// #IMPORTANT - don't send client's own messages to them.
void* handleClient(void* arg) {
    client_t* client = (client_t*)arg;
    int clientSocket = client -> sockfd;

    if (!getUsername(clientSocket, client)) {
        close(clientSocket);
        removeClient(clientSocket);
        printf("Invalid Username");
        return NULL;
    }

    char buffer[BUFFERSIZE + 2] = {0};
    int message;
    while ((message = read(clientSocket, buffer, BUFFERSIZE)) > 0) {
        buffer[message] = '\0';
        printf("%s: %.*s\n", client -> username, message, buffer);

        // change it. support all client sockets.
        
        buffer[message] = '\n';
        buffer[message + 1] = '\0';
        sendAll(buffer, message + 1, client -> username);
    }

    // cleanup
    close(clientSocket);
    removeClient(clientSocket);
    printf("Client[%d] disconnected.\n", clientSocket);
    return NULL;
}

// Prepares the set of sockets to be monitored
void selectSockets(fd_set* readfds, SocketWrapper* ipv4Socket, SocketWrapper* ipv6Socket) {
    FD_ZERO(readfds); // clean
    FD_SET(ipv4Socket -> sock, readfds); // monitor for events
    FD_SET(ipv6Socket -> sock, readfds); // monitor for events
}

void acceptConnection(SocketWrapper* socketWrapper, fd_set* readfds) {
    struct sockaddr_in6 clientAddr;
    socklen_t clientAddrLen = sizeof(clientAddr);

    if (FD_ISSET(socketWrapper -> sock, readfds)) {
        int clientSocket = accept(socketWrapper -> sock, (struct sockaddr*) &clientAddr, &clientAddrLen);
        if (clientSocket < 0) {
            perror("accept");
            return;
        }

        char clientIP[INET6_ADDRSTRLEN];
        inet_ntop(socketWrapper -> family, &clientAddr.sin6_addr, clientIP, sizeof(clientIP));
        // ntohs -> converts the port number from network byte order to host byte order
        printf("New client: %d\n", clientSocket);
        printf("IP address: %s\n", clientIP);
        printf("Port      : %d\n", ntohs(clientAddr.sin6_port));

        client_t* client = (client_t*)malloc(sizeof *client);
        client -> sockfd = clientSocket;
        addClient(client);

        pthread_t threadID;
        if (pthread_create(&threadID, NULL, handleClient, client) != 0) {
            perror("pthread_create");
            removeClient(clientSocket);
            close(clientSocket);
            free(client);
            return;
        }

        pthread_detach(threadID);
    }
}

int main(int args, char* argv[]) {
    signal(SIGINT, handleShutdown); // signal handler for proper shutdown
   
    SocketWrapper ipv6Socket;
    memset(&ipv6Socket, 0, sizeof(ipv6Socket.addr));
    ipv6Socket.sock = createSocket(AF_INET6);
    ipv6Socket.family = AF_INET6;

    SocketWrapper ipv4Socket;
    memset(&ipv4Socket, 0, sizeof(ipv4Socket.addr));
    ipv4Socket.sock = createSocket(AF_INET);
    ipv4Socket.family = AF_INET;

    if (ipv6Socket.sock < 0 || ipv4Socket.sock < 0) {
        return 1;
    }
    
    int optionValue = 1;
    if (setsockopt(ipv6Socket.sock, IPPROTO_IPV6, IPV6_V6ONLY, &optionValue, sizeof(optionValue)) < 0) {
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

    fd_set readfds;
    int clientSockets[MAXUSERS + 1];
    int clientAmount = 0;

    while (running) {
        selectSockets(&readfds, &ipv4Socket, &ipv6Socket);

        // monitor both sockets for incoming connections
        int maxfd, activity;
        if (ipv6Socket.sock > ipv4Socket.sock) {
            maxfd = ipv6Socket.sock;
        } else {
            maxfd = ipv4Socket.sock;
        }

        // updating readfd and determining maximum file descriptor
        for (int i = 0; i < clientAmount; i++) {
            FD_SET(clientSockets[i], &readfds);
            if (clientSockets[i] > maxfd) {
                maxfd = clientSockets[i];
            }
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

    close(ipv6Socket.sock);
    close(ipv4Socket.sock);
    printf("Server Shutdown.\n");
    return 0;
}

// There are a lot of comments that are completely redundant.
// They were written in educational purposes (I'm the one who needs them to learn properly :D)
