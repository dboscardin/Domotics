CC = gcc
CFLAGS = -Wall -Wextra -pedantic -std=c11 -D_POSIX_C_SOURCE=200809L -D_DEFAULT_SOURCE
LDLIBS = -lpthread

TARGET = domotics
SRCS = code/main.c code/controller.c code/bulb.c code/window.c code/fridge.c code/ipc.c code/hub.c code/device.c code/timer.c
OBJS = $(SRCS:.c=.o)

ARGS = scenario.txt

.PHONY: all build clean run

# Default target
all: build

# Builds the main executable
build: $(TARGET)

# Links object files into the final executable
# $@ --> current target (domotics)
# $^ --> all dependencies
$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $^ $(LDLIBS)

# Compiles each .c file into the corresponding .o file
# $< --> first dependency (main.c)
# $@ --> .o file to produce
%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

# To start from a clean state and clean up any artifacts
clean:
	rm -f $(OBJS) $(TARGET)
	rm -f /tmp/domotica_* 2>/dev/null || true
# Compiles first if needed, then runs the program
run: build
		echo "============================================="; \
		echo " Avvio domotics con scenario: $(ARGS)"; \
		echo "============================================="; \
		cat $(ARGS) - | ./$(TARGET); \
