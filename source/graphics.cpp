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
#include <algorithm>
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

// LUT from http://problemkaputt.de/gbatek-3ds-video-texture-swizzling.htm
static u8 swizzle_lut[64] = {
	0x00, 0x01, 0x08, 0x09, 0x02, 0x03, 0x0A, 0x0B,
	0x10, 0x11, 0x18, 0x19, 0x12, 0x13, 0x1A, 0x1B,
	0x04, 0x05, 0x0C, 0x0D, 0x06, 0x07, 0x0E, 0x0F,
	0x14, 0x15, 0x1C, 0x1D, 0x16, 0x17, 0x1E, 0x1F,
	0x20, 0x21, 0x28, 0x29, 0x22, 0x23, 0x2A, 0x2B,
	0x30, 0x31, 0x38, 0x39, 0x32, 0x33, 0x3A, 0x3B,
	0x24, 0x25, 0x2C, 0x2D, 0x26, 0x27, 0x2E, 0x2F,
	0x34, 0x35, 0x3C, 0x3D, 0x36, 0x37, 0x3E, 0x3F,
};

void http_png_callback(const char *url, uint8_t *memory, size_t size, TilemapTownClient *client, void *userdata) {
	//printf("Url %s Size %d\n", url, size);

	// ---
	// Based on https://github.com/asiekierka/atari800-3ds/blob/ee116ce923ccaafd76e2c44d51c71fe969f07ea2/src/3ds/grapefruit.c

	png_image image;
	u32 *linear_pixels, *swizzled_pixels;
	C3D_Tex* tex = (C3D_Tex*)linearAlloc(sizeof(C3D_Tex));

	memset(&image, 0, sizeof(image));
	image.version = PNG_IMAGE_VERSION;

	png_image_begin_read_from_memory(&image, memory, size);

	image.format = PNG_FORMAT_ABGR;

	// Prepare a place to put the decoded PNG, and decode it

	C3D_TexInit(tex, next_power_of_two(image.width), next_power_of_two(image.height), GPU_RGBA8);
	linear_pixels   = (u32*)linearAlloc(tex->width * tex->height * sizeof(u32));
	swizzled_pixels = (u32*)linearAlloc(tex->width * tex->height * sizeof(u32));
	size_t stride = tex->width * sizeof(u32);

	if(!png_image_finish_read(&image, NULL, linear_pixels, stride, NULL)) {
		linearFree(linear_pixels);
		linearFree(swizzled_pixels);
		C3D_TexDelete(tex);
		return;
	}

	// Swizzle the texture
	memset(swizzled_pixels, 0xff, tex->width * tex->height * sizeof(u32));

	u32 *sw = swizzled_pixels;
	for(size_t ty = 0; ty < tex->height/8; ty++) {
		for(size_t tx = 0; tx < tex->width/8; tx++) {
			for(size_t px =0; px<64; px++) {
				u8 from_table = swizzle_lut[px];
				u8 table_x = from_table & 7;
				u8 table_y = (from_table >> 3) & 7;

				*sw = linear_pixels[(tex->height-1-(ty*8+table_y))*(tex->width) + (tx*8+table_x)];
				sw++;
			}
		}
	}

	C3D_TexUpload(tex, swizzled_pixels);
	C3D_TexSetFilter(tex, GPU_LINEAR, GPU_NEAREST);
	C3D_TexSetWrap(tex, GPU_REPEAT, GPU_REPEAT);
	C3D_TexBind(0, tex);

	linearFree(linear_pixels);
	linearFree(swizzled_pixels);

	LoadedTextureInfo loaded_texture_info;
	loaded_texture_info.original_width  = image.width;
	loaded_texture_info.original_height = image.height;
	loaded_texture_info.texture = tex;

	client->texture_for_url[std::string(url)] = loaded_texture_info;
	client->need_redraw = true;
	//puts("Finished decoding texture");
}

bool string_is_http_url(std::string &url) {
	return url.starts_with("https://") || url.starts_with("http://");
}

