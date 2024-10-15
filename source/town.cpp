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

using namespace std;

void TilemapTownClient::log_message(std::string text, std::string style) {
	puts(text.c_str());
}

void TownMap::init_map(int width, int height) {
	this->width = width;
	this->height = height;
	this->cells.clear();
	this->cells.resize(width * height);
}

// .-------------------------------------------------------
// | Map tile functions
// '-------------------------------------------------------

std::shared_ptr<MapTileInfo> TilemapTownClient::get_shared_pointer_to_tile(MapTileInfo *tile) {
	std::size_t hash = tile->hash();
	std::shared_ptr<MapTileInfo> ptr;

	// Look for it in the JSON tileset
	auto it = this->json_tileset.find(hash);
	if(it != this->json_tileset.end()) {
		ptr = (*it).second.lock();
		if(ptr)
			return ptr;
	}

	// Not found, so cache it for later
	ptr = make_shared<MapTileInfo>(*tile);
	this->json_tileset[hash] = ptr;
	return ptr;
}

MapTileInfo* MapTileReference::get(TilemapTownClient *client) {
	// If there's a pointer to the tile already, just return that tile
	if(const auto ptr = std::get_if<std::shared_ptr<MapTileInfo>>(&this->tile)) {
		return (*ptr).get();
	}

	// Is the tile available yet?
	if(const auto str = std::get_if<std::string>(&this->tile)) {
		// Is it in the client's tileset?
		auto it = client->tileset.find(*str);
		if(it != client->tileset.end()) {
			// Keep the reference to avoid needing to look it up next time
			this->tile = (*it).second;
			return (*it).second.get();
		}
	}

	return nullptr;
}

MapTileReference::MapTileReference(std::string str, TilemapTownClient *client) {
	// Is it in the client's tileset?
	auto it = client->tileset.find(str);
	if(it != client->tileset.end()) {
		this->tile = (*it).second;
		return;
	}
	// Otherwise, just store the string
	this->tile = str;
}

MapTileReference::MapTileReference(std::string str) {
	this->tile = str;
}

MapTileReference::MapTileReference(std::shared_ptr<MapTileInfo> tile) {
	this->tile = tile;
}

MapTileReference::MapTileReference(MapTileInfo* tile, TilemapTownClient *client) {
	this->tile = client->get_shared_pointer_to_tile(tile);
}

MapTileReference::MapTileReference() {
	this->tile = std::monostate();
}

std::size_t hash_combine(std::size_t a, std::size_t b) {
    unsigned prime = 0x01000193;
    a *= prime;
    a ^= b;
	return a;
}

std::size_t MapTileInfo::hash() {
	std::hash<uint32_t> uint32_hash;
	std::hash<uint8_t> uint8_hash;
	std::hash<int8_t> int8_hash;
	std::hash<std::string> str_hash;
	std::hash<bool> bool_hash;

	std::size_t hash = str_hash(this->key);
	hash = hash_combine(hash, str_hash(this->name));
	hash = hash_combine(hash, str_hash(this->message));
	hash = hash_combine(hash, uint32_hash(this->autotile_class));
	hash = hash_combine(hash, this->pic.hash());
	hash = hash_combine(hash, bool_hash(this->over));
	hash = hash_combine(hash, uint8_hash(this->autotile_layout));
	hash = hash_combine(hash, uint8_hash(this->walls));
	hash = hash_combine(hash, bool_hash(this->obj));
	hash = hash_combine(hash, uint8_hash(this->type));
	hash = hash_combine(hash, uint8_hash(this->animation_frames));
	hash = hash_combine(hash, uint8_hash(this->animation_speed));
	hash = hash_combine(hash, uint8_hash(this->animation_mode));
	hash = hash_combine(hash, int8_hash(this->animation_offset));
	return hash;
}

std::size_t Pic::hash() {
	std::hash<int> int_hash;
	std::hash<std::string> str_hash;

	std::size_t hash = str_hash(this->key);
	hash = hash_combine(hash, int_hash(this->x));
	hash = hash_combine(hash, int_hash(this->y));
	return hash;
}

// --------------------------------------------------------

MapCell::MapCell() {
	this->turf = MapTileReference();
}

MapCell::MapCell(struct MapTileReference turf) {
	this->turf = turf;
}

// .-------------------------------------------------------
// | Game logic/movement related
// '-------------------------------------------------------

Entity *TilemapTownClient::your_entity() {
	if(this->your_id.empty())
		return nullptr;
	auto it = this->who.find(this->your_id);
	if(it == this->who.end())
		return nullptr;
	return &(*it).second;
}

void TilemapTownClient::update_camera(float offset_x, float offset_y) {
	Entity *you = this->your_entity();
	if(!you)
		return;

	float target_x = you->x*16 - ((float)VIEW_WIDTH_TILES*8) + offset_x;
	float target_y = you->y*16 - ((float)VIEW_HEIGHT_TILES*8) + offset_y;

	float difference_x = target_x - this->camera_x;
	float difference_y = target_y - this->camera_y;

	if(fabs(difference_x) > 0.5)
		this->camera_x += difference_x / 12;
	if(fabs(difference_y) > 0.5)
		this->camera_y += difference_y / 12;
}

void TilemapTownClient::turn_player(int direction) {
	Entity *you = this->your_entity();
	if(!you)
		return;
	you->update_direction(direction);

	cJSON *json = cJSON_CreateObject();
	cJSON_AddNumberToObject(json, "dir", (double)direction);
	this->websocket_write("MOV", json);
	cJSON_Delete(json);
}

