#ifndef CONFIG_H
#define CONFIG_H

enum {
	CONF_MAX_KEY_LEN = 64,
	CONF_MAX_SECTION_LEN = 64
};

class Config {
public:
	Config();
	~Config();
	
	void setDefaults(void);
	int read(char *filename);
	
	char *dest_url;
	char *dest_username;
	char *dest_password;
	
	int capture_time;
	int crop_left;
	int crop_right;
	int crop_top;
	int crop_bottom;
	
	int daemon_port;
	int daemon_timeout;
};

#endif