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
#include "cJSON.h"
#include <stdarg.h>

#define protocol_command_as_int(a,b,c) (a) | (b<<8) | (c<<16)

#define get_json_item cJSON_GetObjectItemCaseSensitive

/*-
 *  Adapted/copied from https://web.mit.edu/freebsd/head/sys/libkern/crc32.c
 *
 *  COPYRIGHT (C) 1986 Gary S. Brown.  You may use this program, or
 *  code or tables extracted from it, as desired without restriction.
 */

/*
 *  First, the polynomial itself and its table of feedback terms.  The
 *  polynomial is
 *  X^32+X^26+X^23+X^22+X^16+X^12+X^11+X^10+X^8+X^7+X^5+X^4+X^2+X^1+X^0
 *
 *  Note that we take it "backwards" and put the highest-order term in
 *  the lowest-order bit.  The X^32 term is "implied"; the LSB is the
 *  X^31 term, etc.  The X^0 term (usually shown as "+1") results in
 *  the MSB being 1
 *
 *  Note that the usual hardware shift register implementation, which
 *  is what we're using (we're merely optimizing it by doing eight-bit
 *  chunks at a time) shifts bits into the lowest-order term.  In our
 *  implementation, that means shifting towards the right.  Why do we
 *  do it this way?  Because the calculated CRC must be transmitted in
 *  order from highest-order term to lowest-order term.  UARTs transmit
 *  characters in order from LSB to MSB.  By storing the CRC this way
 *  we hand it to the UART in the order low-byte to high-byte; the UART
 *  sends each low-bit to hight-bit; and the result is transmission bit
 *  by bit from highest- to lowest-order term without requiring any bit
 *  shuffling on our part.  Reception works similarly
 *
 *  The feedback terms table consists of 256, 32-bit entries.  Notes
 *
 *      The table can be generated at runtime if desired; code to do so
 *      is shown later.  It might not be obvious, but the feedback
 *      terms simply represent the results of eight shift/xor opera
 *      tions for all combinations of data and CRC register values
 *
 *      The values must be right-shifted by eight bits by the "updcrc
 *      logic; the shift must be unsigned (bring in zeroes).  On some
 *      hardware you could probably optimize the shift in assembler by
 *      using byte-swap instructions
 *      polynomial $edb88320
 *
 *
 * CRC32 code derived from work by Gary S. Brown.
 */

