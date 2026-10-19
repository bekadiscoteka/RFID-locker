#ifndef WEB_H
#define WEB_H

/*
 * Start the HTTP server on port 80:
 *   GET    /api/log              the access log as JSON, newest first
 *   GET    /api/cards            the allowed cards as JSON
 *   POST   /api/cards            add or rename a card (form: uid, name)
 *   DELETE /api/cards?uid=...    remove a card
 *   GET    /                     the page that uses all of the above
 * POST and DELETE need the admin password (menuconfig) in an X-Password header.
 * Call after wifi_init_sta(), accesslog_init() and cards_init().
 */
void web_start(void);

#endif
