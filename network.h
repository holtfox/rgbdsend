#ifndef NETWORK_H
#define NETWORK_H

#include <curl/curl.h>

enum {
	COMMAND_MAXSIZE = 65536
};

class Command {
public:
	Command();
	Command(const char *header, void *data, uint32_t len);
	
	char header[4];
	uint32_t datalen;
	void *data;
};

class Daemon {
private:
	int receiveCommandSock(int sock, Command *buf);
	int sendCommandSock(int sock, const char*, void *data, uint32_t len);
	
	int recvAll(int sock, void *buf, size_t length);
public:
	Daemon();
	
	void init(int port, int timeout);
	void acceptConnection(void);
	int receiveCommand(Command *buf);
	int sendCommand(const char*, void *data, uint32_t len);
	void closeConnection(void);
	
	int sock;
	int port;
	
	int csock;
	
	int timeout;
	clock_t lastcommand;
	
	char buf[COMMAND_MAXSIZE];
};

CURL *init_curl(void);
void send_file(CURL *curl, char *filename, char *url, char *user, char *password);
void cleanup_curl(CURL *curl);

#endif