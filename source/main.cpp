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
#include <3ds.h>

int network_init();
void network_finish();

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

void http_callback(const char *url, uint8_t *memory, size_t size, void *userdata) {
	puts("Callback called");
	printf("Url %s Size %d\n", url, size);
}

int main(int argc, char* argv[]) {
	TilemapTownClient client;

	gfxInitDefault();
	consoleInit(GFX_TOP, NULL);
	if(network_init() == 0) {
		printf("network_init call failed!\n");
		wait_for_key();
		goto cleanup;
	}

	puts("Attempting to connect to the server...");
	client.network_connect("novasquirrel.com", "/townws/", "443");
	puts("Connected!");

	// --------------------------------------------------------------

	// Main loop
	while (aptMainLoop()) {
		gspWaitForVBlank();
		gfxSwapBuffers();
		hidScanInput();

		client.network_update();

		u32 kDown = hidKeysDown();
		if (kDown & KEY_A) {
			puts("Requesting");
			client.http.http_get("https://novasquirrel.com/robots.txt", http_callback, NULL);
		}

		if (kDown & KEY_START)
			break;
	}

	client.network_disconnect();

cleanup:
	network_finish();
	gfxExit();
	return 0;
}
