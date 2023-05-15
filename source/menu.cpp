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

const char *prompt_for_text(const char *hint, const char *initial, size_t limit);

void text_xy(int x, int y) {
	printf("\x1b[%d;%dH", y+1, x+1);
}

#define MAIN_MENU_CHOICES 8

// Login settings
char login_username[256] = "";
char login_password[256] = "";
bool guest_login = false;

char login_hostname[256] = "novasquirrel.com";
char login_path[256]     = "/townws/";
char login_port[6]       = "443";

// ----------------------------------------------

enum config_types {
	CONFIG_STRING,
	CONFIG_INTEGER,
};

struct config_item {
	const char *group;
	const char *item;
	void *data;
	enum config_types type;
	size_t len;
	bool hidden;
} config_options[] = {
	{"User", "Username", &login_username, CONFIG_STRING, sizeof(login_username), false},
	{"User", "Password", &login_password, CONFIG_STRING, sizeof(login_password), false},
	{"Server", "Hostname", &login_hostname, CONFIG_STRING, sizeof(login_hostname), false},
	{"Server", "Path",     &login_path, CONFIG_STRING, sizeof(login_path), false},
	{"Server", "Port",     &login_port, CONFIG_STRING, sizeof(login_port), false},
	{NULL}
};

void INI_handler(const char *group, const char *item, const char *value) {
	for(int i=0; config_options[i].group !=NULL; i++) {
		if(!strcasecmp(config_options[i].group, group) && !strcasecmp(config_options[i].item, item)) {
			switch(config_options[i].type) {
				case CONFIG_INTEGER:
					*(int*)config_options[i].data = strtol(value, NULL, 10);
					return;
				case CONFIG_STRING:
					strlcpy((char*)config_options[i].data, value, config_options[i].len);
					return;
			}
			break;
		}
	}
	printf("Config item \"[%s] %s\" not valid\n", group, item);
}

int load_ini_file(FILE *file, void (*handler)(const char *group, const char *item, const char *value)) {
	char group[64]="";
	char *item, *value;
	char line[512]="";
	char c;
	char *split;
	if(file == NULL)
		return 0;
	int i;
	while(!feof(file)) {
		// Read one line - maybe just use fgets
		for(i=0;;i++) {
			if(i >= 511) { // Line too long
				fclose(file);
				return 0;
			}
			c = fgetc(file);
			if(c=='\r' || c=='\n') {
				line[i]=0;
				break;
			}
			line[i] = c;
		}
		while(c=='\r' || c=='\n')
			c = fgetc(file);
		fseek(file, -1 , SEEK_CUR);
		if(!*line)
			break;
		else if(*line == ';' || *line == '#') // comment
			;
		else if(*line == '[') { // group
			split = strchr(line, ']');
			if(split)
				*split = 0;
			strcpy(group, line+1);
		} else { // item
			split = strchr(line, '=');
			if(split) {
				*split = 0;
				item = line;
				value = split+1;
				handler(group, item, value);
			}
		}
	}
	fclose(file);
	return 1;
}

int save_ini_file(FILE *file, struct config_item *options) {
	const char *group = "";
	for(int i=0; options[i].group; i++) {
		if(strcmp(group, options[i].group)) {
			group = options[i].group;
			fprintf(file, "[%s]\r\n", group);
		}

		switch(options[i].type) {
			case CONFIG_STRING:
				if (!options[i].hidden || strlen((const char*)options[i].data) > 0)
					fprintf(file, "%s=%s\r\n", options[i].item, (const char*)options[i].data);
				break;
			case CONFIG_INTEGER:
				if (!options[i].hidden || *(int*)options[i].data != 0)
					fprintf(file, "%s=%d\r\n", options[i].item, *(int*)options[i].data);
				break;
		}
	}
	fclose(file);
	return 1;
}

bool load_settings() {
	FILE *settings = fopen("TilemapTown.ini", "rb");
	if(settings) {
		load_ini_file(settings, INI_handler); // takes care of closing the file
		return true;
	}
	return false;
}

bool save_settings() {
	FILE *settings = fopen("TilemapTown.ini", "wb");
	if(settings) {
		save_ini_file(settings, config_options); // takes care of closing the file
		return true;
	} else {
		puts("Couldn't save preferences file");
		return false;
	}
}

// ----------------------------------------------------------------------------

void strcpy_if_not_null(char *target, const char *source) {
	if(source)
		strcpy(target, source);
}

bool main_menu() {
	bool main_menu_exit = false;
	bool settings_changed = false;
	bool redraw_menu = true;
	int cursor_y = 0, old_cursor_y = 0;

	load_settings();

	while(aptMainLoop() && !main_menu_exit) {
		if(redraw_menu) {
			consoleClear();
			printf("TILEMAP TOWN 3DS\n"
			"  Connect to server!\n"
			"  Connect as guest\n"
			"  Set username: %s\n"
			"  Set password: %s\n"
			"  Set server: %s\n"
			"  Delete username and password\n"
			"  Reset server details\n"
			"  Quit", login_username, login_password[0]?"*":"", login_hostname
			);
			text_xy(0, cursor_y+1);
			putchar('>');
			redraw_menu = false;
		}

		hidScanInput();

		u32 kDown       = hidKeysDown();
		u32 kDownRepeat = hidKeysDownRepeat();

		if(kDownRepeat & KEY_DOWN) {
			cursor_y++;
			if(cursor_y >= MAIN_MENU_CHOICES)
				cursor_y = 0;
		}
		if(kDownRepeat & KEY_UP) {
			cursor_y--;
			if(cursor_y < 0)
				cursor_y = MAIN_MENU_CHOICES-1;
		}
		if(kDown & KEY_A) {
			switch(cursor_y) {
				case 0: // Connect with credentials
					guest_login = false;
					main_menu_exit = true;
					break;
				case 1: // Connect as guest
					guest_login = true;
					main_menu_exit = true;
					break;
				case 2: // Set username
					strcpy_if_not_null(login_username, prompt_for_text("Username", login_username, sizeof(login_username)));
					settings_changed = true;
					redraw_menu = true;
					break;
				case 3: // Set password
					strcpy_if_not_null(login_password, prompt_for_text("Password", login_password, sizeof(login_username)));
					settings_changed = true;
					redraw_menu = true;
					break;
				case 4: // Set server
					strcpy_if_not_null(login_hostname, prompt_for_text("Server hostname", login_hostname, sizeof(login_hostname)));
					strcpy_if_not_null(login_path, prompt_for_text("Server path", login_path, sizeof(login_path)));
					strcpy_if_not_null(login_port, prompt_for_text("Server port ", login_port, sizeof(login_port)));
					settings_changed = true;
					redraw_menu = true;
					break;
				case 5: // Erase credentials
					strcpy(login_username, "");
					strcpy(login_password, "");
					settings_changed = true;
					redraw_menu = true;
					break;
				case 6: // Reset server details
					strcpy(login_hostname, "novasquirrel.com");
					strcpy(login_path,     "/townws/");
					strcpy(login_port,     "443");
					settings_changed = true;
					redraw_menu = true;
					break;
				case 7: // Quit
					if(settings_changed)
						save_settings();
					return false;
			}
		}

		if(cursor_y != old_cursor_y) {
			text_xy(0, old_cursor_y+1);
			putchar(' ');
			text_xy(0, cursor_y+1);
			putchar('>');

			old_cursor_y = cursor_y;
		}

		gfxFlushBuffers();
		gfxSwapBuffers();
		gspWaitForVBlank();			
	}

	consoleClear();

	if(settings_changed)
		save_settings();
	return true;
}