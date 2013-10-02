#ifndef SENDFILE_H
#define SENDFILE_H

#include <curl/curl.h>

CURL *init_curl(void);
void send_file(CURL *curl, char *filename, char *url, char *user, char *password);
void curl_cleanup(CURL *curl);

#endif