void TilemapTownClient::move_player(int offset_x, int offset_y) {
	Entity *you = this->your_entity();
	if(!you)
		return;
	int original_x = you->x;
	int original_y = you->y;

	// Figure out the direction from the offset
	int new_direction = 0;
	if(offset_x > 0 && offset_y == 0) {
		new_direction = 0;
	} else if(offset_x > 0 && offset_y > 0) {
		new_direction = 1;
	} else if(offset_x == 0 && offset_y > 0) {
		new_direction = 2;
	} else if(offset_x < 0 && offset_y > 0) {
		new_direction = 3;
	} else if(offset_x < 0 && offset_y == 0) {
		new_direction = 4;
	} else if(offset_x < 0 && offset_y < 0) {
		new_direction = 5;
	} else if(offset_x == 0 && offset_y < 0) {
		new_direction = 6;
	} else if(offset_x > 0 && offset_y < 0) {
		new_direction = 7;
	}
	you->update_direction(new_direction);

	bool bumped = false;
	int bumped_x, bumped_y;

	int new_x = original_x + offset_x;
	if(new_x < 0)
		new_x = 0;    
	if(new_x >= this->town_map.width)
		new_x = this->town_map.width-1;

	int new_y = original_y + offset_y;
	if(new_y < 0)
		new_y = 0;
	if(new_y >= this->town_map.height)
		new_y = this->town_map.height-1;
	if((new_x != original_x + offset_x) || (new_y != original_y + offset_y)) {
		bumped = true;
		bumped_x = original_x + offset_x;
		bumped_y = original_y + offset_y;
	}

	you->x = new_x;
	you->y = new_y;

	if(!this->walk_through_walls) {
		////////////////////////////
		// Check old tile for walls
		////////////////////////////
		MapCell *cell = &this->town_map.cells[original_y * this->town_map.width + original_x];

		MapTileInfo *turf = cell->turf.get(this);
		if(turf && (turf->walls & (1 << new_direction))) {
			// Go back
			bumped = true;
			bumped_x = original_x;
			bumped_y = original_y;
			you->x = original_x;
			you->y = original_y;
		}

		for(auto & obj_reference : cell->objs) {
			MapTileInfo *obj = obj_reference.get(this);
			if(!obj)
				continue;
			if(obj->walls & (1 << new_direction)) {
				if(!bumped) {
					bumped = true;
					bumped_x = original_x;
					bumped_y = original_y;
				}
				// Go back
				you->x = original_x;
				you->y = original_y;
			}
		}

		////////////////////////////
		// Check new tile for walls
		////////////////////////////
		if (!bumped) {
			int dense_wall_bit = 1 << ((new_direction + 4) & 7); // For the new cell, the direction to check is rotated 180 degrees
			cell = &this->town_map.cells[new_y * this->town_map.width + new_x];

			turf = cell->turf.get(this);
			if(turf && turf->type == MAP_TILE_SIGN) {
				printf("\x1b[35m%s says: %s\x1b[0m\n", (turf->name=="sign" || turf->name.empty()) ? "The sign" : turf->name.c_str(), turf->message.c_str());
			}
			if(turf && (turf->walls & dense_wall_bit)) {
				// Go back
				bumped = true;
				bumped_x = you->x;
				bumped_y = you->y;
				you->x = original_x;
				you->y = original_y;
			}

			for(auto & obj_reference : cell->objs) {
				MapTileInfo *obj = obj_reference.get(this);
				if(!obj)
					continue;
				if(obj->type == MAP_TILE_SIGN) {
					printf("\x1b[35m%s says: %s\x1b[0m\n", (obj->name=="sign" || obj->name.empty()) ? "The sign" : obj->name.c_str(), obj->message.c_str());
				}
				if(obj->walls & dense_wall_bit) {
					if(!bumped) {
						bumped = true;
						bumped_x = you->x;
						bumped_y = you->y;
					}
					// Go back
					you->x = original_x;
					you->y = original_y;
				}
			}
		}
	}

	//////////////////////////////////////
	// Tell the server about the movement
	//////////////////////////////////////
	cJSON *json = cJSON_CreateObject();
	if(!bumped) {
		int from_array[2] = {original_x, original_y};
		cJSON *json_from = cJSON_CreateIntArray(from_array, 2);
		cJSON_AddItemToObject(json, "from", json_from);

		int to_array[2] = {you->x, you->y};
		cJSON *json_to = cJSON_CreateIntArray(to_array, 2);
		cJSON_AddItemToObject(json, "to", json_to);
	} else {
		int bump_array[2] = {bumped_x, bumped_y};
		cJSON *json_bump = cJSON_CreateIntArray(bump_array, 2);
		cJSON_AddItemToObject(json, "bump", json_bump);

		// Tell server what map the bump is intended for
		if(this->town_map.id != 0) {
			cJSON_AddNumberToObject(json, "if_map", this->town_map.id);
		}
	}
	cJSON_AddNumberToObject(json, "dir", (double)new_direction);

	this->websocket_write("MOV", json);
	cJSON_Delete(json);

	you->walk_timer = 30+1; // 30*(16.6666ms/1000) = 0.5
}

void Entity::update_direction(int direction) {
	this->direction = direction;

	if((direction & 1) == 0) // Four directions only
		this->direction_4 = direction;
	if(direction == 0 || direction == 4) // Left and right only
		this->direction_lr = direction;
}
