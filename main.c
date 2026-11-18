#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>
#include <pthread.h>

#define PORT 8080

void* handleClient(void* arg){
    int client_socket = *((int *)arg);
    free(arg);

    pthread_detach(pthread_self());
    
    char buffer[1024] = {0};
    ssize_t bytes_read = read(client_socket, buffer, sizeof(buffer) - 1);

    if (bytes_read > 0) {
        printf("--- Received Request ---\n%s\n", buffer);

        const char *response = "HTTP/1.1 200 OK\r\n"
                               "Content-Type: text/html; charset=UTF-8\r\n"
                               "\r\n"
                               "<html><body><h1>Hello from your Multi-Threaded C Server!</h1></body></html>";
        write(client_socket, response, strlen(response));
    }

    close(client_socket);
    return NULL;
}

int main() {
    int server_socket;
    struct sockaddr_in server_address;

    if ((server_socket = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("Creating the socket failed");
        exit(EXIT_FAILURE);
    }
    
    int opt = 1;
    if (setsockopt(server_socket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        perror("setsockopt failed");
        exit(EXIT_FAILURE);
    }

    server_address.sin_family = AF_INET;
    server_address.sin_port = htons(PORT);
    server_address.sin_addr.s_addr = INADDR_ANY;

    if (bind(server_socket, (struct sockaddr*)&server_address, sizeof(server_address)) < 0) {
        perror("Binding the socket failed");
        exit(EXIT_FAILURE);
    }

    if (listen(server_socket, 10) < 0) {
        perror("Socket listening failed");
        exit(EXIT_FAILURE);
    }

    printf("Multi-threaded server listening on port %d...\n", PORT);

    while (1) {
        struct sockaddr_in client_address;
        socklen_t client_addr_len = sizeof(client_address);

        int *client_socket_ptr = malloc(sizeof(int));
        if (!client_socket_ptr) {
            perror("Heap allocation failed");
            continue;
        }

        *client_socket_ptr = accept(server_socket, (struct sockaddr*)&client_address, &client_addr_len);
        if (*client_socket_ptr < 0) {
            perror("Accepting client failed");
            free(client_socket_ptr);
            continue;
        }

        pthread_t thread_id;

        if (pthread_create(&thread_id, NULL, handleClient, client_socket_ptr) != 0) {
            perror("Thread creation failed");
            close(*client_socket_ptr);
            free(client_socket_ptr);
        }
    }

    close(server_socket);
    return 0;
}