C3D_Tex* Pic::get_texture(TilemapTownClient *client) {
	if(this->ready_to_draw) {
		return this->image.tex;
	}

	// Try to turn a key into a URL if needed
	std::string *real_url = &this->key;
	if(!string_is_http_url(this->key)) {
		auto it2 = client->url_for_tile_sheet.find(this->key);
		if(it2 != client->url_for_tile_sheet.end()) {
			real_url = &(*it2).second;
		} else {
			client->request_image_asset(this->key);
			return nullptr;
		}
	}

	// If it's already a loaded texture, get it
	auto it = client->texture_for_url.find(*real_url);
	if(it != client->texture_for_url.end()) {
		this->ready_to_draw = true;
		C3D_Tex *tex = (*it).second.texture;

		this->extra_info = &(*it).second; // Save this so we can get the original size later
		this->subtexture = calc_subtexture(tex->width, tex->height, 16, 16, this->x, this->y);
		this->image = {tex, &this->subtexture}; // Record the C2D_Image so Pic::get() can have it
		return tex;
	} else {
		client->http.get(*real_url, http_png_callback, nullptr);
		return nullptr;
	}
}

C2D_Image* Pic::get(TilemapTownClient *client) {
	if(this->ready_to_draw) {
		return &this->image;
	} else if(this->get_texture(client) != nullptr) {
		return &this->image;
	}
	return nullptr;
}

bool sort_entity_by_y_pos(Entity *a, Entity *b) {
    return (a->y < b->y);
}

void draw_atom_with_pic_offset(TilemapTownClient *client, MapTileInfo *turf, int offset_x, int offset_y, float draw_x, float draw_y) {
	C2D_Image *image = turf->pic.get(client);
	if(image) {
		Tex3DS_SubTexture subtexture = calc_subtexture(image->tex->width, image->tex->height, 16, 16, turf->pic.x + offset_x, turf->pic.y + offset_y);
		C2D_Image new_image = {image->tex, &subtexture};
		C2D_DrawImageAt(new_image, draw_x, draw_y, 0, NULL, 1.0f, -1.0f);
	}
}

void draw_atom_quadrant_with_pic_offset(TilemapTownClient *client, MapTileInfo *turf, int offset_x, int offset_y, float draw_x, float draw_y) {
	C2D_Image *image = turf->pic.get(client);
	if(image) {
		Tex3DS_SubTexture subtexture = calc_subtexture(image->tex->width, image->tex->height, 8, 8, turf->pic.x*2 + offset_x, turf->pic.y*2 + offset_y);
		C2D_Image new_image = {image->tex, &subtexture};
		C2D_DrawImageAt(new_image, draw_x, draw_y, 0, NULL, 1.0f, -1.0f);
	}
}

