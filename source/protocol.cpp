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




void TilemapTownClient::websocket_message(const char *text, size_t length) {
	if(length < 3)
		return;
	cJSON *json = NULL;
	if(length > 4)
		json = cJSON_ParseWithLength(text+4, length-4);

	printf("Received %c%c%c\n", text[0], text[1], text[2]);

	switch(protocol_command_as_int(text[0], text[1], text[2])) {
		case protocol_command_as_int('P', 'I', 'N'):
			this->websocket_write("PIN");
			break;

		case protocol_command_as_int('M', 'O', 'V'):
		{
// <-- MOV {"from": [x1,y1], "to": [x2,y2], "dir": 0, "rc": 0}
			cJSON *i_to   = get_json_item(json, "to");
			cJSON *i_from = get_json_item(json, "from");
			cJSON *i_dir  = get_json_item(json, "dir");
			cJSON *i_id   = get_json_item(json, "id");
			break;
		}

		case protocol_command_as_int('M', 'A', 'I'):
		{
			cJSON *i_pos     = get_json_item(json, "pos");
// <-- MAI {"name": map_name, "id": map_id, "owner": whoever, "admins": list, "default": default_turf, "size": [width, height], "public": true/false, "private": true/false, "build_enabled": true/false, "full_sandbox": true/false, "you_allow": list, "you_deny": list
			cJSON *i_name          = get_json_item(json, "name");
			cJSON *i_id            = get_json_item(json, "id");
			cJSON *i_owner         = get_json_item(json, "owner");
			cJSON *i_admin         = get_json_item(json, "admins");
			cJSON *i_default       = get_json_item(json, "default");
			cJSON *i_size          = get_json_item(json, "size");
			cJSON *i_public        = get_json_item(json, "public");
			cJSON *i_private       = get_json_item(json, "private");
			cJSON *i_build_enabled = get_json_item(json, "build_enabled");
			cJSON *i_full_sandbox  = get_json_item(json, "full_sandbox");
			cJSON *i_you_allow     = get_json_item(json, "you_allow");
			cJSON *i_you_deny      = get_json_item(json, "you_deny");
			break;
		}
		case protocol_command_as_int('M', 'A', 'P'):
		{
// <-- MAP {"pos":[x1, y1, x2, y2], "default": default_turf, "turf": [turfs], "obj": [objs]}
			cJSON *i_pos     = get_json_item(json, "pos");
			cJSON *i_default = get_json_item(json, "default");
			cJSON *i_turf    = get_json_item(json, "turf");
			cJSON *i_obj     = get_json_item(json, "obj");
			if(!i_pos || !i_default || !i_turf || !i_obj)
				break;
			int x1, y1, x2, y2;
			if(unpack_json_int_array(i_pos, 4, &x1, &y1, &x2, &y2)) {
				
			}
			break;
		}
		case protocol_command_as_int('B', 'L', 'K'):
		{
//<-- BLK {"turf": [[x, y, type, w, h], ...], "obj": [[x, y, [type], w, h], ...], "username": username}
//<-- BLK {"copy": [{"turf": true/false, "obj": true/false, "src":[x,y,w,h], "dst":[x,y]}, ...]], "username": username}
			cJSON *i_copy = get_json_item(json, "copy");
			if(i_copy) {

			}

			cJSON *i_turf = get_json_item(json, "turf");
			if(i_turf) {

			}

			cJSON *i_obj = get_json_item(json, "obj");
			if(i_obj) {

			}
			break;
		}

		case protocol_command_as_int('W', 'H', 'O'):
		{
//<-- WHO {"list": {"[id]": {"name": name, "pic": [s, x, y], "x": x, "y": y, "dir": dir, "id": id}, "you":id}
//<-- WHO {"add": {"name": name, "pic": [s, x, y], "x": x, "y": y, "dir", dir, "id": id}}
//<-- WHO {"update": {"id": id, other fields}}
//<-- WHO {"remove": id}
//<-- WHO {"new_id": {"id": old_id, "new_id", id}}
			cJSON *i_list = get_json_item(json, "list");
			if(i_list) {

			}

			cJSON *i_add = get_json_item(json, "add");
			if(i_add) {

			}

			cJSON *i_remove = get_json_item(json, "remove");
			if(i_remove) {

			}

			cJSON *i_update = get_json_item(json, "update");
			if(i_update) {

			}

			cJSON *i_new_id = get_json_item(json, "new_id");
			if(i_new_id) {

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
//<-- RSC {"images": {"id": "url", ...}, "tilesets": {"id": {}, ...}}
			cJSON *i_images   = get_json_item(json, "images");
			if(i_images) {

			}
			cJSON *i_tilesets = get_json_item(json, "tilesets");
			if(i_tilesets) {

			}
			break;
		}

		case protocol_command_as_int('I', 'M', 'G'):
		{
// <-- IMG {"id": number, "url": string}
			cJSON *i_id  = get_json_item(json, "id");
			const char *i_url = get_json_string(json, "url");
			if(i_id) {

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
// <-- PRI {"text": "[text"], "name": display name, "username": username, "receive": true/false}
			cJSON *i_text     = get_json_item(json, "text");
			cJSON *i_name     = get_json_item(json, "name");
			cJSON *i_username = get_json_item(json, "username");
			cJSON *i_receive  = get_json_item(json, "receive");
			break;
		}

		case protocol_command_as_int('E', 'R', 'R'):
		{
// <-- ERR {"text": "[text]"}
			const char *i_text = get_json_string(json, "text");
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
			break;
		}
	}

	if(json)
		cJSON_Delete(json);
}
