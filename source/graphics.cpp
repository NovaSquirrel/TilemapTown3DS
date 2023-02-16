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


void http_png_callback(const char *url, uint8_t *memory, size_t size, TilemapTownClient *client, void *userdata) {
	puts("Callback called");
	printf("Url %s Size %d\n", url, size);

	// ---

	png_image image;
	memset(&image, 0, sizeof(image));
	image.version = PNG_IMAGE_VERSION;

	png_image_begin_read_from_memory(&image, memory, size);
	image.format = PNG_FORMAT_RGBA;

	size_t input_data_length = PNG_IMAGE_SIZE(image);
	uint8_t *pixel_data = (uint8_t*)calloc(1, input_data_length);

	if(png_image_finish_read(&image, NULL, pixel_data, 0, NULL) == 0) {
		puts("Image read failed");
	}

	printf("%.2x %.2x %.2x %.2x %.2x %.2x %.2x %.2x\n", pixel_data[0], pixel_data[1], pixel_data[2], pixel_data[3], pixel_data[4], pixel_data[5], pixel_data[6], pixel_data[7]);

	free(pixel_data);
	png_image_free(&image);
}
