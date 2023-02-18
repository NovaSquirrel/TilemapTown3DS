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

MapTileInfo* MapTileReference::get(TilemapTownClient *client) {
	if(const auto ptr = std::get_if<std::shared_ptr<MapTileInfo>>(&this->tile)) {
		return (*ptr).get();
	}

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

MapTileReference::MapTileReference(MapTileInfo tile) {
	this->tile = make_shared<MapTileInfo>(tile);
}

MapTileReference::MapTileReference() {
	this->tile = std::monostate();
}

MapCell::MapCell() {
	this->turf = MapTileReference();
}

MapCell::MapCell(struct MapTileReference turf) {
	this->turf = turf;
}

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
}

void TilemapTownClient::move_player(int offset_x, int offset_y) {
	Entity *you = this->your_entity();
	if(!you)
		return;
	int player_x = you->x;
	int player_y = you->y;

	// Figure out the direction
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

	int new_x = player_x + offset_x;
	if(new_x < 0)
		new_x = 0;
	if(new_x >= this->town_map.width)
		new_x = this->town_map.width-1;

	int new_y = player_y + offset_y;
	if(new_y < 0)
		new_y = 0;
	if(new_y >= this->town_map.height)
		new_y = this->town_map.height-1;

	you->x = new_x;
	you->y = new_y;

	MapCell *cell = &this->town_map.cells[new_y * this->town_map.width + new_x];

	MapTileInfo *turf = cell->turf.get(this);
	if(turf && turf->density) {
		// Go back
		you->x = player_x;
		you->y = player_y;
	}

	for(auto & obj_reference : cell->objs) {
		MapTileInfo *obj = obj_reference.get(this);
		if(!obj)
			continue;
		if(obj->density) {
			// Go back
			you->x = player_x;
			you->y = player_y;
		}
	}

	cJSON *json = cJSON_CreateObject();
	int from_array[2] = {player_x, player_y};
	cJSON *json_from = cJSON_CreateIntArray(from_array, 2);
	cJSON_AddItemToObject(json, "from", json_from);

	int to_array[2] = {you->x, you->y};
	cJSON *json_to = cJSON_CreateIntArray(to_array, 2);
	cJSON_AddItemToObject(json, "to", json_to);

	cJSON_AddNumberToObject(json, "dir", (double)new_direction);

	this->websocket_write("MOV", json);
	cJSON_Delete(json);
}

void Entity::update_direction(int direction) {
	this->direction = direction;

	if((direction & 1) == 0) // Four directions only
		this->direction_4 = direction;
	if(direction == 0 || direction == 4) // Left and right only
		this->direction_lr = direction;
}
