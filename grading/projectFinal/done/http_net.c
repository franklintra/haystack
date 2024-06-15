/*
 * @file http_net.c
 * @brief HTTP server layer for CS-202 project
 *
 * @author Konstantinos Prasopoulos
 */

#include <stdlib.h>
#include <stdio.h>
#include <sys/socket.h>
#include <string.h>
#include <stdint.h>
#include <unistd.h>
#include <signal.h>

#include "http_prot.h"
#include "http_net.h"
#include "socket_layer.h"
#include "error.h"
#include "util.h"
#include <pthread.h>

static int passive_socket = -1;
static EventCallback cb;

#define MK_OUR_ERR(X) \
static int our_ ## X = X

MK_OUR_ERR(ERR_NONE);
MK_OUR_ERR(ERR_INVALID_ARGUMENT);
MK_OUR_ERR(ERR_OUT_OF_MEMORY);
MK_OUR_ERR(ERR_IO);

#define MAX_SIZE_T_STRING_SIZE 19


/**
 * @brief Handles a client connection for HTTP message processing.
 *
 * It manages the entire process of handling a client connection, including reading HTTP headers, parsing the
 * HTTP message, and reading the message content.
 *
 * @param arg Pointer to the client socket file descriptor.
 * @return Pointer to the error on failure, or our_ERR_NONE on success.
 */
static void *handle_connection(void *arg)
{
    sigset_t mask;
    sigemptyset(&mask);
    sigaddset(&mask, SIGINT );
    sigaddset(&mask, SIGTERM);
    pthread_sigmask(SIG_BLOCK, &mask, NULL);

    if (arg == NULL) return &our_ERR_INVALID_ARGUMENT;

    int client_socket = *(int*)arg;
    free(arg);
    int err = our_ERR_NONE;
    char *buffer = (char*)malloc(MAX_HEADER_SIZE + 1);
    if (buffer == NULL) {
        return &our_ERR_OUT_OF_MEMORY;
    }
    zero_init_ptr(buffer);
    ssize_t received = 0;
    size_t total_received = 0;
    size_t actual_header_size = 0;
    int content_len = 0;
    int parse_result = 0;
    struct http_message message;

    // Load header until HTTP_HDR_END_DELIM is reached
    while (strstr(buffer, HTTP_HDR_END_DELIM) == NULL && total_received < MAX_HEADER_SIZE) {
        received = tcp_read(client_socket, buffer + total_received, MAX_HEADER_SIZE - total_received);
        if (received < 0) {
            err = our_ERR_IO;
            perror("Error while reading\n");
            break;
        } else if (received == 0) {
            perror("No data received, closing connection\n");
            close(client_socket);
            free(buffer);
            return &our_ERR_NONE;
        }
        total_received += (size_t)received;
    }

    // Finding the actual header size
    char *header_end = strstr(buffer, HTTP_HDR_END_DELIM);
    if (header_end != NULL) {
        actual_header_size = (size_t)(header_end - buffer) + strlen(HTTP_HDR_END_DELIM);
    }


    parse_result = http_parse_message(buffer, actual_header_size, &message, &content_len);
    debug_printf("parse_result: %d\n", parse_result);
    size_t buffer_received = 0;
    if (parse_result == 0) {
        // reallocate buffer for content based on Content-Length and read the content
        buffer = (char*)realloc(buffer, actual_header_size + (size_t) content_len + 1);
        if (buffer == NULL) {
            return &our_ERR_OUT_OF_MEMORY;
        }
        buffer_received = total_received - actual_header_size;
        debug_printf("content_len: %d\n", content_len);
        debug_printf("buffer_received: %zu\n", buffer_received);
        while (buffer_received < (size_t) content_len) {
            received = tcp_read(client_socket, buffer + total_received, actual_header_size + ((size_t) content_len) - total_received);
            if (received < 0) {
                err = our_ERR_IO;
                perror("Error while reading content\n");
                break;
            } else if (received == 0) {
                break;
            }
            buffer_received += (size_t)received;
            total_received += (size_t)received;
        }
    } else {
        // Invoke callback on message parsing error
        err = cb(&message, client_socket);
        debug_printf("Callback returned: %d\n", err);
        close(client_socket);
        free(buffer);
        return &our_ERR_INVALID_ARGUMENT;
    }

    debug_printf("buffer_received: %zu\n", buffer_received);
    debug_printf("content_len: %d\n", content_len);
    debug_printf("total_received: %zu\n", buffer_received + actual_header_size);
    debug_printf("total_received: %zu\n", total_received);
    if (buffer_received == (size_t) content_len) {
        // parse again now everything is received
        parse_result = http_parse_message(buffer, total_received, &message, &content_len);
        debug_printf("Parse result: %d\n", parse_result);
        if (parse_result == 1) {
            err = our_ERR_NONE;
        } else {
            err = our_ERR_IO;
        }
    } else {
        err = our_ERR_IO;
    }

    err = cb(&message, client_socket);
    if (err != ERR_NONE) {
        perror("Error while invoking callback\n");
        if (buffer != NULL) {
            free(buffer);
        }
        close(client_socket);
        return &our_ERR_IO;
    }

    if (buffer != NULL) {
        free(buffer);
    }

    close(client_socket);
    return &our_ERR_NONE;
}


