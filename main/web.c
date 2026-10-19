#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "esp_http_server.h"
#include "esp_log.h"

#include "accesslog.h"
#include "admin_secrets.h"
#include "cards.h"
#include "web.h"

static const char *TAG = "web";

/* index.html, linked into the firmware by EMBED_TXTFILES (null-terminated). */
extern const char index_html[] asm("_binary_index_html_start");

/* "DEADBEEF" -> { 0xDE, 0xAD, 0xBE, 0xEF }. Exactly 8 hex digits. */
static bool parse_uid(const char *s, uint8_t uid[RC522_UID_LEN])
{
	if (strlen(s) != 2 * RC522_UID_LEN)
		return false;
	for (size_t i = 0; i < 2 * RC522_UID_LEN; i++)
		if (!isxdigit((unsigned char)s[i]))
			return false;

	for (size_t i = 0; i < RC522_UID_LEN; i++) {
		char byte[3] = { s[2 * i], s[2 * i + 1], '\0' };
		uid[i] = strtol(byte, NULL, 16);
	}
	return true;
}

/* Decode a form value in place: '+' -> ' ', "%XX" -> byte. */
static void url_decode(char *s)
{
	char *out = s;
	for (; *s; s++) {
		if (*s == '+') {
			*out++ = ' ';
		} else if (*s == '%' && isxdigit((unsigned char)s[1]) && isxdigit((unsigned char)s[2])) {
			char byte[3] = { s[1], s[2], '\0' };
			*out++ = strtol(byte, NULL, 16);
			s += 2;
		} else {
			*out++ = *s;
		}
	}
	*out = '\0';
}

/* Changes need the admin password in the X-Password header. Sends 401 if not. */
static bool authorized(httpd_req_t *req)
{
	char pass[64];
	if (httpd_req_get_hdr_value_str(req, "X-Password", pass, sizeof pass) == ESP_OK &&
	    strcmp(pass, ADMIN_PASS) == 0)
		return true;

	httpd_resp_send_err(req, HTTPD_401_UNAUTHORIZED, "Wrong admin password");
	return false;
}

/* Read the whole request body into buf as a string. */
static esp_err_t read_body(httpd_req_t *req, char *buf, size_t size)
{
	if (req->content_len >= size)
		return ESP_FAIL;

	size_t got = 0;
	while (got < req->content_len) {
		int r = httpd_req_recv(req, buf + got, req->content_len - got);
		if (r <= 0)
			return ESP_FAIL;
		got += r;
	}
	buf[got] = '\0';
	return ESP_OK;
}

static esp_err_t get_index(httpd_req_t *req)
{
	httpd_resp_set_type(req, "text/html");
	return httpd_resp_sendstr(req, index_html);
}

/* {"now":<epoch>,"entries":[{"t":<epoch>,"uid":"DEADBEEF","ok":true},...]} */
static esp_err_t get_log(httpd_req_t *req)
{
	accesslog_entry_t e[ACCESSLOG_LEN];
	size_t n = accesslog_snapshot(e, ACCESSLOG_LEN);
	char buf[96];

	httpd_resp_set_type(req, "application/json");
	snprintf(buf, sizeof buf, "{\"now\":%lld,\"entries\":[", (long long)time(NULL));
	httpd_resp_sendstr_chunk(req, buf);

	for (size_t i = 0; i < n; i++) {
		snprintf(buf, sizeof buf, "%s{\"t\":%lld,\"uid\":\"%02X%02X%02X%02X\",\"ok\":%s}",
			 i ? "," : "", (long long)e[i].time,
			 e[i].uid[0], e[i].uid[1], e[i].uid[2], e[i].uid[3],
			 e[i].granted ? "true" : "false");
		httpd_resp_sendstr_chunk(req, buf);
	}

	httpd_resp_sendstr_chunk(req, "]}");
	return httpd_resp_sendstr_chunk(req, NULL);	/* end of response */
}

