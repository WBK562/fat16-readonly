# Compiler settings
CC = gcc
CFLAGS = -Wall -Wextra -std=c11 -g

# Target executable name
TARGET = fat16_parser

# Source files (dostosuj nazwy plików jeśli masz inne)
SRCS = main.c

# Object files
OBJS = $(SRCS:.c=.o)

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $(TARGET) $(OBJS)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJS) $(TARGET)

.PHONY: all clean
