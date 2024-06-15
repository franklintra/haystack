/**
 * @file tcp-test-server.c
 * @brief A tcp server to test our current implementation
 */

#include "socket_layer.h"
#include "error.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>

#define BUFFER_SIZE 8192
#define DELIMITER "<EOF>"
#define SUCCESS "200 OK"
#define SUCCESS_SIZE 7
#define FILE_SIZE_LIMIT 4096

int main(int argc, char *argv[])
{
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <port>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    uint16_t port = (uint16_t) atoi(argv[1]);
    int server_fd = tcp_server_init(port);
    if (server_fd < 0) {
        perror("Server initialization failed");
        exit(EXIT_FAILURE);
    }

    printf("Server started on port %d\n", port);

    while (1) {
        int new_socket = tcp_accept(server_fd);
        if (new_socket < 0) {
            perror("Accept failed");
            continue;
        }
        printf("Connection accepted.\n");

        printf("Waiting for a size...\n");

        char buffer[BUFFER_SIZE] = {0};
        tcp_read(new_socket, buffer, BUFFER_SIZE);
        // find the delimiter to separate file size from the rest
        char *delimiter_position = strstr(buffer, DELIMITER);
        if (delimiter_position != NULL) {
            *delimiter_position = '\0';
            long filesize = atol(buffer);
            printf("Received a size: %ld --> ", filesize);

            if (filesize < FILE_SIZE_LIMIT) {
                tcp_send(new_socket, SUCCESS, strlen(SUCCESS));
                printf("Accepted\n");
            } else {
                printf("Rejected\n");
                close(new_socket);
                continue;
            }

            memset(buffer, 0, BUFFER_SIZE);
            printf("About to receive file of %ld bytes\n", filesize);
            read(new_socket, buffer, BUFFER_SIZE);
            // look for the EOF delimiter to mark the end of the file
            char *file_end = strstr(buffer, DELIMITER);
            if (file_end != NULL) {
                *file_end = '\0';
            }
            printf("Received a file:\n%s\n", buffer);
            send(new_socket, SUCCESS, SUCCESS_SIZE, 0);
        } else {
            printf("File size delimiter not found.\n");
        }
        close(new_socket);
    }
}