const uint32_t crc32_tab[] = {
	0x00000000, 0x77073096, 0xee0e612c, 0x990951ba, 0x076dc419, 0x706af48f,
	0xe963a535, 0x9e6495a3,	0x0edb8832, 0x79dcb8a4, 0xe0d5e91e, 0x97d2d988,
	0x09b64c2b, 0x7eb17cbd, 0xe7b82d07, 0x90bf1d91, 0x1db71064, 0x6ab020f2,
	0xf3b97148, 0x84be41de,	0x1adad47d, 0x6ddde4eb, 0xf4d4b551, 0x83d385c7,
	0x136c9856, 0x646ba8c0, 0xfd62f97a, 0x8a65c9ec,	0x14015c4f, 0x63066cd9,
	0xfa0f3d63, 0x8d080df5,	0x3b6e20c8, 0x4c69105e, 0xd56041e4, 0xa2677172,
	0x3c03e4d1, 0x4b04d447, 0xd20d85fd, 0xa50ab56b,	0x35b5a8fa, 0x42b2986c,
	0xdbbbc9d6, 0xacbcf940,	0x32d86ce3, 0x45df5c75, 0xdcd60dcf, 0xabd13d59,
	0x26d930ac, 0x51de003a, 0xc8d75180, 0xbfd06116, 0x21b4f4b5, 0x56b3c423,
	0xcfba9599, 0xb8bda50f, 0x2802b89e, 0x5f058808, 0xc60cd9b2, 0xb10be924,
	0x2f6f7c87, 0x58684c11, 0xc1611dab, 0xb6662d3d,	0x76dc4190, 0x01db7106,
	0x98d220bc, 0xefd5102a, 0x71b18589, 0x06b6b51f, 0x9fbfe4a5, 0xe8b8d433,
	0x7807c9a2, 0x0f00f934, 0x9609a88e, 0xe10e9818, 0x7f6a0dbb, 0x086d3d2d,
	0x91646c97, 0xe6635c01, 0x6b6b51f4, 0x1c6c6162, 0x856530d8, 0xf262004e,
	0x6c0695ed, 0x1b01a57b, 0x8208f4c1, 0xf50fc457, 0x65b0d9c6, 0x12b7e950,
	0x8bbeb8ea, 0xfcb9887c, 0x62dd1ddf, 0x15da2d49, 0x8cd37cf3, 0xfbd44c65,
	0x4db26158, 0x3ab551ce, 0xa3bc0074, 0xd4bb30e2, 0x4adfa541, 0x3dd895d7,
	0xa4d1c46d, 0xd3d6f4fb, 0x4369e96a, 0x346ed9fc, 0xad678846, 0xda60b8d0,
	0x44042d73, 0x33031de5, 0xaa0a4c5f, 0xdd0d7cc9, 0x5005713c, 0x270241aa,
	0xbe0b1010, 0xc90c2086, 0x5768b525, 0x206f85b3, 0xb966d409, 0xce61e49f,
	0x5edef90e, 0x29d9c998, 0xb0d09822, 0xc7d7a8b4, 0x59b33d17, 0x2eb40d81,
	0xb7bd5c3b, 0xc0ba6cad, 0xedb88320, 0x9abfb3b6, 0x03b6e20c, 0x74b1d29a,
	0xead54739, 0x9dd277af, 0x04db2615, 0x73dc1683, 0xe3630b12, 0x94643b84,
	0x0d6d6a3e, 0x7a6a5aa8, 0xe40ecf0b, 0x9309ff9d, 0x0a00ae27, 0x7d079eb1,
	0xf00f9344, 0x8708a3d2, 0x1e01f268, 0x6906c2fe, 0xf762575d, 0x806567cb,
	0x196c3671, 0x6e6b06e7, 0xfed41b76, 0x89d32be0, 0x10da7a5a, 0x67dd4acc,
	0xf9b9df6f, 0x8ebeeff9, 0x17b7be43, 0x60b08ed5, 0xd6d6a3e8, 0xa1d1937e,
	0x38d8c2c4, 0x4fdff252, 0xd1bb67f1, 0xa6bc5767, 0x3fb506dd, 0x48b2364b,
	0xd80d2bda, 0xaf0a1b4c, 0x36034af6, 0x41047a60, 0xdf60efc3, 0xa867df55,
	0x316e8eef, 0x4669be79, 0xcb61b38c, 0xbc66831a, 0x256fd2a0, 0x5268e236,
	0xcc0c7795, 0xbb0b4703, 0x220216b9, 0x5505262f, 0xc5ba3bbe, 0xb2bd0b28,
	0x2bb45a92, 0x5cb36a04, 0xc2d7ffa7, 0xb5d0cf31, 0x2cd99e8b, 0x5bdeae1d,
	0x9b64c2b0, 0xec63f226, 0x756aa39c, 0x026d930a, 0x9c0906a9, 0xeb0e363f,
	0x72076785, 0x05005713, 0x95bf4a82, 0xe2b87a14, 0x7bb12bae, 0x0cb61b38,
	0x92d28e9b, 0xe5d5be0d, 0x7cdcefb7, 0x0bdbdf21, 0x86d3d2d4, 0xf1d4e242,
	0x68ddb3f8, 0x1fda836e, 0x81be16cd, 0xf6b9265b, 0x6fb077e1, 0x18b74777,
	0x88085ae6, 0xff0f6a70, 0x66063bca, 0x11010b5c, 0x8f659eff, 0xf862ae69,
	0x616bffd3, 0x166ccf45, 0xa00ae278, 0xd70dd2ee, 0x4e048354, 0x3903b3c2,
	0xa7672661, 0xd06016f7, 0x4969474d, 0x3e6e77db, 0xaed16a4a, 0xd9d65adc,
	0x40df0b66, 0x37d83bf0, 0xa9bcae53, 0xdebb9ec5, 0x47b2cf7f, 0x30b5ffe9,
	0xbdbdf21c, 0xcabac28a, 0x53b39330, 0x24b4a3a6, 0xbad03605, 0xcdd70693,
	0x54de5729, 0x23d967bf, 0xb3667a2e, 0xc4614ab8, 0x5d681b02, 0x2a6f2b94,
	0xb40bbe37, 0xc30c8ea1, 0x5a05df1b, 0x2d02ef8d
};

uint32_t town_crc32(const void *buf, size_t size) {
	const uint8_t *p = (const uint8_t *)buf;
	uint32_t crc;

	crc = ~0U;
	while (size--)
		crc = crc32_tab[(crc ^ *p++) & 0xFF] ^ (crc >> 8);
	return crc ^ ~0U;
}

const char *get_json_string(cJSON *json, const char *name) {
	json = cJSON_GetObjectItemCaseSensitive(json, name);
	if(json == NULL)
		return NULL;
	return cJSON_GetStringValue(json);
}

