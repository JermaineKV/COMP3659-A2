CC = gcc
CFLAGS = -Wall -Wextra -pthread -g
TARGET = myserver
SRCS = myserver.c globals.c worker.c queue.c files.c
OBJS = $(SRCS:.c=.o)
HEADERS = globals.h worker.h queue.h files.h

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $(TARGET) $(OBJS)

%.o: %.c $(HEADERS)
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJS) $(TARGET)

.PHONY: all clean
