#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>

int port = 63321;
#define BUFFER_SIZE 2077
int server_fd, new_socket;
struct sockaddr_in address;
int addrlen = sizeof(address);
char incomingBuffer[BUFFER_SIZE] = {0};
char outgoingBuffer[BUFFER_SIZE] = {0};
static pthread_t thread_id;

void* acceptMessage(void *arg) {
    int sock = *(int*)arg;
    free(arg);
    char buffer[BUFFER_SIZE] = {0};
    char response[128] = {0};

    while (1) {
        memset(buffer, 0, BUFFER_SIZE);
        int valread = read(sock, buffer, BUFFER_SIZE - 1);

        if (valread <= 0) {
            printf("\nclient disconnected (sock %d)", sock);
            break;
        }

        buffer[valread] = '\0';
        printf("\nrecieved from client: %s", buffer);

        if (strcmp(buffer, "test/") == 0) {
            strcpy(response, "ok");
        }
        else if (strncmp(buffer, "recieve-message/", 16) == 0) {
            // TODO: сохранить сообщение в базу
            printf("\nsaving message: %s", buffer);
            strcpy(response, "ok");
        }
        else if (strncmp(buffer, "createId/", 9) == 0) {
            srand(time(NULL) + clock());
            long id = rand()%2147483647;
            sprintf(response, "%ld", id);
        }
        else if (strncmp(buffer, "save-profile/", 13) == 0) {
            printf("\n[save-profile] received: %s\n", buffer);

            char *parts[10] = {0};
            int count = 0;
            char *token = strtok(buffer + 13, "\x1E");

            while (token && count < 10) {
                parts[count++] = token;
                token = strtok(NULL, "\x1E");
            }

            // TODO save tp database
            if (count >= 6) {
                printf("  UserID:       %s\n", parts[0]);
                printf("  Name:         %s\n", parts[1]);
                printf("  Email:        %s\n", parts[2]);
                printf("  PasswordHash: %s\n", parts[3]);
                printf("  AvatarUrl:    %s\n", parts[4]);
                printf("  ProfileDesc:  %s\n", parts[5]);
            }

            strcpy(response, "ok");
        }

        send(sock, response, strlen(response), 0);
        printf("\nsent responce for request: %s -> %s\n", buffer, response);
    }

    close(sock);
    return NULL;
}

int main(void) {
    // Create socket v4
    server_fd = socket(AF_INET, SOCK_STREAM, 0);

    // Define server address
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY; // Listen on all interfaces
    address.sin_port = htons(port);

    // Bind port
    bind(server_fd, (struct sockaddr *)&address, sizeof(address));

    // Start listening
    listen(server_fd, 10);
    printf("---");
    printf("\n");
    printf("server is listening on port %d", port);
    printf("\n");
    printf("---");

    while (1) {
        new_socket = accept(server_fd, (struct sockaddr *)&address, (socklen_t*)&addrlen);
        if (new_socket < 0) continue;

        // CRITICAL: We must allocate memory for the socket ID so it isn't
        // overwritten by the next accept() before the thread starts!
        int* client_sock_ptr = malloc(sizeof(int));
        *client_sock_ptr = new_socket;

        if (pthread_create(&thread_id, NULL, acceptMessage, client_sock_ptr) == 0) {
            // Tell the OS to reclaim thread resources automatically on exit
            pthread_detach(thread_id);
        } else {
            close(new_socket);
            free(client_sock_ptr);
        }
    }
    return 0;

    // Cleanup
    close(new_socket);
    close(server_fd);
    printf("\n");
    return 0;
}
