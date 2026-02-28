CC          := gcc
CFLAGS      := -Wall -Wextra -std=c99
TARGET      := build/tx
SRC_DIR     := src
BUILD_DIR   := build

SRCS        := $(wildcard $(SRC_DIR)/*.c)

OBJS        := $(SRCS:$(SRC_DIR)/%.c=$(BUILD_DIR)/%.o)

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(OBJS) -o $(TARGET) -Llib -lm

$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

clean:
	rm -rf $(BUILD_DIR)/*.o $(TARGET)

.PHONY: all clean
