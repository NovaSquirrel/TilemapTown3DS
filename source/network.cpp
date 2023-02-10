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
#include <stdlib.h>

#ifdef __3DS__
#include <3ds.h>
#include <malloc.h>
#endif

#ifdef __3DS__
// 3DS socket buffer
#define SOC_ALIGN       0x1000
#define SOC_BUFFERSIZE  0x100000
#endif

static u32 *SOC_buffer = NULL;

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
	return 1;
}

void network_finish() {
	curl_global_cleanup();
	#ifdef __3DS__
	socExit();
	#endif
}

// ----------------------------------------------

void curl_test() {
	CURL *curl;
	CURLcode res;

	curl_global_init(CURL_GLOBAL_DEFAULT);

	curl = curl_easy_init();
	if(curl) {
		curl_easy_setopt(curl, CURLOPT_URL, "https://novasquirrel.com/robots.txt");

		// maybe CURLOPT_CAPATH
		curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);

		#ifdef SKIP_HOSTNAME_VERIFICATION
		/*
		 * If the site you are connecting to uses a different host name that what
		 * they have mentioned in their server certificate's commonName (or
		 * subjectAltName) fields, libcurl will refuse to connect. You can skip
		 * this check, but this will make the connection less secure.
		 */
		curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);
		#endif

		/* Perform the request, res will get the return code */
		res = curl_easy_perform(curl);
		printf("Result: %d\n", res);
		/* Check for errors */
		if(res != CURLE_OK)
		  printf("curl_easy_perform() failed: %s\n",
				  curl_easy_strerror(res));

			/* always cleanup */
			curl_easy_cleanup(curl);
	}
}

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
	puts("mbedtls_net_connect");
	if(mbedtls_net_connect(&this->server_fd, host.c_str(), port.c_str(), MBEDTLS_NET_PROTO_TCP) != 0) {
		puts("mbedtls_net_connect failed");
        goto fail;
	}
	if(mbedtls_net_set_nonblock(&this->server_fd)) {
		puts("mbedtls_net_set_nonblock failed");
		goto fail;
	}

	// Setup
	puts("mbedtls_ssl_config_defaults");
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

	puts("mbedtls_ssl_handshake");

	// Handshake
	while((ret = mbedtls_ssl_handshake(&this->ssl)) != 0) {
		if(ret != MBEDTLS_ERR_SSL_WANT_READ && ret != MBEDTLS_ERR_SSL_WANT_WRITE) {
			puts("mbedtls_ssl_handshake failed");
			goto fail;
		}
	}

	// -----------------------------------------------------------------------

	len = connect_string.length();

	puts("mbedtls_ssl_write");

	// Write
    while((ret = mbedtls_ssl_write(&this->ssl, (const unsigned char*)connect_string.c_str(), len)) <= 0) {
        if (ret != MBEDTLS_ERR_SSL_WANT_READ && ret != MBEDTLS_ERR_SSL_WANT_WRITE) {
			puts("mbedtls_ssl_write failed");
            goto fail;
        }
    }

	puts("mbedtls_ssl_read");

    // Read
    do {
        len = sizeof(buf) - 1;
        memset(buf, 0, sizeof(buf));

        ret = mbedtls_ssl_read(&this->ssl, buf, len);
        if(ret == MBEDTLS_ERR_SSL_WANT_READ || ret == MBEDTLS_ERR_SSL_WANT_WRITE) {
            continue;
        }
        if(ret == MBEDTLS_ERR_SSL_PEER_CLOSE_NOTIFY) {
            break;
        }
        if(ret < 0) {
            mbedtls_printf("failed\n  ! mbedtls_ssl_read returned %d\n\n", ret);
            break;
        }
        if(ret == 0) {
            mbedtls_printf("\n\nEOF\n\n");
            break;
        }

        len = ret;
		printf("%d bytes read\n", len);
		printf("Data read: %s\n", (char*)buf);
    } while (1);

	this->connected = true;
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

}
