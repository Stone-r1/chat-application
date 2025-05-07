#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <string.h>
#include <stdio.h>

#define MYPORT 7270
#define MAXUSERS 5
#define BUFFERSIZE 1025

/* TODO
 * add multpiple client handling (concurency)
 * log client IP/port
 * normal shut down handling
 * handle partial reads/writes
 * add signal handler to close socket
*/

int main() {

    // add separate function for readability
    char buffer[BUFFERSIZE] = {0};
    int sock = socket(AF_INET6, SOCK_STREAM, 0), newSock; // creating socket 
    if (sock < 0) {
        perror("socket");
        return 1;
    }    
    
    int opt = 1;
    setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in6 clientAddr, serverAddr; // ipv6 socket address
    socklen_t clientAddrLen = sizeof(clientAddr), serverAddrLen = sizeof(serverAddr);
    
    memset(&serverAddr, 0, serverAddrLen);
    serverAddr.sin6_family = AF_INET6; // specifies family
    serverAddr.sin6_addr = in6addr_any; // bind to any local address
    serverAddr.sin6_port = htons(MYPORT);
    
    // binds local name to the socket
    if (bind(sock, (struct sockaddr*) &serverAddr, serverAddrLen) < 0) {
        perror("bind");
        return 1;
    }    

    // up to 5 users TODO: add concurency as of right now it receives one user and then dies.
    if (listen(sock, MAXUSERS) < 0) {
        perror("listen");
        return 1;
    }

    // replace with a loop and threading/forking for multiple clients
    if ((newSock = accept(sock, (struct sockaddr*) &clientAddr, &clientAddrLen)) < 0) {
        perror("accept");
        return 1;
    }

    // move to a separate function
    int message;
    while ((message = read(newSock, buffer, BUFFERSIZE - 1)) > 0) {
        buffer[message] = '\0';
        printf("Client: %s", buffer);
        write(newSock, "Server has received your message!\n", 34);
    };

    close(newSock);
    close(sock);
}
