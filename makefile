CC = gcc
CFLAGS = -Wall -Wextra -pthread -g
TARGET = myserver
TEST_TARGET = test_units
SRCS = myserver.c globals.c worker.c queue.c files.c
OBJS = $(SRCS:.c=.o)
HEADERS = globals.h worker.h queue.h files.h

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $(TARGET) $(OBJS)

%.o: %.c $(HEADERS)
	$(CC) $(CFLAGS) -c $< -o $@

# Unit tests target
test: $(TEST_TARGET)
	./$(TEST_TARGET)

$(TEST_TARGET): test_units.c globals.c queue.c worker.c files.c $(HEADERS)
	$(CC) $(CFLAGS) -o $(TEST_TARGET) test_units.c globals.c queue.c worker.c files.c

clean:
	rm -f $(OBJS) $(TARGET) $(TEST_TARGET)

.PHONY: all clean test
