#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/stat.h>
#include <fcntl.h>

#define PORT 8080

char** parseRequest(char* request) {
    if (!request || strlen(request) == 0 || request[0] == ' ') {
        fprintf(stderr, "Invalid request line\n");
        return NULL;
    }

    size_t method_len = strcspn(request, " ");
    char* method = malloc(method_len + 1);
    if (!method) return NULL;
    
    memcpy(method, request, method_len);
    method[method_len] = '\0'; 

    char* ptr = request + method_len;
    while (*ptr == ' ') ptr++;

    size_t path_len = strcspn(ptr, " \r\n");
    char* path = malloc(path_len + 1);
    if (!path) {
        free(method);
        return NULL;
    }

    memcpy(path, ptr, path_len);
    path[path_len] = '\0';

    char** parsed = malloc(2 * sizeof(char*));
    if (!parsed) {
        free(method);
        free(path);
        return NULL;
    }

    parsed[0] = method;
    parsed[1] = path;

    return parsed;
}

void handleGETRequest(int client_socket, const char* path){
    char file_path[1024];

    if(strcmp(path, "/") == 0){
        snprintf(file_path, sizeof(file_path), "www/index.html");
    }
    else{
        snprintf(file_path, sizeof(file_path), "www%s", path);
    }

    struct stat st;

    if(stat(file_path, &st) < 0 || S_ISDIR(st.st_mode)){
        const char* body = "<html><body><h1>404 Not Found</h1></body></html>";
        char header[512];
        snprintf(header, sizeof(header),
                 "HTTP/1.1 404 Not Found\r\n"
                 "Content-Type: text/html; charset=UTF-8\r\n"
                 "Content-Length: %zu\r\n"
                 "\r\n", strlen(body));
        
        write(client_socket, header, strlen(header));
        write(client_socket, body, strlen(body));
        return;
    }

    FILE *file = fopen(file_path, "rb");
    if (!file) {
        perror("Failed to open file");
        return;
    }

    char header[512];
    snprintf(header, sizeof(header),
             "HTTP/1.1 200 OK\r\n"
             "Content-Type: text/html; charset=UTF-8\r\n"
             "Content-Length: %ld\r\n"
             "\r\n", st.st_size);
    write(client_socket, header, strlen(header));

    char buffer[4096];
    size_t bytes_read;
    while((bytes_read = fread(buffer, 1, sizeof(buffer), file)) > 0){
        write(client_socket, buffer, bytes_read);
    }
    
    fclose(file);
}

void* handleClient(void* arg){
    int client_socket = *((int *)arg);
    free(arg);

    pthread_detach(pthread_self());
    
    char buffer[2048] = {0};
    ssize_t bytes_read = read(client_socket, buffer, sizeof(buffer) - 1);

    if (bytes_read > 0) {
        printf("--- Received Request ---\n%s\n", buffer);

        char** parsed = parseRequest(buffer);
        if (parsed) {
            printf("Method: %s | Path: %s\n", parsed[0], parsed[1]);
            
            if(strcmp(parsed[0], "GET") == 0){
                handleGETRequest(client_socket, parsed[1]);
            }

            free(parsed[0]);
            free(parsed[1]);
            free(parsed);
        }
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