void draw_atom_with_autotile(TilemapTownClient *client, MapTileInfo *atom, int real_x, int real_y, float draw_x, float draw_y, bool obj) {
	switch(atom->autotile_layout) {
		case 0: // No autotiling
		{
			C2D_Image *image = atom->pic.get(client);
			if(image) {
				C2D_DrawImageAt(*image, draw_x, draw_y, 0, NULL, 1.0f, -1.0f);
			}
			break;
		}
		case 1: // 4-direction autotiling, 9 tiles, origin is middle
		{
			unsigned int autotile_index = obj ? client->get_obj_autotile_index_4(atom, real_x, real_y) : client->get_turf_autotile_index_4(atom, real_x, real_y);
			const static int offset_x_list[] = {0,0,0,0,   0,1,-1,0,    0, 1,-1, 0,  0,1,-1,0};
			const static int offset_y_list[] = {0,0,0,0,   0,1, 1,1,    0,-1,-1,-1,  0,0, 0,0};
			draw_atom_with_pic_offset(client, atom, offset_x_list[autotile_index], offset_y_list[autotile_index], draw_x, draw_y);
			break;
		}
		case 2: // 4-direction autotiling, 9 tiles, origin is middle, horizonal & vertical & single as separate tiles
		case 3: // Same as 2, but origin point is single
		{
			unsigned int autotile_index = obj ? client->get_obj_autotile_index_4(atom, real_x, real_y) : client->get_turf_autotile_index_4(atom, real_x, real_y);
			const static int offset_x_list[] = { 2,1,-1,0};
			const static int offset_y_list[] = {-2,1,-1,0};
			bool isThree = atom->autotile_layout == 3;
			draw_atom_with_pic_offset(client, atom, offset_x_list[autotile_index&3] - (isThree?2:0), offset_y_list[autotile_index>>2] + (isThree?2:0), draw_x, draw_y);
			break;
		}
		case 4: // 8-direction autotiling, origin point is middle
		case 5: // 8-direction autotiling, origin point is single
		{
			unsigned int autotile_index = obj ? client->get_obj_autotile_index_4(atom, real_x, real_y) : client->get_turf_autotile_index_4(atom, real_x, real_y);
			const static int offset_0x[] = {-2, 2,-2, 0,-2, 2,-2, 0,-2, 2,-2, 0,-2, 2,-2, 0};
			const static int offset_0y[] = {-4,-2,-2,-2, 2, 2, 2, 2,-2,-2,-2,-2, 0, 0, 0, 0};
			const static int offset_1x[] = {-1, 3,-1, 1, 3, 3,-1, 1, 3, 3,-1, 1, 3, 3,-1, 1};
			const static int offset_1y[] = {-4,-2,-2,-2, 2, 2, 2, 2,-2,-2,-2,-2, 0, 0, 0, 0};
			const static int offset_2x[] = {-2, 2,-2, 0,-2, 2,-2, 0,-2, 2,-2, 0,-2, 2,-2, 0};
			const static int offset_2y[] = {-3, 3, 3, 3, 3, 3, 3, 3,-1, 1,-1,-1, 1, 1, 1, 1};
			const static int offset_3x[] = {-1, 3,-1, 1, 3, 3,-1, 1, 3, 3,-1, 1, 3, 3,-1, 1};
			const static int offset_3y[] = {-3, 3, 3, 3, 3, 3, 3, 3,-1, 1,-1,-1, 1, 1, 1, 1};

			int t0x = offset_0x[autotile_index], t0y = offset_0y[autotile_index];
			int t1x = offset_1x[autotile_index], t1y = offset_1y[autotile_index];
			int t2x = offset_2x[autotile_index], t2y = offset_2y[autotile_index];
			int t3x = offset_3x[autotile_index], t3y = offset_3y[autotile_index];
			
			// Add the inner parts of turns
			if(((autotile_index &  5) ==  5)
			&& !(obj ? client->is_obj_autotile_match(atom, real_x-1, real_y-1) : client->is_turf_autotile_match(atom, real_x-1, real_y-1))) {
				t0x = 2; t0y = -4;
			}
			if(((autotile_index &  6) ==  6)
			&& !(obj ? client->is_obj_autotile_match(atom, real_x+1, real_y-1) : client->is_turf_autotile_match(atom, real_x+1, real_y-1))) {
				t1x = 3; t1y = -4;
			}
			if(((autotile_index &  9) ==  9)
			&& !(obj ? client->is_obj_autotile_match(atom, real_x-1, real_y+1) : client->is_turf_autotile_match(atom, real_x-1, real_y+1))) {
				t2x = 2; t2y = -3;
			}
			if(((autotile_index & 10) == 10)
			&& !(obj ? client->is_obj_autotile_match(atom, real_x+1, real_y+1) : client->is_turf_autotile_match(atom, real_x+1, real_y+1))) {
				t3x = 3; t3y = -3;
			}

			// For 4 the origin point is on the single tile instead of the all-connected tile
			if(atom->autotile_layout == 5) {
				t0x += 2; t1x += 2; t2x += 2; t3x += 2;
				t0y += 4; t1y += 4; t2y += 4; t3y += 4;
			}

			// Draw the four tiles
			draw_atom_quadrant_with_pic_offset(client, atom, t0x, t0y, draw_x,   draw_y  );
			draw_atom_quadrant_with_pic_offset(client, atom, t1x, t1y, draw_x+8, draw_y  );
			draw_atom_quadrant_with_pic_offset(client, atom, t2x, t2y, draw_x,   draw_y+8);
			draw_atom_quadrant_with_pic_offset(client, atom, t3x, t3y, draw_x+8, draw_y+8);
			break;
		}
	}
}

