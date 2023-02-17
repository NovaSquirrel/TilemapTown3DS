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

int network_init();
void network_finish();
void http_png_callback(const char *url, uint8_t *memory, size_t size, TilemapTownClient *client, void *userdata);

extern int texture_loaded_yet;
extern C3D_Tex loaded_texture;

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

	C3D_RenderTarget* top = C2D_CreateScreenTarget(GFX_TOP, GFX_LEFT);

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
		//gspWaitForVBlank();
		//gfxSwapBuffers();
		hidScanInput();

		client.network_update();

		u32 kDown = hidKeysDown();
		if (kDown & KEY_B) {
			printf("How many tiles: %d\n", client.tileset.size());
		}
		if (kDown & KEY_A) {
			puts("Requesting");
			client.http.get("https://novasquirrel.com/town/img/potluck.png", http_png_callback, NULL);
		}

		if (kDown & KEY_START)
			break;

		// Render the scene
		C3D_FrameBegin(C3D_FRAME_SYNCDRAW); // vsync
		C2D_TargetClear(top, C2D_Color32f(0.0f, 0.5f, 0.0f, 1.0f));
		C2D_SceneBegin(top);

		C2D_DrawRectangle(10, 10, 0, 60, 60, C2D_Color32f(0.0f, 1.0f, 0.0f, 1.0f), C2D_Color32f(0.0f, 1.0f, 0.0f, 1.0f), C2D_Color32f(0.0f, 1.0f, 0.0f, 1.0f), C2D_Color32f(0.0f, 1.0f, 0.0f, 1.0f));

//		if(texture_loaded_yet) {
//			C2D_DrawRectangle(20, 20, 0, 60, 60, C2D_Color32f(1.0f, 1.0f, 0.0f, 1.0f), C2D_Color32f(0.0f, 1.0f, 0.0f, 1.0f), C2D_Color32f(0.0f, 1.0f, 0.0f, 1.0f), C2D_Color32f(0.0f, 	1.0f, 0.0f, 1.0f));
/*
			C2D_Image image;

			Tex3DS_SubTexture sub;
			sub.width  = 512;
			sub.height = 512;
			sub.left   = 0;
			sub.top    = 0;
			sub.right  = 1;
			sub.bottom = 1;
*/

/*
			for(int i=0; i<10; i++) {
//				Tex3DS_SubTexture sub = calc_subtexture(512, 512, 16, 16, i, i); //{16, 16, 0.0f, 1.0f, 1.0f, 0.0f};
//				Tex3DS_SubTexture sub = {512, 512, 0.0f, 1.0f, 1.0f, 0.0f};

				image.tex = &loaded_texture;
				image.subtex = &sub;

				C2D_DrawImageAt(image, 5+i*16, 5+i*16, 0, NULL, 1.0f, -1.0f);
			}
*/
//		}

		client.draw_map(0, 0);

		C3D_FrameEnd(0);
	}

	client.network_disconnect();

cleanup:
	network_finish();
	C2D_Fini();
	C3D_Fini();
	gfxExit();
	return 0;
}
