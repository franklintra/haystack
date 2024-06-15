/*
 * @file imgfs_server.c
 * @brief ImgFS server part, main
 *
 * @author Konstantinos Prasopoulos
 */

#include "util.h"
#include "imgfs.h"
#include "socket_layer.h"
#include "http_net.h"
#include "imgfs_server_service.h"

#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <stdlib.h> //abort()
#include <vips/vips.h>

/**
 * @brief Signal handler for server shutdown.
 *
 * This function handles termination signals to properly shutdown the server.
 *
 * @param sig Signal number.
 */
static void signal_handler(int sig_num _unused)
{
    server_shutdown();
    exit(0);
}

/**
 * @brief Sets the signal handler for termination.
 *
 * This function sets up the signal handler for SIGINT.
 */
static void set_signal_handler(void)
{
    struct sigaction action;
    if (sigemptyset(&action.sa_mask) == -1) {
        perror("sigemptyset() in set_signal_handler()");
        abort();
    }
    action.sa_handler = signal_handler;
    action.sa_flags   = 0;
    if ((sigaction(SIGINT,  &action, NULL) < 0) ||
        (sigaction(SIGTERM,  &action, NULL) < 0)) {
        perror("sigaction() in set_signal_handler()");
        abort();
    }
}

/**
 * @brief Main function to start the ImgFS server.
 *
 * @param argc Argument count.
 * @param argv Argument values.
 * @return Exit status.
 */
int main (int argc, char *argv[])
{
    debug_printf("Starting ImgFS server...\n", NULL);
    VIPS_INIT(argv[0]);
    int err = server_startup(argc, argv); // Start the server
    if (err != ERR_NONE) {
        debug_printf("Error on ImgFS server startup ...\n", NULL);
        if (err == ERR_NOT_ENOUGH_ARGUMENTS) {
            debug_printf("Usage: %s <imgfs_file> [port]\n", argv[0]);
        }
        return err;
    }

    set_signal_handler(); // Set up signal handler

    while (1) {
        err = http_receive(); // Receive HTTP requests
        if (err != ERR_NONE) {
            debug_printf("Error receiving HTTP request: %s\n", ERR_MSG(err));
            break;
        }
    }
    server_shutdown(); // Shutdown the server
    vips_shutdown();
    return err;
}
