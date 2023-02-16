/*
 * Tilemap Town client for 3DS
 *
 * Copyright (C) 2023 NovaSquirrel
 *
 * This program is free software: you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 3 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */
#include "town.hpp"
#include <3ds.h>
#include <png.h>

uint8_t *png_memory_reader_data;
size_t png_memory_reader_index;
size_t png_memory_reader_size;
void png_read_from_memory(png_structp png_ptr, png_bytep out, png_size_t size) {
	if(png_memory_reader_index + size >= png_memory_reader_size) {
		png_error(png_ptr, "Read past the end of the PNG");
		return;
	}
	memcpy(out, png_memory_reader_data + png_memory_reader_index, size);
	png_memory_reader_index += size;
}

void http_png_callback(const char *url, uint8_t *memory, size_t size, TilemapTownClient *client, void *userdata) {
	puts("Callback called");
	printf("Url %s Size %d\n", url, size);

	// ---

	png_structp png_ptr;
	png_infop info_ptr;

	int width, height;
	png_byte color_type;
	png_byte bit_depth;
	int number_of_passes;
	png_bytep * row_pointers;

	if(png_sig_cmp(memory, 0, 8)) {
		puts("Not a PNG file");
		return;
	}

	png_ptr = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
	if(!png_ptr) {
		puts("png_create_read_struct failed");
		return;
	}

	info_ptr = png_create_info_struct(png_ptr);
	if(!info_ptr) {
		png_destroy_read_struct(&png_ptr, NULL, NULL);
		puts("png_create_info_struct failed");
		return;
	}

	png_memory_reader_data  = memory;
	png_memory_reader_index = 8;
	png_memory_reader_size  = size;
	png_set_read_fn(png_ptr, NULL, png_read_from_memory);
	png_set_sig_bytes(png_ptr, 8);

	// Read info
	png_read_info(png_ptr, info_ptr);
	width      = png_get_image_width(png_ptr, info_ptr);
	height     = png_get_image_height(png_ptr, info_ptr);
	color_type = png_get_color_type(png_ptr, info_ptr);
	bit_depth  = png_get_bit_depth(png_ptr, info_ptr);
	number_of_passes = png_set_interlace_handling(png_ptr);
	printf("PNG info: %d %d, %d %d %d\n", width, height, color_type, bit_depth, number_of_passes);

	// Apply transformations
	if(color_type == PNG_COLOR_TYPE_PALETTE)
        png_set_palette_to_rgb(png_ptr);
	if(color_type == PNG_COLOR_TYPE_GRAY &&
		bit_depth < 8) png_set_expand_gray_1_2_4_to_8(png_ptr);
	if(png_get_valid(png_ptr, info_ptr, PNG_INFO_tRNS))
		png_set_tRNS_to_alpha(png_ptr);
	if(bit_depth == 16)
		png_set_strip_16(png_ptr);
	png_read_update_info(png_ptr, info_ptr);

	// Read pixels
	if(setjmp(png_jmpbuf(png_ptr))) {
		puts("Error during read_image");
		return;
	}

	row_pointers = (png_bytep*)malloc(sizeof(png_bytep) * height);
	for(int y=0; y<height; y++)
		row_pointers[y] = (png_byte*)malloc(png_get_rowbytes(png_ptr, info_ptr));
	png_read_image(png_ptr, row_pointers);

	puts("Read everything");

	// Cleanup
	png_destroy_read_struct(&png_ptr, &info_ptr, NULL);
	for(int y=0; y<height; y++)
		free(row_pointers[y]);
}
