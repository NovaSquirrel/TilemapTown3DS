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

// https://libctru.devkitpro.org/swkbd_8h.htm

volatile bool run_keyboard_thread = true;
Thread thread_handle;
Handle thread_request;
#define STACKSIZE (4 * 1024)

void keyboard_thread(void *arg) {
	TilemapTownClient *client = (TilemapTownClient*)arg;

	while(run_keyboard_thread) {
		client->network_update();
		svcSleepThread(16666666ULL);
	}
}

void show_keyboard(TilemapTownClient *client) {
	// Create background thread

	// Use slightly more priority
	run_keyboard_thread = true;
	s32 prio = 0;
	svcGetThreadPriority(&prio, CUR_THREAD_HANDLE);
	thread_handle = threadCreate(keyboard_thread, client, STACKSIZE, prio-1, -2, false);

	// Display keyboard

	static SwkbdState swkbd;
	static char text_buffer[1024];
	static SwkbdStatusData swkbdStatus;
	static SwkbdLearningData swkbdLearning;

	swkbdInit(&swkbd, SWKBD_TYPE_NORMAL, 2, -1);
	swkbdSetInitialText(&swkbd, "");
	swkbdSetHintText(&swkbd, "Chat!");
	swkbdSetButton(&swkbd, SWKBD_BUTTON_LEFT, "Cancel", false);
	swkbdSetButton(&swkbd, SWKBD_BUTTON_RIGHT, "Send", true);
	swkbdSetFeatures(&swkbd, SWKBD_PREDICTIVE_INPUT);
	static bool reload = false;
	swkbdSetStatusData(&swkbd, &swkbdStatus, reload, true);
	swkbdSetLearningData(&swkbd, &swkbdLearning, reload, true);
	reload = true;
	SwkbdButton button = swkbdInputText(&swkbd, text_buffer, sizeof(text_buffer));

	// Stop thread

	run_keyboard_thread = false;
	threadJoin(thread_handle, U64_MAX);
	threadFree(thread_handle);

	// Send the message

	if(button == SWKBD_BUTTON_CONFIRM && text_buffer[0]) {
		if(!strcmp(text_buffer, "/clear")) {
			consoleClear();
		} else {
			cJSON *json = cJSON_CreateObject();
			if(text_buffer[0] == '/' && !(text_buffer[1] == 'm' && text_buffer[2] == 'e' && text_buffer[3] == ' ')) {
				cJSON_AddStringToObject(json, "text", text_buffer+1);
				client->websocket_write("CMD", json);
			} else {
				cJSON_AddStringToObject(json, "text", text_buffer);
				client->websocket_write("MSG", json);
			}
			cJSON_Delete(json);
		}
	}

}
