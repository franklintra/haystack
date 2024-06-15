/**
 * @file imgfs_list.c
 * @brief Implementation of the function to list contents of an imgFS file system.
 */

#include "imgfs.h"
#include "util.h"
#include "error.h"
#include <inttypes.h>
#include <openssl/sha.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <json-c/json.h>

// prototypes to declare them later
static void print_stdout_from_metadata(const struct imgfs_file* imgfs_file);
static int create_json_from_metadata(const struct imgfs_file* imgfs_file, char** json);

/**
 * @brief Lists the contents of an imgFS file.
 *
 * This function prints the header of the imgFS file and then, depending on the
 * contents of the file, it either prints a message indicating the imgFS is empty
 * or prints the metadata of all valid images contained within the file.
 * If the output mode is set to STDOUT, the function will print to the standard output.
 * If any other output mode is selected, the function will return an error.
 *
 * @param imgfs_file A pointer to the imgfs_file structure to list the contents from.
 * @param output_mode The mode of output, can be STDOUT or JSON (not implemented yet).
 * @param json A pointer to a char* that will hold the JSON output if JSON mode is selected.
 * @return ERR_NONE if the function executed successfully.
 *         Returns other error codes in case of error. see 'error.h' for more details.
 */
int do_list(const struct imgfs_file* imgfs_file, enum do_list_mode output_mode, char** json)
{
    M_REQUIRE_NON_NULL(imgfs_file);

    if (output_mode == STDOUT) {
        print_stdout_from_metadata(imgfs_file);
    } else if (output_mode == JSON) {
        return create_json_from_metadata(imgfs_file, json);
    } else {
        return ERR_INVALID_ARGUMENT;
    }

    return ERR_NONE;
}

static void print_stdout_from_metadata(const struct imgfs_file* imgfs_file)
{
    print_header(&imgfs_file->header);
    if (imgfs_file->header.nb_files == 0) {
        printf("<< empty imgFS >>\n");
    } else {
        for (uint32_t i = 0; i < imgfs_file->header.max_files; i++) {
            if (imgfs_file->metadata[i].is_valid) {
                print_metadata(&imgfs_file->metadata[i]);
            }
        }
    }
}

static int create_json_from_metadata(const struct imgfs_file* imgfs_file, char** json)
{
    *json = NULL; // init the output pointer
    json_object *jobj = json_object_new_object();
    json_object *jarray = json_object_new_array();

    // iterate over all metadata and add img_id to the JSON array
    for (uint32_t i = 0; i < imgfs_file->header.max_files; i++) {
        if (imgfs_file->metadata[i].is_valid) {
            json_object *jstring = json_object_new_string(imgfs_file->metadata[i].img_id);
            json_object_array_add(jarray, jstring);
        }
    }

    // add the JSON array to the JSON object
    json_object_object_add(jobj, "Images", jarray);

    // convert the JSON object to a string and assign it to the output pointer
    *json = strdup(json_object_to_json_string(jobj));

    // decrement the reference count of the JSON object
    json_object_put(jobj);

    // check if strdup failed
    if (*json == NULL) {
        return ERR_OUT_OF_MEMORY;
    }

    return ERR_NONE;
}
