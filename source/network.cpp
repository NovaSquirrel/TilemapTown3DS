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
#include <stdlib.h>

#ifdef __3DS__
#include <3ds.h>
#include <malloc.h>
#endif

// ----------------------------------------------
// - Defines
// ----------------------------------------------

#ifdef __3DS__
// 3DS socket buffer
#define SOC_ALIGN       0x1000
#define SOC_BUFFERSIZE  0x100000
#endif

// ----------------------------------------------
// - Globals
// ----------------------------------------------

ssize_t wslay_recv(wslay_event_context_ptr ctx, uint8_t *data, size_t len, int flags, void *user_data);
ssize_t wslay_send(wslay_event_context_ptr ctx, const uint8_t *data, size_t len, int flags, void *user_data);
int wslay_genmask(wslay_event_context_ptr ctx, uint8_t *buf, size_t len, void *user_data);
void wslay_message(wslay_event_context_ptr ctx, const struct wslay_event_on_msg_recv_arg *arg, void *user_data);
void wait_for_key();

struct wslay_event_callbacks wslay_callbacks = {
	wslay_recv,
	wslay_send,
	wslay_genmask,
	NULL,
	NULL,
	NULL,
	wslay_message,
};

static u32 *SOC_buffer = NULL;

// Login details
extern char login_username[256];
extern char login_password[256];
extern bool guest_login;

// ----------------------------------------------
// - Initialization
// ----------------------------------------------

int network_init() {
	int ret;

	#ifdef __3DS__
	// allocate buffer for SOC service
	SOC_buffer = (u32*)memalign(SOC_ALIGN, SOC_BUFFERSIZE);

	if(SOC_buffer == NULL) {
		printf("memalign: failed to allocate\n");
		return 0;
	}

	// Now intialise soc:u service
	if ((ret = socInit(SOC_buffer, SOC_BUFFERSIZE)) != 0) {
    	printf("socInit: 0x%08X\n", (unsigned int)ret);
		return 0;
	}
	#endif

	curl_global_init(CURL_GLOBAL_DEFAULT);

	return 1;
}

void network_finish() {
	curl_global_cleanup();

	#ifdef __3DS__
	socExit();
	#endif
}

// ----------------------------------------------
// - Connection management
// ----------------------------------------------

static void my_debug(void *ctx, int level,
                     const char *file, int line,
                     const char *str) {
    ((void) level);

    mbedtls_fprintf((FILE *) ctx, "%s:%04d: %s", file, line, str);
    fflush((FILE *) ctx);
}

