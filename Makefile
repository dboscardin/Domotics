CC = gcc
CFLAGS = -Wall -Wextra -pedantic -std=c11 -D_POSIX_C_SOURCE=200809L -D_DEFAULT_SOURCE
LDLIBS = -lpthread

TARGET = domotics
SRCS = code/main.c code/controller.c code/bulb.c code/window.c code/fridge.c code/ipc.c code/hub.c code/device.c code/timer.c
OBJS = $(SRCS:.c=.o)

ARGS ?=

.PHONY: all build clean run

# defaul target
all: build

# costruisce l'eseguibile principale
build: $(TARGET)

# Collega object files nell'exe finale
# $@ --> target corrente (domotics)
# $^ --> tutte le dependencies
$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $^ $(LDLIBS)

# Compila ogni file .c nel corrispondente .o
# $< --> prima dependency (main.c)
# $@ --> file .o da riprodurre
%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

# per partire da uno stato pulito e restituire eventuali errori
clean:
	rm -f $(OBJS) $(TARGET)
	rm -f /tmp/domotica_* 2>/dev/null || true
# prima compila se serve, poi esegue il programma
run: build
		@if [ -n "$(ARGS)" ]; then \
		echo "============================================="; \
		echo " Avvio domotics con scenario: $(ARGS)"; \
		echo "============================================="; \
		cat $(ARGS) - | ./$(TARGET); \
	else \
		echo "Avvio normale..."; \
		./$(TARGET); \
	fi
