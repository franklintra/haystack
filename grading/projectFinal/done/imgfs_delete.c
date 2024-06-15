/**
 * @file imgfs_delete.c
 * @brief Delete an image from the image file system.
 */

#include "imgfs.h"
#include "error.h"
#include <stdio.h>
#include <string.h>

/**
 * @brief Deletes an image from the imgFS by invalidating its metadata entry and updating the header.
 *
 * This function searches for the image with the given ID in the metadata array.
 * If found, it invalidates the entry by setting is_valid to EMPTY and updates the metadata / header.
 * The changes are written directly to the disk.
 *
 * @param imgID The ID of the image to delete.
 * @param imgfs_file A pointer to the imgfs_file structure where the images and metadata are stored.
 * @return Returns ERR_NONE if the image is successfully deleted.
 *         Returns other error codes in case of error. see 'error.h' for more details.
 */
int do_delete(const char *imgID, struct imgfs_file *imgfs_file)
{
    // Argument checking
    M_REQUIRE_NON_NULL(imgID);
    M_REQUIRE_NON_NULL(imgfs_file);

    int found = 0;
    for (uint32_t i = 0; i < imgfs_file->header.max_files; i++) {
        if (imgfs_file->metadata[i].is_valid && strcmp(imgfs_file->metadata[i].img_id, imgID) == 0) {
            imgfs_file->metadata[i].is_valid = EMPTY; // Mark the image as deleted
            fseek(imgfs_file->file,sizeof(struct imgfs_header)+sizeof(struct img_metadata) * i, SEEK_SET);
            if (fwrite(&imgfs_file->metadata[i], sizeof(struct img_metadata), 1, imgfs_file->file) != 1) {
                return ERR_IO;
            }
            found = 1;
            break; // Stop the search once the image is found and marked
        }
    }

    if (!found) {
        perror("Image not found in metadata for deletion.\n");
        return ERR_IMAGE_NOT_FOUND;
    } else {
        imgfs_file->header.nb_files--;
        imgfs_file->header.version++;
        fseek(imgfs_file->file, 0, SEEK_SET); // move to start of file
        if (fwrite(&imgfs_file->header, sizeof(struct imgfs_header), 1, imgfs_file->file) != 1) {
            return ERR_IO;
        }
    }
    return ERR_NONE;
}

