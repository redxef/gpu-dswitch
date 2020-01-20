#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <unistd.h>
#include <ctype.h>
#include <string.h>
#include <sys/io.h>

#define GMUX_PORT_SWITCH_DISPLAY	0x10
#define GMUX_PORT_SWITCH_DDC		0x28
#define GMUX_PORT_SWITCH_EXTERNAL	0x40
#define GMUX_PORT_DISCRETE_POWER	0x50
#define GMUX_PORT_VALUE			0xc2
#define GMUX_PORT_READ			0xd0
#define GMUX_PORT_WRITE			0xd4

#define GMUX_IOSTART			0x700

enum gpu_type {
	TYPE_UNKNOWN,
	TYPE_INTEGRATED,
	TYPE_DISCRETE,
};

enum gpu_state {
	STATE_UNKNOWN,
	STATE_ON,
	STATE_OFF
};

enum gpu_option {
	OPTION_UNKNOWN,
	OPTION_USE,
	OPTION_DONT_USE,
	OPTION_POWEROFF,
	OPTION_POWERON
};

void index_write(int port, uint8_t val) {
	outb(val, GMUX_IOSTART + GMUX_PORT_VALUE);
	outb((port & 0xff), GMUX_IOSTART + GMUX_PORT_WRITE);
}

uint8_t index_read(int port) {
	outb((port & 0xff), GMUX_IOSTART+ GMUX_PORT_READ);
	return inb(GMUX_IOSTART + GMUX_PORT_VALUE);
}

void gpu_set_state(enum gpu_type t, enum gpu_state s) {
	if (t == TYPE_DISCRETE && s == STATE_ON) {
		index_write(GMUX_PORT_DISCRETE_POWER, 1);
		index_write(GMUX_PORT_DISCRETE_POWER, 3);
	} else if (t == TYPE_DISCRETE && s == STATE_OFF) {
		index_write(GMUX_PORT_DISCRETE_POWER, 1);
		index_write(GMUX_PORT_DISCRETE_POWER, 0);
	}
}

void gpu_switch_to(enum gpu_type t) {
	if (t == TYPE_INTEGRATED) {
		index_write(GMUX_PORT_SWITCH_DDC, 1);
		index_write(GMUX_PORT_SWITCH_DISPLAY, 2);
		index_write(GMUX_PORT_SWITCH_EXTERNAL, 2);
	} else if (t == TYPE_DISCRETE) {
		index_write(GMUX_PORT_SWITCH_DDC, 2);
		index_write(GMUX_PORT_SWITCH_DISPLAY, 3);
		index_write(GMUX_PORT_SWITCH_EXTERNAL, 3);
	}
}

void strtolower(char *c, size_t len) {
	size_t i = 0;
	while (i < len && c[i] != '\0') {
		c[i] = tolower(c[i]);
		i++;
	}
}


enum gpu_option strtoopt(const char *str) {
	char strbuff[64];
	size_t strbufflen = sizeof(strbuff)/sizeof(*strbuff);

	strncpy(strbuff, str, strbufflen);
	strbuff[strbufflen-1] = '\0';
	strtolower(strbuff, strbufflen);
	
	if (strcmp(strbuff, "use") == 0)
		return OPTION_USE;
	else if (strcmp(strbuff, "poweroff") == 0)
		return OPTION_POWEROFF;
	else if (strcmp(strbuff, "poweron") == 0)
		return OPTION_POWERON;
	return OPTION_UNKNOWN;
}

const char *opttostr(enum gpu_option o) {
	switch(o) {
		default:
		case OPTION_UNKNOWN:
			return "UNKNOWN";
		case OPTION_USE:
			return "USE";
		case OPTION_DONT_USE:
			return "DONT USE";
		case OPTION_POWEROFF:
			return "POWEROFF";
		case OPTION_POWERON:
			return "POWERON";

	}
}

int main(int argc, char **argv) {
	int c, i;
	char strbuff[64];
	size_t strbufflen = sizeof(strbuff)/sizeof(*strbuff);

	enum gpu_type use_gpu = TYPE_UNKNOWN;
	enum gpu_state state_i = STATE_UNKNOWN;
	enum gpu_state state_d = STATE_UNKNOWN;
	enum gpu_option go_i = OPTION_UNKNOWN;
	enum gpu_option go_d = OPTION_UNKNOWN;

	while ((c = getopt(argc, argv, "i:d:h")) != -1) {
		switch (c) {
			default:
				return -1;
			case 'i':
				go_i = strtoopt(optarg);
				break;
			case 'd':
				go_d = strtoopt(optarg);
				break;
			case 'h':
				printf("See: https://wiki.archlinux.org/index.php/MacBookPro10,x#What_does_not_work_(early_August_2013,_3.10.3-1)");
				break;

	
		}
	}

	/* If only one gpu was specified, we default to letting the other one stay powered on. */
	if (go_i == OPTION_USE && go_d == OPTION_UNKNOWN) {
		go_d = OPTION_POWERON;
	} else if (go_d == OPTION_USE && go_i == OPTION_UNKNOWN) {
		go_i = OPTION_POWERON;
	}

	/*
	if (go_d == OPTION_UNKNOWN && go_i == OPTION_UNKNOWN) {
		fprintf(stderr, "Don't know what to do with gpu (i: %s, d: %s)\n", opttostr(go_i), opttostr(go_d));
		return -1;
	}
	*/
	printf("Applying config: integrated: %s, discrete: %s\n", opttostr(go_i), opttostr(go_d));

	if (iopl(3) < 0) {
		perror("No io permission");
		return -1;
	}
	
	if (go_i == OPTION_POWEROFF) state_i = STATE_OFF;
	else state_i = STATE_ON;
	if (go_d == OPTION_POWEROFF) state_d = STATE_OFF;
	else state_d = STATE_ON;
	if (go_i == OPTION_USE) use_gpu = TYPE_INTEGRATED;
	if (go_d == OPTION_USE) use_gpu = TYPE_DISCRETE;

	printf("Switching to GPU: %s\n", (use_gpu == TYPE_INTEGRATED)? "INTEGRATED" : "DISCRETE");
	gpu_switch_to(use_gpu);
	printf("new integrated GPU state: %s\n", (state_i == STATE_ON) ? "ON" : "OFF");
	gpu_set_state(TYPE_INTEGRATED, state_i);
	printf("new discrete GPU state: %s\n", (state_d == STATE_ON) ? "ON" : "OFF");
	gpu_set_state(TYPE_DISCRETE, state_d);

	return 0;
}