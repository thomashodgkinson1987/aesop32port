CC = gcc
CFLAGS = -m32 -g -std=c17 -Wall -Wextra -Wpedantic -Wshadow -Wconversion -Wformat=2 -rdynamic -I./src $(shell sdl2-config --cflags)
LDFLAGS = -m32 -g $(shell sdl2-config --libs)

SRC_DIR = src
OBJ_DIR = obj
BIN_DIR = bin

TARGET_NAME = aesop
TARGET = $(BIN_DIR)/$(TARGET_NAME)

RES_FILE = EYE.RES
START_OBJ = start

SRCS = $(wildcard $(SRC_DIR)/*.c)
OBJS = $(patsubst $(SRC_DIR)/%.c, $(OBJ_DIR)/%.o, $(SRCS))

.PHONY: all build clean run

all: build

build: $(TARGET)

$(TARGET): $(OBJS) | $(BIN_DIR)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c | $(OBJ_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BIN_DIR) $(OBJ_DIR):
	mkdir -p $@

clean:
	rm -rf $(OBJ_DIR) $(BIN_DIR)

# logic to capture extra arguments for 'make run'
ifeq (run,$(firstword $(MAKECMDGOALS)))
  RUN_ARGS := $(wordlist 2,$(words $(MAKECMDGOALS)),$(MAKECMDGOALS))
  # This prevents make from complaining about the arguments being unknown targets
  $(eval $(RUN_ARGS):;@:)
endif

run: build
	cp $(TARGET) eobiii/
	@if [ -z "$(RUN_ARGS)" ]; then \
		cd eobiii && ./$(TARGET_NAME) $(RES_FILE) $(START_OBJ); \
	else \
		SEC=$(word 1,$(RUN_ARGS)); \
		OUT=$(word 2,$(RUN_ARGS)); \
		echo "Running $(TARGET_NAME) in eobiii for $$SEC seconds, saving output to $$OUT..."; \
		cd eobiii && timeout --foreground $${SEC}s ./$(TARGET_NAME) $(RES_FILE) $(START_OBJ) > ../$$OUT 2>&1 || true; \
		echo "Done. Output saved to $$OUT."; \
	fi
