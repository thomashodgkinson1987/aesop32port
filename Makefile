CC = gcc

# 32-bit SDL2 location. Defaults to the vendored build in the parent
# directory (see ../reference/build-32bit-sdl2.sh). Override on the
# command line to use another build, e.g.:
#   make build SDL2_DIR=/usr   (if you have 32-bit SDL2 dev files there)
# Paths are quoted so workspaces containing spaces are supported.
SDL2_DIR ?= $(abspath ../SDL2_32)

CFLAGS = -g -m32 -std=c17 -Wall -Wextra -Wpedantic -Wshadow -Wconversion -Wformat=2 -rdynamic -I./src -I"$(SDL2_DIR)/include/SDL2" -D_REENTRANT
LDFLAGS = -m32 -L"$(SDL2_DIR)/lib" -Wl,-rpath,"$(SDL2_DIR)/lib" -lSDL2

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
