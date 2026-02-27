CC = gcc
CFLAGS = -Wall -Wextra -pedantic -std=c99
SOURCES = main.c terminal.c editor.c output.c input.c
OBJECTS = $(SOURCES:.c=.o)
TARGET = tx

$(TARGET): $(OBJECTS)
	$(CC) $(CFLAGS) -o $(TARGET) $(OBJECTS)

%.o: %.c tx.h
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(TARGET) $(OBJECTS)
