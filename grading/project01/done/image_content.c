/**
 * @file image_content.c
 * @brief Image content manipulation functions.
 *
 * This file contains the implementation of the functions that manipulate the content of the image file system.
 *
 *
 * @author Franklin Tranie & Paris Messin
 */

#include "imgfs.h"
#include "error.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <vips/vips.h>

/**
 * @brief Create a new resolution for an image in the file system if it does not already exist.
 *
 * This function checks if the image at the given position in the imgfs_file already has a copy at the given resolution.
 * If it does not, it creates a new image with the given resolution and stores it at the end of the file.
 * It also updates the metadata of the image with the offset of the new image so that it can be accessed later.
 * If the image already has a copy at the given resolution, the function does nothing.
 *
 * @param resolution The desired resolution to create.
 * @param imgfs_file A pointer to the imgfs_file structure where the image and metadata are stored.
 * @param position The index of the image in the metadata array.
 * @return Returns ERR_NONE if everything went well.
 *         Returns other error codes in case of error. see 'error.h' for more details.
 */

int lazily_resize(int resolution, struct imgfs_file *imgfs_file, size_t position)
{
    // Check argument validity
    M_REQUIRE_NON_NULL(imgfs_file);
    M_REQUIRE_NON_NULL(imgfs_file->metadata);

    if (resolution < THUMB_RES || resolution > ORIG_RES) {
        return ERR_RESOLUTIONS;
    }

    if (position >= imgfs_file->header.nb_files) {
        return ERR_INVALID_IMGID;
    }

    if (imgfs_file->metadata[position].size[resolution] != EMPTY) {
        return ERR_NONE;
    }

    // End check argument validity

    // If the res is
    if (resolution == ORIG_RES) {
        return ERR_NONE;
    }

    // Load image with original resolution
    int err = fseek(imgfs_file->file, (long)imgfs_file->metadata[position].offset[ORIG_RES], SEEK_SET);
    if (err != 0) return ERR_IO;


    void *original_buffer = malloc(imgfs_file->metadata[position].size[ORIG_RES]);
    if (original_buffer == NULL) return ERR_OUT_OF_MEMORY;


    size_t read_items = fread(original_buffer, imgfs_file->metadata[position].size[ORIG_RES], 1, imgfs_file->file);
    size_t write_items;
    if (read_items != 1) {
        free(original_buffer);
        return ERR_IO;
    }

    // Create the VipsImage with the original resolution
    VipsImage *original_vips_image = NULL;
    if (vips_jpegload_buffer(original_buffer, imgfs_file->metadata[position].size[ORIG_RES], &original_vips_image, NULL) != 0) {
        free(original_buffer);
        return ERR_IMGLIB;
    }

    // Create the VipsImage with the new resolution
    VipsImage *new_vips_image = NULL;
    uint16_t width = imgfs_file->header.resized_res[2 * resolution];
    // uint16_t height = imgfs_file->header.resized_res[2 * resolution + 1];
    if (vips_thumbnail_image(original_vips_image, &new_vips_image, width, "size", VIPS_SIZE_BOTH, NULL) != 0) {
        g_object_unref(original_vips_image);
        free(original_buffer);
        return ERR_IMGLIB;
    }

    // Convert the new VipsImage in a tab
    size_t resolution_size = 0;
    void *new_buffer = NULL;
    if (vips_jpegsave_buffer(new_vips_image, &new_buffer, &resolution_size, NULL) != 0) {
        g_object_unref(original_vips_image);
        g_object_unref(new_vips_image);
        free(original_buffer);
        return ERR_IMGLIB;
    }

    // Write the new image with the given resoltution at the end of the file
    // Explicitly cast the result of ftell to uint64_t
    // Store the offset of the image at the given resolution
    err = fseek(imgfs_file->file, 0L, SEEK_END);
    if (err != 0) {
        g_object_unref(original_vips_image);
        g_object_unref(new_vips_image);
        free(original_buffer);
        g_free(new_buffer);
        return ERR_IO;
    }

    uint64_t new_offset = (uint64_t)ftell(imgfs_file->file);
    if (new_offset == (uint64_t)-1) {
        g_object_unref(original_vips_image);
        g_object_unref(new_vips_image);
        free(original_buffer);
        g_free(new_buffer);
        return ERR_IO;
    }

    write_items = fwrite(new_buffer, resolution_size, 1, imgfs_file->file);
    if (write_items != 1) {
        g_object_unref(original_vips_image);
        g_object_unref(new_vips_image);
        free(original_buffer);
        g_free(new_buffer);
        return ERR_IO;
    }

    // Update the metadata
    imgfs_file->metadata[position].offset[resolution] = new_offset;
    imgfs_file->metadata[position].size[resolution] = (uint32_t) resolution_size;

    // Write the new metadata of the image with the offset at the given resolution
    err = fseek(imgfs_file->file, (long) (sizeof(struct imgfs_header) + position * sizeof(struct img_metadata)), SEEK_SET);
    if (err != 0) {
        g_object_unref(original_vips_image);
        g_object_unref(new_vips_image);
        free(original_buffer);
        g_free(new_buffer);
        return ERR_IO;
    }

    write_items = fwrite(&imgfs_file->metadata[position], sizeof(struct img_metadata), 1, imgfs_file->file);
    if (write_items != 1) {
        g_object_unref(original_vips_image);
        g_object_unref(new_vips_image);
        free(original_buffer);
        g_free(new_buffer);
        return ERR_IO;
    }

    g_object_unref(original_vips_image);
    g_object_unref(new_vips_image);
    free(original_buffer);
    g_free(new_buffer);

    return ERR_NONE;
}
