/*
 * @file imgfs_server_service.c
 * @brief ImgFS server part, bridge between HTTP server layer and ImgFS library
 *
 * @author Konstantinos Prasopoulos
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h> // uint16_t

#include "error.h"
#include "util.h" // atouint16
#include "imgfs.h"
#include "http_net.h"
#include "imgfs_server_service.h"
#include <pthread.h>

pthread_mutex_t lock;

// Main in-memory structure for imgFS
static struct imgfs_file fs_file;
static uint16_t server_port = 8000;

#define URI_ROOT "/imgfs"

/**
 * @brief Startup function. Create imgFS file and load in-memory structure.
 *
 * @param argc Number of command line arguments.
 * @param argv Array of command line arguments. The first argument should be the ImgFS file name,
 *             and the optional second argument should be the port number.
 * @return Error indicating success or type of error.
 */
int server_startup(int argc, char **argv)
{
    if (argc < 2) return ERR_NOT_ENOUGH_ARGUMENTS;

    pthread_mutex_init(&lock, NULL);
    int ret = do_open(argv[1], "rb+", &fs_file);
    if (ret != ERR_NONE) {
        fprintf(stderr, "Failed to open ImgFS file: %s\n", ERR_MSG(ret));
        return ret;
    }

    print_header(&fs_file.header);

    if (argc > 2) {
        uint16_t port = atouint16(argv[2]); // Convert port number
        if (port != 0) {
            server_port = port;
        } else {
            perror("Invalid port number\n");
            return ERR_INVALID_ARGUMENT;
        }
    }

    ret = http_init(server_port, handle_http_message);
    if (ret < 0) {
        fprintf(stderr, "Failed to initialize HTTP connection: %s\n", ERR_MSG(ret));
        return ret;
    }

    printf("ImgFS server started on http://localhost:%d\n", server_port);

    return ERR_NONE;
}

/**
 * @brief Shutdown function. Free the structures and close the file.
 */
void server_shutdown(void)
{
    fprintf(stderr, "Shutting down the imgfs server...\n");
    http_close();
    do_close(&fs_file);
    pthread_mutex_destroy(&lock);
}

/**
 * @brief Sends an error message as an HTTP response.
 *
 * It constructs an error message which is based on the provided error code
 * and it sends it as an HTTP response.
 *
 * @param connection The HTTP connection file descriptor.
 * @param error The error code to be included in the message.
 * @return Error indicating success or the type of error.
 */
static int reply_error_msg(int connection, int error)
{
#define ERR_MSG_SIZE 256
    char err_msg[ERR_MSG_SIZE]; // enough for any reasonable err_msg
    if (snprintf(err_msg, ERR_MSG_SIZE, "Error: %s\n", ERR_MSG(error)) < 0) {
        fprintf(stderr, "reply_error_msg(): sprintf() failed...\n");
        return ERR_RUNTIME;
    }
    return http_reply(connection, "500 Internal Server Error", "",
                      err_msg, strlen(err_msg));
}

/**
 * @brief Sends a 302 Found message as an HTTP response.
 *
 * @param connection The HTTP connection file descriptor.
 * @return int Error code indicating success or type of error.
 */
static int reply_302_msg(int connection)
{
    char location[ERR_MSG_SIZE];
    if (snprintf(location, ERR_MSG_SIZE, "Location: http://localhost:%d/" BASE_FILE HTTP_LINE_DELIM,
                 server_port) < 0) {
        fprintf(stderr, "reply_302_msg(): sprintf() failed...\n");
        return ERR_RUNTIME;
    }
    return http_reply(connection, "302 Found", location, "\n", 0);
}

/**
 * @brief Handles a request to list images.
 *
 * @param connection The HTTP connection file descriptor.
 * @return Error indicating success or type of error.
 */
static int handle_list_call(int connection)
{
    debug_printf("handle_list_call() on connection %d\n", connection);
    char* json_output = NULL;
    pthread_mutex_lock(&lock);
    int err = do_list(&fs_file, JSON, &json_output);
    pthread_mutex_unlock(&lock);
    if (err != ERR_NONE) {
        free(json_output);
        return reply_error_msg(connection, err);
    }

    int ret = http_reply(connection, HTTP_OK,
                         "Content-Type: application/json" HTTP_LINE_DELIM,
                         json_output, strlen(json_output));
    free(json_output);
    return ret;
}

/**
 * @brief Handles a request to read an image.
 *
 * @param connection The HTTP connection file descriptor.
 * @param msg Pointer to the HTTP message structure containing the request details.
 * @return Error which is indicating success or type of error.
 */