bool TilemapTownClient::is_turf_autotile_match(MapTileInfo *turf, int x, int y) {
	// Is the turf tile on the map at x,y the "same" as 'turf' for autotiling purposes?
	if(x < 0 || x >= this->town_map.width || y < 0 || y >= this->town_map.height)
		return true;
	MapTileInfo *other = this->town_map.cells[y * this->town_map.width + x].turf.get(this);

	if(turf->autotile_class)
		return turf->autotile_class == other->autotile_class;
	if(!turf->name.empty())
		return turf->name == other->name;
	return false;
}

bool TilemapTownClient::is_obj_autotile_match(MapTileInfo *obj, int x, int y) {
	// Is any obj tile on the map at x,y the "same" as 'obj' for autotiling purposes?
	if(x < 0 || x >= this->town_map.width || y < 0 || y >= this->town_map.height)
		return true;
	
	for(auto & element : this->town_map.cells[y * this->town_map.width + x].objs) {
		MapTileInfo *other_obj = element.get(this);
		if(!other_obj)
			continue;
		if(obj->autotile_class) {
			if(obj->autotile_class == other_obj->autotile_class)
				return true;
		} else if(!obj->name.empty()) {
			if(obj->name == other_obj->name)
				return true;
		}
	}
	return false;
}

unsigned int TilemapTownClient::get_turf_autotile_index_4(MapTileInfo *turf, int x, int y) {
	/* Check on the four adjacent tiles and see if they "match", to get an index for an autotile lookup table.
		Will result in one of the following:
		 0 durl  1 durL  2 duRl  3 duRL
		 4 dUrl  5 dUrL  6 dURl  7 dURL
		 8 Durl  9 DurL 10 DuRl 11 DuRL
		12 DUrl 13 DUrL 14 DURl 15 DURL
	*/
	return (this->is_turf_autotile_match(turf, x-1, y) << 0)
	     | (this->is_turf_autotile_match(turf, x+1, y) << 1)
	     | (this->is_turf_autotile_match(turf, x, y-1) << 2)
	     | (this->is_turf_autotile_match(turf, x, y+1) << 3);
}

unsigned int TilemapTownClient::get_obj_autotile_index_4(MapTileInfo *turf, int x, int y) {
	/* Check on the four adjacent tiles and see if they "match", to get an index for an autotile lookup table.
		Will result in one of the following:
		 0 durl  1 durL  2 duRl  3 duRL
		 4 dUrl  5 dUrL  6 dURl  7 dURL
		 8 Durl  9 DurL 10 DuRl 11 DuRL
		12 DUrl 13 DUrL 14 DURl 15 DURL
	*/
	return (this->is_obj_autotile_match(turf, x-1, y) << 0)
	     | (this->is_obj_autotile_match(turf, x+1, y) << 1)
	     | (this->is_obj_autotile_match(turf, x, y-1) << 2)
	     | (this->is_obj_autotile_match(turf, x, y+1) << 3);
}

