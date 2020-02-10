#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <unistd.h>
#include <ctype.h>
#include <string.h>
#include <sys/io.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <linux/fs.h>
#include <e2p/e2p.h>


#define GMUX_PORT_SWITCH_DISPLAY                0x10
#define GMUX_PORT_SWITCH_DDC                    0x28
#define GMUX_PORT_SWITCH_EXTERNAL               0x40
#define GMUX_PORT_DISCRETE_POWER                0x50
#define GMUX_PORT_VALUE                         0xc2
#define GMUX_PORT_READ                          0xd0
#define GMUX_PORT_WRITE                         0xd4

#define GMUX_IOSTART                            0x700

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

#define ERR_CANT_STAT                           0x01
#define ERR_NO_REG_FILE                         0x02
#define ERR_CANT_GET_FLAGS                      0x04
#define ERR_CANT_SET_FLAGS                      0x08
#define ERR_NO_GPU_SELECTED                     0x10
#define ERR_NO_FILE_PERMISSION                  0x20
#define ERR_NO_SUDO                             0x40

void index_write(int port, uint8_t val) {
        outb(val, GMUX_IOSTART + GMUX_PORT_VALUE);
        outb((port & 0xff), GMUX_IOSTART + GMUX_PORT_WRITE);
}

uint8_t index_read(int port) {
        outb((port & 0xff), GMUX_IOSTART+ GMUX_PORT_READ);
        return inb(GMUX_IOSTART + GMUX_PORT_VALUE);
}

int gpu_set_state(enum gpu_type t, enum gpu_state s) {
        if (t == TYPE_DISCRETE && s == STATE_ON) {
                index_write(GMUX_PORT_DISCRETE_POWER, 1);
                index_write(GMUX_PORT_DISCRETE_POWER, 3);
        } else if (t == TYPE_DISCRETE && s == STATE_OFF) {
                index_write(GMUX_PORT_DISCRETE_POWER, 1);
                index_write(GMUX_PORT_DISCRETE_POWER, 0);
        }
        return 0;
}

uint8_t gpu_get_state(enum gpu_type t) {
        if (t == TYPE_DISCRETE) {
                return index_read(GMUX_PORT_DISCRETE_POWER);
        }
        return -1;
}

