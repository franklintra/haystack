/**
 * @file image_dedup.c
 * @brief Image deduplication.
 */

#include "image_dedup.h"
#include "imgfs.h"
#include "error.h"
#include <string.h>


/**
 * @brief Deduplicates images by name and content in an imgFS file system.
 *
 * This function iterates over all valid images in the imgfs_file, excluding the image at the given index.
 * It checks for duplicate names (img_id) and content (SHA value) among the images.
 * If a duplicate name is found, it returns ERR_DUPLICATE_ID.
 * If a duplicate content is found, it updates the metadata at the index to reference the attributes of the found copy.
 * If no duplicate content is found, it sets the ORIG_RES offset to 0.
 * If no duplicate name is found, it returns ERR_NONE.
 *
 * @param imgfs_file A pointer to the imgfs_file structure where the images and metadata are stored.
 * @param index The index of the image in the metadata array to check for duplication.
 * @return Returns ERR_NONE if deduplication is successful or no duplicates are found.
 *         Returns ERR_DUPLICATE_ID if a duplicate name is found.
 *         Returns other error codes in case of error. see 'error.h' for more details.
 */
int do_name_and_content_dedup(struct imgfs_file* imgfs_file, uint32_t index)
{
    M_REQUIRE_NON_NULL(imgfs_file);


    if (index >= imgfs_file->header.max_files) {
        return ERR_IMAGE_NOT_FOUND;
    }

    // Get the metadata for the target image
    struct img_metadata* target_metadata = &imgfs_file->metadata[index];
    if (!target_metadata->is_valid) {
        return ERR_IMAGE_NOT_FOUND;
    }

    target_metadata->offset[ORIG_RES] = 0;
    for (uint32_t i = 0; i < imgfs_file->header.max_files; ++i) {
        if (i != index && imgfs_file->metadata[i].is_valid) {
            debug_printf("target img_id: %s\n", target_metadata->img_id);
            debug_printf("current img_id: %s\n", imgfs_file->metadata[i].img_id);
            if (strcmp(imgfs_file->metadata[i].img_id, target_metadata->img_id) == 0) {
                return ERR_DUPLICATE_ID;
            }

            if (memcmp(imgfs_file->metadata[i].SHA, target_metadata->SHA, SHA256_DIGEST_LENGTH) == 0) {
                // deduplication of the content
                for (int res = 0; res < NB_RES; ++res) {
                    target_metadata->offset[res] = imgfs_file->metadata[i].offset[res];
                    target_metadata->size[res] = imgfs_file->metadata[i].size[res];
                }
            }
        }
    }

    return ERR_NONE;
}
