/*
 * Tilemap Town client for 3DS
 *
 * Copyright (C) 2023-2024 NovaSquirrel
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

int network_init();
void network_finish();
void http_png_callback(const char *url, uint8_t *memory, size_t size, TilemapTownClient *client, void *userdata);

bool main_menu();
void show_keyboard(TilemapTownClient *client);
const char *prompt_for_text(const char *hint, const char *initial);

extern int texture_loaded_yet;
extern C3D_Tex loaded_texture;

// Login settings
extern char login_hostname[256];
extern char login_path[256];
extern char login_port[6];

void wait_for_key() {
	// Just wait for keys and then exit
	while(aptMainLoop()) {
		gspWaitForVBlank();
		gfxSwapBuffers();
		hidScanInput();

		u32 kDown = hidKeysDown();
		if (kDown & (KEY_START | KEY_A))
			break;
	}
}

int main(int argc, char* argv[]) {
	TilemapTownClient client = TilemapTownClient();
	gfxInitDefault();
	C3D_Init(C3D_DEFAULT_CMDBUF_SIZE);
	C2D_Init(C2D_DEFAULT_MAX_OBJECTS);
	C2D_Prepare();
	consoleInit(GFX_BOTTOM, NULL);
	hidSetRepeatParameters(20, 10);

	C3D_RenderTarget* top = C2D_CreateScreenTarget(GFX_TOP, GFX_LEFT);
	bool want_to_exit = false;

	if(network_init() == 0) {
		printf("network_init call failed!\n");
		wait_for_key();
		goto cleanup;
	}

	while(!want_to_exit) {
		if(!main_menu())
			break;

		puts("Attempting to connect to the server...");
		if(!client.network_connect(login_hostname, login_path, login_port)) {
			puts("Couldn't connect to the server");
			wait_for_key();
			continue;
		}
		puts("Connected! Press X to chat.");

		// --------------------------------------------------------------

		// Main loop
		while (aptMainLoop() && client.connected) {
			//gspWaitForVBlank();
			//gfxSwapBuffers();
			hidScanInput();

			client.network_update();
			
			u32 kHeld       = hidKeysHeld();
			u32 kDown       = hidKeysDown();
			u32 kDownRepeat = hidKeysDownRepeat();
			if(kDown & KEY_B) {
				printf("How many tiles: %d %d\n", client.tileset.size(), client.json_tileset.size());
			}
			if(kDown & KEY_X) {
				show_keyboard(&client);
			}

			client.walk_through_walls = (kHeld & KEY_Y); // Temporary

			if(kDownRepeat & KEY_LEFT)  client.move_player(-1,  0);
			if(kDownRepeat & KEY_DOWN)  client.move_player( 0,  1);
			if(kDownRepeat & KEY_UP)    client.move_player( 0, -1);
			if(kDownRepeat & KEY_RIGHT) client.move_player( 1,  0);

			if(kDown & KEY_CSTICK_LEFT)  client.turn_player(4);
			if(kDown & KEY_CSTICK_DOWN)  client.turn_player(2);
			if(kDown & KEY_CSTICK_UP)    client.turn_player(6);
			if(kDown & KEY_CSTICK_RIGHT) client.turn_player(0);

			if(kDown & KEY_START) {
				want_to_exit = true;
				break;
			}

			// Render the scene
			C3D_FrameBegin(C3D_FRAME_SYNCDRAW); // vsync
			C2D_TargetClear(top, C2D_Color32(0, 0, 0, 255));
			C2D_SceneBegin(top);
			client.update_camera(0, 0);
			client.draw_map(round(client.camera_x), round(client.camera_y));

			C3D_FrameEnd(0);
		}

		client.network_disconnect();
		// Free this here instead of inside network_disconnect because network_disconnect can be called within a websocket callback
		// and freeing the websocket in there can be bad.
		if(client.websocket) {
			wslay_event_context_free(client.websocket);
			client.websocket = nullptr;
		}
	}

cleanup:
	network_finish();
	C2D_Fini();
	C3D_Fini();
	gfxExit();
	return 0;
}
