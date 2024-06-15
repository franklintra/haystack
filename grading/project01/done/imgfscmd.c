/**
 * @file imgfscmd.c
 * @brief imgFS command line interpreter for imgFS core commands.
 *
 * Image Filesystem Command Line Tool
 *
 * @author Mia Primorac
 */

#include "imgfs.h"
#include "imgfscmd_functions.h"
#include "util.h"   // for _unused

#include <stdlib.h>
#include <vips/vips.h>
#include <string.h>

// Define a command type
typedef int (*command_t)(int, char**);

// Define a struct command_mapping type
typedef struct command_mapping {
    const char* name;
    command_t command;
} command_mapping_t;

command_mapping_t commands[] = {
    {"list", *do_list_cmd},
    {"create", *do_create_cmd},
    {"help", *help},
    {"delete", *do_delete_cmd},
    {NULL, NULL}
};

/*******************************************************************************
 * MAIN
 */
int main(int argc, char* argv[])
{
    int ret = 0;

    VIPS_INIT(argv[0]);

    if (argc < 2) {
        ret = ERR_NOT_ENOUGH_ARGUMENTS;
    } else {
        argc--; argv++; // skips command call name

        for (command_mapping_t* cmd = commands; cmd->name != NULL; cmd++) {
            // If the command name matches the first command line argument
            if (strcmp(cmd->name, argv[0]) == 0) {
                // If the command function is not NULL
                if (cmd->command != NULL) {
                    // Call the command function with the adjusted argc and argv
                    ret = cmd->command(argc-1, argv+1);
                } else {
                    // If the command function is NULL, print an error message
                    fprintf(stderr, "ERROR: Command '%s' not implemented.\n", cmd->name);
                    ret = ERR_INVALID_COMMAND;
                }
                break;
            } else {
                ret = ERR_INVALID_COMMAND;
            }
        }
    }

    if (ret) {
        fprintf(stderr, "ERROR: %s\n", ERR_MSG(ret));
        help(argc - 1, argv + 1);
    }

    vips_shutdown();

    return ret;
}
