FILENAME := gpu-dswitch
SRCS     := main.c
OBJS     := $(SRCS:.c=.o)
CC       := gcc
CFLAGS   := -c -Wall -Wpedantic -Wextra
LFLAGS   := 

$(FILENAME): $(OBJS)
	$(CC) $(LFLAGS) -o $@ $<

%.o: %.c
	$(CC) $(CFLAGS) -o $@ $<

