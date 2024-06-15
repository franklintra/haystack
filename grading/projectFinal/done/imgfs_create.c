/**
 * @file imgfs_create.c
 * @brief Image file system creation.
 */

#include "imgfs.h"
#include "error.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


/**
 * @brief Creates a new imgFS database file.
 *
 * This function initializes the imgfs_file structure and writes it to disk.
 * It sets the database name from CAT_TXT and writes the header followed by
 * the metadata array to the file. If the file already exists, it is overwritten without warning.
 * It also prints the number of items written to the disk as a side effect.
 *
 * @param filename The name of the database file to create.
 * @param imgfs_file A pointer to the imgfs_file structure to be initialized and written to disk.
 * @return Returns ERR_NONE on success
 *         Returns other error codes in case of error. see 'error.h' for more details.
 */
int do_create(const char *filename, struct imgfs_file *imgfs_file)
{
    // Argument checking
    M_REQUIRE_NON_NULL(filename);
    M_REQUIRE_NON_NULL(imgfs_file);

    strncpy(imgfs_file->header.name, CAT_TXT, MAX_IMGFS_NAME);
    imgfs_file->header.name[MAX_IMGFS_NAME] = '\0';

    // open file in wb -> write byte mode
    FILE *fp = fopen(filename, "wb");
    if (!fp) { // handle opening pbs
        return ERR_IO;
    }

    // header init
    imgfs_file->file=fp;
    imgfs_file->header.version = 0;
    imgfs_file->header.nb_files = 0;


    // alloc and inits memory for metadata
    imgfs_file->metadata = (struct img_metadata*) calloc(imgfs_file->header.max_files, sizeof(struct img_metadata));
    if (imgfs_file->metadata == NULL) {
        fclose(fp);
        return ERR_OUT_OF_MEMORY;
    }

    // write header to file
    if (fwrite(&imgfs_file->header, sizeof(struct imgfs_header), 1, fp) != 1) {
        free(imgfs_file->metadata);
        fclose(fp);
        return ERR_IO;
    }

    // write metadata to the file
    if (fwrite(imgfs_file->metadata, sizeof(struct img_metadata), imgfs_file->header.max_files, fp) != imgfs_file->header.max_files) {
        free(imgfs_file->metadata);
        fclose(fp);
        return ERR_IO;
    }

    // side effect
    // 1 for header, rest for metadata
    size_t items_written = 1 + imgfs_file->header.nb_files;
    printf("%zu item(s) written\n", items_written);

    return ERR_NONE;
}
