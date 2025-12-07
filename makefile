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

# Run integration tests (requires server to be built)
integration-test: $(TARGET)
	@echo "Running integration tests..."
	@chmod +x test_server.sh
	./test_server.sh

# Quick test (server must be running)
quick-test:
	@chmod +x quick_test.sh
	./quick_test.sh

clean:
	rm -f $(OBJS) $(TARGET) $(TEST_TARGET) server_output.log test_output.tmp

.PHONY: all clean test integration-test quick-test
