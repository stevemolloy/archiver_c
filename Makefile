CC=gcc
CFLAGS = -O0 -Wall -Wpedantic -Wextra -std=c18 -ggdb
CINCLUDES = -I`pg_config --includedir` -I./thirdparty/strptime/
CLIBS = -L`pg_config --libdir` -lpq

SRC = src
OBJ = objs

SRCS = $(wildcard $(SRC)/*.c)
OBJS = $(patsubst $(SRC)/%.c, $(OBJ)/%.o, $(SRCS))

BINDIR = bin
BIN = $(BINDIR)/archiver

all: $(BIN)

$(BIN): $(OBJS)
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) $^ -o $@ $(CLIBS)

$(OBJ)/%.o: $(SRC)/%.c
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) $(CINCLUDES) -c $< -o $@

clean:
	rm -rf $(BINDIR) $(OBJ)

$(OBJ):
	@mkdir -p $@

run: $(BIN)
	$(BIN)

