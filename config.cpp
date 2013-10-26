#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <cerrno>

#include "config.h"

Config::Config() {
	memset(this, 0, sizeof(Config));
	
	setDefaults();
}

void Config::setDefaults(void ) {
	delete[] this->dest_url;
	delete[] this->dest_username;
	delete[] this->dest_password;
	
	this->dest_url = NULL;
	this->dest_username = NULL;
	this->dest_password = NULL;
	
	this->capture_time = 2000;
	
	this->daemon_port = 11222;
	this->daemon_timeout = 3;
}

Config::~Config() {
	delete[] this->dest_url;
	delete[] this->dest_username;
	delete[] this->dest_password;
}

static void conf_strval(char *str, void *dest) {
	char **d = (char **)dest;
	int l = strlen(str) + 1;
	delete[] *d;
	*d = new char[l];
	strcpy(*d, str);
}

static void conf_intval(char *str, void *dest) {
	int *d = (int *)dest;
	*d = atoi(str);
}

int Config::read(char *filename) {
	char buf[512];
	int buflen;
	
	int line = 0;
	char *p;
	
	struct ConfigKeyword {
		char key[CONF_MAX_KEY_LEN];
		void *dest;
		void (*setfunc)(char *, void *);
	} conf_section_destination[] = {
		{"url", &this->dest_url, conf_strval},
		{"username", &this->dest_username, conf_strval},
		{"password", &this->dest_password, conf_strval}},
	  conf_section_capture[] = {
		{"capture_time", &this->capture_time, conf_intval}},
	  conf_section_daemon[] = {
		{"port", &this->daemon_port, conf_intval},
		{"timeout", &this->daemon_timeout, conf_intval}
	};
	
	struct ConfigSection {
		char name[CONF_MAX_SECTION_LEN];
		ConfigKeyword *keys;
		int num;
	} conf_sections[] = {
		{"Destination", conf_section_destination, sizeof(conf_section_destination)/sizeof(ConfigKeyword)},
		{"Capture", conf_section_capture, sizeof(conf_section_capture)/sizeof(ConfigKeyword)},
		{"Daemon", conf_section_daemon, sizeof(conf_section_daemon)/sizeof(ConfigKeyword)}
	};
		
	
	int current_section = -1;
	
	FILE *cfgfile = fopen(filename, "r");
	if(cfgfile == NULL) {
		printf("Config Warning: couldn't open '%s': %s.\n", filename, strerror(errno));
		return 0;
	}
		
nextline: while(fgets(buf, sizeof(buf), cfgfile)) {
		line++;
		
		if(buf[0] == '#' || buf[0] == '\n')
			continue;
		
		buflen = strlen(buf)-1;
		buf[buflen] = 0;
		
		if(buf[0] == '[') {
			if(buf[buflen-1] != ']') {
				printf("Config Error: line %d: illegal section declaration. Missing ']'.\n", line);
				return 0;
			}
				
			
			for(unsigned int i = 0; i < sizeof(conf_sections)/sizeof(ConfigSection); i++) {
				if(strncmp(buf+1, conf_sections[i].name, buflen-2) == 0) {
					current_section = i;
					goto nextline;
				}
			}
			
			printf("Config Error: line %d: section does not exist.\n", line);
			return 0;
		}
		
		p = buf;
		while(*p != '\0' && *p != ' ')
			p++;
		
		if(*p != ' ' || (*p != '\0' && *(p+1) == '\0')) {
			printf("Config Error: line %d: expected value after key.\n", line);
			return 0;
		}
		
		if(current_section == -1) {
			printf("Config Error: line %d: keyword without section detected.\n", line);
			return 0;
		}
		
		for(int i = 0; i < conf_sections[current_section].num; i++) {
			if(strncmp(buf, conf_sections[current_section].keys[i].key, p-buf) == 0) {
				conf_sections[current_section].keys[i].setfunc(p+1, conf_sections[current_section].keys[i].dest);
				goto nextline;
			}
		}
		
		printf("Config Error: line %d: illegal keyword detected.\n", line);
		return 0;
	}
	
		
	fclose(cfgfile);
	
	printf("Successfully read '%s'\n", filename);
	
	return 1;

}