/**
 * @brief Initializes the HTTP server.
 *
 * @param port The port number.
 * @param callback The callback function.
 * @return The passive socket file descriptor on success, ERR_IO on failure.
 */
int http_init(uint16_t port, EventCallback callback)
{
    passive_socket = tcp_server_init(port);
    if (passive_socket < 0) {
        return ERR_IO;
    }
    cb = callback;
    return passive_socket;
}

/**
 * @brief Closes the HTTP server.
 */
void http_close(void)
{
    if (passive_socket > 0) {
        if (close(passive_socket) == -1)
            debug_printf("close() in http_close()", NULL);
        else
            passive_socket = -1;
    }
}


/**
 * @brief Receives and handles HTTP connections.
 *
 * This function accepts a connection using tcp_accept() and
 * creates a thread to handle the connection using the function handle_connection().
 *
 * @return The error (ERR_NONE = 0 if success).
 */
int http_receive(void)
{
    int client_socket = tcp_accept(passive_socket);
    if (client_socket < 0) {
        perror("Accept failed in client socket\n");
        return ERR_IO;
    }

    // alloc memory on the heap for the client socket and free the pointer in the thread
    int *active_socket = malloc(sizeof(int));
    if (active_socket == NULL) {
        perror("Failed to allocate memory for active socket\n");
        return ERR_OUT_OF_MEMORY;
    }
    *active_socket = client_socket;

    // init pthread attributes
    pthread_attr_t attr;
    if (pthread_attr_init(&attr) != 0) {
        perror("Failed to initialize pthread attributes\n");
        free(active_socket);
        return ERR_IO;
    }

    // set the pthread attributes as detached
    if (pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED) != 0) {
        perror("Failed to set pthread attributes to PTHREAD_CREATE_DETACHED\n");
        pthread_attr_destroy(&attr);
        free(active_socket);
        return ERR_IO;
    }

    // create a new thread to run handle_connection
    pthread_t thread;
    if (pthread_create(&thread, &attr, handle_connection, active_socket) != 0) {
        perror("Failed to create a new thread\n");
        pthread_attr_destroy(&attr);
        free(active_socket);
        return ERR_IO;
    }

    // destoy the pthread attributes at the end
    pthread_attr_destroy(&attr);

    debug_printf("Connection accepted and handled.\n", NULL);
    return ERR_NONE;
}

/**
 * @brief Serves a file over an HTTP connection.
 *
 * @param connection The connection file descriptor.
 * @param filename The name of the file to be served.
 * @return HTTP response status code.
 */
