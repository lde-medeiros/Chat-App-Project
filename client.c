// client_file.c

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <pthread.h>

#define PORT 8080
#define BUFFER_SIZE 1024
#define FILE_BUFFER_SIZE 8192

// Struct to pass multiple arguments to threads
struct ThreadArgs {
    int client_socket;
};

// Function to handle sending messages from the client
void *handle_send(void *arg) {
    struct ThreadArgs *thread_args = (struct ThreadArgs *)arg;
    int client_socket = thread_args->client_socket;
    char buffer[BUFFER_SIZE];
    char file_name[BUFFER_SIZE];
    FILE *file;

    while (1) {
        memset(buffer, 0, sizeof(buffer));
        printf("Client: ");
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

// Function to handle receiving messages on the client
void *handle_receive(void *arg) {
    struct ThreadArgs *thread_args = (struct ThreadArgs *)arg;
    int client_socket = thread_args->client_socket;
    char buffer[BUFFER_SIZE];
    char file_name[BUFFER_SIZE];
    FILE *file;

    while (1) {
        memset(buffer, 0, sizeof(buffer));
        if (recv(client_socket, buffer, sizeof(buffer), 0) == 0) {
            // Connection closed by the server
            printf("Connection closed by the server\n");
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
            printf("Server says: %s", buffer);
        }
    }

    return NULL;
}

int main() {
    int client_socket;
    struct sockaddr_in server_addr;
    pthread_t send_thread, receive_thread;
    struct ThreadArgs thread_args;

    // Create socket
    if ((client_socket = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
        perror("Socket creation failed");
        exit(EXIT_FAILURE);
    }

    // Initialize server address struct
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = inet_addr("127.0.0.1");  // Change this to the server's IP address if not running locally
    server_addr.sin_port = htons(PORT);

    // Connect to the server
    if (connect(client_socket, (struct sockaddr *)&server_addr, sizeof(server_addr)) == -1) {
        perror("Connection failed");
        exit(EXIT_FAILURE);
    }

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

    // The main thread is not doing any work here, so we can pause it to let the other threads run
    pause();

    // Close the client socket (this code is unreachable in the current implementation)
    close(client_socket);

    return 0;
}

// gcc client.c -o client
