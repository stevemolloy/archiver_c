PG_INCLUDES := $(shell pg_config --includedir)
PG_LIBS := $(shell pg_config --libdir)
CFLAGS = -O0 -Wall -Wpedantic -Wextra -std=c11 -ggdb
CINCLUDES = -I$(PG_INCLUDES)
CLIBS = -L$(PG_LIBS) -lpq

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