/* {"cards":[{"uid":"DEADBEEF","name":"..."},...]}. Names are JSON-safe (see cards.h). */
static esp_err_t get_cards(httpd_req_t *req)
{
	card_t c[CARDS_MAX];
	size_t n = cards_snapshot(c, CARDS_MAX);
	char buf[96];

	httpd_resp_set_type(req, "application/json");
	httpd_resp_sendstr_chunk(req, "{\"cards\":[");

	for (size_t i = 0; i < n; i++) {
		snprintf(buf, sizeof buf, "%s{\"uid\":\"%02X%02X%02X%02X\",\"name\":\"%s\"}",
			 i ? "," : "", c[i].uid[0], c[i].uid[1], c[i].uid[2], c[i].uid[3],
			 c[i].name);
		httpd_resp_sendstr_chunk(req, buf);
	}

	httpd_resp_sendstr_chunk(req, "]}");
	return httpd_resp_sendstr_chunk(req, NULL);
}

/* POST /api/cards, form body "uid=DEADBEEF&name=Front+door": add or rename. */
static esp_err_t post_card(httpd_req_t *req)
{
	if (!authorized(req))
		return ESP_OK;

	char body[160], hex[16], name[96] = "";
	uint8_t uid[RC522_UID_LEN];

	if (read_body(req, body, sizeof body) != ESP_OK ||
	    httpd_query_key_value(body, "uid", hex, sizeof hex) != ESP_OK ||
	    !parse_uid(hex, uid))
		return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "UID must be 8 hex digits");

	/* Name is optional; a too-long one is cut by cards_add() anyway. */
	httpd_query_key_value(body, "name", name, sizeof name);
	url_decode(name);

	esp_err_t err = cards_add(uid, name);
	if (err == ESP_ERR_NO_MEM)
		return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Card list is full");
	if (err != ESP_OK)
		return httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Could not save");

	ESP_LOGI(TAG, "card %s allowed (\"%s\")", hex, name);
	return httpd_resp_sendstr(req, "ok");
}

/* DELETE /api/cards?uid=DEADBEEF */
static esp_err_t delete_card(httpd_req_t *req)
{
	if (!authorized(req))
		return ESP_OK;

	char query[32], hex[16];
	uint8_t uid[RC522_UID_LEN];

	if (httpd_req_get_url_query_str(req, query, sizeof query) != ESP_OK ||
	    httpd_query_key_value(query, "uid", hex, sizeof hex) != ESP_OK ||
	    !parse_uid(hex, uid))
		return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "UID must be 8 hex digits");

	esp_err_t err = cards_remove(uid);
	if (err == ESP_ERR_NOT_FOUND)
		return httpd_resp_send_err(req, HTTPD_404_NOT_FOUND, "Card not in the list");
	if (err != ESP_OK)
		return httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Could not save");

	ESP_LOGI(TAG, "card %s removed", hex);
	return httpd_resp_sendstr(req, "ok");
}

void web_start(void)
{
	httpd_handle_t srv = NULL;
	httpd_config_t cfg = HTTPD_DEFAULT_CONFIG();
	ESP_ERROR_CHECK(httpd_start(&srv, &cfg));

	const httpd_uri_t routes[] = {
		{ .uri = "/",		.method = HTTP_GET,	.handler = get_index },
		{ .uri = "/api/log",	.method = HTTP_GET,	.handler = get_log },
		{ .uri = "/api/cards",	.method = HTTP_GET,	.handler = get_cards },
		{ .uri = "/api/cards",	.method = HTTP_POST,	.handler = post_card },
		{ .uri = "/api/cards",	.method = HTTP_DELETE,	.handler = delete_card },
	};
	for (size_t i = 0; i < sizeof routes / sizeof routes[0]; i++)
		ESP_ERROR_CHECK(httpd_register_uri_handler(srv, &routes[i]));

	ESP_LOGI(TAG, "server started on port %d", cfg.server_port);
}
