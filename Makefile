CC = gcc
CFLAGS = -m32 -g -Wall -Wextra -rdynamic -I./src
SRC_DIR = src
OBJ_DIR = obj
TARGET = aesop_game
RES_FILE = EYE.RES
START_OBJ = start

# List all C files in src/
SRCS = $(wildcard $(SRC_DIR)/*.c)
# Generate object file names in obj/
OBJS = $(SRCS:$(SRC_DIR)/%.c=$(OBJ_DIR)/%.o)

.PHONY: all build clean run

all: build

build: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $^

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c | $(OBJ_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(OBJ_DIR):
	mkdir -p $(OBJ_DIR)

clean:
	rm -rf $(OBJ_DIR) $(TARGET)

# logic to capture extra arguments for 'make run'
ifeq (run,$(firstword $(MAKECMDGOALS)))
  RUN_ARGS := $(wordlist 2,$(words $(MAKECMDGOALS)),$(MAKECMDGOALS))
  # This prevents make from complaining about the arguments being unknown targets
  $(eval $(RUN_ARGS):;@:)
endif

run: build
	cp $(TARGET) eobiii/
	@if [ -z "$(RUN_ARGS)" ]; then \
		cd eobiii && ./$(TARGET) $(RES_FILE) $(START_OBJ); \
	else \
		SEC=$(word 1,$(RUN_ARGS)); \
		OUT=$(word 2,$(RUN_ARGS)); \
		echo "Running $(TARGET) in eobiii for $$SEC seconds, saving output to $$OUT..."; \
		cd eobiii && timeout --foreground $${SEC}s ./$(TARGET) $(RES_FILE) $(START_OBJ) > ../$$OUT 2>&1 || true; \
		echo "Done. Output saved to $$OUT."; \
	fi
