CC          := gcc
CFLAGS      := -Wall -Wextra -std=c99 -g -Iinclude
TARGET      := build/tx
SRC_DIR     := src
BUILD_DIR   := build

SRCS        := $(wildcard $(SRC_DIR)/*.c)
OBJS        := $(SRCS:$(SRC_DIR)/%.c=$(BUILD_DIR)/%.o)
DEPS        := $(OBJS:.o=.d)

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(OBJS) -o $(TARGET)

$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -MMD -MP -c $< -o $@

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

clean:
	rm -rf $(BUILD_DIR)/*.o $(BUILD_DIR)/*.d $(TARGET)

run: all
	./$(TARGET) $(ARGS)

-include $(DEPS)

.PHONY: all clean run
