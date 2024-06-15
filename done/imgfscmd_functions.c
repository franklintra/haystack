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

/**
 * @brief Displays some explanations to help our customer !
 *
 * @param argc Number of command line arguments.
 * @param argv Array of command line arguments.
 * @return 0 on success.
 */
int help(int _unused argc, char** _unused argv)
{
    const char* help_message =
    "imgfscmd [COMMAND] [ARGUMENTS]\n"
    "  help: displays this help.\n"
    "  list <imgFS_filename>: list imgFS content.\n"
    "  create <imgFS_filename> [options]: create a new imgFS.\n"
    "      options are:\n"
    "          -max_files <MAX_FILES>: maximum number of files.\n"
    "                                  default value is 128\n"
    "                                  maximum value is 4294967295\n"
    "          -thumb_res <X_RES> <Y_RES>: resolution for thumbnail images.\n"
    "                                  default value is 64x64\n"
    "                                  maximum value is 128x128\n"
    "          -small_res <X_RES> <Y_RES>: resolution for small images.\n"
    "                                  default value is 256x256\n"
    "                                  maximum value is 512x512\n"
    "  read   <imgFS_filename> <imgID> [original|orig|thumbnail|thumb|small]:\n"
    "      read an image from the imgFS and save it to a file.\n"
    "      default resolution is \"original\".\n"
    "  insert <imgFS_filename> <imgID> <filename>: insert a new image in the imgFS.\n"
    "  delete <imgFS_filename> <imgID>: delete image imgID from imgFS.\n";
    printf("%s", help_message);
    return 0;
}

/**
 * @brief Opens imgFS file and calls do_list().
 *
 * @param argc Number of command line arguments.
 * @param argv Array of command line arguments.
 *             The first argument is the imgFS filename.
 * @return The error code indicating success or type of error.
 */
int do_list_cmd(int argc, char** argv)
{
    M_REQUIRE_NON_NULL(argv);

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

    // If I want to test JSON output uncomment this and comment previous line
    /*
    char* json_output = NULL;
    err = do_list(&imgfs_file, JSON, &json_output);

    if (err == ERR_NONE) {
        printf("%s\n", json_output);
    }

    free(json_output);
    */

    // End of JSON testing

    do_close(&imgfs_file);

    return err;
}

/**
 * @brief Prepares and calls do_create command.
 *
 * @param argc Number of command line arguments.
 * @param argv Array of command line arguments, with the first argument being the imgFS filename
 *             and optional arguments for file and resolution settings.
 * @return Error code indicating success or the type of error we get.
 */
