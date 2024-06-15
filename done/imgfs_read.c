/**
 * @file imgfs_read.c
 * @brief Implements the functionality to read an image from the imgFS file system at a given resolution.
 */

#include "imgfs.h"
#include "error.h"
#include "image_content.h"
#include <stdlib.h>
#include <string.h>

/**
 * @brief Searches for an image ID in imgFS and returns its metadata position.
 *
 * @param imgfs_file Pointer to the imgFS file structure.
 * @param img_id Image ID to search for.
 * @return Position in metadata array if found, -1 otherwise.
 */
static long find_pos_by_id(struct imgfs_file* imgfs_file, const char* img_id)
{
    for (long i = 0; i < imgfs_file->header.max_files; ++i) {
        if (imgfs_file->metadata[i].is_valid == NON_EMPTY && strcmp(imgfs_file->metadata[i].img_id, img_id) == 0) {
            return i;
        }
    }

    return -1L;
}


/**
 * @brief Reads an image from the imgFS file system at a given resolution.
 *
 * This function finds the metadata entry for the given image ID, checks if the image exists
 * at the requested resolution, and if not, calls lazily_resize() to create it. The image
 * is then read into a dynamically allocated buffer.
 *
 * @param img_id The ID of the image to read.
 * @param resolution The resolution at which to read the image.
 * @param image_buffer Pointer to the buffer where the image will be stored.
 * @param image_size Pointer to the variable where the size of the image will be stored.
 * @param imgfs_file The imgFS file system from which to read the image.
 * @return ERR_NONE if the function executed successfully.
 *         Returns other error codes in case of error. see 'error.h' for more details.
 */

int do_read(const char* img_id, int resolution, char** image_buffer, uint32_t* image_size, struct imgfs_file* imgfs_file)
{
    M_REQUIRE_NON_NULL(img_id);
    M_REQUIRE_NON_NULL(image_buffer);
    M_REQUIRE_NON_NULL(image_size);
    M_REQUIRE_NON_NULL(imgfs_file);

    long pos = find_pos_by_id(imgfs_file, img_id);
    if (pos == -1L) return ERR_IMAGE_NOT_FOUND;

    size_t position = (size_t) pos;

    // call lazily resize if res doesn't already exist
    if (resolution != ORIG_RES && (imgfs_file->metadata[position].offset[resolution] == 0 || imgfs_file->metadata[position].size[resolution] == EMPTY)) {
        int err = lazily_resize(resolution, imgfs_file, position);
        if (err != ERR_NONE) return err;
    }

    *image_size = imgfs_file->metadata[position].size[resolution];
    if (fseek(imgfs_file->file, (long)imgfs_file->metadata[position].offset[resolution], SEEK_SET) != 0) return ERR_IO;

    *image_buffer = (char *)malloc(*image_size);
    if (*image_buffer == NULL) {
        fclose(imgfs_file->file);
        free(*image_buffer);
        return ERR_OUT_OF_MEMORY;
    }

    size_t read_items = fread(*image_buffer, *image_size, 1, imgfs_file->file);
    if (read_items != 1) {
        free(*image_buffer);
        return ERR_IO;
    }

    return ERR_NONE;
}
