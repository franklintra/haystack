/**
 * @file imgfs_list.c
 * @brief Implementation of the function to list contents of an imgFS file system.
 *
 *
 * @author Franklin Tranié
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
    // Argument checking
    M_REQUIRE_NON_NULL(imgfs_file);

    if (output_mode == STDOUT) {
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
    } else if (output_mode == JSON) {
        // JSON output mode is not implemented yet
        TO_BE_IMPLEMENTED();
        return NOT_IMPLEMENTED;
    } else {
        return ERR_INVALID_ARGUMENT;
    }

    return ERR_NONE;
}