void TilemapTownClient::draw_map(int camera_x, int camera_y) {
	if(!this->map_received)
		return;
	this->animation_tick = (this->animation_tick+1) % 60000;

	int camera_offset_x = camera_x % 16;
	int camera_offset_y = camera_y % 16;
	int camera_tile_x = camera_x / 16;
	int camera_tile_y = camera_y / 16;

	for(int y=0; y<=VIEW_HEIGHT_TILES; y++) {
		for(int x=0; x<=VIEW_WIDTH_TILES; x++) {
			int real_x = camera_tile_x + x;
			int real_y = camera_tile_y + y;
			if(real_x < 0 || real_x >= this->town_map.width || real_y < 0 || real_y >= this->town_map.height)
				continue;

			int index = real_y * this->town_map.width + real_x;
			float draw_x = x*16-camera_offset_x;
			float draw_y = y*16-camera_offset_y;

			// Draw turf
			MapTileInfo *turf = this->town_map.cells[index].turf.get(this);
			if(turf) {
				draw_atom_with_autotile(this, turf, real_x, real_y, draw_x, draw_y, false);
			}

			// Draw objects
			for(auto & element : this->town_map.cells[index].objs) {
				MapTileInfo *obj = element.get(this);
				if(!obj || obj->over)
					continue;
				draw_atom_with_autotile(this, obj, real_x, real_y, draw_x, draw_y, true);
			}
		}
	}

	// Draw entities

	std::vector<Entity*> sorted_entities;
	for(auto& [key, entity] : this->who) {
		sorted_entities.push_back(&entity);
	}
	std::sort(sorted_entities.begin(), sorted_entities.end(), sort_entity_by_y_pos);

	for(auto& entity : sorted_entities) {
		if(entity->walk_timer)
			entity->walk_timer--;
		if(
			(entity->x < (camera_tile_x - 3)) ||
			(entity->y < (camera_tile_y - 3)) ||
			(entity->x > (camera_tile_x + VIEW_WIDTH_TILES + 3)) ||
			(entity->y > (camera_tile_y + VIEW_HEIGHT_TILES + 3))
		)
			continue;

		const C2D_Image *image = entity->pic.get(this);
		if(image) {
			int tileset_width  = entity->pic.extra_info->original_width;
			int tileset_height = entity->pic.extra_info->original_height;
			//bool player_is_16x16 = false;

			if(tileset_width == 16 && tileset_height == 16) {
				//player_is_16x16 = true;

				Tex3DS_SubTexture subtexture = calc_subtexture(image->tex->width, image->tex->height, 16, 16, 0, 0);
				C2D_Image new_image = {image->tex, &subtexture};
				C2D_DrawImageAt(new_image, (entity->x*16)-camera_x+entity->offset_x, (entity->y*16)-camera_y+entity->offset_y, 0, NULL, 1.0f, -1.0f);
			} else if(string_is_http_url(entity->pic.key)) {
				int frame_x = 0, frame_y = 0;
				int frame_count_from_animation_tick = this->animation_tick / 6;
				bool is_walking = entity->walk_timer != 0;

				switch(tileset_height / 32) { // Directions
					case 2: frame_y = entity->direction_lr / 4; break;
					case 4: frame_y = entity->direction_4 / 2; break;
					case 8: frame_y = entity->direction; break;
				}
				switch(tileset_width / 32) { // Frames per direction
					case 2: frame_x = (is_walking * 1); break;
					case 4: frame_x = (is_walking * 2) + (frame_count_from_animation_tick & 1); break;
					case 6: frame_x = (is_walking * 3) + (frame_count_from_animation_tick % 3); break;
					case 8: frame_x = (is_walking * 4) + (frame_count_from_animation_tick & 3); break;
				}

				Tex3DS_SubTexture subtexture = calc_subtexture(image->tex->width, image->tex->height, 32, 32, frame_x, frame_y);
				C2D_Image new_image = {image->tex, &subtexture};
				C2D_DrawImageAt(new_image, (entity->x*16-8)-camera_x+entity->offset_x, (entity->y*16-16)-camera_y+entity->offset_y, 0, NULL, 1.0f, -1.0f);
			} else {
				C2D_DrawImageAt(*image, (entity->x*16)-camera_x+entity->offset_x, (entity->y*16)-camera_y+entity->offset_y, 0, NULL, 1.0f, -1.0f);
			}

		}
	}

	// Display "over" objects (Could be reworked to avoid scanning over the map a second time, but it's probably fine)
	for(int y=0; y<=VIEW_HEIGHT_TILES; y++) {
		for(int x=0; x<=VIEW_WIDTH_TILES; x++) {
			int real_x = camera_tile_x + x;
			int real_y = camera_tile_y + y;
			if(real_x < 0 || real_x >= this->town_map.width || real_y < 0 || real_y >= this->town_map.height)
				continue;

			int index = real_y * this->town_map.width + real_x;

			// Draw objects
			for(auto & element : this->town_map.cells[index].objs) {
				MapTileInfo *obj = element.get(this);
				if(!obj || !obj->over)
					continue;
				draw_atom_with_autotile(this, obj, real_x, real_y, x*16-camera_offset_x, y*16-camera_offset_y, true);
			}
		}
	}
}

