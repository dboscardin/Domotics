CC = gcc
CFLAGS = -Wall -Wextra -pedantic -std=c11
TARGET = domotics
SRCS = code/main.c code/controller.c code/bulb.c
OBJS = $(SRCS:.c=.o)

.PHONY: all build clean run

all: build

# quando eseguiamo make, prima controlla se target è eseguibile, sennò lo builda
build: $(TARGET)

# Collega object files nell'exe finale
# $@ --> target corrente (domotics)
# $^ --> tutte le dependencies
$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $^

# Compila ogni file .c nel corrispondente .o
# $< --> prima dependency (main.c)
# $@ --> file .o da riprodurre
%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

#per partire da uno stato pulito
clean:
	rm -f $(OBJS) $(TARGET)

#prima compila se serve, poi esegue il programma
run: build
	./$(TARGET)

# A MAekfile contains
# A list of source files
# A list of object files (compiled source files)
# A list of dependencies, that specify which files depend on which other files
# A set of rules to compile and link the program

# run 'make' nel terminale per compilare
# solitamente si esegue da solo ma si possono specificare determinate regole (make clean)