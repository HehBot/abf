CC := gcc
LD := gcc
TARGET_NAME := abf

BUILD_DIR := build
BIN_DIR := bin
SRC_DIR := src

TARGET_EXEC := $(BIN_DIR)/$(TARGET_NAME)

SRCS := $(shell find $(SRC_DIR) -name '*.c')
OBJS := $(SRCS:%=$(BUILD_DIR)/%.o)

DEPS := $(OBJS:.o=.d)

CCFLAGS := -Wall -Wpedantic -Werror -Wimplicit-fallthrough=3 -g -MMD -MP
LDFLAGS :=
LIBFLAGS :=

.PHONY: all clean

all: $(TARGET_EXEC)

$(TARGET_EXEC): $(OBJS)
	@mkdir -p $(dir $@)
	$(LD) $(LDFLAGS) -o $@ $^ $(LIBFLAGS)

$(BUILD_DIR)/%.c.o: %.c
	@mkdir -p $(dir $@)
	$(CC) -c $(CCFLAGS) -o $@ $<

clean:
	$(RM) -r $(BUILD_DIR) $(BIN_DIR)

.PHONY: all clean

-include $(DEPS)
