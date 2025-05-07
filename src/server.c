#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <string.h>
#include <stdio.h>

#define MYPORT 7270

int main() {
    int buffer[1025] = {0};
    int s = socket(AF_INET6, SOCK_STREAM, 0), newSocket; // creating socket 
    if (s < 0) {
        perror("socket");
        return 1;
    }    

    struct sockaddr_in6 sin6; // ipv6 socket address

    memset(&sin6, 0, sizeof(sin6));
    sin6.sin6_family = AF_INET6; // specifies family
    sin6.sin6_addr = in6addr_any; // bind to any local address
    sin6.sin6_port = htons(MYPORT);
    
    // binds local name to the socket
    if (bind(s, (struct sockaddr*) &sin6, sizeof sin6) < 0) {
        perror("bind");
        return 1;
    }    

    if (listen(s, 5) < 0) {
        perror("listen");
        return 1;
    }

    socklen_t addrLen = sizeof(sin6);

    if ((newSocket = accept(s, (struct sockaddr*) &sin6, &addrLen)) < 0) {
        perror("accept");
        return 1;
    }

    message = read(newSocket, buffer, 1024);
    printf("\s\n", message);

    close(newSocket);
    close(s);
    // printf("everything works damn\n");
}