int TilemapTownClient::network_connect(std::string host, std::string path, std::string port) {
	// Based on "SSL client demonstration program"
	// available under the Apache 2.0 license
	// https://github.com/Mbed-TLS/mbedtls/blob/development/programs/ssl/ssl_client1.c

    //uint32_t flags;
	int ret, len;
	const char *personal = "TilemapTown"; // Used to add more entropy?

	std::string connect_string = "GET "+path+" HTTP/1.1\r\n"
		"Host: "+host+"\r\n"
		"Upgrade: websocket\r\n"
		"Connection: Upgrade\r\n"
		"Sec-WebSocket-Key: x3JJHMbDL1EzLkh9GBhXDw==\r\n"
		"Sec-WebSocket-Version: 13\r\n"
		"\r\n";

	unsigned char buf[1024];

	mbedtls_net_init(&this->server_fd);
	mbedtls_ssl_init(&this->ssl);
	mbedtls_ssl_config_init(&this->conf);
	mbedtls_x509_crt_init(&this->cacert);
	mbedtls_ctr_drbg_init(&this->ctr_drbg); // "deterministic random bit generator"

	mbedtls_entropy_init(&this->entropy);
	if(mbedtls_ctr_drbg_seed(&this->ctr_drbg, mbedtls_entropy_func, &this->entropy, (const unsigned char *)personal, strlen(personal)) != 0) {
		puts("mbedtls_ctr_drbg_seed failed");
		goto fail;
	}

	// Start connection
	//puts("mbedtls_net_connect");
	if(mbedtls_net_connect(&this->server_fd, host.c_str(), port.c_str(), MBEDTLS_NET_PROTO_TCP) != 0) {
		puts("mbedtls_net_connect failed");
        goto fail;
	}
	if(mbedtls_net_set_block(&this->server_fd)) {
		puts("mbedtls_net_set_block failed");
		goto fail;
	}

	// Setup
	//puts("mbedtls_ssl_config_defaults");
	if(mbedtls_ssl_config_defaults(&conf, MBEDTLS_SSL_IS_CLIENT, MBEDTLS_SSL_TRANSPORT_STREAM, MBEDTLS_SSL_PRESET_DEFAULT) != 0) {
		puts("mbedtls_ssl_config_defaults failed");
		goto fail;
	}

	mbedtls_ssl_conf_authmode(&conf, MBEDTLS_SSL_VERIFY_OPTIONAL);
	mbedtls_ssl_conf_ca_chain(&conf, &cacert, NULL);
	mbedtls_ssl_conf_rng(&conf, mbedtls_ctr_drbg_random, &this->ctr_drbg);
	mbedtls_ssl_conf_dbg(&conf, my_debug, stdout);

	if(mbedtls_ssl_setup(&this->ssl, &conf) != 0) {
		puts("mbedtls_ssl_setup failed");
		goto fail;
	}

	if(mbedtls_ssl_set_hostname(&this->ssl, host.c_str()) != 0) {
		puts("mbedtls_ssl_set_hostname failed");
		goto fail;
	}

	mbedtls_ssl_set_bio(&this->ssl, &this->server_fd, mbedtls_net_send, mbedtls_net_recv, NULL); // Set what functions to use for reading and writing

	//puts("mbedtls_ssl_handshake");

	// Handshake
	while((ret = mbedtls_ssl_handshake(&this->ssl)) != 0) {
		if(ret != MBEDTLS_ERR_SSL_WANT_READ && ret != MBEDTLS_ERR_SSL_WANT_WRITE) {
			puts("mbedtls_ssl_handshake failed");
			goto fail;
		}
	}

	// -----------------------------------------------------------------------

	len = connect_string.length();

	//puts("mbedtls_ssl_write");

	// Write
    while((ret = mbedtls_ssl_write(&this->ssl, (const unsigned char*)connect_string.c_str(), len)) <= 0) {
        if (ret != MBEDTLS_ERR_SSL_WANT_READ && ret != MBEDTLS_ERR_SSL_WANT_WRITE) {
			puts("mbedtls_ssl_write failed");
            goto fail;
        }
    }

	//puts("mbedtls_ssl_read");

    // Read
	len = 0;
    do {
        memset(buf, 0, sizeof(buf));

        ret = mbedtls_ssl_read(&this->ssl, buf, sizeof(buf)-len-1);
        if(ret == MBEDTLS_ERR_SSL_WANT_READ || ret == MBEDTLS_ERR_SSL_WANT_WRITE) {
            continue;
        } else if(ret == MBEDTLS_ERR_SSL_PEER_CLOSE_NOTIFY) {
			puts("Server disconnected");
            goto fail;
        } else if(ret < 0) {
            mbedtls_printf("failed\n  ! mbedtls_ssl_read returned %d\n\n", ret);
            goto fail;
        } else if(ret == 0) {
            mbedtls_printf("\n\nEOF\n\n");
            break;
        }
		len += ret;
		buf[len] = 0;

		if(strstr((const char*)buf, "\r\n\r\n")) { // Handshake complete
			if(strstr((const char*)buf, "Sec-WebSocket-Accept")) {
				puts("Handshake successful");
				break;
			} else {
				puts("Handshake failed");
			}
		}

		printf("%d bytes read\n", ret);
    } while(1);

	// Switch it to nonblocking mode
	if(mbedtls_net_set_nonblock(&this->server_fd)) {
		puts("mbedtls_net_set_nonblock failed");
		goto fail;
	}

	// Set up websockets
	if(wslay_event_context_client_init(&this->websocket, &wslay_callbacks, this)) {
		puts("wslay_event_context_client_init failed");
		goto fail;
	}
	wslay_event_config_set_max_recv_msg_length(this->websocket, 0x80000*2); // 1024KB

	// Other initialization
	this->http.client = this;
	this->connected = true;

	{
	// Kick off the connection by sending a IDN message!
	// Build the IDN message to send.
	cJSON *json = cJSON_CreateObject();
	cJSON *json_features = cJSON_CreateObject();
	cJSON *json_features_batch = cJSON_CreateObject();
	cJSON *json_features_bulk_build = cJSON_CreateObject();

	cJSON_AddItemToObjectCS(json, "features", json_features);

	cJSON_AddItemToObjectCS(json_features, "batch", json_features_batch);
	cJSON_AddStringToObject(json_features_batch, "version", "0.0.1");

	cJSON_AddItemToObjectCS(json_features, "bulk_build", json_features_bulk_build);
	cJSON_AddStringToObject(json_features_bulk_build, "version", "0.0.1");

	if(!guest_login && login_username[0] && login_password[0]) {
		cJSON_AddStringToObject(json, "username", login_username);
		cJSON_AddStringToObject(json, "password", login_password);
	}
	cJSON_AddStringToObject(json, "client_name", "Tilemap Town 3DS Client");
	this->websocket_write("IDN", json);
	cJSON_Delete(json);
	}

	//this->websocket_write("IDN {\"features\": {\"batch\": {\"version\": \"0.0.1\"}}}");
	//this->websocket_write("CMD {\"text\": \"nick 3ds\"}");
	return 1;

fail:
    mbedtls_net_free(&this->server_fd);
    mbedtls_x509_crt_free(&this->cacert);
    mbedtls_ssl_free(&this->ssl);
    mbedtls_ssl_config_free(&this->conf);
    mbedtls_ctr_drbg_free(&this->ctr_drbg);
    mbedtls_entropy_free(&this->entropy);
	return 0;
}

