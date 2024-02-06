// server_file.c

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <pthread.h>

#define PORT 8080
#define MAX_CONNECTIONS 5
#define BUFFER_SIZE 1024
#define FILE_BUFFER_SIZE 8192

// Struct to pass multiple arguments to threads
struct ThreadArgs {
    int client_socket;
};

// Function to handle sending messages from the server
void *handle_send(void *arg) {
    struct ThreadArgs *thread_args = (struct ThreadArgs *)arg;
    int client_socket = thread_args->client_socket;
    char buffer[BUFFER_SIZE];
    char file_name[BUFFER_SIZE];
    FILE *file;

    while (1) {
        memset(buffer, 0, sizeof(buffer));
        printf("Server: ");
        fgets(buffer, sizeof(buffer), stdin);

        // Check if the command is to send a file
        if (strncmp(buffer, "/sendfile", 9) == 0) {
            sscanf(buffer, "/sendfile %s", file_name);

            // Open the file for reading
            file = fopen(file_name, "rb");
            if (file == NULL) {
                perror("File not found");
                continue;
            }

            // Read and send the file in chunks
            size_t bytesRead;
            char file_buffer[FILE_BUFFER_SIZE];
            while ((bytesRead = fread(file_buffer, 1, sizeof(file_buffer), file)) > 0) {
                send(client_socket, file_buffer, bytesRead, 0);
            }

            // Signal end of file transfer
            send(client_socket, "FILE_TRANSFER_COMPLETE", sizeof("FILE_TRANSFER_COMPLETE"), 0);

            // Close the file
            fclose(file);

            printf("File sent: %s\n", file_name);
        } else {
            // Send regular messages
            send(client_socket, buffer, strlen(buffer), 0);
        }
    }

    return NULL;
}

// Function to handle receiving messages on the server
void *handle_receive(void *arg) {
    struct ThreadArgs *thread_args = (struct ThreadArgs *)arg;
    int client_socket = thread_args->client_socket;
    char buffer[BUFFER_SIZE];
    char file_name[BUFFER_SIZE];
    FILE *file;

    while (1) {
        memset(buffer, 0, sizeof(buffer));
        if (recv(client_socket, buffer, sizeof(buffer), 0) == 0) {
            // Connection closed by the client
            printf("Connection closed by the client\n");
            break;
        }

        // Check if the received message is a file
        if (strncmp(buffer, "/receivefile", 12) == 0) {
            sscanf(buffer, "/receivefile %s", file_name);

            // Open the file for writing
            file = fopen(file_name, "wb");
            if (file == NULL) {
                perror("File creation failed");
                break;
            }

            // Receive and write the file in chunks
            while (1) {
                memset(buffer, 0, sizeof(buffer));
                ssize_t bytesRead = recv(client_socket, buffer, sizeof(buffer), 0);
                if (bytesRead <= 0) {
                    // Either connection closed or error
                    break;
                }

                // Check for the end of file transfer signal
                if (strncmp(buffer, "FILE_TRANSFER_COMPLETE", sizeof("FILE_TRANSFER_COMPLETE")) == 0) {
                    break;
                }

                // Write the received data to the file
                fwrite(buffer, 1, bytesRead, file);
            }

            // Close the file
            fclose(file);

            printf("File received: %s\n", file_name);
        } else {
            // Print regular messages
            printf("Client says: %s", buffer);
        }
    }

    return NULL;
}

int main() {
    int server_socket, client_socket;
    struct sockaddr_in server_addr, client_addr;
    pthread_t send_thread, receive_thread;
    struct ThreadArgs thread_args;

    // Create socket
    if ((server_socket = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
        perror("Socket creation failed");
        exit(EXIT_FAILURE);
    }

    // Initialize server address struct
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(PORT);

    // Bind the socket
    if (bind(server_socket, (struct sockaddr *)&server_addr, sizeof(server_addr)) == -1) {
        perror("Binding failed");
        exit(EXIT_FAILURE);
    }

    // Listen for incoming connections
    if (listen(server_socket, MAX_CONNECTIONS) == -1) {
        perror("Listening failed");
        exit(EXIT_FAILURE);
    }

    printf("Server listening on port %d...\n", PORT);

    // Accept connections and handle messages
    while (1) {
        socklen_t addr_len = sizeof(client_addr);
        if ((client_socket = accept(server_socket, (struct sockaddr *)&client_addr, &addr_len)) == -1) {
            perror("Acceptance failed");
            exit(EXIT_FAILURE);
        }

        printf("Connection established with %s:%d\n", inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));

        // Set up thread arguments
        thread_args.client_socket = client_socket;

        // Create threads for sending and receiving messages
        if (pthread_create(&send_thread, NULL, handle_send, (void *)&thread_args) != 0 ||
            pthread_create(&receive_thread, NULL, handle_receive, (void *)&thread_args) != 0) {
            perror("Thread creation failed");
            exit(EXIT_FAILURE);
        }

        // Wait for the threads to finish (this will not block the main loop)
        pthread_detach(send_thread);
        pthread_detach(receive_thread);
    }

    // Close the server socket (this code is unreachable in the current implementation)
    close(server_socket);

    return 0;
}
