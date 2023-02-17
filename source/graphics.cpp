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
#include <png.h>

Tex3DS_SubTexture calc_subtexture(int width, int height, int tile_width, int tile_height, int tile_x, int tile_y) {
	Tex3DS_SubTexture out;
	out.width  = tile_width;
	out.height = tile_height;

	float one_x = (width / tile_width);
	float one_y = (height / tile_height);
	out.left   = (float)(tile_x+0)/one_x;
	out.bottom = (float)(tile_y+0)/one_y;
	out.right  = (float)(tile_x+1)/one_x;
	out.top    = (float)(tile_y+1)/one_y;
	return out;
}

static u32 next_power_of_two(u32 v) {
	v--;
	v |= v >> 1;
	v |= v >> 2;
	v |= v >> 4;
	v |= v >> 8;
	v |= v >> 16;
	return v + 1;
}

int texture_loaded_yet = 0;

void http_png_callback(const char *url, uint8_t *memory, size_t size, TilemapTownClient *client, void *userdata) {
	printf("Url %s Size %d\n", url, size);

	// ---
	// Based on https://github.com/asiekierka/atari800-3ds/blob/ee116ce923ccaafd76e2c44d51c71fe969f07ea2/src/3ds/grapefruit.c

	png_image image;
	u32 *data;
	C3D_Tex* tex = (C3D_Tex*)linearAlloc(sizeof(C3D_Tex));

	memset(&image, 0, sizeof(image));
	image.version = PNG_IMAGE_VERSION;

	png_image_begin_read_from_memory(&image, memory, size);

	image.format = PNG_FORMAT_ABGR;

	C3D_TexInitVRAM(tex, next_power_of_two(image.width), next_power_of_two(image.height), GPU_RGBA8);
	data = (u32*)linearAlloc(tex->width * tex->height * sizeof(u32));

	if(!png_image_finish_read(&image, NULL, data, tex->width * sizeof(u32), NULL)) {
		linearFree(data);
		C3D_TexDelete(tex);
		return;
	}

	GSPGPU_FlushDataCache(data, tex->width * tex->height * sizeof(u32));

	C3D_SyncDisplayTransfer(data, GX_BUFFER_DIM(tex->width, tex->height),
		(u32*)tex->data, GX_BUFFER_DIM(tex->width, tex->height),
		GX_TRANSFER_FLIP_VERT(1) | GX_TRANSFER_OUT_TILED(1) | GX_TRANSFER_RAW_COPY(0) |
		GX_TRANSFER_IN_FORMAT(GPU_RGBA8) | GX_TRANSFER_OUT_FORMAT(GPU_RGBA8)
		| GX_TRANSFER_SCALING(GX_TRANSFER_SCALE_NO));

	linearFree(data);

	client->texture_for_url[std::string(url)] = tex;
	client->need_redraw = true;
}

C2D_Image* Pic::get(TilemapTownClient *client) {
	if(this->ready_to_draw) {
		return &this->image;
	}

	// Try to turn a key into a URL if needed
	std::string *real_url = &this->key;
	if(!this->key.starts_with("https://") && !this->key.starts_with("http://")) {
		auto it2 = client->url_for_tile_sheet.find(this->key);
		if(it2 != client->url_for_tile_sheet.end()) {
			real_url = &(*it2).second;
		} else {
			if(client->requested_tile_sheets.find(this->key) != client->requested_tile_sheets.end()) {
				puts("Requesting a tileset");
				client->requested_tile_sheets.insert(this->key);
				return nullptr;
			}
		}
	}

	// If it's already a loaded texture, get it
	auto it = client->texture_for_url.find(*real_url);
	if(it != client->texture_for_url.end()) {
		this->ready_to_draw = true;
		C3D_Tex *tex = (*it).second;

		this->subtexture = calc_subtexture(tex->width, tex->height, 16, 16, this->x, this->y);
		this->image = {tex, &this->subtexture};
		return &this->image;
	} else {
		client->http.get(*real_url, http_png_callback, nullptr);
		return nullptr;
	}
}

void TilemapTownClient::draw_map(int camera_x, int camera_y) {
	if(!this->map_received)
		return;

	for(int y=0; y<15; y++) {
		for(int x=0; x<25; x++) {
			int index = y * this->town_map.width + x;
			MapTileInfo *turf = this->town_map.cells[index].turf.get(this);

			// Draw turf

			if(turf) {
				C2D_Image *image = turf->pic.get(this);
				if(image) {
					C2D_DrawImageAt(*image, x*16, y*16, 0, NULL, 1.0f, -1.0f);
				} else {
					C2D_DrawRectangle(x*16, y*16, 0, 16, 16, C2D_Color32(255, 0, 0, 255), C2D_Color32(255, 0, 0, 255), C2D_Color32(255, 0, 0, 255), C2D_Color32(255, 0, 0, 255));
				}
			} else {
				C2D_DrawRectangle(x*16, y*16, 0, 16, 16, C2D_Color32(255, 255, 0, 255), C2D_Color32(255, 255, 0, 255), C2D_Color32(255, 255, 0, 255), C2D_Color32(255, 255, 0, 255));
			}

			// Draw objects

			for(auto & element : this->town_map.cells[index].objs) {
				MapTileInfo *obj = element.get(this);
				if(!obj)
					continue;
				C2D_Image *image = obj->pic.get(this);
				if(image) {
					C2D_DrawImageAt(*image, x*16, y*16, 0, NULL, 1.0f, -1.0f);
				}
			}
		}
	}
}