static int handle_read_call(int connection, struct http_message* msg)
{
    debug_printf("handle_read_call() on connection %d\n", connection);
    char res_value[MAX_IMG_ID + 1] = {0};
    char img_id_value[MAX_IMG_ID + 1] = {0};

    int res_get_var_result = http_get_var(&msg->uri, "res", res_value, sizeof(res_value));
    int img_id_get_var_result = http_get_var(&msg->uri, "img_id", img_id_value, sizeof(img_id_value));

    if (res_get_var_result < 0) {
        return reply_error_msg(connection, res_get_var_result);
    } else if (img_id_get_var_result < 0) {
        return reply_error_msg(connection, img_id_get_var_result);
    } else if (res_get_var_result == 0 || img_id_get_var_result == 0) {
        return reply_error_msg(connection, ERR_NOT_ENOUGH_ARGUMENTS);
    }

    debug_printf("Resolution asked: %s\n", res_value);
    int resolution = resolution_atoi(res_value);
    if (resolution == -1) {
        return reply_error_msg(connection, ERR_RESOLUTIONS);
    }

    char *image_buffer = NULL;
    uint32_t image_size = 0;
    pthread_mutex_lock(&lock);
    int ret = do_read(img_id_value, resolution, &image_buffer, &image_size, &fs_file);
    pthread_mutex_unlock(&lock);
    if (ret != ERR_NONE) {
        debug_printf("Error reading image: %s\n", ERR_MSG(ret));
        return reply_error_msg(connection, ret);
    }

    ret = http_reply(connection, HTTP_OK,
                     "Content-Type: image/jpeg" HTTP_LINE_DELIM,
                     image_buffer, image_size);
    free(image_buffer);

    return ret;
}

/**
 * @brief Handles a request to delete an image.
 *
 * @param connection The HTTP connection file descriptor.
 * @param msg Pointer to the HTTP message structure containing the request details.
 * @return Error code indicating success or type of error.
 */
static int handle_delete_call(int connection, struct http_message* msg)
{
    debug_printf("handle_delete_call() on connection %d\n", connection);
    char img_id_value[MAX_IMG_ID + 1] = {0};

    int img_id_get_var = http_get_var(&msg->uri, "img_id", img_id_value, sizeof(img_id_value));
    if (img_id_get_var < 0) {
        return reply_error_msg(connection, img_id_get_var);
    } else if (img_id_get_var == 0) {
        return reply_error_msg(connection, ERR_NOT_ENOUGH_ARGUMENTS);
    }

    img_id_value[MAX_IMG_ID] = '\0';

    debug_printf("Deleting image with id: %s\n", img_id_value);

    pthread_mutex_lock(&lock);
    int err = do_delete(img_id_value, &fs_file);
    pthread_mutex_unlock(&lock);

    if (err != ERR_NONE) {
        return reply_error_msg(connection, err);
    }

    return reply_302_msg(connection);
}

/**
 * @brief Handles a request to insert an image.
 *
 * @param connection The HTTP connection file descriptor.
 * @param msg Pointer to the HTTP message structure.
 * @return Error code indicating success or type of error.
 */
static int handle_insert_call(int connection, struct http_message* msg)
{
    debug_printf("handle_insert_call() on connection %d\n, body length: %zu\n", connection, msg->body.len);
    if (msg == NULL || msg->body.len == 0) {
        return reply_error_msg(connection, ERR_INVALID_ARGUMENT);
    }

    // retrieve the image_id from the parameter
    char img_id_value[MAX_IMG_ID + 1] = {0};
    int name_get_var = http_get_var(&msg->uri, "name", img_id_value, sizeof(img_id_value));
    if (name_get_var < 0) {
        return reply_error_msg(connection, name_get_var);
    } else if (name_get_var == 0) {
        return reply_error_msg(connection, ERR_NOT_ENOUGH_ARGUMENTS);
    }

    img_id_value[MAX_IMG_ID] = '\0';

    // retrieve the image content
    char* image_buffer = (char*)malloc(msg->body.len);
    if (image_buffer == NULL) {
        debug_printf("Error allocating memory for image\n", NULL);
        return reply_error_msg(connection, ERR_OUT_OF_MEMORY);
    }
    memcpy(image_buffer, msg->body.val, msg->body.len);

    // insert the image into the ImgFS
    pthread_mutex_lock(&lock);
    int err = do_insert(image_buffer, msg->body.len, img_id_value, &fs_file);
    pthread_mutex_unlock(&lock);
    free(image_buffer);

    if (err != ERR_NONE) {
        debug_printf("Error inserting image: %s\n", ERR_MSG(err));
        return reply_error_msg(connection, err);
    }

    return reply_302_msg(connection);
}

/**
 * @brief Handles incoming HTTP messages and routes them to the appropriate handler.
 *
 * This function processes incoming HTTP messages and routes them to the appropriate
 * handler function based on the URI and method.
 *
 * @param msg Pointer to the HTTP message structure containing the request details.
 * @param connection The HTTP connection file descriptor.
 * @return Error code indicating success or type of error.
 */
int handle_http_message(struct http_message* msg, int connection)
{
    M_REQUIRE_NON_NULL(msg);
    debug_printf("handle_http_message() on connection %d. URI: %.*s\n", connection, (int) msg->uri.len, msg->uri.val);

    // Serve the base file if the URI matches
    if (http_match_verb(&msg->uri, "/") || http_match_verb(&msg->uri, "/index.html")) {
        debug_printf("Serving base file\n", NULL);
        return http_serve_file(connection, BASE_FILE);
    }

    // Route to the appropriate handler based on the URI
    if (http_match_uri(msg, URI_ROOT "/list")) {
        return handle_list_call(connection);
    } else if (http_match_uri(msg, URI_ROOT "/read")) {
        return handle_read_call(connection, msg);
    } else if (http_match_uri(msg, URI_ROOT "/delete")) {
        return handle_delete_call(connection, msg);
    } else if (http_match_uri(msg, URI_ROOT "/insert") && http_match_verb(&msg->method, "POST")) {
        return handle_insert_call(connection, msg);
    } else {
        perror("Invalid command\n");
        return reply_error_msg(connection, ERR_INVALID_COMMAND);
    }
}