std::string json_as_string(cJSON *json) {
	if(cJSON_IsString(json))
		return std::string(json->valuestring);
	return std::to_string(json->valueint);
}

int unpack_json_int_array(cJSON *json, int count, int *ptr, ...) {
	if(cJSON_GetArraySize(json) != count)
		return 0;

	va_list list;
	va_start(list, ptr);

	int success = 1; // Successful until it isn't

	for(int index=0; index<count; index++) {
		cJSON *json_int = cJSON_GetArrayItem(json, index);
		if(!cJSON_IsNumber(json_int)) {
			success = 0;
			break;
		}
		*ptr = json_int->valueint;

		ptr = va_arg(list, int*);
	}

	va_end(list);
	return success;
}

int pic_from_json(cJSON *json, struct Pic *out) {
	if(cJSON_GetArraySize(json) != 3)
		return 0;

	cJSON *i_sheet = cJSON_GetArrayItem(json, 0);
	cJSON *i_x     = cJSON_GetArrayItem(json, 1);
	cJSON *i_y     = cJSON_GetArrayItem(json, 2);

	if(cJSON_IsNumber(i_x) && cJSON_IsNumber(i_y)) {
		out->x = i_x->valueint;
		out->y = i_y->valueint;
	} else {
		return 0;
	}

	if(cJSON_IsNumber(i_sheet)) {
		out->key = std::to_string(i_sheet->valueint);
	} else if(cJSON_IsString(i_sheet)) {
		out->key = std::string(i_sheet->valuestring);
	} else {
		return 0;
	}

	return 1;
}

int map_tile_from_json(cJSON *json, MapTileInfo *out) {
	cJSON *i_name    = get_json_item(json, "name");
	cJSON *i_pic     = get_json_item(json, "pic");
	cJSON *i_obj     = get_json_item(json, "obj");
	cJSON *i_density = get_json_item(json, "density");
	cJSON *i_walls   = get_json_item(json, "walls");
	cJSON *i_type    = get_json_item(json, "type");
	cJSON *i_message = get_json_item(json, "message");
	cJSON *i_over    = get_json_item(json, "over");
	cJSON *i_autotile_layout = get_json_item(json, "autotile_layout");
	cJSON *i_autotile_class  = get_json_item(json, "autotile_class");

	if(!pic_from_json(i_pic, &out->pic)) {
		return 0;
	}

	const char *s_name = cJSON_GetStringValue(i_name);
	out->name    = std::string(s_name ? s_name : "");

	out->obj     = cJSON_IsTrue(i_obj);
	out->walls   = cJSON_IsTrue(i_density) ? 255 : 0;
	if (i_walls) out->walls |= i_walls->valueint;
	out->over    = cJSON_IsTrue(i_over);

	const char *s_type = cJSON_GetStringValue(i_type);
	if(s_type)
		out->type = !strcmp(s_type, "sign") ? MAP_TILE_SIGN : MAP_TILE_NONE;

	// Autotile information
	if(i_autotile_layout)
		out->autotile_layout = i_autotile_layout->valueint;
	const char *s_autotile_class = cJSON_GetStringValue(i_autotile_class);
	if(s_autotile_class)
		out->autotile_class = town_crc32(s_autotile_class, strlen(s_autotile_class));

	// Optional message field, for signs
	const char *s_message = cJSON_GetStringValue(i_message);
	if(s_message)
		out->message = std::string(s_message);

	return 1;
}

std::string Entity::apply_json(cJSON *json) {
	cJSON *i_name         = get_json_item(json, "name");
	cJSON *i_pic          = get_json_item(json, "pic");
	cJSON *i_x            = get_json_item(json, "x");
	cJSON *i_y            = get_json_item(json, "y");
	cJSON *i_dir          = get_json_item(json, "dir");
	cJSON *i_offset       = get_json_item(json, "offset");
	cJSON *i_id           = get_json_item(json, "id");
	cJSON *i_passengers   = get_json_item(json, "passengers");
	cJSON *i_vehicle      = get_json_item(json, "vehicle");
	cJSON *i_is_following = get_json_item(json, "is_following");
	cJSON *i_type         = get_json_item(json, "type");
	cJSON *i_in_user_list = get_json_item(json, "in_user_list");
	cJSON *i_typing       = get_json_item(json, "typing");

	if(cJSON_IsArray(i_pic)) pic_from_json(i_pic, &this->pic);
	if(cJSON_IsString(i_name)) this->name = json_as_string(i_name);
	if(cJSON_IsNumber(i_x))    this->x = i_x->valueint;
	if(cJSON_IsNumber(i_y))    this->y = i_y->valueint;
	if(cJSON_IsNumber(i_dir))  this->update_direction(i_dir->valueint);
	if(cJSON_IsArray(i_passengers)) {
		this->passengers.clear();
		cJSON *passenger;
		cJSON_ArrayForEach(passenger, i_passengers) {
			this->passengers.insert(json_as_string(passenger));
		}
	}
	if(cJSON_IsString(i_vehicle)) this->vehicle_id = json_as_string(i_vehicle);
	if(i_is_following) this->is_following = cJSON_IsTrue(i_is_following);
	if(i_type);
	if(i_in_user_list) this->in_user_list = cJSON_IsTrue(i_in_user_list);
	if(i_typing)       this->is_typing = cJSON_IsTrue(i_typing);
	if(i_offset) {
		int offset_x, offset_y;
		if(unpack_json_int_array(i_offset, 2, &offset_x, &offset_y)) {
			this->offset_x = offset_x;
			this->offset_y = offset_y;
		} else {
			this->offset_x = 0;
			this->offset_y = 0;
		}
	}
	if(i_id) return json_as_string(i_id);
	return "";
}

