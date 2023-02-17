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

int map_tile_from_json(cJSON *json, MapTileInfo *out);

MapTileReference::MapTileReference(cJSON *json) {
	if(cJSON_IsString(json)) {
		this->tile = std::string(json->valuestring);
	} else if(cJSON_IsObject(json)) {
		MapTileInfo tile_info;
		if(map_tile_from_json(json, &tile_info)) {
			this->tile = make_shared<MapTileInfo>(tile_info);
		}
	}
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
  