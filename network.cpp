#include <cerrno>
#include <cstring>
#include <sys/socket.h>
#include <sys/select.h>
#include <stdint.h>
#include <unistd.h>
#include <time.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "network.h"

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
	
	int urllen = strlen(url);
	int urlbufsize = strlen(filename) + urllen;
	
	if(url[urllen-1] != '/')
		urlbufsize++;
			
	char *urlbuf = new char[urlbufsize];
	strcpy(urlbuf,url);
	if(url[urllen-1] != '/') {
		urlbuf[urllen] = '/';
		urlbuf[urllen+1] = 0;
	}
	
	strcat(urlbuf,filename);
	
	curl_easy_setopt(curl, CURLOPT_USERPWD, buf);
	
	curl_easy_setopt(curl, CURLOPT_ERRORBUFFER, curl_errbuf);
	curl_easy_setopt(curl, CURLOPT_READFUNCTION, readfile_callback);
    curl_easy_setopt(curl, CURLOPT_UPLOAD, 1L);
    curl_easy_setopt(curl, CURLOPT_URL, urlbuf);
    curl_easy_setopt(curl, CURLOPT_READDATA, file);
    curl_easy_setopt(curl, CURLOPT_INFILESIZE_LARGE, (curl_off_t) fsize);
	curl_easy_setopt(curl, CURLOPT_USE_SSL, CURLUSESSL_TRY);
	curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
	curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);
	
	if(curl_easy_perform(curl) != 0) {
		printf("Upload Error: %s.\n", curl_errbuf);
	} else {
		printf("Upload successful.\n");
	}
	
	curl_easy_reset(curl);
	
	fclose(file);
	delete[] buf;
	delete[] urlbuf;
}

void curl_cleanup(CURL* curl) {
	curl_easy_cleanup(curl);
	curl_global_cleanup();
}

Daemon::Daemon() {
	this->sock = -1;
	this->port = 0;
	
	this->csock = -1;
}

void Daemon::init(int port, int timeout) {
	sockaddr_in name;
	this->sock = socket(AF_INET,SOCK_STREAM,0);
	this->port = port;
	
	name.sin_family = AF_INET;
	name.sin_addr.s_addr = htonl(INADDR_ANY);
	name.sin_port = htons(port);
	
	bind(this->sock, (sockaddr *)&name, sizeof(name));
	listen(this->sock, 2);
	
	this->timeout = timeout;
	
	printf("Listening on port %d\n", port);
}

void Daemon::acceptConnection(void) {
	int cs;
	
	cs = accept(this->sock, 0, 0);
	
	Command c;
	int r;
	do {
		r = receiveCommandSock(cs, &c);
		if(r == 0)
			return;
	} while(r == 2); // filter keep alives	
	
	if(strncmp(c.header, "subs", 4) == 0) {
		if(this->csock == -1) {
			int rc = sendCommandSock(cs, "okay", 0, 0);
			if(rc) {
				this->csock = cs;
				printf("Client connected.\n");
			}
		} else {
			sendCommandSock(cs, "fail", 0, 0);
		}
	}	
}

void Daemon::closeConnection(void) {
	printf("Client disconnected.\n");
	close(this->csock);
	this->csock = -1;
}

int Daemon::recvAll(int sock, void *buf, size_t length) {
	uint readbytes = 0;
	int lastrecv = clock();
	while(readbytes < length) {
		int n = recv(sock, ((char *)buf)+readbytes, length-readbytes, 0);
		if(n == -1 || clock() - lastrecv > timeout*CLOCKS_PER_SEC)
			return -1;
				
		if(n > 0) {
			readbytes += n;
			lastrecv = clock();			
		}
	}
	
	return length;
}

int Daemon::receiveCommandSock(int sock, Command *buf) {
	uint n;
	
	if(sock == -1)
		return 0;
	
	n = recvAll(sock, this->buf, 4);
	if(n != 4)
		return 0;
		
	strncpy(buf->header, this->buf, 4);
	buf->datalen = 0;
	buf->data = 0;
	
// as of now, the client doesn't have any commands with data blocks, so we're
// done.
	
	if(strcmp(buf->header, "aliv") == 0) { // filter keep-alives.
		return 2;
	}	
		
	return 1;
}

int Daemon::sendCommandSock(int sock, const char* cmd, void *data, uint32_t len) {
	uint n;
	
	if(sock == -1)
		return 0;
	
	strncpy(this->buf, cmd, 4);
	*((uint32_t*)(this->buf+4)) = htonl(len);
	
	if(len > COMMAND_MAXSIZE-8)
		return 0;
	
	memcpy(this->buf+8, data, len);
	
	n = send(sock, this->buf, 4+(len != 0)*(4+len), 0);
		
	if(n != 4+(len != 0)*(4+len))
		return 0;
	
	return 1;
}

int Daemon::receiveCommand(Command *buf) {
	return receiveCommandSock(this->csock, buf);
}
	
int Daemon::sendCommand(const char *cmd, void *data, uint32_t len) {
	return sendCommandSock(this->csock, cmd, data, len);
}

Command::Command() {
	memset(this, 0, sizeof(Command));
}

Command::Command(const char *header, void *data, uint32_t len) {
	strncpy(this->header, header, 4);
	this->data = data;
	this->datalen = len;
}