MapTileReference::MapTileReference(cJSON *json, TilemapTownClient *client) {
	if(cJSON_IsString(json)) {
		this->tile = std::string(json->valuestring);
	} else if(cJSON_IsObject(json)) {
		MapTileInfo tile_info = MapTileInfo();
		if(map_tile_from_json(json, &tile_info)) {
			this->tile = client->get_shared_pointer_to_tile(&tile_info);
		}
	}
}

void TilemapTownClient::websocket_message(const char *text, size_t length) {
	if(length < 3)
		return;
	cJSON *json = NULL;

	if(length > 4) {
		// Batch messages need special parsing
		if(text[0] == 'B' && text[1] == 'A' && text[2] == 'T' && text[3] == ' ') {
			size_t base = 4, scan = 4;
			while(scan < length) {
				if(text[scan] == '\n') {
					this->websocket_message(text+base, scan-base);
					base = scan+1;
				}
				scan++;
			}
			this->websocket_message(text+base, scan-base);
			return;
		} else {
			json = cJSON_ParseWithLength(text+4, length-4);
		}
	}
	// printf("Received %c%c%c\n", text[0], text[1], text[2]);

	switch(protocol_command_as_int(text[0], text[1], text[2])) {
		case protocol_command_as_int('P', 'I', 'N'):
			this->websocket_write("PIN");
			break;

		case protocol_command_as_int('M', 'O', 'V'):
		{
			cJSON *i_to   = get_json_item(json, "to");
			cJSON *i_from = get_json_item(json, "from");
			cJSON *i_dir  = get_json_item(json, "dir");
			cJSON *i_id   = get_json_item(json, "id");
			cJSON *i_offset = get_json_item(json, "offset");
			if(!cJSON_IsString(i_id) && !cJSON_IsNumber(i_id))
				break;
			std::string str_id = json_as_string(i_id);
			if(str_id == this->your_id && i_from)
				break;
			// Find this entity
			auto it = this->who.find(str_id);
			if(it != this->who.end()) {
				int to_x, to_y, offset_x, offset_y;
				Entity *entity = &(*it).second;

				if(unpack_json_int_array(i_to, 2, &to_x, &to_y)) {
					entity->x = to_x;
					entity->y = to_y;
					if(entity->vehicle_id.empty() || entity->is_following) {
						entity->walk_timer = 30+1; // 30*(16.6666ms/1000) = 0.5
					}
				}

				if(i_offset) {
					if(unpack_json_int_array(i_offset, 2, &offset_x, &offset_y)) {
						entity->offset_x = offset_x;
						entity->offset_y = offset_y;
					} else {
						entity->offset_x = 0;
						entity->offset_y = 0;
					}
				}

				if(cJSON_IsNumber(i_dir)) {
					entity->update_direction(i_dir->valueint);
				}
			}
			break;
		}

		case protocol_command_as_int('M', 'A', 'I'):
		{
// <-- MAI {"name": map_name, "id": map_id, "owner": whoever, "admins": list, "default": default_turf, "size": [width, height], "public": true/false, "private": true/false, "build_enabled": true/false, "full_sandbox": true/false, "you_allow": list, "you_deny": list
			this->json_tileset.clear();
			this->map_received = false;

			//cJSON *i_name          = get_json_item(json, "name");
			cJSON *i_id            = get_json_item(json, "id");
			//cJSON *i_owner         = get_json_item(json, "owner");
			//cJSON *i_default       = get_json_item(json, "default");

			//cJSON *i_public        = get_json_item(json, "public");
			//cJSON *i_private       = get_json_item(json, "private");
			//cJSON *i_build_enabled = get_json_item(json, "build_enabled");
			//cJSON *i_full_sandbox  = get_json_item(json, "full_sandbox");
			//cJSON *i_you_allow     = get_json_item(json, "you_allow");
			//cJSON *i_you_deny      = get_json_item(json, "you_deny");

			cJSON *i_size          = get_json_item(json, "size");
			int width, height;
			if(unpack_json_int_array(i_size, 2, &width, &height)) {
				this->town_map.init_map(width, height);
			}
			if(cJSON_IsNumber(i_id)) {
				this->town_map.id = i_id->valueint;
			} else {
				this->town_map.id = 0;
			}
			break;
		}
		case protocol_command_as_int('M', 'A', 'P'):
		{
			this->map_received = true;

			cJSON *i_pos     = get_json_item(json, "pos");
			cJSON *i_default = get_json_item(json, "default");
			cJSON *i_turf    = get_json_item(json, "turf");
			cJSON *i_obj     = get_json_item(json, "obj");
			if(!i_pos || !i_default || !i_turf || !i_obj)
				break;

			MapTileReference default_tile = MapTileReference(i_default, this);

			// Write default turf
			int x1, y1, x2, y2;
			if(unpack_json_int_array(i_pos, 4, &x1, &y1, &x2, &y2)) {
				if(x1 > x2 || y1 > y2)
					break;
				for(int x=x1; x<=x2; x++) {
					for(int y=y1; y<=y2; y++) {
						int index = y * this->town_map.width + x;
						this->town_map.cells[index] = MapCell(default_tile);
					}
				}
			}

			cJSON *element;

			cJSON_ArrayForEach(element, i_turf) {
				if(cJSON_GetArraySize(element) != 3)
					continue;
				cJSON *i_x    = cJSON_GetArrayItem(element, 0);
				cJSON *i_y    = cJSON_GetArrayItem(element, 1);
				cJSON *i_tile = cJSON_GetArrayItem(element, 2);
				if(cJSON_IsNumber(i_x) && cJSON_IsNumber(i_y)) {
					int index = i_y->valueint * this->town_map.width + i_x->valueint;
					this->town_map.cells[index] = MapCell(MapTileReference(i_tile, this));
				}
			}

			cJSON_ArrayForEach(element, i_obj) {
				if(cJSON_GetArraySize(element) != 3)
					continue;
				cJSON *i_x    = cJSON_GetArrayItem(element, 0);
				cJSON *i_y    = cJSON_GetArrayItem(element, 1);
				cJSON *i_tile = cJSON_GetArrayItem(element, 2);
				if(cJSON_IsNumber(i_x) && cJSON_IsNumber(i_y)) {
					int index = i_y->valueint * this->town_map.width + i_x->valueint;

					std::vector<struct MapTileReference> *objs = &this->town_map.cells[index].objs;
					objs->clear();

					cJSON *object;
					cJSON_ArrayForEach(object, i_tile) {
						objs->push_back(MapTileReference(object, this));
					}
				}
			}
			
			break;
		}
		case protocol_command_as_int('B', 'L', 'K'):
		{
			cJSON *i_copy = get_json_item(json, "copy");
			if(i_copy) {
				cJSON *item;
				cJSON_ArrayForEach(item, i_copy) {
					if(!cJSON_IsObject(item))
						continue;
					cJSON *i_copy_turf = get_json_item(item, "turf");
					cJSON *i_copy_obj  = get_json_item(item, "obj");
					cJSON *i_copy_src  = get_json_item(item, "src");
					cJSON *i_copy_dst  = get_json_item(item, "dst");
					if(!i_copy_turf || !i_copy_obj || !i_copy_src || !i_copy_dst)
						continue;
					bool b_copy_turf = cJSON_IsTrue(i_copy_turf);
					bool b_copy_obj  = cJSON_IsTrue(i_copy_obj);

					int copy_from_x, copy_from_y, copy_from_w, copy_from_h, copy_to_x, copy_to_y;
					if(!unpack_json_int_array(i_copy_src, 4, &copy_from_x, &copy_from_y, &copy_from_w, &copy_from_h)) {
						if(!unpack_json_int_array(i_copy_src, 2, &copy_from_x, &copy_from_y)) {
							break;
						}
						copy_from_w = 1;
						copy_from_h = 1;
					}
					if(!unpack_json_int_array(i_copy_dst, 2, &copy_to_x, &copy_to_y))
						break;
					std::vector<MapCell> copy_buffer;

					// Make a copy of the area
					for(int rect_y = 0; rect_y < copy_from_h; rect_y++) {
						for(int rect_x = 0; rect_x < copy_from_w; rect_x++) {
							int map_x = copy_from_x + rect_x;
							int map_y = copy_from_y + rect_y;
							if(map_x < 0 || map_y < 0 || map_x >= this->town_map.width || map_y >= this->town_map.height)
								continue;
							int map_index = map_y * this->town_map.width + map_x;
							copy_buffer.push_back(this->town_map.cells[map_index]);
						}
					}

					if(copy_buffer.size() != (size_t)(copy_from_w * copy_from_h))
						break;

					// Copy the tiles into the place
					for(int rect_y = 0; rect_y < copy_from_h; rect_y++) {
						for(int rect_x = 0; rect_x < copy_from_w; rect_x++) {
							int map_x = copy_to_x + rect_x;
							int map_y = copy_to_y + rect_y;
							if(map_x < 0 || map_y < 0 || map_x >= this->town_map.width || map_y >= this->town_map.height)
								continue;

							int map_index = map_y * this->town_map.width + map_x;
							int rect_index = rect_y * copy_from_w + rect_x;

							MapCell *cell = &this->town_map.cells[map_index];
							if(b_copy_turf)
								cell->turf = copy_buffer[rect_index].turf;
							if(b_copy_obj)
								cell->objs = copy_buffer[rect_index].objs;
						}
					}

				}
			}

			cJSON *i_turf = get_json_item(json, "turf");
			if(i_turf) {
				cJSON *item;
				cJSON_ArrayForEach(item, i_turf) {
					if(!cJSON_IsArray(item))
						continue;
					int len = cJSON_GetArraySize(item);
					if(len != 3 && len != 5)
						continue;
					cJSON *i_x = cJSON_GetArrayItem(item, 0);
					cJSON *i_y = cJSON_GetArrayItem(item, 1);
					cJSON *i_t = cJSON_GetArrayItem(item, 2);
					cJSON *i_w = (len == 5)?cJSON_GetArrayItem(item, 3) : nullptr;
					cJSON *i_h = (len == 5)?cJSON_GetArrayItem(item, 4) : nullptr;
					int width = i_w ? i_w->valueint : 1;
					int height = i_h ? i_h->valueint : 1;
					if(!cJSON_IsNumber(i_x) || !cJSON_IsNumber(i_y) || (i_w&&!cJSON_IsNumber(i_w)) || (i_h&&!cJSON_IsNumber(i_h)) )
						continue;

					MapTileReference tile = MapTileReference(i_t, this);
					for(int rect_y = 0; rect_y < height; rect_y++) {
						for(int rect_x = 0; rect_x < width; rect_x++) {
							int map_x = i_x->valueint + rect_x;
							int map_y = i_y->valueint + rect_y;
							if(map_x < 0 || map_y < 0 || map_x >= this->town_map.width || map_y >= this->town_map.height)
								continue;
							size_t map_index = map_y * this->town_map.width + map_x;
							this->town_map.cells[map_index].turf = tile;
						}
					}
				}
			}

			cJSON *i_obj = get_json_item(json, "obj");
			if(i_obj) {
				cJSON *item;
				cJSON_ArrayForEach(item, i_obj) {
					if(!cJSON_IsArray(item))
						continue;
					int len = cJSON_GetArraySize(item);
					if(len != 3 && len != 5)
						continue;
					cJSON *i_x = cJSON_GetArrayItem(item, 0);
					cJSON *i_y = cJSON_GetArrayItem(item, 1);
					cJSON *i_t = cJSON_GetArrayItem(item, 2);
					cJSON *i_w = (len == 5)?cJSON_GetArrayItem(item, 3) : nullptr;
					cJSON *i_h = (len == 5)?cJSON_GetArrayItem(item, 4) : nullptr;
					int width = i_w ? i_w->valueint : 1;
					int height = i_h ? i_h->valueint : 1;
					if(!cJSON_IsNumber(i_x) || !cJSON_IsNumber(i_y) || (i_w&&!cJSON_IsNumber(i_w)) || (i_h&&!cJSON_IsNumber(i_h)) || !cJSON_IsArray(i_t))
						continue;

					// Get object list
					std::vector<struct MapTileReference> objs;
					cJSON *object;
					cJSON_ArrayForEach(object, i_t) {
						objs.push_back(MapTileReference(object, this));
					}
					for(int rect_y = 0; rect_y < height; rect_y++) {
						for(int rect_x = 0; rect_x < width; rect_x++) {
							int map_x = i_x->valueint + rect_x;
							int map_y = i_y->valueint + rect_y;
							if(map_x < 0 || map_y < 0 || map_x >= this->town_map.width || map_y >= this->town_map.height)
								continue;
							size_t map_index = map_y * this->town_map.width + map_x;
							this->town_map.cells[map_index].objs = objs;
						}
					}
				}
			}

			break;
		}

		case protocol_command_as_int('W', 'H', 'O'):
		{
			cJSON *i_you = get_json_item(json, "you");
			if(i_you) {
				this->your_id = json_as_string(i_you);
			}

			cJSON *i_list = get_json_item(json, "list");
			if(cJSON_IsObject(i_list)) {
				this->who.clear();

				cJSON *i_user;
				cJSON_ArrayForEach(i_user, i_list) {
					if(!cJSON_IsObject(i_user))
						break;
					Entity entity = Entity();
					std::string id = entity.apply_json(i_user);
					if(!id.empty())
						this->who[id] = entity;
				}
			}

			cJSON *i_add = get_json_item(json, "add");
			if(cJSON_IsObject(i_add)) {
				Entity entity = Entity();
				std::string id = entity.apply_json(i_add);
				if(!id.empty())
					this->who[id] = entity;
			}

			cJSON *i_update = get_json_item(json, "update");
			if(cJSON_IsObject(i_update)) {
				cJSON *i_id = get_json_item(i_add, "id");
				if(!i_id) break;

				auto it = this->who.find(json_as_string(i_id));
				if(it != this->who.end()) {
					(*it).second.apply_json(i_update);
				}
			}

			cJSON *i_remove = get_json_item(json, "remove");
			if(cJSON_IsString(i_remove) || cJSON_IsNumber(i_remove)) {
				this->who.erase(json_as_string(i_remove));
			}

			cJSON *i_new_id = get_json_item(json, "new_id");
			if(cJSON_IsObject(i_new_id)) {
				cJSON *i_id  = get_json_item(i_new_id, "id");
				cJSON *i_id2 = get_json_item(i_new_id, "new_id");
				if(!i_id || !i_id2)
					break;
				std::string str_id     = json_as_string(i_id);
				std::string str_new_id = json_as_string(i_id2);

				if(str_id == this->your_id) {
					this->your_id = str_new_id;
				}

				auto it = this->who.find(str_id);
				if(it != this->who.end()) {
					this->who[str_new_id] = (*it).second;
					this->who.erase(str_id);
				}
			}
			break;
		}

		case protocol_command_as_int('B', 'A', 'G'):
		{
//<-- BAG {"info": {"id": id, ...}}
//<-- BAG {"update": {item info}}
//<-- BAG {"list": [{item info}], "container": id, "clear": false}
//<-- BAG {"new_id": {"id": old_id, "new_id": id}}
//<-- BAG {"remove": {"id": id}}
			cJSON *i_info = get_json_item(json, "info");
			if(i_info) {

			}

			cJSON *i_update = get_json_item(json, "update");
			if(i_update) {

			}

			cJSON *i_list = get_json_item(json, "list");
			if(i_list) {

			}

			cJSON *i_new_id = get_json_item(json, "new_id");
			if(i_new_id) {

			}

			cJSON *i_remove = get_json_item(json, "remove");
			if(i_remove) {

			}
			break;
		}

		case protocol_command_as_int('R', 'S', 'C'):
		{
			cJSON *i_images   = get_json_item(json, "images");
			if(i_images) {
				cJSON *element;
				cJSON_ArrayForEach(element, i_images) {
					this->url_for_tile_sheet[element->string] = cJSON_GetStringValue(element);
				}
			}
			cJSON *i_tilesets = get_json_item(json, "tilesets");
			if(i_tilesets) {
				cJSON *tileset;
				cJSON_ArrayForEach(tileset, i_tilesets) {
					std::string prefix;
					if(*tileset->string) {
						prefix = std::string(tileset->string) + ":";
					} else {
						prefix = std::string("");
					}

					cJSON *tile_in_tileset;
					cJSON_ArrayForEach(tile_in_tileset, tileset) {
						std::string key = std::string(tile_in_tileset->string);

						struct MapTileInfo tile = MapTileInfo();
						map_tile_from_json(tile_in_tileset, &tile);
						tile.key = key;

						this->tileset[prefix+key] = std::make_shared<MapTileInfo>(tile);
					}
				}
			}
			break;
		}

		case protocol_command_as_int('I', 'M', 'G'):
		{
			cJSON *i_id  = get_json_item(json, "id");
			cJSON *i_update = get_json_item(json, "update");

			const char *i_url = get_json_string(json, "url");
			if(i_id) {
				std::string id = json_as_string(i_id);
				this->url_for_tile_sheet[id] = i_url;
				this->requested_tile_sheets.erase(id);

				// Update images that are on preexisting tiles
				if(cJSON_IsTrue(i_update)) {
					for (auto value : this->tileset) {
						if((*value.second).pic.key == id) {
							(*value.second).pic.ready_to_draw = false;
						}
					}
					for (auto value : this->json_tileset) {
						std::shared_ptr<MapTileInfo> tile = value.second.lock();
						if((*tile).pic.key == id) {
							(*tile).pic.ready_to_draw = false;
						}
					}
				}
			}
			break;
		}

		case protocol_command_as_int('T', 'S', 'D'):
		{
// <-- TSD {"id": number, "data": "[id, info, id, info, id, info, ...]"}
			//cJSON *i_id   = get_json_item(json, "id");
			//cJSON *i_data = get_json_item(json, "data");
			//if(i_id) {

			//}
			break;
		}

		case protocol_command_as_int('E', 'M', 'L'):
		{
// <-- EML {"receive": {"id": id, "subject": subject, "contents": contents, "to": [username, ...], "from": username, "flags": flags}}
// <-- EML {"list": [{"id": id, "subject": subject, "contents": contents, "to": [username, ...], "from": username, "flags": flags}]}
// <-- EML {"sent": {"subject", subject}}''
			cJSON *i_receive = get_json_item(json, "receive");
			if(i_receive) {

			}

			cJSON *i_list = get_json_item(json, "list");
			if(i_list) {

			}

			cJSON *i_sent = get_json_item(json, "sent");
			if(i_sent) {

			}
			break;
		}

		case protocol_command_as_int('P', 'R', 'I'):
		{
			const char *i_text = get_json_string(json, "text");
			const char *i_name = get_json_string(json, "name");
			cJSON *i_username = get_json_item(json, "username");
			cJSON *i_receive  = get_json_item(json, "receive");

			if(!i_text || !i_name || !i_username || !i_receive)
				break;
			std::string username = json_as_string(i_username);
			bool b_receive = cJSON_IsTrue(i_receive);
			printf("\x1b[32m%s[%s(%s)] %s\x1b[0m\n", b_receive?"<--":"-->", i_name, username.c_str(), i_text);
			break;
		}

		case protocol_command_as_int('E', 'R', 'R'):
		{
			const char *i_text = get_json_string(json, "text");
			if(i_text)
				printf("\x1b[31m%s\x1b[0m\n", i_text);
			break;
		}

		case protocol_command_as_int('C', 'M', 'D'):
		case protocol_command_as_int('M', 'S', 'G'):
		{
// <-- MSG {"text": "[text]", "name": speaker, "class": classname, "username": username}
// <-- MSG {"text": "[text]", "name": speaker, "class": classname, "buttons": ["name 1", "command 1", "name 2", "command 2"]}
			const char *i_text  = get_json_string(json, "text");
			const char *i_name  = get_json_string(json, "name");
			//const char *i_class = get_json_string(json, "class");
			//cJSON *i_buttons    = get_json_item(json, "buttons");
			if(i_text) {
				if(i_name) {
					if(i_text[0] == '/' && i_text[1] == 'm' && i_text[2] == 'e' && i_text[3] == ' ') {
						printf("* %s %s\n", i_name, i_text+4);
					} else if(i_text[0] == '/' && i_text[1] == 'o' && i_text[2] == 'o' && i_text[3] == 'c' && i_text[4] == ' ') {
						printf("\x1b[34m[OOC] %s: %s\x1b[0m\n", i_name, i_text+5);
					} else if(i_text[0] == '/' && i_text[1] == 's' && i_text[2] == 'p' && i_text[3] == 'o' && i_text[4] == 'o' && i_text[5] == 'f' && i_text[6] == ' ') {
						printf("* %s \x1b[34m(by %s)\x1b[0m\n", i_text+7, i_name);
					} else {
						printf("<%s> %s\n", i_name, i_text);
					}
				} else {
					printf("\x1b[35m%s\x1b[0m\n", i_text);
				}
			}
			break;
		}
	}

	if(json)
		cJSON_Delete(json);
}

void TilemapTownClient::websocket_write(std::string command, cJSON *json) {
	if(json == NULL) {
		this->websocket_write(command);
		return;
	}
	char *as_string = cJSON_PrintUnformatted(json);
	if(!as_string)
		return;
	this->websocket_write(command + " " + std::string(as_string));
	free(as_string);
}

void TilemapTownClient::request_image_asset(std::string key) {
	if(this->requested_tile_sheets.find(key) != this->requested_tile_sheets.end()) {
		return;
	}
	this->requested_tile_sheets.insert(key);

	cJSON *json = cJSON_CreateObject();
	cJSON_AddStringToObject(json, "id", key.c_str());
	this->websocket_write("IMG", json);
	cJSON_Delete(json);
}