void TilemapTownClient::network_disconnect() {
	if(this->connected) {
		mbedtls_ssl_close_notify(&this->ssl);

		mbedtls_net_free(&this->server_fd);
		mbedtls_x509_crt_free(&this->cacert);
		mbedtls_ssl_free(&this->ssl);
		mbedtls_ssl_config_free(&this->conf);
		mbedtls_ctr_drbg_free(&this->ctr_drbg);
		mbedtls_entropy_free(&this->entropy);

		this->connected = false;
	}
}

void TilemapTownClient::network_update() {
	if(this->connected)
		wslay_event_recv(this->websocket);
	if(this->connected && wslay_event_want_write(this->websocket))
        wslay_event_send(this->websocket);
	this->http.run_transfers();
}

// ----------------------------------------------
// - HTTP
// ----------------------------------------------

HttpFileCache::HttpFileCache() {
	this->http = curl_multi_init();
	this->http_in_progress = false;
}

HttpFileCache::~HttpFileCache() {
	for(const auto& kv : this->cache) {
		free(kv.second.memory);
	}

	curl_multi_cleanup(this->http);
}

size_t http_write_callback(void *contents, size_t size, size_t nmemb, void *userdata) {
	struct http_file *file = (struct http_file*)userdata;
	size_t real_size = size * nmemb;
	//printf("http_write_callback %d \n", real_size);

	file->memory = (uint8_t*)realloc(file->memory, file->size + real_size);
	if(!file->memory)
		return 0;
	memcpy(file->memory + file->size, contents, real_size);

	file->size += real_size;
	return real_size;
}

void HttpFileCache::run_transfers() {
	if(this->http_in_progress) {
		// Update all http transfers
		int still_running;
		curl_multi_perform(this->http, &still_running);
		if(!still_running)
			this->http_in_progress = false;

		// Check on all transfers
		int queue_size;
		CURLMsg *multi_info;
		while((multi_info = curl_multi_info_read(this->http, &queue_size))) {
			if(multi_info->msg != CURLMSG_DONE)
				continue;
			// Get info
			CURL *easy = multi_info->easy_handle;
			CURLcode result = multi_info->data.result; // https://curl.se/libcurl/c/libcurl-errors.html - error if nonzero
			struct http_transfer *transfer;
			if(curl_easy_getinfo(easy, CURLINFO_PRIVATE, (char*)&transfer) != CURLE_OK)
				continue;

			// Call callback function with the data retrieved
			if(result == CURLE_OK) {
				transfer->callback(transfer->url, transfer->file.memory, transfer->file.size, this->client, transfer->userdata);
			} else {
				puts(curl_easy_strerror(result));
			}
			free(transfer->userdata);

			// Put it in the cache, and remove from requested URLs
			std::string url = std::string(transfer->url);
			this->cache[url] = transfer->file;
			this->requested_urls.erase(url);

			this->total_size += transfer->file.size;
			transfer->file.last_accessed = time(NULL);

			// Clean up
			free((void*)transfer->url);
			free(transfer);
			curl_multi_remove_handle(this->http, easy);
			curl_easy_cleanup(easy);
		}
	}
}