int http_serve_file(int connection, const char* filename)
{
    M_REQUIRE_NON_NULL(filename);

    // open file
    FILE* file = fopen(filename, "r");
    if (file == NULL) {
        fprintf(stderr, "http_serve_file(): Failed to open file \"%s\"\n", filename);
        return http_reply(connection, "404 Not Found", "", "", 0);
    }

    // get its size
    fseek(file, 0, SEEK_END);
    const size_t pos = (size_t) ftell(file);
    if (pos < 0) {
        fprintf(stderr, "http_serve_file(): Failed to tell file size of \"%s\"\n",
                filename);
        fclose(file);
        return ERR_IO;
    }
    rewind(file);
    const size_t file_size = pos;

    // read file content
    char* const buffer = calloc(file_size + 1, 1);
    if (buffer == NULL) {
        fprintf(stderr, "http_serve_file(): Failed to allocate memory to serve \"%s\"\n", filename);
        fclose(file);
        return ERR_IO;
    }

    const size_t bytes_read = fread(buffer, 1, file_size, file);
    if (bytes_read != file_size) {
        fprintf(stderr, "http_serve_file(): Failed to read \"%s\"\n", filename);
        fclose(file);
        return ERR_IO;
    }

    // send the file
    const int  ret = http_reply(connection, HTTP_OK,
                                "Content-Type: text/html; charset=utf-8" HTTP_LINE_DELIM,
                                buffer, file_size);

    // garbage collecting
    fclose(file);
    free(buffer);
    return ret;
}

/**
 * @brief Sends an HTTP reply.
 *
 * It sends an HTTP reply with the specified status, headers, and the body.
 *
 * @param connection The connection file descriptor.
 * @param status The HTTP status code.
 * @param headers The HTTP headers.
 * @param body The body content (can be NULL if body_len is 0).
 * @param body_len The length of the body content.
 * @return Error code indicating success or type of error.
 */
int http_reply(int connection, const char* status, const char* headers, const char *body, size_t body_len)
{
    M_REQUIRE_NON_NULL(status);
    M_REQUIRE_NON_NULL(headers);
    if ((body == NULL && body_len > 0)) {
        return ERR_INVALID_ARGUMENT;
    }

    // Calculate the maximum total length of the HTTP response
    size_t max_total_len = strlen(HTTP_PROTOCOL_ID) + strlen(" ") + strlen(status) +
                           strlen(HTTP_LINE_DELIM) + strlen(headers) + strlen(HTTP_LINE_DELIM) +
                           strlen("Content-Length: ") + MAX_SIZE_T_STRING_SIZE +
                           strlen(HTTP_LINE_DELIM) + (body_len > 0 ? body_len : 0) + strlen("\0");

    char* response = (char *) malloc(max_total_len + 1);
    if (response == NULL) {
        return ERR_OUT_OF_MEMORY;
    }
    zero_init_ptr(response);

    // Format the HTTP response header
    size_t currentLen = (size_t) snprintf(response, max_total_len + 1,
                                          "%s%s%s%sContent-Length: %zu%s",
                                          HTTP_PROTOCOL_ID, status, HTTP_LINE_DELIM,
                                          headers, body_len, HTTP_HDR_END_DELIM);

    // We copy the body content into the response buffer if there is one
    if (body_len > 0) {
        memcpy(response + currentLen, body, body_len);
        currentLen += body_len;
    }

    ssize_t sent = tcp_send(connection, response, currentLen);

    free(response);

    if (sent < 0) {
        perror("Error while sending response to http request\n");
        return ERR_IO;
    }

    if ((size_t)sent != currentLen) {
        debug_printf("Mismatch in response length: sent %zd, expected %zu\n", sent, currentLen);
        return ERR_IO;
    }

    // Ensure the connection is correctly closed
    if (shutdown(connection, SHUT_WR) < 0) {
        perror("shutdown() failed");
        return ERR_IO;
    }

    return ERR_NONE;
}
