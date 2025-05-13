#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <arpa/inet.h>
#include <net/if.h>

#define PORT 7270
#define BUFFERSIZE 1024
#define MAXUSERNAMELEN 65

int createSocket() {
    int sock = socket(AF_INET6, SOCK_STREAM, 0);
    if (sock < 0) {
        perror("socket");
        exit(0);
    }
    return sock;
}

void assignConnection(struct sockaddr_in6* addr) {
    memset(addr, 0, sizeof(struct sockaddr_in6));
    
    struct addrinfo hints, *res;
    const char* serverIP = "::1";
    int status;
    
    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_INET6;
    hints.ai_socktype = SOCK_STREAM;

    if ((status = getaddrinfo(serverIP, NULL, &hints, &res)) != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(status));
        exit(EXIT_FAILURE);
    }

    memcpy(addr, res -> ai_addr, sizeof(struct sockaddr_in6));
    addr -> sin6_port = htons(PORT);
    freeaddrinfo(res);
}

void chat(int socket, const char* username) {
    // tempBuffer => raw message | finalBuffer => username + message
    char tempBuffer[BUFFERSIZE + 1] = {0};
    char finalBuffer[BUFFERSIZE + MAXUSERNAMELEN] = {0};
    int n;

    while (1) {
        memset(tempBuffer, 0, sizeof(tempBuffer));
        printf("%s: ", username); // show username locally
        n = 0;
        while ((tempBuffer[n] = getchar()) != '\n' && n < BUFFERSIZE - 1) {
            n++;
        }
        tempBuffer[n] = '\0';

        write(socket, tempBuffer, strlen(tempBuffer));
        bzero(finalBuffer, sizeof(finalBuffer));
        read(socket, finalBuffer, sizeof(finalBuffer));
        printf("From server: %s\n", finalBuffer);
        
        const char* errorMessage = "exit\n";
        if ((strncmp(tempBuffer, errorMessage, sizeof(errorMessage))) == 0) {
            printf("Client Exit...\n");
            break;
        }
    }
}

int main(int args, char* argv[]) {
    int sock;
    struct sockaddr_in6 serverAddr;
    
    sock = createSocket();
    // assign IP/Port
    assignConnection(&serverAddr);
    if (connect(sock, (struct sockaddr*) &serverAddr, sizeof(serverAddr)) != 0) {
        printf("Connection failed...\n");
        perror("connect");
        exit(0);
    } else {
        printf("Connection established\n");
    }

    char username[MAXUSERNAMELEN];
    printf("Enter your username: ");
    fgets(username, sizeof(username), stdin);
    username[strcspn(username, "\n")] = '\0'; // find first of
    write(sock, username, strlen(username)); // send username outright

    chat(sock, username);
    close(sock);
}