int do_create_cmd(int argc, char** argv)
{
    M_REQUIRE_NON_NULL(argv);

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



/**
 * @brief Deletes an image from the imgFS.
 *
 * @param argc Number of command line arguments.
 * @param argv Array of command line arguments, with the first argument as the imgFS filename
 *             and the second argument as the imgID of the image to delete.
 * @return int Error code indicating success or type of error.
 */
int do_delete_cmd(int argc, char **argv)
{
    M_REQUIRE_NON_NULL(argv);

    if (argc < DELETE_MINIMUM_ARGUMENTS) {
        return ERR_NOT_ENOUGH_ARGUMENTS;
    }

    M_REQUIRE_NON_NULL(argv[0]);
    M_REQUIRE_NON_NULL(argv[1]);

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

/**
 * @brief Creates a new name for the image based on the resolution.
 *
 * This function generates a new name for the image appending the appropriate
 * resolution to the img_id and adding the ".jpg" extension.
 *
 * @param img_id The image ID.
 * @param resolution The resolution of the image.
 * @param new_name Pointer to the buffer where the new name will be stored.
 */
static void create_name(const char* img_id, int resolution, char** new_name)
{
    if (img_id == NULL) return;
    if (resolution < THUMB_RES || resolution > ORIG_RES) return;
    if (strlen(img_id) > MAX_IMG_ID) return;

    const char* suffix;
    switch (resolution) {
    case ORIG_RES:
        suffix = "_orig";
        break;
    case SMALL_RES:
        suffix = "_small";
        break;
    case THUMB_RES:
        suffix = "_thumb";
        break;
    default:
        suffix = "";
        break;
    }

    size_t name_length = strlen(img_id) + strlen(suffix) + 5; // dot + jpg + end terminator
    *new_name = (char*)malloc(name_length);
    if (*new_name != NULL) {
        snprintf(*new_name, name_length, "%s%s.jpg", img_id, suffix);
    } else {
        return;
    }
}

/**
 * @brief Writes an image buffer to a file on disk.
 *
 * @param filename The name of the file to write.
 * @param image_buffer The buffer containing the image data.
 * @param image_size The size of the image data.
 * @return The error code indicating success or the type of error we got.
 */
static int write_disk_image(const char *filename, const char *image_buffer, uint32_t image_size)
{
    M_REQUIRE_NON_NULL(filename);
    M_REQUIRE_NON_NULL(image_buffer);

    if (image_size == 0) return ERR_INVALID_ARGUMENT;

    FILE *file = fopen(filename, "wb");
    if (!file) return ERR_IO;
    size_t written = fwrite(image_buffer, 1, image_size, file);
    fclose(file);
    if (written != image_size) return ERR_IO;
    return ERR_NONE;
}

/**
 * @brief Reads an image file from disk into a buffer.
 *
 * @param path The path to the image file.
 * @param image_buffer Pointer to the buffer where the image data will be stored.
 * @param image_size Pointer to the variable where the size of the image data will be stored.
 * @return Error code indicating success or type of error.
 */
static int read_disk_image(const char *path, char **image_buffer, uint32_t *image_size)
{
    M_REQUIRE_NON_NULL(path);
    M_REQUIRE_NON_NULL(image_buffer);
    M_REQUIRE_NON_NULL(image_size);

    // Open the image file for reading in the binary mode
    FILE *file = fopen(path, "rb");
    if (!file) {
        return ERR_IO;
    }

    // Move the pointer to the end of the file in order to find its size
    if (fseek(file, 0, SEEK_END) != 0) {
        fclose(file);
        return ERR_IO;
    }

    // Get the current position of the file pointer
    long size = ftell(file);
    if (size == -1) {
        fclose(file);
        return ERR_IO;
    }

    // Store the file size in the image_size variable
    *image_size = (uint32_t)size;

    // Move the file pointer back to the beginning of the file
    if (fseek(file, 0, SEEK_SET) != 0) {
        fclose(file);
        return ERR_IO;
    }

    *image_buffer = (char *)malloc(*image_size);
    if (*image_buffer == NULL) {
        fclose(file);
        return ERR_OUT_OF_MEMORY;
    }

    size_t read_size = fread(*image_buffer, 1, *image_size, file);
    fclose(file);

    if (read_size != *image_size) {
        free(*image_buffer);
        return ERR_IO;
    }

    return ERR_NONE;
}

/**
 * @brief Reads an image from the imgFS and saves it to a file.
 *
 * @param argc Number of command line arguments.
 * @param argv Array of command line arguments.
 * @return The error code (ERR_NONE if none).
 */
int do_read_cmd(int argc, char **argv)
{
    M_REQUIRE_NON_NULL(argv);
    if (argc != 2 && argc != 3) return ERR_NOT_ENOUGH_ARGUMENTS;

    const char* img_id = argv[1];

    int resolution = (argc == 3) ? resolution_atoi(argv[2]) : ORIG_RES;
    if (resolution == -1) return ERR_RESOLUTIONS;

    struct imgfs_file myfile;
    zero_init_var(myfile);
    int error = do_open(argv[0], "rb+", &myfile);
    if (error != ERR_NONE) return error;

    char *image_buffer = NULL;
    uint32_t image_size = 0;
    error = do_read(img_id, resolution, &image_buffer, &image_size, &myfile);
    do_close(&myfile);
    if (error != ERR_NONE) {
        return error;
    }

    // Extracting to a separate image file.
    char* tmp_name = NULL;
    create_name(img_id, resolution, &tmp_name);
    if (tmp_name == NULL) return ERR_OUT_OF_MEMORY;
    error = write_disk_image(tmp_name, image_buffer, image_size);
    free(tmp_name);
    free(image_buffer);

    return error;
}

/**
 * @brief Inserts a new image into the imgFS.
 *
 * @param argc Number of command line arguments.
 * @param argv Array of command line arguments.
 * @return The error code (ERR_NONE if none).
 */
int do_insert_cmd(int argc, char **argv)
{
    M_REQUIRE_NON_NULL(argv);
    if (argc != 3) return ERR_NOT_ENOUGH_ARGUMENTS;

    struct imgfs_file myfile;
    zero_init_var(myfile);
    int error = do_open(argv[0], "rb+", &myfile);
    if (error != ERR_NONE) return error;

    char *image_buffer = NULL;
    uint32_t image_size;

    // Reads image from the disk.
    error = read_disk_image(argv[2], &image_buffer, &image_size);
    if (error != ERR_NONE) {
        do_close(&myfile);
        return error;
    }

    error = do_insert(image_buffer, image_size, argv[1], &myfile);
    free(image_buffer);
    do_close(&myfile);
    return error;
}
