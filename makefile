FILENAME := gpu-dswitch
SRCS     := main.c
OBJS     := $(SRCS:.c=.o)
CC       := gcc
CFLAGS   := -c -Wall -Wpedantic -Wextra
LFLAGS   := -le2p

$(FILENAME): $(OBJS)
	$(CC) $(LFLAGS) -o $@ $<

%.o: %.c
	$(CC) $(CFLAGS) -o $@ $<

clean:
	$(RM) $(FILENAME) $(OBJS)

install: $(FILENAME)
	cp $(FILENAME) /usr/local/bin/$(FILENAME)

.PHONY: clean
