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

#include <memory>
#include <vector>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <variant>

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include <curl/curl.h>
#include <wslay/wslay.h>

#include "mbedtls/platform.h"
#include "mbedtls/net_sockets.h"
#include "mbedtls/debug.h"
#include "mbedtls/ssl.h"
#include "mbedtls/entropy.h"
#include "mbedtls/ctr_drbg.h"
#include "mbedtls/error.h"
#include "mbedtls/timing.h"

#define VIEW_WIDTH_TILES 25
#define VIEW_HEIGHT_TILES 15

#ifdef __3DS__
#include <3ds.h>
#include <citro2d.h>
#endif

class TilemapTownClient;

struct http_file {
	uint8_t *memory;
	size_t size;
	time_t last_accessed;
};

struct http_transfer {
	struct http_file file;
	const char *url;

	void (*callback) (const char *url, uint8_t *data, size_t size, TilemapTownClient *client, void *userdata);
	void *userdata;
};

// ------------------------------------
struct MapTileInfo;

struct MapTileReference {
	std::variant<std::monostate, std::string, std::shared_ptr<MapTileInfo>> tile;

	MapTileInfo* get(TilemapTownClient *client);

	MapTileReference();
	MapTileReference(struct cJSON *json, TilemapTownClient *client);
	MapTileReference(std::string str);
	MapTileReference(std::string str, TilemapTownClient *client);
	MapTileReference(MapTileInfo *tile, TilemapTownClient *client);
	MapTileReference(std::shared_ptr<MapTileInfo> tile);
};

struct MapCell {
	struct MapTileReference turf;
	std::vector<struct MapTileReference> objs;

	MapCell();
	MapCell(struct MapTileReference turf);
};

class TownMap {
public:
	int width, height;
	std::vector<MapCell> cells;

	// Metadata
	int id;

	void init_map(int width, int height);
};

struct LoadedTextureInfo {
	int original_width;  // Width of the source image, rather than the texture
	int original_height;
	C3D_Tex* texture;
};

struct Pic {
	std::string key; // URL or integer
	int x;
	int y;

	bool ready_to_draw; // Map tile has the texture loaded in

	#ifdef __3DS__
	Tex3DS_SubTexture subtexture;
	C2D_Image image;               // Texture and subtexture
	LoadedTextureInfo *extra_info; // Can use this to get the original size

	C2D_Image *get(TilemapTownClient *client);
	C3D_Tex *get_texture(TilemapTownClient *client);
	#endif

	std::size_t hash();
};

class Entity {
public:
	std::string name;
	struct Pic pic;
	int x;
	int y;
	bool in_user_list;

	std::string vehicle_id;
	std::unordered_set <std::string> passengers;
	bool is_following;

	bool is_typing;

	// Animation
	int walk_timer;
	int direction;
	int direction_4;
	int direction_lr;
	int offset_x;
	int offset_y;

	std::string apply_json(cJSON *json);
	void update_direction(int direction);
};

enum MapTileType {
	MAP_TILE_NONE,
	MAP_TILE_SIGN,
};

struct MapTileInfo {
	std::string key;  // Key used to look up this MapTileInfo
	std::string name; // Name, for metadata
	std::string message; // For signs
	uint32_t autotile_class;

	// Appearance
	Pic pic;          // [sheet, x, y] format
	bool over;        // Display on top of entities
	uint8_t autotile_layout;

	// Game logic related
	uint8_t walls;
	bool obj;
	enum MapTileType type;

	std::size_t hash();
};

// ------------------------------------

class HttpFileCache {
	std::unordered_map<std::string, struct http_file> cache;
	CURLM *http;
	std::unordered_set<std::string> requested_urls;
	bool http_in_progress;
	size_t total_size;

public:
	TilemapTownClient *client;
	HttpFileCache();
	~HttpFileCache();

	void get(std::string url, void (*callback) (const char *url, uint8_t *data, size_t size, TilemapTownClient *client, void *userdata), void *userdata);
	void run_transfers();
};

class TilemapTownClient {
public:
	// Network
	wslay_event_context_ptr websocket;
	HttpFileCache http;

	// TLS
    mbedtls_net_context server_fd;
    mbedtls_entropy_context entropy;
    mbedtls_ctr_drbg_context ctr_drbg;
    mbedtls_ssl_context ssl;
    mbedtls_ssl_config conf;
    mbedtls_x509_crt cacert;
	bool connected;

	// Game state
	TownMap town_map;
	std::unordered_map<std::string, std::shared_ptr<MapTileInfo>> tileset;
	std::unordered_map<std::size_t, std::weak_ptr<MapTileInfo>> json_tileset; // Custom JSON tiles, referenced by hash
	std::unordered_map<std::string, Entity> who;
	#ifdef __3DS__
	std::unordered_map<std::string, LoadedTextureInfo> texture_for_url;
	#endif

	std::unordered_map<std::string, std::string> url_for_tile_sheet;
	std::unordered_set<std::string> requested_tile_sheets;

	bool map_received;
	bool need_redraw;
	int animation_tick;

	// Player state
	std::string your_id;
	float camera_x;
	float camera_y;

	bool fly;

	void websocket_write(std::string text);
	void websocket_write(std::string command, cJSON *json);
	void websocket_message(const char *text, size_t length);
	int network_connect(std::string host, std::string path, std::string port);
	void network_disconnect();
	void network_update();

	void request_image_asset(std::string key);
	void log_message(std::string text, std::string style);
	void update_camera(float offset_x, float offset_y);
	void draw_map(int camera_x, int camera_y);
	Entity *your_entity();
	void turn_player(int direction);
	void move_player(int offset_x, int offset_y);

	// Utility
	bool is_autotile_match(MapTileInfo *turf, int x, int y);
	unsigned int get_autotile_index_4(MapTileInfo *turf, int x, int y);
	std::shared_ptr<MapTileInfo> get_shared_pointer_to_tile(MapTileInfo *tile);
};
