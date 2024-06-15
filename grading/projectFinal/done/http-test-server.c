/**
 * @file http-test-server.c
 * @brief Main file for ImgFS server initialization and handling HTTP requests.
 */

#include "error.h"
#include "http_net.h"
#include "imgfs_server_service.h" // for DEFAULT_LISTENING_PORT
#include <stdio.h>

/**
 * @brief Main function in order to initialize and run the ImgFS server.
 *
 * Initializes the HTTP server (on the default listening port).
 * Handles incoming HTTP requests and provides error messages if initialization or request handling fails.
 *
 * @return The error code (0 if it ERR_NONE).
 */
int main(void)
{
    // Initialization of the http server
    int err = http_init(DEFAULT_LISTENING_PORT, NULL);
    if (err < 0) {
        fprintf(stderr, "http_init() failed\n");
        fprintf(stderr, "%s\n", ERR_MSG(err));
        return err;
    }

    printf("ImgFS server started on http://localhost:%u\n",
           DEFAULT_LISTENING_PORT);

    while ((err = http_receive()) == ERR_NONE);

    fprintf(stderr, "http_receive() failed\n");
    fprintf(stderr, "%s\n", ERR_MSG(err));

    return err;
}