int gpu_switch_to(enum gpu_type t) {
        size_t i;
        const char *efi_file_path = "/sys/firmware/efi/efivars/gpu-power-prefs-fa4ce28d-b62f-4c99-9cc3-6815686e30f9";
        FILE *fp = NULL;

        struct stat st;
        unsigned long fsflags;

        char seq_arr[8] = {0x07, 0x00, 0x00, 0x00, 0x0f, 0x00, 0x00, 0x00};
        size_t seq_arrlen = sizeof(seq_arr)/sizeof(*seq_arr);


        /* this does not work on MacBook11,5 */
        /*
        if (t == TYPE_INTEGRATED) {
                index_write(GMUX_PORT_SWITCH_DDC, 1);
                index_write(GMUX_PORT_SWITCH_DISPLAY, 2);
                index_write(GMUX_PORT_SWITCH_EXTERNAL, 2);
        } else if (t == TYPE_DISCRETE) {
                index_write(GMUX_PORT_SWITCH_DDC, 2);
                index_write(GMUX_PORT_SWITCH_DISPLAY, 3);
                index_write(GMUX_PORT_SWITCH_EXTERNAL, 3);
        }
        */

        /* this does: */

        /* the following clears the write protection flag on the file */
        if (lstat(efi_file_path, &st) == -1) return ERR_CANT_STAT;
        if (!S_ISREG(st.st_mode) && !S_ISLNK(st.st_mode) && !S_ISDIR(st.st_mode)) return ERR_NO_REG_FILE;
        if (fgetflags(efi_file_path, &fsflags) == -1) {
                perror("failed to get flags of file");
                return ERR_CANT_GET_FLAGS;
        }
        fsflags &= ~EXT2_IMMUTABLE_FL;
        if (fsetflags(efi_file_path, fsflags) == -1) return ERR_CANT_SET_FLAGS;

        /* now write the magic byte sequence into the file, 1 for integrated, 0 for discrete */
        if (t == TYPE_INTEGRATED)
                seq_arr[4] = 1;
        else if (t == TYPE_DISCRETE)
                seq_arr[4] = 0;
        else {
                fprintf(stderr, "No target GPU specified, aborting\n");
                return ERR_NO_GPU_SELECTED;
        }
                

        fp = fopen(efi_file_path, "wb+");
        if (fp == NULL) {
                perror("Couldn't open file");
                return ERR_NO_FILE_PERMISSION;
        }
        for (i = 0; i < seq_arrlen; i++)
                fputc(seq_arr[i], fp);
        fflush(fp);
        fclose(fp);
        return 0;
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
        int c, ret = 0;

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
                                printf("NAME\n");
                                printf("gpu-dswitch\n\n");
                                printf("SYNOPSIS\n");
                                printf("gpu-dswitch [-i mode] [-d mode] [-h]\n\n");
                                printf("DESCRIPTION\n");
                                printf("This utility switches to the integrated or dedicated gpu on a MacBook and can turn the other one off (as of now only the dedicated).\n");
                                printf("See: https://wiki.archlinux.org/index.php/MacBookPro10,x#What_does_not_work_(early_August_2013,_3.10.3-1)\n");
                                printf("See: https://github.com/0xbb/gpu-switch\n\n");
                                printf("OPTIONS\n");
                                printf("-i use|poweroff|poweron\n");
                                printf("    Change the state of the internal gpu.\n");
                                printf("\n");
                                printf("-d use|poweroff|poweron\n");
                                printf("    Change the state of the dedicated gpu.\n");
                                printf("-h\n");
                                printf("    Display this help dialog\n");

                                return 0;
                }
        }

        if (go_i == OPTION_UNKNOWN && go_d == OPTION_UNKNOWN) {
                printf("Don't know what to do, maybe pass some options?\nRun `gpu-dswitch -h' for help.\n");
                return 0;
        }

        /* If only one gpu was specified, we default to letting the other one stay powered on. */
        if (go_i == OPTION_USE && go_d == OPTION_UNKNOWN) {
                go_d = OPTION_POWERON;
        } else if (go_d == OPTION_USE && go_i == OPTION_UNKNOWN) {
                go_i = OPTION_POWERON;
        }

        printf("Applying config: integrated: %s, discrete: %s\n", opttostr(go_i), opttostr(go_d));

        if (iopl(3) < 0) {
                perror("No io permission");
                return ERR_NO_SUDO;
        }
        
        if (go_i == OPTION_POWEROFF) state_i = STATE_OFF;
        else state_i = STATE_ON;
        if (go_d == OPTION_POWEROFF) state_d = STATE_OFF;
        else state_d = STATE_ON;
        if (go_i == OPTION_USE) use_gpu = TYPE_INTEGRATED;
        if (go_d == OPTION_USE) use_gpu = TYPE_DISCRETE;

        printf("Switching to GPU: %s\n", (use_gpu == TYPE_INTEGRATED)? "INTEGRATED" : "DISCRETE");
        ret |= gpu_switch_to(use_gpu) << 0;
        printf("new integrated GPU state: %s\n", (state_i == STATE_ON) ? "ON" : "OFF");
        ret |= gpu_set_state(TYPE_INTEGRATED, state_i) << 8;
        printf("new discrete GPU state: %s\n", (state_d == STATE_ON) ? "ON" : "OFF");
        ret |= gpu_set_state(TYPE_DISCRETE, state_d) << 16;

        if (ret != 0) {
                printf("Operation failed, sorry (%d)\n", ret);
                return -1;
        }
        return 0;
}