void HttpFileCache::get(std::string url, void (*callback) (const char *url, uint8_t *data, size_t size, TilemapTownClient *client, void *userdata), void *userdata) {
	// Don't request it if it's currently being requested
	if(this->requested_urls.find(url) != this->requested_urls.end()) {
		return;
	}

	// Try to find it in the cache
	auto it = this->cache.find(url);
	if(it != this->cache.end()) {
		// If it's already there, don't re-request it, just get the cached version
		callback(url.c_str(), (*it).second.memory, (*it).second.size, this->client, userdata);
		(*it).second.last_accessed = time(NULL);
		return;
	}

	// Stop this url from being requested again until the transfer has finished
	this->requested_urls.insert(url);

	// Set up the transfer and start it
	struct http_transfer *transfer = (struct http_transfer*)calloc(1, sizeof(struct http_transfer));
	if(!transfer)
		return;
	transfer->callback = callback;
	transfer->userdata = userdata;
	transfer->url = strdup(url.c_str());

	CURL *curl = curl_easy_init();

	curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
	curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 1); // Don't use a progress meter
	curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, http_write_callback);
	curl_easy_setopt(curl, CURLOPT_WRITEDATA,     &transfer->file);
	curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
	curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);
	curl_easy_setopt(curl, CURLOPT_PRIVATE, transfer);

	curl_multi_add_handle(this->http, curl);

	this->http_in_progress = true;
}

// ----------------------------------------------
// - Websockets
// ----------------------------------------------

ssize_t wslay_recv(wslay_event_context_ptr ctx, uint8_t *data, size_t len, int flags, void *user_data) {
	TilemapTownClient *client = (TilemapTownClient*)user_data;

	if(!client->connected)
		return WSLAY_ERR_WOULDBLOCK;
	int ret = mbedtls_ssl_read(&client->ssl, data, len);
	if(ret <= 0) {
		if(ret < 0 && ret != MBEDTLS_ERR_SSL_WANT_WRITE && ret != MBEDTLS_ERR_SSL_WANT_READ) {
			client->connected = false;
			printf("Received recv error code %d\n", ret);
		}
		return WSLAY_ERR_WOULDBLOCK;
	}
	return ret;
}

ssize_t wslay_send(wslay_event_context_ptr ctx, const uint8_t *data, size_t len, int flags, void *user_data) {
	TilemapTownClient *client = (TilemapTownClient*)user_data;

	if(!client->connected)
		return WSLAY_ERR_WOULDBLOCK;
	int ret = mbedtls_ssl_write(&client->ssl, data, len);
	if(ret <= 0) {
		if(ret < 0 && ret != MBEDTLS_ERR_SSL_WANT_WRITE && ret != MBEDTLS_ERR_SSL_WANT_READ) {
			client->connected = false;
			printf("Received send error code %d\n", ret);
		}
		return WSLAY_ERR_WOULDBLOCK;
	}
	return ret;
}

int wslay_genmask(wslay_event_context_ptr ctx, uint8_t *buf, size_t len, void *user_data) {
	for(size_t i=0; i<len; i++)
		buf[i] = rand()&255;
	return 0;
}

void wslay_message(wslay_event_context_ptr ctx, const struct wslay_event_on_msg_recv_arg *arg, void *user_data) {
	TilemapTownClient *client = (TilemapTownClient*)user_data;
	if(arg->opcode == WSLAY_TEXT_FRAME) {
		client->websocket_message((const char*)arg->msg, arg->msg_length);
	} else if(arg->opcode == WSLAY_CONNECTION_CLOSE) {
		puts("\x1b[31mConnection closed\x1b[0m\nPress A to continue");
		client->network_disconnect();
		wait_for_key();
	}
}

void TilemapTownClient::websocket_write(std::string text) {
	struct wslay_event_msg event_message;
	event_message.opcode = WSLAY_TEXT_FRAME;
	event_message.msg = (const uint8_t*)text.c_str();
	event_message.msg_length = text.size();
	wslay_event_queue_msg(this->websocket, &event_message);
}
