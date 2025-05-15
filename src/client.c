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
#include <poll.h>
#include <termios.h>

#define PORT 7270
#define BUFFERSIZE 1024
#define MAXUSERNAMELEN 65


static struct termios originalTerminal;

void disableRawMode() {
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &originalTerminal);
}

void enableRawMode() {
    tcgetattr(STDIN_FILENO, &originalTerminal);
    atexit(disableRawMode);

    struct termios rawTerminal = originalTerminal;
    rawTerminal.c_lflag &= ~(ECHO | ICANON); 
    // waits up to VTIME, returns as soon as data is available or timeout
    rawTerminal.c_cc[VMIN] = 0;
    rawTerminal.c_cc[VTIME] = 1;
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &rawTerminal);
}

void redrawPrompt(const char* prompt, char* buffer, size_t len) {
    write(STDOUT_FILENO, "\r\033[K", 4); // clear the current line
    write(STDOUT_FILENO, prompt, strlen(prompt));
    write(STDOUT_FILENO, buffer, len);
}

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

int exitCommand(const char* buffer) {
    if (strncmp(buffer, "/exit", 5) == 0) {
        const char* exitMessage = "Client Exit...\n";
        write(STDOUT_FILENO, exitMessage, strlen(exitMessage));
        return 1;
    }
    return 0;
}

int handleInput(int socket, char* buffer, size_t* bufferLen, const char* prompt) {
    char c;
    ssize_t n = read(STDIN_FILENO, &c, 1);
    if (n <= 0) {
        return 0;
    }

    if (c == '\r' || c == '\n') { 
        write(STDOUT_FILENO, "\r\n", 2);
         
        // === CHAT FUNCTIONS ===
        if (exitCommand(buffer)) {
            return 1;
        }
        // ======================

        if (*bufferLen > 0) {
            write(socket, buffer, *bufferLen);
        }

        *bufferLen = 0;
        memset(buffer, 0, BUFFERSIZE);
        write(STDOUT_FILENO, prompt, strlen(prompt));
    } else if (c == 127 || c == '\b') {
        if (*bufferLen) {
            (*bufferLen)--;
            write(STDOUT_FILENO, "\b \b", 3);
        }
    } else if (*bufferLen < BUFFERSIZE - 1) {
        buffer[(*bufferLen)++] = c;
        write(STDOUT_FILENO, &c, 1);
    }

    return 0;
}

int handleBroadcasting(int socket, char* buffer, size_t bufferLen, const char* prompt) {
    char message[BUFFERSIZE + 1];
    ssize_t n = read(socket, message, BUFFERSIZE);
    if (n <= 0) {
        return 1;
    }

    message[n] = '\0';
    write(STDOUT_FILENO, "\r\033[K", 4);
    write(STDOUT_FILENO, message, n);
    redrawPrompt(prompt, buffer, bufferLen);
    return 0;
}

void chat(int socket, const char* username) {
    enableRawMode();

    char prompt[MAXUSERNAMELEN + 4];
    snprintf(prompt, sizeof(prompt), "[%s]: ", username);
    char buffer[BUFFERSIZE] = {0};
    size_t bufferLen = 0;
    
    write(STDOUT_FILENO, prompt, strlen(prompt));

    struct pollfd fds[2];
    fds[0].fd = STDIN_FILENO; // user typing
    fds[0].events = POLLIN;
    fds[1].fd = socket; // server messages
    fds[1].events = POLLIN;

    while (1) {
        if ((poll(fds, 2, -1)) < 0) {
            perror("poll");
            break; 
        }        

        if (fds[0].revents & POLLIN) {
            if(handleInput(socket, buffer, &bufferLen, prompt)) {
                break;
            }
        }
        if (fds[1].revents & POLLIN) {
            if (handleBroadcasting(socket, buffer, bufferLen, prompt)) {
                break;
            }
        }
    }
}

int main(int args, char* argv[]) {
    int sock;
    struct sockaddr_in6 serverAddr;
    
    sock = createSocket();
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
    username[strcspn(username, "\n")] = '\0'; 
    write(sock, username, strlen(username));

    chat(sock, username);
    close(sock);
}
