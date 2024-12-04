SRC_DIR = src
INCLUDE_DIR = include
OBJ_DIR = obj
BIN_DIR = bin
TOOLS_DIR = tools
TESTS_DIR = test

CC = cc
#CFLAGS = -Wall -Wextra -g -I$(INCLUDE_DIR)
CFLAGS = -Oz -Wall -Werror -I$(INCLUDE_DIR)
SRC_FILES = $(wildcard $(SRC_DIR)/*.c)
OBJ_FILES = $(patsubst $(SRC_DIR)/%.c,$(OBJ_DIR)/%.o,$(SRC_FILES))
TOOL_SRC_FILES = $(wildcard $(TOOLS_DIR)/*.c)
TOOL_BINARIES = $(patsubst $(TOOLS_DIR)/%.c, $(BIN_DIR)/%, $(TOOL_SRC_FILES))

all: $(TOOL_BINARIES)

test: $(OBJ_FILES) test/test.c test/bench.c
	$(CC) $(CFLAGS) -I $(SRC_DIR) `pkg-config --cflags --libs sqlite3` `pkg-config --cflags --libs expat` test/test.c -o test/test obj/*.o
	$(CC) $(CFLAGS) -I $(SRC_DIR) `pkg-config --cflags --libs sqlite3` `pkg-config --cflags --libs expat` test/bench.c -o test/bench obj/*.o

format:
	clang-format -i $(SRC_DIR)/*.c $(INCLUDE_DIR)/*.h $(TOOLS_DIR)/*.c $(TESTS_DIR)/*.c

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c
	@mkdir -p $(OBJ_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BIN_DIR)/%: $(TOOLS_DIR)/%.c $(OBJ_FILES)
	@mkdir -p $(BIN_DIR)
	$(CC) $(CFLAGS) -lsqlite3 -lexpat -o $@ $< $(OBJ_FILES)

clean:
	rm -rf $(OBJ_DIR) $(BIN_DIR)

.PHONY: all clean

