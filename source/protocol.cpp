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

using namespace std;

#define protocol_command_as_int(a,b,c) (a) | (b<<8) | (c<<16)

#define get_json_item cJSON_GetObjectItemCaseSensitive

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
	cJSON *i_type    = get_json_item(json, "type");
	cJSON *i_message = get_json_item(json, "message");
	cJSON *i_over    = get_json_item(json, "over");

	if(!pic_from_json(i_pic, &out->pic)) {
		return 0;
	}

	const char *s_name = cJSON_GetStringValue(i_name);
	out->name    = std::string(s_name ? s_name : "");

	out->obj     = cJSON_IsTrue(i_obj);
	out->density = cJSON_IsTrue(i_density);
	out->over    = cJSON_IsTrue(i_over);

	const char *s_type = cJSON_GetStringValue(i_type);
	if(s_type)
		out->type    = !strcmp(s_type, "sign") ? MAP_TILE_SIGN : MAP_TILE_NONE;

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

MapTileReference::MapTileReference(cJSON *json) {
	if(cJSON_IsString(json)) {
		this->tile = std::string(json->valuestring);
	} else if(cJSON_IsObject(json)) {
		MapTileInfo tile_info = MapTileInfo();
		if(map_tile_from_json(json, &tile_info)) {
			this->tile = make_shared<MapTileInfo>(tile_info);
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
			this->map_received = false;

			cJSON *i_name          = get_json_item(json, "name");
			cJSON *i_id            = get_json_item(json, "id");
			cJSON *i_owner         = get_json_item(json, "owner");
			cJSON *i_admin         = get_json_item(json, "admins");
			cJSON *i_default       = get_json_item(json, "default");

			cJSON *i_public        = get_json_item(json, "public");
			cJSON *i_private       = get_json_item(json, "private");
			cJSON *i_build_enabled = get_json_item(json, "build_enabled");
			cJSON *i_full_sandbox  = get_json_item(json, "full_sandbox");
			cJSON *i_you_allow     = get_json_item(json, "you_allow");
			cJSON *i_you_deny      = get_json_item(json, "you_deny");

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

			MapTileReference default_tile = MapTileReference(i_default);

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
					this->town_map.cells[index] = MapCell(i_tile);
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
						objs->push_back(object);
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

					MapTileReference tile = MapTileReference(i_t);
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
						objs.push_back(object);
					}
					for(int rect_y = 0; rect_y < i_h->valueint; rect_y++) {
						for(int rect_x = 0; rect_x < i_w->valueint; rect_x++) {
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

						this->tileset[prefix+key] = make_shared<MapTileInfo>(tile);
					}
				}
			}
			break;
		}

		case protocol_command_as_int('I', 'M', 'G'):
		{
			cJSON *i_id  = get_json_item(json, "id");
			const char *i_url = get_json_string(json, "url");
			if(i_id) {
				std::string id = json_as_string(i_id);
				this->url_for_tile_sheet[id] = i_url;
				this->requested_tile_sheets.erase(id);
			}
			break;
		}

		case protocol_command_as_int('T', 'S', 'D'):
		{
// <-- TSD {"id": number, "data": "[id, info, id, info, id, info, ...]"}
			cJSON *i_id   = get_json_item(json, "id");
			cJSON *i_data = get_json_item(json, "data");
			if(i_id) {

			}
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
			const char *i_class = get_json_string(json, "class");
			cJSON *i_buttons    = get_json_item(json, "buttons");
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
