/**
 * @file tcp-test-client.c
 * @brief A tcp client to test our current implementation
 */


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include "error.h"
#include "util.h"

#define BUFFER_SIZE 8192
#define DELIMITER "<EOF>"
#define SUCCESS "200 OK"
#define FILE_SIZE_LIMIT 4096

int main(int argc, char *argv[])
{
    if (argc != 3) {
        fprintf(stderr, "Usage: %s <port> <file>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    uint16_t port = (uint16_t) atoi(argv[1]);
    const char* filename = argv[2];

    // open the file
    FILE *file = fopen(filename, "rb");
    if (!file) {
        perror("Failed to open file");
        exit(EXIT_FAILURE);
    }

    // Get file size
    fseek(file, 0, SEEK_END);
    long filesize = ftell(file);
    fseek(file, 0, SEEK_SET);

    if (filesize > FILE_SIZE_LIMIT) {
        fprintf(stderr, "File is too large.\n");
        fclose(file);
        exit(EXIT_FAILURE);
    }

    // create new socket
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        perror("Socket creation failed");
        fclose(file);
        exit(EXIT_FAILURE);
    }

    // defin server address
    struct sockaddr_in server_address;
    zero_init_var(server_address);
    server_address.sin_family = AF_INET;
    server_address.sin_port = htons(port);
    server_address.sin_addr.s_addr = inet_addr("127.0.0.1");

    // connect to the server
    if (connect(sock, (struct sockaddr*)&server_address, sizeof(server_address)) < 0) {
        perror("Connection failed");
        close(sock);
        fclose(file);
        exit(EXIT_FAILURE);
    }

    printf("Talking to %d\n", port);

    // send the file size
    char size_msg[BUFFER_SIZE];
    snprintf(size_msg, BUFFER_SIZE, "%ld", filesize);
    strcat(size_msg, DELIMITER); // Append delimiter
    if (send(sock, size_msg, strlen(size_msg), 0) < 0) {
        perror("Failed to send file size");
        close(sock);
        fclose(file);
        exit(EXIT_FAILURE);
    }

    printf("Sending size %ld:\n", filesize);

    // wait for server acknowledgment
    char ack[BUFFER_SIZE];
    if (recv(sock, ack, BUFFER_SIZE, 0) < 0) {
        perror("Failed to receive acknowledgment");
        close(sock);
        fclose(file);
        exit(EXIT_FAILURE);
    }

    // check server response
    if (strcmp(ack, SUCCESS) != 0) {
        fprintf(stderr, "Server responded: \"%s\"\n", ack);
        close(sock);
        fclose(file);
        exit(EXIT_FAILURE);
    }

    printf("Sending %s:\n", filename);

    // send the content
    char buffer[BUFFER_SIZE];
    size_t bytes_read;
    while ((bytes_read = fread(buffer, 1, BUFFER_SIZE, file)) > 0) {
        if (send(sock, buffer, bytes_read, 0) < 0) {
            perror("Failed to send file content");
            close(sock);
            fclose(file);
            exit(EXIT_FAILURE);
        }
    }


    // Append EOF str
    send(sock, DELIMITER, strlen(DELIMITER), 0);

    printf("Accepted\n");

    // wait for final response
    if (recv(sock, ack, BUFFER_SIZE, 0) < 0) {
        perror("Failed to receive final acknowledgment");
        close(sock);
        fclose(file);
        exit(EXIT_FAILURE);
    }

    printf("Done\n");

    // clean
    close(sock);
    fclose(file);

    return 0;
}
