#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <arpa/inet.h>
#include <signal.h>

#define MYPORT 7270
#define MAXUSERS 5
#define BUFFERSIZE 1024

/* TODO
 * log client IP/port
 * normal shut down handling
 * handle partial reads/writes
*/

int createSocket() {
    int sock = socket(AF_INET6, SOCK_STREAM, 0);
    if (sock < 0) {
        perror("socket");
        return 1;
    }
    return sock;
}

void setupServerDetails(struct sockaddr_in6* addr) {
    memset(addr, 0, sizeof(struct sockaddr_in6));
    addr -> sin6_family = AF_INET6; // specifies family
    addr -> sin6_addr = in6addr_any; // bind to any local address
    addr -> sin6_port = htons(MYPORT);    
}

void handleClient(int clientSocket) {
    char buffer[BUFFERSIZE + 1] = {0};
    int message;
    while ((message = read(clientSocket, buffer, BUFFERSIZE)) > 0) {
        buffer[message] = '\0';
        printf("Client: %s", buffer);
        const char* replyText = "Server has received your message\n";
        write(clientSocket, replyText, strlen(replyText));
    }
}

int main(int args, char* argv[]) {
    int sock = createSocket(), newSock;
    signal(SIGCHLD, SIG_IGN); // automatically clean up 'zombie' processes
    
    int opt = 1;
    setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in6 clientAddr, serverAddr; // ipv6 socket address
    socklen_t clientAddrLen = sizeof(clientAddr), serverAddrLen = sizeof(serverAddr);
    setupServerDetails(&serverAddr);

    
    // binds local name to the socket
    if (bind(sock, (struct sockaddr*) &serverAddr, serverAddrLen) < 0) {
        perror("bind");
        return 1;
    }    

    if (listen(sock, MAXUSERS) < 0) {
        perror("listen");
        return 1;
    } else {
        while (1) {
            // accept the user and create a socket for them
            int clientSocket = accept(sock, (struct sockaddr*) &clientAddr, &clientAddrLen);
            if (clientSocket < 0) {
                perror("accept");
                return 1;
            }

            // after accepting I can get IP/port and info about machine
            getpeername(clientSocket, (struct sockaddr*) &clientAddr, &clientAddrLen);
            char ipStr[INET6_ADDRSTRLEN];
            inet_ntop(AF_INET6, &clientAddr.sin6_addr, ipStr, sizeof(ipStr));
            printf("IP address: %s\n", ipStr);
            printf("Port      : %d\n", ntohs(clientAddr.sin6_port));

            pid_t pid = fork();
            if (pid < 0) {
                perror("fork");
                close(clientSocket);
                return 1;
            } else if (pid == 0) {
                close(sock); // close the listening socket in the child
                handleClient(clientSocket);
                close(clientSocket);
                exit(EXIT_SUCCESS);
            } else {
                close(clientSocket);
            }
        }
    }
}
