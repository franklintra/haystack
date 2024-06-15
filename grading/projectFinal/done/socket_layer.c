/**
 * @file socket_layer.c
 * @brief Socket layer of the project (TCP) for network communication.
 *
 * This file primarily consists of interfaces to the corresponding
 * system-level socket operations.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include "error.h"
#include "util.h"

#define MAX_PENDING_CONNECTIONS 25

/**
 * @brief Initializes the TCP server socket.
 *
 * @param port Port number.
 * @return The file descriptor or ERR_IO if there is one.
 */
int tcp_server_init(uint16_t port)
{
    int server_fd;
    struct sockaddr_in address;

    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd == -1) {
        perror("The creation of the socket failed");
        return ERR_IO;
    }

    int optval = 1;
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval)) < 0) {
        perror("Setting socket option failed");
        close(server_fd);
        return ERR_IO;
    }

    // Set up the server address structure
    zero_init_var(address);
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(port);

    // Bind the socket to the address
    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
        perror("The binding of the socket failed");
        close(server_fd);
        return ERR_IO;
    }

    // Start listening on the socket
    if (listen(server_fd, MAX_PENDING_CONNECTIONS) < 0) {
        perror("The start to listen failed");
        close(server_fd);
        return ERR_IO;
    }

    return server_fd;
}

/**
 * @brief Accepts new connections on a TCP server socket.
 *
 * @param passive_socket File descriptor of the socket.
 * @return The file descriptor or -1 if an error occurs.
 */
int tcp_accept(int passive_socket)
{
    return accept(passive_socket, NULL, NULL);
}


/**
 * @brief Reads data from TCP socket.
 *
 * @param active_socket The file descriptor of the socket to read from.
 * @param buf The buffer to store the data we read.
 * @param buf_len Length of the buffer.
 * @return Number of bytes read, ERR_IO on error, or ERR_INVALID_ARGUMENT if args are invalid.
 */
ssize_t tcp_read(int active_socket, char* buf, size_t buf_len)
{
    debug_printf("buffer length: %ld\n", buf_len);
    if (buf == NULL || buf_len == 0) {
        debug_printf("Invalid buffer or buffer length\n", NULL);
        return ERR_INVALID_ARGUMENT;
    }
    ssize_t result = recv(active_socket, buf, buf_len, 0);
    debug_printf("result: %ld\n", result);
    if (result < 0) {
        printf("recv failed");
        return ERR_IO;
    }
    return result;
}

/**
 * @brief Sends data over a TCP socket.
 *
 * @param active_socket File descriptor of the socket to send data to.
 * @param response The data to be sent.
 * @param response_len The length of the data to be sent.
 * @return Number of bytes sent, ERR_IO on error, or ERR_INVALID_ARGUMENT if arguments are invalid.
 */
ssize_t tcp_send(int active_socket, const char* response, size_t response_len)
{
    M_REQUIRE_NON_NULL(response);
    if (active_socket < 0 || response_len <= 0) return ERR_INVALID_ARGUMENT;
    ssize_t result = send(active_socket, response, response_len, 0);
    if (result < 0) return ERR_IO;
    return result;
}
