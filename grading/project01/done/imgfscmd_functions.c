/**
 * @file imgfscmd_functions.c
 * @brief imgFS command line interpreter for imgFS core commands.
 *
 * @author Mia Primorac
 */

#include "imgfs.h"
#include "imgfscmd_functions.h"
#include "util.h"   // for _unused
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>

#define LIST_NUMBER_ARGUMENTS 1
#define CREATE_MINIMUM_ARGUMENTS 1
#define DELETE_MINIMUM_ARGUMENTS 2

// default values
static const uint32_t default_max_files = 128;
static const uint16_t default_thumb_res =  64;
static const uint16_t default_small_res = 256;

// max values
static const uint16_t MAX_THUMB_RES = 128;
static const uint16_t MAX_SMALL_RES = 512;

/**********************************************************************
 * Displays some explanations.
 ********************************************************************** */
int help(int _unused argc, char** _unused argv)
{
    printf("imgfscmd [COMMAND] [ARGUMENTS]\n");
    printf("  help: displays this help.\n");
    printf("  list <imgFS_filename>: list imgFS content.\n");
    printf("  create <imgFS_filename> [options]: create a new imgFS.\n");
    printf("      options are:\n");
    printf("          -max_files <MAX_FILES>: maximum number of files.\n");
    printf("                                  default value is 128\n");
    printf("                                  maximum value is 4294967295\n");
    printf("          -thumb_res <X_RES> <Y_RES>: resolution for thumbnail images.\n");
    printf("                                  default value is 64x64\n");
    printf("                                  maximum value is 128x128\n");
    printf("          -small_res <X_RES> <Y_RES>: resolution for small images.\n");
    printf("                                  default value is 256x256\n");
    printf("                                  maximum value is 512x512\n");
    printf("  delete <imgFS_filename> <imgID>: delete image imgID from imgFS.\n");
    return 0; // 0 for success
}

/**********************************************************************
 * Opens imgFS file and calls do_list().
 ********************************************************************** */
int do_list_cmd(int argc, char** argv)
{
    if (argv == NULL) {
        return ERR_INVALID_ARGUMENT;
    }

    if (argc < LIST_NUMBER_ARGUMENTS) {
        return ERR_NOT_ENOUGH_ARGUMENTS;
    }

    if (argc > LIST_NUMBER_ARGUMENTS) {
        return ERR_INVALID_COMMAND;
    }

    M_REQUIRE_NON_NULL(argv[0]);

    struct imgfs_file imgfs_file;
    int err = do_open(argv[0], "rb", &imgfs_file);
    if(err != ERR_NONE) {
        return err;
    }

    err = do_list(&imgfs_file, STDOUT, NULL);
    do_close(&imgfs_file);

    return err;
}

/**********************************************************************
 * Prepares and calls do_create command.
********************************************************************** */
int do_create_cmd(int argc, char** argv)
{
    if (argv == NULL) {
        return ERR_INVALID_ARGUMENT;
    }

    if(argc < CREATE_MINIMUM_ARGUMENTS) {
        return ERR_NOT_ENOUGH_ARGUMENTS;
    }

    const char* filename = argv[0];  // mandatory arg
    struct imgfs_file imgfs_file;
    zero_init_var(imgfs_file);  // init struct full of 0's

    // default parameters
    imgfs_file.header.max_files = default_max_files;
    imgfs_file.header.resized_res[0] = default_thumb_res;
    imgfs_file.header.resized_res[1] = default_thumb_res;
    imgfs_file.header.resized_res[2] = default_small_res;
    imgfs_file.header.resized_res[3] = default_small_res;

    // parsing the optional args
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-max_files") == 0) {
            if (i + 1 >= argc) return ERR_NOT_ENOUGH_ARGUMENTS;
            uint32_t max_files = atouint32(argv[++i]);
            if (max_files == 0 || max_files > UINT32_MAX) return ERR_MAX_FILES;
            imgfs_file.header.max_files = max_files;
        } else if (strcmp(argv[i], "-thumb_res") == 0) {
            if (i + 2 >= argc) return ERR_NOT_ENOUGH_ARGUMENTS;
            uint16_t res_x = atouint16(argv[++i]);
            uint16_t res_y = atouint16(argv[++i]);
            if (res_x == 0 || res_y == 0 || res_x > MAX_THUMB_RES || res_y > MAX_THUMB_RES) return ERR_RESOLUTIONS;
            imgfs_file.header.resized_res[0] = res_x;
            imgfs_file.header.resized_res[1] = res_y;
        } else if (strcmp(argv[i], "-small_res") == 0) {
            if (i + 2 >= argc) return ERR_NOT_ENOUGH_ARGUMENTS;
            uint16_t res_x = atouint16(argv[++i]);
            uint16_t res_y = atouint16(argv[++i]);
            if (res_x == 0 || res_y == 0 || res_x > MAX_SMALL_RES || res_y > MAX_SMALL_RES) return ERR_RESOLUTIONS;
            imgfs_file.header.resized_res[2] = res_x;
            imgfs_file.header.resized_res[3] = res_y;
        } else {
            return ERR_INVALID_ARGUMENT;
        }
    }

    int err = do_create(filename, &imgfs_file);

    do_close(&imgfs_file);

    return err;
}



/**********************************************************************
 * Deletes an image from the imgFS.
********************************************************************** */
int do_delete_cmd(int argc, char **argv)
{
    M_REQUIRE_NON_NULL(argv);

    if (argc < DELETE_MINIMUM_ARGUMENTS) {
        return ERR_NOT_ENOUGH_ARGUMENTS;
    }

    if (argv[0] == NULL || argv[1] == NULL) {
        return ERR_INVALID_ARGUMENT;
    }

    const char *filename = argv[0];

    const char *imgID = argv[1];
    if (strlen(imgID) == 0 || strlen(imgID) > MAX_IMG_ID) {
        return ERR_INVALID_IMGID;
    }

    struct imgfs_file imgfs_file;

    // r+b -> read + write permissions
    int err = do_open(filename, "r+b", &imgfs_file);

    // delete the img if the file is opened
    if (err == ERR_NONE) {
        err = do_delete(imgID, &imgfs_file);
    }

    // in all cases, close the file
    do_close(&imgfs_file);

    return err;
}
