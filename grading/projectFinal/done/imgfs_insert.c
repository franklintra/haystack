/**
 * @file imgfs_insert.c
 * @brief Handles the insertion of images into the imgFS file system.
 */

#include "imgfs.h"
#include "error.h"
#include "image_content.h"
#include "image_dedup.h"
#include "util.h"
#include <openssl/sha.h>
#include <string.h>

/**
 * @brief Inserts an image into the imgFS file system.
 *
 * This function adds an image to the imgFS, performing several checks and operations:
 * - Verifies that there is space available for the new image.
 * - Finds a free index in the metadata array.
 * - Computes the SHA256 hash of the image and stores it in the metadata.
 * - Copies the image ID into the metadata.
 * - Stores the image size and original resolution (width and height) in the metadata.
 * - Calls the do_name_and_content_dedup() function to handle name and content deduplication.
 * - If the image is not a duplicate, writes the image content and metadata to the file.
 *
 * @param image_buffer Pointer to the raw image content.
 * @param image_size Size of the image in bytes.
 * @param img_id Unique identifier for the image.
 * @param imgfs_file Pointer to the imgfs_file structure representing the imgFS file system.
 * @return ERR_NONE if the function executed successfully.
 *         Returns other error codes in case of error. see 'error.h' for more details.
 */
int do_insert(const char* image_buffer, size_t image_size, const char* img_id, struct imgfs_file* imgfs_file)
{
    M_REQUIRE_NON_NULL(imgfs_file);
    M_REQUIRE_NON_NULL(img_id);
    M_REQUIRE_NON_NULL(image_buffer);
    if (image_size == 0) {
        return ERR_INVALID_ARGUMENT;
    }

    if (imgfs_file->header.nb_files >= imgfs_file->header.max_files) {
        return ERR_IMGFS_FULL;
    }

    // Find the first free index in the metadata array
    long free_index = -1L;
    for (long i = 0; i < imgfs_file->header.max_files; i++) {
        if (imgfs_file->metadata[i].is_valid == EMPTY) {
            free_index = i;
            break;
        }
    }

    uint32_t index = (uint32_t) free_index;

    // Initialize the metadata to 0 !
    struct img_metadata* md = &imgfs_file->metadata[index];
    zero_init_ptr(md);

    // Compute the SHA and store it in the metadata array
    SHA256((const unsigned char *) image_buffer, image_size, imgfs_file->metadata[index].SHA);


    // Store the img_id
    strncpy(imgfs_file->metadata[index].img_id, img_id, MAX_IMG_ID);
    imgfs_file->metadata[index].img_id[MAX_IMG_ID] = '\0';


    // Update metadata and increment nb_fles
    imgfs_file->metadata[index].is_valid = NON_EMPTY;
    imgfs_file->header.nb_files++;


    uint32_t height = 0;
    uint32_t width = 0;

    int err;
    err = get_resolution(&height, &width, image_buffer, image_size);
    if (err != ERR_NONE) {
        return err;
    }

    imgfs_file->metadata[index].orig_res[0] = width;
    imgfs_file->metadata[index].orig_res[1] = height;

    err = do_name_and_content_dedup(imgfs_file, index);
    if (err != ERR_NONE) {
        if (err == ERR_DUPLICATE_ID) {
            // Handle the duplicate ID error by cleaning up and returning
            zero_init_var(imgfs_file->metadata[index]);
            imgfs_file->header.nb_files--;
            return err;
        } else {
            return err;
        }
    }

    if (imgfs_file->metadata[index].offset[ORIG_RES] == 0) {

        if (fseek(imgfs_file->file, 0, SEEK_END) != 0) {
            return ERR_IO;
        }

        long offset = ftell(imgfs_file->file);

        if (offset == -1L) {
            return ERR_IO;
        }
        imgfs_file->metadata[index].offset[ORIG_RES] = (uint64_t) offset;
        if (fwrite(image_buffer, image_size, 1, imgfs_file->file) != 1) {
            return ERR_IO;
        }
        // update the metadata
        imgfs_file->metadata[index].size[ORIG_RES] = (uint32_t) image_size;
    }

    // update header version
    imgfs_file->header.version += 1;

    // Write the new header
    if (fseek(imgfs_file->file, 0, SEEK_SET) != 0) {
        return ERR_IO;
    }
    if (fwrite(&imgfs_file->header, sizeof(struct imgfs_header), 1, imgfs_file->file) != 1) {
        return ERR_IO;
    }

    // Write the new metadata
    if (fseek(imgfs_file->file, sizeof(struct imgfs_header) + index * sizeof(struct img_metadata), SEEK_SET) != 0) {
        return ERR_IO;
    }
    if (fwrite(&imgfs_file->metadata[index], sizeof(struct img_metadata), 1, imgfs_file->file) != 1) {
        return ERR_IO;
    }

    return ERR_NONE;
}
