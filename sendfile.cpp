#include <cerrno>
#include <cstring>

#include "sendfile.h"

char curl_errbuf[CURL_ERROR_SIZE];

CURL *init_curl(void) {
	curl_global_init(CURL_GLOBAL_ALL);
	return curl_easy_init();	
}

static int readfile_callback(void *ptr, size_t size, size_t nmemb, void *userdata) {
	FILE *file = (FILE *)userdata;
	
	return fread(ptr, size, nmemb, file);
}

void send_file(CURL *curl, char *filename, char *url, char *user, char *password) {	
	FILE *file = fopen(filename, "r");
	
	if(file == NULL) {
		printf("Upload Error: Could't read '%s': %s.\n", filename, strerror(errno));
		return;
	}	
	
	int fsize;
	
	fseek(file, 0, SEEK_END);
	fsize = ftell(file);
	fseek(file, 0, SEEK_SET);
	
	int userlen = strlen(user);	
	char *buf = new char[userlen+strlen(password)+2];
	strcpy(buf, user);
	buf[userlen] = ':';
	strcpy(buf+userlen+1, password);
	
	curl_easy_setopt(curl, CURLOPT_USERPWD, buf);
	
	curl_easy_setopt(curl, CURLOPT_ERRORBUFFER, curl_errbuf);
	curl_easy_setopt(curl, CURLOPT_READFUNCTION, readfile_callback);
    curl_easy_setopt(curl, CURLOPT_UPLOAD, 1L);
    curl_easy_setopt(curl, CURLOPT_URL, url);
    curl_easy_setopt(curl, CURLOPT_READDATA, file);
    curl_easy_setopt(curl, CURLOPT_INFILESIZE_LARGE, (curl_off_t) fsize);
		
	if(curl_easy_perform(curl) != 0) {
		printf("Upload Error: %s.\n", curl_errbuf);
	}	
	
	curl_easy_reset(curl);
	
	fclose(file);
	delete[] buf;
}

void curl_cleanup(CURL* curl) {
	curl_easy_cleanup(curl);
	curl_global_cleanup();
}